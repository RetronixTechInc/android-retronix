/****************************************************************************
*
*    Copyright 2012 - 2017 Vivante Corporation, Santa Clara, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/


#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/mman.h>

#include <xf86drm.h>

#include <vivante_drm.h>

#include "vivante_bo.h"

struct drm_vivante
{
    /* driver fd. */
    int fd;

    struct drm_vivante_bo *bo_list;
    pthread_mutex_t mutex;
};

struct drm_vivante_bo
{
    struct drm_vivante *drm;

    uint32_t handle;

    /* export prime fd if any */
    int fd;

    uint32_t flags;
    uint32_t size;

    void *vaddr;

    int refcount;
    struct drm_vivante_bo *next;
};


int drm_vivante_create(int fd, struct drm_vivante **drmp)
{
    int supported = 0;
    drmVersionPtr version;
    struct drm_vivante *drm;

    version = drmGetVersion(fd);
    if (!version)
        return -ENOMEM;

    if (!strncmp(version->name, "vivante", version->name_len))
        supported = 1;

    drmFreeVersion(version);

    if (!supported)
        return -ENOTSUP;

    drm = calloc(1, sizeof(struct drm_vivante));
    if (!drm)
        return -ENOMEM;

    drm->fd = fd;
    drm->bo_list = NULL;
    pthread_mutex_init(&drm->mutex, NULL);

    *drmp = drm;
    return 0;
}

void drm_vivante_close(struct drm_vivante *drm)
{
    free(drm);
}

static inline void drm_vivante_bo_add_locked(struct drm_vivante *drm,
                    struct drm_vivante_bo *bo)
{
    bo->refcount = 1;
    bo->next = drm->bo_list;
    drm->bo_list = bo;
}

static void drm_vivante_bo_add(struct drm_vivante *drm,
                    struct drm_vivante_bo *bo)
{
    pthread_mutex_lock(&drm->mutex);
    drm_vivante_bo_add_locked(drm, bo);
    pthread_mutex_unlock(&drm->mutex);
}

static struct drm_vivante_bo *drm_vivante_bo_lookup(
                    struct drm_vivante *drm, uint32_t handle)
{
    struct drm_vivante_bo *bo;
    int err;

    for (bo = drm->bo_list; bo != NULL; bo = bo->next) {
        if (bo->handle == handle)
            break;
    }
    if (bo)
        bo->refcount++;
    return bo;
}

/* returns refcount. */
static int drm_vivante_bo_decref(struct drm_vivante *drm,
                struct drm_vivante_bo *bo)
{
    int ret;

    pthread_mutex_lock(&drm->mutex);
    ret = --bo->refcount;

    if (ret > 0)
        goto out;

    /* unlink bo. */
    if (bo == drm->bo_list)
        drm->bo_list = bo->next;
    else {
        struct drm_vivante_bo *prev = NULL;
        for (prev = drm->bo_list; prev != NULL; prev = prev->next) {
            if (prev->next == bo) {
                prev->next = bo->next;
                break;
            }
        }
    }

out:
    pthread_mutex_unlock(&drm->mutex);
    return ret;
}

static int drm_vivante_bo_init(struct drm_vivante *drm,
                struct drm_vivante_bo **bop)
{
    struct drm_vivante_bo *bo;

    bo = calloc(1, sizeof(*bo));
    if (!bo)
        return -ENOMEM;

    bo->drm = drm;
    bo->fd = -1;
    bo->vaddr = NULL;

    *bop = bo;
    return 0;
}

int drm_vivante_bo_create(struct drm_vivante *drm,
            uint32_t flags, uint32_t size, struct drm_vivante_bo **bop)
{
    int err = 0;
    struct drm_vivante_bo *bo;
    struct drm_viv_gem_create args = {
        .flags = flags,
        .size  = size
    };

    if (size == 0)
        return -EINVAL;

    if (!drm || !bop)
        return -EINVAL;

    err = drm_vivante_bo_init(drm, &bo);
    if (err) {
        return err;
    }

    if (drmIoctl(drm->fd, DRM_IOCTL_VIV_GEM_CREATE, &args)) {
        free(bo);
        return -errno;
    }
    bo->handle = args.handle;
    bo->flags = flags;
    bo->size = size;

    drm_vivante_bo_add(drm, bo);
    *bop = bo;
    return 0;
}

