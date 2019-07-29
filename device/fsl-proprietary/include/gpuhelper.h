/*
 *  Copyright (C) 2014 Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/*
 *	gpuhelper.h
 *	This header file declare all gpu helper interface for Android platform
 *	History :
 *	Date(y.m.d)        Author            Version        Description
 *	2014-06-23         Li Xianzhong      0.1            Created
*/

#ifndef __GPU_HELPER_H__
#define __GPU_HELPER_H__

// create/destroy private handle.
private_handle_t* graphic_handle_create(int fd, int size, int flags);
void graphic_handle_destroy(private_handle_t* hnd);
int graphic_handle_validate(buffer_handle_t handle);

// graphic buffer alloc/free relevant.
int graphic_buffer_alloc(int w, int h, int format, int usage,
            int stride, size_t size, private_handle_t** handle);
int graphic_buffer_free(private_handle_t* hnd);
int graphic_buffer_register(private_handle_t* hnd);
int graphic_buffer_unregister(private_handle_t* hnd);
int graphic_buffer_lock(private_handle_t* hnd);
int graphic_buffer_unlock(private_handle_t* hnd);

// used for framebuffer to do wrap/register/unwrap.
int graphic_buffer_wrap(private_handle_t* hnd,
         int width, int height, int format, int stride,
         unsigned long phys, void* vaddr);
int graphic_buffer_register_wrap(private_handle_t* hnd,
         unsigned long phys, void* vaddr);
int graphic_buffer_unwrap(private_handle_t* hnd);

#endif
