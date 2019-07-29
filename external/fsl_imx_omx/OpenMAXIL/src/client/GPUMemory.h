/**
 *  Copyright (c) 2010-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */


#ifndef GPUMemory_h
#define GPUMemory_h

#include "OMX_Core.h"

typedef struct {
	OMX_PTR phy_addr;	/*!physical memory address allocated */
	OMX_PTR virt_uaddr;	/*!virtual user space address */
} GPUMEMADDR;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

OMX_PTR CreateGPUMeoryContext();
OMX_ERRORTYPE DestroyGPUMeoryContext(OMX_PTR Context);
GPUMEMADDR GetGPUMBuffer(OMX_PTR Context, OMX_U32 nSize, OMX_U32 nNum);
OMX_ERRORTYPE FreeGPUMBuffer(OMX_PTR Context, OMX_PTR pBuffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
/* File EOF */
