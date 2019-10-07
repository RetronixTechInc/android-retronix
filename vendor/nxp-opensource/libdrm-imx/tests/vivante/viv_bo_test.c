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


/*
 * Copyright © 2015 Canonical Ltd. (Maarten Lankhorst)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/ioctl.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "xf86drm.h"
#include "vivante_bo.h"

static int import_fd = -1;

static void *
openclose(void *dev)
{
    struct drm_vivante *drm = dev;
    struct drm_vivante_bo *bo;
    int i;

    for (i = 0; i < 100000; ++i) {
        if (!drm_vivante_bo_import_from_fd(drm, import_fd, &bo))
            drm_vivante_bo_destroy(bo);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    drmVersionPtr version;
    const char *device = NULL;
    int err, fd1, fd2;
    struct drm_vivante *drm1, *drm2;
    struct drm_vivante_bo *bo;
    pthread_t t1, t2;

    if (argc < 2) {
        fd1 = drmOpenWithType("vivante", NULL, DRM_NODE_RENDER);
        if (fd1 >= 0)
            fd2 = drmOpenWithType("vivante", NULL, DRM_NODE_RENDER);
    } else {
        device = argv[1];

        fd1 = open(device, O_RDWR);
        if (fd1 >= 0)
            fd2 = open(device, O_RDWR);
        else
            fd2 = fd1 = -errno;
    }

    if (fd1 < 0) {
        fprintf(stderr, "Opening 1st vivante render node failed with %i\n", fd1);
        return device ? -fd1 : 77;
    }

    if (fd2 < 0) {
        fprintf(stderr, "Opening 2nd vivante render node failed with %i\n", -errno);
        return errno;
    }

    version = drmGetVersion(fd1);
    if (version) {
        printf("Version: %d.%d.%d\n", version->version_major,
               version->version_minor, version->version_patchlevel);
        printf("  Name: %s\n", version->name);
        printf("  Date: %s\n", version->date);
        printf("  Description: %s\n", version->desc);

        drmFreeVersion(version);
    }

    err = drm_vivante_create(fd1, &drm1);
    if (!err)
        err = drm_vivante_create(fd2, &drm2);
    if (err < 0)
        return 1;

    err = drm_vivante_bo_create(drm2, 0, 1920*1080*4, &bo);
    if (!err)
        err = drm_vivante_bo_export_to_fd(bo, &import_fd);

    if (!err) {
        pthread_create(&t1, NULL, openclose, (void*)drm1);
        pthread_create(&t2, NULL, openclose, (void*)drm1);
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    drm_vivante_bo_destroy(bo);

    drm_vivante_close(drm2);
    drm_vivante_close(drm1);
    if (device) {
        close(fd2);
        close(fd1);
    } else {
        drmClose(fd2);
        drmClose(fd1);
    }

    return 0;
}
