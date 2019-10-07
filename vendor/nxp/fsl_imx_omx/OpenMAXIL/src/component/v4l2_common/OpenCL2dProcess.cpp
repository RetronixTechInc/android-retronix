/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "OpenCL2dProcess.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <opencl-2d.h>



#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

OpenCL2dProcess::OpenCL2dProcess()
{
    cnt = 0;
    g2d_handle = NULL;
    // Open g2d
    if(cl_g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
        LOG_ERROR("cl_g2d_open failed");
        g2d_handle = NULL;
    }
}

OMX_ERRORTYPE OpenCL2dProcess::Process(DmaBufferHdr *inBufHdlr, OMX_BUFFERHEADERTYPE *outBufHdlr)
{
    int ret = 0;

    struct cl_g2d_surface src;
    struct cl_g2d_surface dst;
    long src_ptr = 0;
    long dst_ptr = 0;
    OMX_PTR pPhyAddr = NULL;

    if(inBufHdlr == NULL || outBufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(g2d_handle == NULL)
        return OMX_ErrorInsufficientResources;

    if(eBufMode != PROCESS3_BUF_DMA_IN_OMX_OUT)
        return OMX_ErrorBadParameter;

    if(!inBufHdlr->bReadyForProcess){
        outBufHdlr->nFilledLen = 0;
        outBufHdlr->nFlags = 0;
        outBufHdlr->nTimeStamp = -1;
        LOG_DEBUG("OpenCL2dProcess::Process inBufHdlr->nFilledLen==0\n");
        return OMX_ErrorNone;
    }

    cnt++;

    LOG_DEBUG("OpenCL2dProcess::Process in=%d,%d,out=%p\n",inBufHdlr->plane[0].fd, inBufHdlr->plane[1].fd, outBufHdlr->pBuffer);

    fsl_osal_memset(&src, 0, sizeof(struct cl_g2d_surface));
    fsl_osal_memset(&dst, 0, sizeof(struct cl_g2d_surface));


    src.format = CL_G2D_NV12_TILED;
    src.usage = CL_G2D_DEVICE_MEMORY;
    
    src.planes[0] = static_cast<int>(inBufHdlr->plane[0].vaddr);
    src.planes[1] = static_cast<int>(inBufHdlr->plane[1].vaddr);
    LOG_DEBUG("OpenCL2dProcess::Process src planes[0]=%x,planes[1]=%x\n",src.planes[0],src.planes[1]);

    src.left = 0;
    src.top = 0;
    src.right = sInFormat.width;
    src.bottom = sInFormat.height;

    src.width = sInFormat.width;
    src.height = sInFormat.height;
    src.stride = sInFormat.stride;
    src.stride = ((src.stride + 255) & ~255);


    dst.format = CL_G2D_NV12;
    dst.usage = CL_G2D_DEVICE_MEMORY;

    dst_ptr = (intptr_t)outBufHdlr->pBuffer;

    dst.planes[0] = dst_ptr;
    dst.planes[1] = dst_ptr + sOutFormat.width * sOutFormat.height;

    dst.left = 0;
    dst.top = 0;
    dst.right = sOutFormat.width;
    dst.bottom = sOutFormat.height;

    dst.stride = sOutFormat.stride;
    dst.width = sOutFormat.width;
    dst.height = sOutFormat.height;

    ret = cl_g2d_blit(g2d_handle, &src, &dst);
    cl_g2d_flush(g2d_handle);
    cl_g2d_finish(g2d_handle);

    if(ret == 0){
        outBufHdlr->nFilledLen = sOutFormat.bufferSize;
        outBufHdlr->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        outBufHdlr->nTimeStamp = -1;
        LOG_DEBUG("OpenCL2dProcess::Process ts=%lld,len=%d,flag=%x\n",outBufHdlr->nTimeStamp,outBufHdlr->nFilledLen,outBufHdlr->nFlags);
        return OMX_ErrorNone;
    }else{
        LOG_DEBUG("OpenCL2dProcess::Process OMX_ErrorUndefined ret=%d",ret);
        return OMX_ErrorUndefined;
    }
}
OMX_ERRORTYPE OpenCL2dProcess::Process(OMX_BUFFERHEADERTYPE *inBufHdlr, DmaBufferHdr *outBufHdlr)
{
    return OMX_ErrorNotImplemented;
}
OpenCL2dProcess::~OpenCL2dProcess()
{
    if(g2d_handle != NULL)
        cl_g2d_close(g2d_handle);
    return;
}

