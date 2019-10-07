/**
 *  Copyright (c) 2010-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */


#include "GPUMemory.h"
#include "Mem.h"
#include "Log.h"
#include "g2d.h"

#define MAX_BUFFER_NUM 32

typedef struct {
    OMX_U32 nTotalSize;
    OMX_U32 nBufferSize;
    OMX_U32 nBufferNum;
    OMX_PTR VAddrBase;
    OMX_PTR PAddrBase;
    OMX_PTR CpuAddrBase;
    GPUMEMADDR BufferSlot[MAX_BUFFER_NUM];
    OMX_BOOL bSlotAllocated[MAX_BUFFER_NUM];
}GPUMCONTEXT;

static OMX_ERRORTYPE InitGPUMemAllocator(GPUMCONTEXT *Context, OMX_U32 nBufferSize, OMX_U32 nBufferNum)
{
    int bytes, total_bytes;
    struct g2d_buf *gbuf;

    LOG_DEBUG("InitGPUMemAllocator: size %d, num %d\n", nBufferSize, nBufferNum);

    OMX_S32 pagesize = getpagesize();

    if(nBufferNum > MAX_BUFFER_NUM)
        return OMX_ErrorBadParameter;

	if ((!nBufferNum) || (!nBufferSize)) {
		LOG_ERROR("Error! InitGPUMemAllocator: Invalid parameters");
		return OMX_ErrorBadParameter;
	}

	bytes = (nBufferSize + pagesize-1) & ~(pagesize - 1);

    total_bytes = bytes * nBufferNum + PAGE_SIZE;

    gbuf = g2d_alloc(total_bytes, 0);
    if(!gbuf) {
        LOG_ERROR("g2d allocator failed to alloc buffer with size %d",  total_bytes);
        return OMX_ErrorHardware;
    }

    Context->nBufferSize = bytes;
    Context->nBufferNum = nBufferNum;
    Context->nTotalSize = total_bytes;

    Context->VAddrBase = (OMX_PTR)(unsigned long)gbuf->buf_vaddr;
    Context->PAddrBase = (OMX_PTR)(unsigned long)gbuf->buf_paddr;
    Context->CpuAddrBase = (OMX_PTR)gbuf;
    Context->VAddrBase = (OMX_PTR)(((unsigned long)Context->VAddrBase + PAGE_SIZE -1) & ~(PAGE_SIZE -1));
    Context->PAddrBase = (OMX_PTR)(((unsigned long)Context->PAddrBase + PAGE_SIZE -1) & ~(PAGE_SIZE -1));
    fsl_osal_memset(Context->VAddrBase, 0, bytes * nBufferNum);

    LOG_DEBUG("<gpu> alloc handle: 0x%x, paddr: 0x%x, vaddr: 0x%x",
		(unsigned long)gbuf, (unsigned long)Context->PAddrBase,
		(unsigned long)Context->VAddrBase);

    for(OMX_U32 i=0; i<nBufferNum; i++) {
        Context->BufferSlot[i].virt_uaddr= (OMX_U8 *)Context->VAddrBase + Context->nBufferSize * i;
        Context->BufferSlot[i].phy_addr = (OMX_U8 *)Context->PAddrBase + Context->nBufferSize * i;
        Context->bSlotAllocated[i] = OMX_FALSE;
    }

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE DeInitGPUMemAllocator(GPUMCONTEXT *Context)
{
    if(Context == NULL)
        return OMX_ErrorBadParameter;

    struct g2d_buf *gbuf = (struct g2d_buf *)Context->CpuAddrBase;
    if(gbuf) {
        if(g2d_free(gbuf) != 0) {
           LOG_ERROR("%s: g2d allocator failed to free buffer 0x%x", __FUNCTION__, (unsigned long)gbuf);
           return OMX_ErrorHardware;
        }
        LOG_DEBUG("<gpu> free handle: 0x%x, paddr: 0x%x, vaddr: 0x%x",
            (unsigned long)gbuf, (unsigned long)Context->PAddrBase,
            (unsigned long)Context->VAddrBase);
    }

    fsl_osal_memset(Context, 0, sizeof(GPUMCONTEXT));

    return OMX_ErrorNone;
}

OMX_PTR CreateGPUMeoryContext()
{
    OMX_PTR Context = NULL;

    Context = (OMX_PTR)FSL_MALLOC(sizeof(GPUMCONTEXT));
    if(Context == NULL) {
        LOG_ERROR("Failed allocate GPUMemory context.\n");
        return NULL;
    }
    fsl_osal_memset(Context, 0, sizeof(GPUMCONTEXT));

    return Context;
}

OMX_ERRORTYPE DestroyGPUMeoryContext(OMX_PTR Context)
{
    if(Context != NULL) {
        DeInitGPUMemAllocator((GPUMCONTEXT*)Context);
        FSL_FREE(Context);
    }

    return OMX_ErrorNone;
}

GPUMEMADDR GetGPUMBuffer(OMX_PTR Context, OMX_U32 nSize, OMX_U32 nNum)
{
    GPUMCONTEXT *pContext = (GPUMCONTEXT*)Context;
    GPUMEMADDR sMemAddr;

    LOG_DEBUG("GetGPUMBuffer, context %p, size %d, num %d\n", Context, nSize, nNum);

    fsl_osal_memset(&sMemAddr, 0, sizeof(GPUMEMADDR));

    if(pContext == NULL)
        return sMemAddr;

    if(pContext->VAddrBase == 0)
        if(OMX_ErrorNone != InitGPUMemAllocator(pContext, nSize, nNum))
            return sMemAddr;

    OMX_S32 i;
    for(i=0; i<(OMX_S32)pContext->nBufferNum; i++)
        if(pContext->bSlotAllocated[i] != OMX_TRUE)
            break;

    if(i == (OMX_S32)pContext->nBufferNum) {
        LOG_ERROR("No freed GPU memory for allocate.\n");
        return sMemAddr;
    }

    pContext->bSlotAllocated[i] = OMX_TRUE;

    LOG_DEBUG("GPUM allocate: %p\n", pContext->BufferSlot[i].virt_uaddr);

    return pContext->BufferSlot[i];
}

OMX_ERRORTYPE FreeGPUMBuffer(OMX_PTR Context, OMX_PTR pBuffer)
{
    GPUMCONTEXT *pContext = (GPUMCONTEXT*)Context;

    if(pContext == NULL)
        return OMX_ErrorBadParameter;

    LOG_DEBUG("GPUM free: %p\n", pBuffer);

    OMX_S32 i;
    for(i=0; i<(OMX_S32)pContext->nBufferNum; i++)
        if(pBuffer == pContext->BufferSlot[i].virt_uaddr)
            break;

    if(i == (OMX_S32)pContext->nBufferNum) {
        LOG_ERROR("Bad GPUMemory address for free.\n");
        return OMX_ErrorBadParameter;
    }

    pContext->bSlotAllocated[i] = OMX_FALSE;

    return OMX_ErrorNone;
}

/* File EOF */
