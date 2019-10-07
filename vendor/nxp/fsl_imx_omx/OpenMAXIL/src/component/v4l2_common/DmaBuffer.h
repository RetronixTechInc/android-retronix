/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef DMABUFFER_H
#define DMABUFFER_H
#include "ComponentBase.h"

#include "DmaBuffer_2.h"


#define MAX_BUFFER_CNT (32)

class DmaBuffer {
public:
    DmaBuffer();
    OMX_ERRORTYPE Create(OMX_U32 num_plane); //create free queue and 
    OMX_ERRORTYPE Allocate(OMX_U32 cnt, OMX_U32 * size, OMX_COLOR_FORMATTYPE format);
    OMX_ERRORTYPE AllocateForOutput(OMX_U32 size, OMX_PTR *outPtr);

    OMX_ERRORTYPE Get(DmaBufferHdr **buf);
    OMX_ERRORTYPE Add(DmaBufferHdr *buf);
    OMX_ERRORTYPE FreeAll();
    OMX_ERRORTYPE Free(DmaBufferHdr *buf);
    OMX_ERRORTYPE Free(OMX_PTR buf);

    OMX_ERRORTYPE Flush();
    ~DmaBuffer();
private:
    DmaBufferHdr sDmaBufferMap[MAX_BUFFER_CNT];
    OMX_U32 nBufferCnt;
    OMX_U32 nBufferSize;
    Queue* pFreeQueue;
    OMX_U32 nPlanesCnt;
    OMX_U32 nOutBufferSize;
};


#endif
