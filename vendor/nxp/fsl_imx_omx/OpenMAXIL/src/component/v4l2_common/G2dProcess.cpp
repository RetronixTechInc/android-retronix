/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "G2dProcess.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include "g2d.h"
#include "g2dExt.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

G2dProcess::G2dProcess()
{
    cnt = 0;
    g2d_handle = NULL;

    // Open g2d
    if(g2d_open(&g2d_handle) == -1 || g2d_handle == NULL) {
        LOG_ERROR("cl_g2d_open failed");
        g2d_handle = NULL;
    }
}

OMX_ERRORTYPE G2dProcess::Process(DmaBufferHdr *inBufHdlr, OMX_BUFFERHEADERTYPE *outBufHdlr)
{
    int ret = 0;

    struct g2d_surfaceEx src;
    struct g2d_surfaceEx dst;
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
        LOG_DEBUG("G2dProcess::Process inBufHdlr->nFilledLen==0\n");
        return OMX_ErrorNone;
    }

    cnt++;

    LOG_DEBUG("G2dProcess::Process in=%d,%d,out=%p\n",inBufHdlr->plane[0].fd, inBufHdlr->plane[1].fd, outBufHdlr->pBuffer);

    fsl_osal_memset(&src, 0, sizeof(struct g2d_surfaceEx));
    fsl_osal_memset(&dst, 0, sizeof(struct g2d_surfaceEx));


    src.base.format = G2D_NV12;
    
    src.base.planes[0] = (int)inBufHdlr->plane[0].phys;
    src.base.planes[1] = (int)inBufHdlr->plane[1].phys;
    LOG_DEBUG("G2dProcess::Process src planes[0]=%p,planes[1]=%p\n",src.base.planes[0],src.base.planes[1]);

    src.base.left = 0;
    src.base.top = 0;
    src.base.right = sInFormat.width;
    src.base.bottom = sInFormat.height;

    src.base.width = sInFormat.width;
    src.base.height = sInFormat.height;
    src.base.stride = sInFormat.stride;
    src.tiling = G2D_AMPHION_TILED;
    if(inBufHdlr->flag & DMA_BUF_INTERLACED){
        src.tiling = (enum g2d_tiling)(src.tiling | G2D_AMPHION_INTERLACED);
        if(cnt == 1)
            LOG_DEBUG("it is interlaced source");
    }

    dst.tiling = G2D_LINEAR;
    dst.base.format = (enum g2d_format)getG2dFormat((enum OMX_COLOR_FORMATTYPE)sOutFormat.format);

    pPhyAddr = NULL;
    if(OMX_ErrorNone == GetHwBuffer(outBufHdlr->pBuffer,&pPhyAddr)){
        dst_ptr = (unsigned long)(pPhyAddr);
    }else
        return OMX_ErrorUndefined;

    dst.base.planes[0] = dst_ptr;
    //dst.base.planes[1] = dst_ptr + sOutFormat.width * sOutFormat.height;
    //dst.base.planes[2] = dst.base.planes[1] + sOutFormat.width * sOutFormat.height / 4;
    //LOG_DEBUG("G2dProcess::Process dst planes[0]=%p,planes[1]=%p,planes[2]=%p\n",dst.base.planes[0],dst.base.planes[1],dst.base.planes[2]);

    dst.base.left = 0;
    dst.base.top = 0;
    dst.base.right = sOutFormat.width;
    dst.base.bottom = sOutFormat.height;

    dst.base.stride = sOutFormat.stride;
    dst.base.width = sOutFormat.width;
    dst.base.height = sOutFormat.height;

    ret = g2d_blitEx(g2d_handle, &src, &dst);
    g2d_finish(g2d_handle);

    if(ret == 0){
        outBufHdlr->nFilledLen = sOutFormat.bufferSize;
        outBufHdlr->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        outBufHdlr->nTimeStamp = -1;
        if(inBufHdlr->flag & DMA_BUF_EOS)
            outBufHdlr->nFlags |= OMX_BUFFERFLAG_EOS;
        LOG_DEBUG("G2dProcess::Process ts=%lld,len=%d,flag=%x\n",outBufHdlr->nTimeStamp,outBufHdlr->nFilledLen,outBufHdlr->nFlags);
        return OMX_ErrorNone;
    }else{
        LOG_DEBUG("G2dProcess::Process OMX_ErrorUndefined ret=%d",ret);
        return OMX_ErrorUndefined;
    }
}
typedef struct{
    g2d_format g2d_fmt;
    OMX_COLOR_FORMATTYPE omx_fmt;
}G2D_FMT_TABLE;

static G2D_FMT_TABLE color_format_table[]={
{G2D_RGB565, OMX_COLOR_Format16bitRGB565},
{G2D_YUYV, OMX_COLOR_FormatYCbYCr}
};