int drm_vivante_bo_create_with_ts(struct drm_vivante *drm,
            uint32_t flags, uint32_t size, struct drm_vivante_bo **bop)
{
    int err = 0;
    uint32_t ts_handle = 0;
    struct drm_vivante_bo *bo;
    struct drm_viv_gem_create args = {
        .flags = flags,
        .size  = size
    };
    struct drm_gem_close close_args;
    struct drm_viv_gem_attach_aux aux_args;
    const uint32_t valid_ts_flags = DRM_VIV_GEM_CONTIGUOUS |
                        DRM_VIV_GEM_SECURE | DRM_VIV_GEM_CMA_LIMIT;

    if (size == 0)
        return -EINVAL;

    if (!drm || !bop)
        return -EINVAL;

    err = drm_vivante_bo_init(drm, &bo);
    if (err)
        return err;

    if (drmIoctl(drm->fd, DRM_IOCTL_VIV_GEM_CREATE, &args)) {
        err = -errno;
        goto err_close;
    }
    bo->handle = args.handle;

    /* alloc ts handle, size is master buffer size / 256, align up to 64B. */
    args.flags = flags & valid_ts_flags;
    args.size  = ((size >> 8) + 0x3f) & ~0x3f;
    if (drmIoctl(drm->fd, DRM_IOCTL_VIV_GEM_CREATE, &args)) {
        err = -errno;
        goto err_close;
    }
    ts_handle = args.handle;

    /* ref ts_handle in master handle. */
    aux_args.handle = bo->handle;
    aux_args.ts_handle = ts_handle;
    if (drmIoctl(drm->fd, DRM_IOCTL_VIV_GEM_ATTACH_AUX, &aux_args)) {
        err = -errno;
        goto err_close;
    }

    /* Now ts was attached to master, destroy it now. */
    close_args.handle = ts_handle;
    if (drmIoctl(drm->fd, DRM_IOCTL_GEM_CLOSE, &close_args)) {
        err = -errno;
        goto err_close;
    }
    ts_handle = 0;

    bo->flags = flags;
    bo->size = size;

    drm_vivante_bo_add(drm, bo);
    *bop = bo;
    return 0;

err_close:
    if (bo->handle) {

        if (ts_handle) {
            close_args.handle = ts_handle;
            drmIoctl(drm->fd, DRM_IOCTL_GEM_CLOSE, &close_args);
        }

        close_args.handle = bo->handle;
        drmIoctl(drm->fd, DRM_IOCTL_GEM_CLOSE, &close_args);
    }

    free(bo);
    return err;
}

int drm_vivante_bo_export_to_fd(struct drm_vivante_bo *bo, int *pfd)
{
    if (!bo || !pfd)
        return -EINVAL;

    if (bo->fd < 0) {
        int fd;
        if (drmPrimeHandleToFD(bo->drm->fd, bo->handle, O_RDWR, &fd))
            return -errno;
        bo->fd = fd;
    }

    *pfd = bo->fd;
    return 0;
}

int drm_vivante_bo_import_from_fd(struct drm_vivante *drm, int fd,
            struct drm_vivante_bo **bop)
{
    int err;
    uint64_t size;
    uint32_t handle = 0;
    struct drm_vivante_bo *bo = NULL;

    if (!drm || !bop || fd < 0)
        return -EINVAL;

    pthread_mutex_lock(&drm->mutex);

    if (drmPrimeFDToHandle(drm->fd, fd, &handle)) {
        err = -errno;
        goto err_close;
    }

    bo = drm_vivante_bo_lookup(drm, handle);
    if (bo) {
        pthread_mutex_unlock(&drm->mutex);
        *bop = bo;
        return 0;
    }

    err = drm_vivante_bo_init(drm, &bo);
    if (err)
        goto err_close;
    bo->handle = handle;

    err = drm_vivante_bo_query(bo, DRM_VIV_GEM_PARAM_SIZE, &size);
    if (err)
        goto err_close;
    bo->size = (uint32_t)size;

    drm_vivante_bo_add_locked(drm, bo);
    pthread_mutex_unlock(&drm->mutex);

    *bop = bo;
    return 0;

err_close:
    pthread_mutex_unlock(&drm->mutex);

    if (handle > 0) {
        struct drm_gem_close close_args = {
            .handle = handle,
        };
        drmIoctl(drm->fd, DRM_IOCTL_GEM_CLOSE, &close_args);
    }
    free(bo);

    return err;
}

void drm_vivante_bo_destroy(struct drm_vivante_bo *bo)
{
    struct drm_gem_close close_args;

    if (!bo)
        return;

    if (drm_vivante_bo_decref(bo->drm, bo) != 0)
        return;

    if (bo->vaddr) {
        drm_vivante_bo_munmap(bo);
    }

    close_args.handle = bo->handle;
    drmIoctl(bo->drm->fd, DRM_IOCTL_GEM_CLOSE, &close_args);

    free(bo);
}

int drm_vivante_bo_get_handle(struct drm_vivante_bo *bo, uint32_t *handle)
{
    if (!bo || !handle)
        return -EINVAL;

    *handle = bo->handle;
    return 0;
}

