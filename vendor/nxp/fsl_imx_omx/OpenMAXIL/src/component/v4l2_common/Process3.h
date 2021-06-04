/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef PROCESS3_H
#define PROCESS3_H

#include <linux/videodev2.h>

#include "ComponentBase.h"
#include "VThread.h"
#include <sys/ioctl.h>
#include <stdio.h>
#include "DmaBuffer_2.h"

class Process3;

#define PROCESS3_OMX_IN (0x1)
#define PROCESS3_DMA_IN (0x2)

#define PROCESS3_OMX_OUT (0x10)
#define PROCESS3_DMA_OUT (0x20)

typedef enum
{
    PROCESS3_BUF_DMA_IN_OMX_OUT = (PROCESS3_DMA_IN | PROCESS3_OMX_OUT),
    PROCESS3_BUF_OMX_IN_DMA_OUT = (PROCESS3_OMX_IN | PROCESS3_DMA_OUT),
}PROCESS3_BUFFER_MODE;


typedef struct {
    OMX_U32 width;
    OMX_U32 height;
    OMX_U32 format;
    OMX_U32 stride;
    OMX_U32 bufferSize;
}PROCESS3_FORMAT;


#define PROCESS3_MAX_BUF_QUEUE_SIZE (30)
#define PROCESS3_BUF_CNT (3)

typedef OMX_ERRORTYPE PROCESS3_CALLBACKTYPE(Process3 *pProcess,OMX_PTR pAppData);

class Process3{
public:
    explicit Process3();

    OMX_ERRORTYPE Create(PROCESS3_BUFFER_MODE buf_mode);

    OMX_ERRORTYPE Flush(); 
    OMX_ERRORTYPE Destroy();

    OMX_ERRORTYPE AddInputFrame(DmaBufferHdr *bufHdlr);
    OMX_ERRORTYPE AddInputFrame(OMX_BUFFERHEADERTYPE *bufHdlr);

    OMX_ERRORTYPE GetInputBuffer(DmaBufferHdr **bufHdlr);
    OMX_ERRORTYPE GetInputBuffer(OMX_BUFFERHEADERTYPE **bufHdlr);

    OMX_ERRORTYPE GetInputReturnBuffer(DmaBufferHdr **bufHdlr);
    OMX_ERRORTYPE GetInputReturnBuffer(OMX_BUFFERHEADERTYPE **bufHdlr);


    OMX_ERRORTYPE AddOutputFrame(DmaBufferHdr *bufHdlr);
    OMX_ERRORTYPE AddOutputFrame(OMX_BUFFERHEADERTYPE *bufHdlr);

    OMX_ERRORTYPE GetOutputBuffer(DmaBufferHdr **bufHdlr);
    OMX_ERRORTYPE GetOutputBuffer(OMX_BUFFERHEADERTYPE **bufHdlr);

    OMX_ERRORTYPE GetOutputReturnBuffer(DmaBufferHdr **bufHdlr);
    OMX_ERRORTYPE GetOutputReturnBuffer(OMX_BUFFERHEADERTYPE **bufHdlr);


    friend void *Process3ThreadHandler(void *arg);
 
    OMX_ERRORTYPE SetCallbackFunc(PROCESS3_CALLBACKTYPE *callback, OMX_PTR appData);

    OMX_ERRORTYPE ConfigInput(PROCESS3_FORMAT* fmt);
    OMX_ERRORTYPE ConfigOutput(PROCESS3_FORMAT* fmt);
    virtual OMX_ERRORTYPE Process(DmaBufferHdr *inBufHdlr, OMX_BUFFERHEADERTYPE *outBufHdlr);
    virtual OMX_ERRORTYPE Process(OMX_BUFFERHEADERTYPE *inBufHdlr, DmaBufferHdr *outBufHdlr);

    virtual ~Process3();
    OMX_BOOL InputBufferAdded();
    OMX_BOOL OutputBufferAdded();

protected:
    PROCESS3_FORMAT sInFormat;
    PROCESS3_FORMAT sOutFormat;
    PROCESS3_BUFFER_MODE eBufMode;

private:

    VThread *pThread;
    Queue* pInQueue;//add input buffer by user
    Queue* pInReturnQueue;//ready for return
    Queue* pOutQueue;//add output buffer by user
    Queue* pOutReturnQueue;//got and set to out obj

    PROCESS3_CALLBACKTYPE* pCallback;
    OMX_PTR pAppData;
    OMX_BOOL bOutputStarted;
    OMX_BOOL bInputStarted;
    OMX_BOOL bRunning;
};
#endif

