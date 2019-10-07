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


#ifndef __DRM_VIVANTE_H__
#define __DRM_VIVANTE_H__

#include <stdint.h>
#include <stdlib.h>
#include <vivante_drm.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct drm_vivante_bo;
struct drm_vivante;

struct drm_vivante_bo_tiling {
    uint32_t tiling_mode;
    uint32_t ts_mode;
    uint64_t clear_value;
};

/* caller owns the driver fd. */
int drm_vivante_create(int fd, struct drm_vivante **drmp);
void drm_vivante_close(struct drm_vivante *drm);

int drm_vivante_bo_create(struct drm_vivante *drm,
            uint32_t flags, uint32_t size, struct drm_vivante_bo **bop);

/* create bo with auxillary tile-status bo. */
int drm_vivante_bo_create_with_ts(struct drm_vivante *drm,
            uint32_t flags, uint32_t size, struct drm_vivante_bo **bop);

/* caller owns the fd. */
int drm_vivante_bo_export_to_fd(struct drm_vivante_bo *bo, int *pfd);
/* caller still owns the fd upon return. */
int drm_vivante_bo_import_from_fd(struct drm_vivante *drm, int fd,
        struct drm_vivante_bo **bop);

void drm_vivante_bo_destroy(struct drm_vivante_bo *bo);

/* do not close the handle directly. */
int drm_vivante_bo_get_handle(struct drm_vivante_bo *bo, uint32_t *handle);

int drm_vivante_bo_mmap(struct drm_vivante_bo *bo, void **vaddr);
int drm_vivante_bo_munmap(struct drm_vivante_bo *bo);

int drm_vivante_bo_query(struct drm_vivante_bo *bo,
            uint32_t param, uint64_t *value);

int drm_vivante_bo_set_tiling(struct drm_vivante_bo *bo,
            const struct drm_vivante_bo_tiling *tiling);
int drm_vivante_bo_get_tiling(struct drm_vivante_bo *bo,
            struct drm_vivante_bo_tiling *tiling);

/* output inc'ed timestamp, optional. */
int drm_vivante_bo_inc_timestamp(struct drm_vivante_bo *bo,
            uint64_t *timestamp);
int drm_vivante_bo_get_timestamp(struct drm_vivante_bo *bo,
            uint64_t *timestamp);

int drm_vivante_bo_ref_node(struct drm_vivante_bo *bo,
            uint32_t *node, uint32_t *ts_node);

#if defined(__cplusplus)
}
#endif

#endif /* __DRM_VIVANTE_H__ */