static int clean_bo_cache(struct drm_vivante_bo *bo)
{
    struct drm_viv_gem_cache args = {
        .op = DRM_VIV_GEM_CLEAN_CACHE,
        .handle = bo->handle,
        .logical = (uint64_t)(uintptr_t)bo->vaddr,
        .bytes = bo->size
    };

    if (drmIoctl(bo->drm->fd, DRM_IOCTL_VIV_GEM_CACHE, &args))
        return -errno;

    return 0;
}


int drm_vivante_bo_mmap(struct drm_vivante_bo *bo, void **vaddr)
{
    struct drm_viv_gem_lock args;

    if (!bo || !vaddr)
        return -EINVAL;

    /* already locked */
    if (bo->vaddr)
        return -EPERM;
        
    args.handle = bo->handle;
    args.cacheable = (bo->flags & DRM_VIV_GEM_CACHED) ? 1 : 0;
    if (drmIoctl(bo->drm->fd, DRM_IOCTL_VIV_GEM_LOCK, &args))
        return -errno;

    bo->vaddr = (void *)(uintptr_t)args.logical;

    *vaddr = bo->vaddr;
    return 0;
}

int drm_vivante_bo_munmap(struct drm_vivante_bo *bo)
{
    struct drm_viv_gem_unlock args;

    if (!bo || !bo->vaddr)
        return -EINVAL;

    args.handle = bo->handle;
    if (bo->flags & DRM_VIV_GEM_CACHED) {
        int err = clean_bo_cache(bo);
        if (err)
            return err;
    }

    if (drmIoctl(bo->drm->fd, DRM_IOCTL_VIV_GEM_UNLOCK, &args))
        return -errno;

    bo->vaddr = NULL;
    return 0;
}

int drm_vivante_bo_query(struct drm_vivante_bo *bo,
            uint32_t param, uint64_t *value)
{
    struct drm_viv_gem_query args = {
        .param = param,
    };

    if (!bo || !value)
        return -EINVAL;

    args.handle = bo->handle;
    if (drmIoctl(bo->drm->fd, DRM_IOCTL_VIV_GEM_QUERY, &args))
        return -errno;

    *value = args.value;
    return 0;
}

int drm_vivante_bo_set_tiling(struct drm_vivante_bo *bo,
            const struct drm_vivante_bo_tiling *tiling)
{
    struct drm_viv_gem_set_tiling args;

    if (!bo || !tiling)
        return -EINVAL;

    args = (struct drm_viv_gem_set_tiling) {
        .handle = bo->handle,
        .tiling_mode = tiling->tiling_mode,
        .ts_mode = tiling->ts_mode,
        .clear_value = tiling->clear_value,
    };

    if (drmIoctl(bo->drm->fd, DRM_IOCTL_VIV_GEM_SET_TILING, &args))
        return -errno;

    return 0;
}

int drm_vivante_bo_get_tiling(struct drm_vivante_bo *bo,
            struct drm_vivante_bo_tiling *tiling)
{
    struct drm_viv_gem_get_tiling args;

    if (!bo || !tiling)
        return -EINVAL;

    args.handle = bo->handle;
    if (drmIoctl(bo->drm->fd, DRM_IOCTL_VIV_GEM_GET_TILING, &args))
        return -errno;

    tiling->tiling_mode = args.tiling_mode;
    tiling->ts_mode = args.ts_mode;
    tiling->clear_value = args.clear_value;

    return 0;
}

static inline int inc_bo_timestamp(struct drm_vivante_bo *bo,
                        uint32_t inc, uint64_t *timestamp)
{
    struct drm_viv_gem_timestamp args = {
        .handle = bo->handle,
        .inc = inc,
    };
    if (drmIoctl(bo->drm->fd, DRM_IOCTL_VIV_GEM_TIMESTAMP, &args))
        return -errno;

    if (timestamp)
        *timestamp = args.timestamp;
    return 0;
}

int drm_vivante_bo_inc_timestamp(struct drm_vivante_bo *bo,
            uint64_t *timestamp)
{
    if (!bo)
        return -EINVAL;
    return inc_bo_timestamp(bo, 1, timestamp);
}

int drm_vivante_bo_get_timestamp(struct drm_vivante_bo *bo,
            uint64_t *timestamp)
{
    if (!bo || !timestamp)
        return -EINVAL;
    return inc_bo_timestamp(bo, 0, timestamp);
}

int drm_vivante_bo_ref_node(struct drm_vivante_bo *bo,
            uint32_t *node, uint32_t *ts_node)
{
    struct drm_viv_gem_ref_node args;

    if (!bo || !node || !ts_node)
        return -EINVAL;

    args.handle = bo->handle;
    if (drmIoctl(bo->drm->fd, DRM_IOCTL_VIV_GEM_REF_NODE, &args))
        return -errno;

    *node = args.node;
    *ts_node = args.ts_node;
    return 0;
}

