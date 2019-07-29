/*
 *  Copyright (C) 2013-2015 Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/*
 *  g2dExt.h
 *  g2dExt.h is a extension to g2d.h. It's private and should not be exported to customer.
 *  So any extension which is private should be here, others will still be updated in g2d.h.
 *  History :
 *  Date(y.m.d)        Author            Version        Description
 *
*/

#ifndef __G2DEXT_H__
#define __G2DEXT_H__

#include "g2d.h"

#ifdef __cplusplus
extern "C"  {
#endif

enum g2d_tiling
{
    G2D_LINEAR              = 0x1,
    G2D_TILED               = 0x2,
    G2D_SUPERTILED          = 0x4,
};

struct g2d_surfaceEx
{
    struct g2d_surface base;
    enum g2d_tiling tiling;
};

int g2d_blitEx(void *handle, struct g2d_surfaceEx *srcEx, struct g2d_surfaceEx *dstEx);

int g2d_set_clipping(void *handle, int left, int top, int right, int bottom);

#if defined(ANDROID)
#include <system/window.h>
//this is private API and not exported in g2d.h
//fsl hwcomposer need get alignment information for gralloc buffer
int get_aligned_size(buffer_handle_t hnd, int *width, int *height);
int get_flip_offset(buffer_handle_t hnd, int *offset);

void* g2d_getRenderBuffer(void *handle, void *BufferHandle);
unsigned int g2d_postBuffer(void *handle, void* PostBuffer);

int g2d_getTiling(buffer_handle_t hnd, enum g2d_tiling* tile);
enum g2d_format g2d_alterFormat(buffer_handle_t hnd, enum g2d_format format);
#endif


#ifdef __cplusplus
}
#endif

#endif