OMX_U32 G2dProcess::getG2dFormat(OMX_COLOR_FORMATTYPE color_format)
{
    OMX_U32 i=0;
    OMX_BOOL bGot=OMX_FALSE;
    OMX_U32 out = 0;
    for(i = 0; i < sizeof(color_format_table)/sizeof(G2D_FMT_TABLE);i++){
        if(color_format == color_format_table[i].omx_fmt){
            bGot = OMX_TRUE;
            out = color_format_table[i].g2d_fmt;
            break;
        }
    }

    if(bGot)
        return out;
    else
        return 0;
}

OMX_ERRORTYPE G2dProcess::Process(OMX_BUFFERHEADERTYPE *inBufHdlr, DmaBufferHdr *outBufHdlr)
{
    int ret = 0;
    long src_ptr = 0;
    OMX_PTR pInBufferPhy = NULL;

    struct g2d_surfaceEx src;
    struct g2d_surfaceEx dst;

    if(inBufHdlr == NULL || outBufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(g2d_handle == NULL)
        return OMX_ErrorInsufficientResources;

    if(eBufMode != PROCESS3_BUF_OMX_IN_DMA_OUT)
        return OMX_ErrorBadParameter;

    if(inBufHdlr->nFilledLen == 0){
        LOG_DEBUG("G2dProcess::Process inBufHdlr->nFilledLen==0\n");
        return OMX_ErrorNone;
    }

    cnt++;

    LOG_DEBUG("G2dProcess::Process in=%p,out=%d,%d\n",inBufHdlr->pBuffer, outBufHdlr->plane[0].fd, outBufHdlr->plane[1].fd );

    fsl_osal_memset(&src, 0, sizeof(struct g2d_surfaceEx));
    fsl_osal_memset(&dst, 0, sizeof(struct g2d_surfaceEx));

    src.tiling = G2D_LINEAR;
    src.base.format = (enum g2d_format)getG2dFormat((enum OMX_COLOR_FORMATTYPE)sInFormat.format);

    if(inBufHdlr->pInputPortPrivate != NULL){
        src_ptr = (unsigned long)inBufHdlr->pInputPortPrivate;
    }else{
        if(OMX_ErrorNone!=GetHwBuffer((OMX_PTR)inBufHdlr->pBuffer,&pInBufferPhy)){
            LOG_ERROR("G2dProcess failed to get physical address");
            return OMX_ErrorUndefined;
        }
        src_ptr = (unsigned long)pInBufferPhy;
    }

    src.base.planes[0] = (int)src_ptr;

    LOG_DEBUG("G2dProcess::Process src planes[0]=%p\n",src.base.planes[0]);

    src.base.left = 0;
    src.base.top = 0;
    src.base.right = sInFormat.width;
    src.base.bottom = sInFormat.height;

    src.base.width = sInFormat.width;
    src.base.height = sInFormat.height;
    src.base.stride = sInFormat.stride;


    dst.tiling = G2D_TILED;
    dst.base.format = G2D_NV12;

    dst.base.planes[0] = outBufHdlr->plane[0].phys;
    dst.base.planes[1] = outBufHdlr->plane[1].phys;

    LOG_DEBUG("G2dProcess::Process dst planes[0]=%p,planes[1]=%p,planes[2]=%p\n",dst.base.planes[0],dst.base.planes[1],dst.base.planes[2]);

    dst.base.left = 0;
    dst.base.top = 0;
    dst.base.right = sOutFormat.width;
    dst.base.bottom = sOutFormat.height;

    dst.base.stride = sOutFormat.stride;
    dst.base.width = sOutFormat.width;
    dst.base.height = sOutFormat.height;

    LOG_DEBUG("G2dProcess::Process g2d_blitEx 0\n");

    ret = g2d_blitEx(g2d_handle, &src, &dst);
    LOG_DEBUG("G2dProcess::Process g2d_blitEx 1 cnt=%d\n",cnt);
    g2d_finish(g2d_handle);

    if(ret == 0){
        outBufHdlr->bReadyForProcess = OMX_TRUE;
        if(inBufHdlr->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
            outBufHdlr->flag |= DMA_BUF_SYNC;//V4L2_OBJECT_FLAG_KEYFRAME
        if(inBufHdlr->nFlags & OMX_BUFFERFLAG_EOS)
            outBufHdlr->flag |= DMA_BUF_EOS;
        LOG_DEBUG("G2dProcess::Process flag=%x\n",outBufHdlr->flag);
        return OMX_ErrorNone;
    }else{
        LOG_DEBUG("G2dProcess::Process OMX_ErrorUndefined ret=%d",ret);
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

G2dProcess::~G2dProcess()
{
    if(g2d_handle != NULL)
        g2d_close (g2d_handle);
    return;
}

