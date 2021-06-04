/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
#include "Process3.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif


void *Process3ThreadHandler(void *arg)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    Process3 *base = (Process3*)arg;
    LOG_DEBUG("Process3 ThreadHandler run once in=%d,out=%d\n",base->pInQueue->Size(),base->pOutQueue->Size());

    while(base->pInQueue->Size() > 0 && base->pOutQueue->Size() > 0){
        if(base->eBufMode == PROCESS3_BUF_DMA_IN_OMX_OUT){
            DmaBufferHdr * inBufHdr = NULL;
            OMX_BUFFERHEADERTYPE * outBufHdr = NULL;
            if(QUEUE_SUCCESS == base->pInQueue->Get(&inBufHdr)
                && QUEUE_SUCCESS == base->pOutQueue->Get(&outBufHdr)){
                //struct timeval tv, tv1;
                //gettimeofday (&tv, NULL);
                if((DMA_BUF_EOS & inBufHdr->flag) && (OMX_FALSE == inBufHdr->bReadyForProcess)){
                    outBufHdr->nFilledLen = 0;
                    outBufHdr->nFlags = OMX_BUFFERFLAG_EOS;
                    outBufHdr->nTimeStamp = -1;
                }else
                    ret = base->Process(inBufHdr,outBufHdr);
                //gettimeofday (&tv1, NULL);
                //LOG_DEBUG("Process3: Process time %d\n", (tv1.tv_sec-tv.tv_sec)*1000+(tv1.tv_usec-tv.tv_usec)/1000);

                base->pInReturnQueue->Add(&inBufHdr);
                base->pOutReturnQueue->Add(&outBufHdr);
                base->pCallback(base, base->pAppData);
            }
        }else if(base->eBufMode == PROCESS3_BUF_OMX_IN_DMA_OUT){
            OMX_BUFFERHEADERTYPE * inBufHdr = NULL;
            DmaBufferHdr * outBufHdr = NULL;
            if(QUEUE_SUCCESS == base->pInQueue->Get(&inBufHdr)
                && QUEUE_SUCCESS == base->pOutQueue->Get(&outBufHdr)){
                //struct timeval tv, tv1;
                //gettimeofday (&tv, NULL);
                if((OMX_BUFFERFLAG_EOS & inBufHdr->nFlags) && (0 == inBufHdr->nFilledLen)){
                    outBufHdr->bReadyForProcess = OMX_FALSE;
                    outBufHdr->flag = DMA_BUF_EOS;
                }else
                    ret = base->Process(inBufHdr,outBufHdr);
                //gettimeofday (&tv1, NULL);
                //LOG_DEBUG("Process3: Process time %d\n", (tv1.tv_sec-tv.tv_sec)*1000+(tv1.tv_usec-tv.tv_usec)/1000);

                base->pInReturnQueue->Add(&inBufHdr);
                base->pOutReturnQueue->Add(&outBufHdr);
                base->pCallback(base, base->pAppData);
            }
        }
    }

    LOG_DEBUG("Process3 ThreadHandler run end\n");
    return NULL;
}
Process3::Process3()
{
    pThread = NULL;
    pInQueue = NULL;
    pInReturnQueue = NULL;
    pOutQueue = NULL;
    pOutReturnQueue = NULL;
    pCallback = NULL;
    pAppData = NULL;
    bOutputStarted = OMX_FALSE;
    bInputStarted = OMX_FALSE;
    bRunning = OMX_FALSE;
}
OMX_ERRORTYPE Process3::Create(PROCESS3_BUFFER_MODE buf_mode)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 inMsgSize = 0;
    OMX_U32 outMsgSize = 0;

    eBufMode = buf_mode;

    if(eBufMode == PROCESS3_BUF_OMX_IN_DMA_OUT){
        inMsgSize = sizeof(OMX_BUFFERHEADERTYPE*);
        outMsgSize = sizeof(DmaBufferHdr*);
    }else if(eBufMode == PROCESS3_BUF_DMA_IN_OMX_OUT){
        inMsgSize = sizeof(DmaBufferHdr*);
        outMsgSize = sizeof(OMX_BUFFERHEADERTYPE*);
    }else
        return OMX_ErrorBadParameter;

    LOG_DEBUG("Process3::Create BEGIN\n");

    pThread = FSL_NEW(VThread,());
    if(pThread == NULL)
        return OMX_ErrorInsufficientResources;

    ret = pThread->create(this,OMX_TRUE,Process3ThreadHandler);
    if(ret != OMX_ErrorNone)
        return ret;

    LOG_DEBUG("Process3::Create pThread SUCCESS\n");

    pInQueue = FSL_NEW(Queue, ());
    if(pInQueue->Create(PROCESS3_MAX_BUF_QUEUE_SIZE, inMsgSize, E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS){
        ret = OMX_ErrorUndefined;
        return ret;
    }

    pInReturnQueue = FSL_NEW(Queue, ());
    if(pInReturnQueue->Create(PROCESS3_MAX_BUF_QUEUE_SIZE, inMsgSize, E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS){
        ret = OMX_ErrorUndefined;
        return ret;
    }

    pOutQueue = FSL_NEW(Queue, ());
    if(pOutQueue->Create(PROCESS3_MAX_BUF_QUEUE_SIZE, outMsgSize, E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS){
        ret = OMX_ErrorUndefined;
        return ret;
    }

    pOutReturnQueue = FSL_NEW(Queue, ());
    if(pOutReturnQueue->Create(PROCESS3_MAX_BUF_QUEUE_SIZE, outMsgSize, E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS){
        ret = OMX_ErrorUndefined;
        return ret;
    }

    bOutputStarted = OMX_FALSE;
    bInputStarted = OMX_FALSE;
    bRunning = OMX_FALSE;


    LOG_DEBUG("Process3::Create eBufMode=%x SUCCESS\n",eBufMode);

    return ret;
}

OMX_ERRORTYPE Process3::Flush()
{
    LOG_DEBUG("Process3::Flush \n");

    fsl_osal_ptr pMsgIn;
    fsl_osal_ptr pMsgOut;

    //clear output queue before flush
    while(pOutQueue->Size() >0){
        pMsgOut = NULL;
        pOutQueue->Get(&pMsgOut);
        pOutReturnQueue->Add(&pMsgOut);
    }

    pThread->flush();

    LOG_DEBUG("Process3::Flush end \n");

    while(pInQueue->Size() >0){
        pMsgIn = NULL;
        pInQueue->Get(&pMsgIn);
        if(PROCESS3_BUF_DMA_IN_OMX_OUT == eBufMode){
            DmaBufferHdr* bufHdr = (DmaBufferHdr*)pMsgIn;
            if(bufHdr->bReadyForProcess){
                bufHdr->bReadyForProcess = OMX_FALSE;
            }
        }else if(PROCESS3_BUF_OMX_IN_DMA_OUT == eBufMode){
            OMX_BUFFERHEADERTYPE* bufHdr = (OMX_BUFFERHEADERTYPE*)pMsgIn;
            if(bufHdr->nFilledLen > 0){
                bufHdr->nFilledLen = 0;
            }
        }
        pInReturnQueue->Add(&pMsgIn);
    }

    while(pOutQueue->Size() >0){
        pMsgOut = NULL;
        pOutQueue->Get(&pMsgOut);
        pOutReturnQueue->Add(&pMsgOut);
    }

    bOutputStarted = OMX_FALSE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Process3::Destroy()
{
    LOG_DEBUG("Process3::Destroy \n");

    if(pOutQueue){
        while(pOutQueue->Size() > 0){
            OMX_BUFFERHEADERTYPE * pHdr =NULL;
            if(OMX_ErrorNone == GetOutputBuffer(&pHdr)){
                ;//FreeFrame(pHdr);
            }
        }
    }

    if(pOutReturnQueue){
        while(pOutReturnQueue->Size() > 0){
            OMX_BUFFERHEADERTYPE * pHdr =NULL;
            if(OMX_ErrorNone == GetOutputReturnBuffer(&pHdr)){
                ;//FreeFrame(pHdr);
            }
        }
    }

    if(pThread){
        pThread->destroy();
        FSL_DELETE(pThread);
    }
    if(pInQueue){
        pInQueue->Free();
        FSL_DELETE(pInQueue);
    }
    if(pInReturnQueue){
        pInReturnQueue->Free();
        FSL_DELETE(pInReturnQueue);
    }
    if(pOutQueue){
        pOutQueue->Free();
        FSL_DELETE(pOutQueue);
    }
    if(pOutReturnQueue){
        pOutReturnQueue->Free();
        FSL_DELETE(pOutReturnQueue);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Process3::AddInputFrame(DmaBufferHdr *bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_DMA_IN))
        return OMX_ErrorBadParameter;

    if(!bufHdlr->bReadyForProcess && !(DMA_BUF_EOS & bufHdlr->flag))
        return OMX_ErrorIncorrectStateOperation;

    LOG_DEBUG("Process3::AddInputFrame fd 0=%d, fd2=%d phys=%x\n",
        bufHdlr->plane[0].fd, bufHdlr->plane[1].fd, (int)bufHdlr->plane[0].phys );

    pInQueue->Add(&bufHdlr);
    if(!bInputStarted)
        bInputStarted = OMX_TRUE;

    pThread->run_once();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Process3::AddInputFrame(OMX_BUFFERHEADERTYPE *bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_OMX_IN))
        return OMX_ErrorBadParameter;

    if(0 == bufHdlr->nFilledLen && (OMX_BUFFERFLAG_EOS & bufHdlr->nFlags))
        return OMX_ErrorIncorrectStateOperation;

    LOG_DEBUG("Process3::AddInputFrame len=%d, ptr=%x\n",
        bufHdlr->nFilledLen, bufHdlr->pBuffer);

    pInQueue->Add(&bufHdlr);
    if(!bInputStarted)
        bInputStarted = OMX_TRUE;
    pThread->run_once();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Process3::GetInputBuffer(DmaBufferHdr **bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_DMA_IN))
        return OMX_ErrorBadParameter;

    if(pInQueue->Size() > 0){
        pInQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}

OMX_ERRORTYPE Process3::GetInputBuffer(OMX_BUFFERHEADERTYPE **bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_OMX_IN))
        return OMX_ErrorBadParameter;

    if(pInQueue->Size() > 0){
        pInQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}

OMX_ERRORTYPE Process3::GetInputReturnBuffer(DmaBufferHdr **bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_DMA_IN))
        return OMX_ErrorBadParameter;

    if(pInReturnQueue->Size() > 0){
        pInReturnQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}

OMX_ERRORTYPE Process3::GetInputReturnBuffer(OMX_BUFFERHEADERTYPE **bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_OMX_IN))
        return OMX_ErrorBadParameter;

    if(pInReturnQueue->Size() > 0){
        pInReturnQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}

OMX_ERRORTYPE Process3::AddOutputFrame(DmaBufferHdr *bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_DMA_OUT))
        return OMX_ErrorBadParameter;

    LOG_DEBUG("Process3::AddOutputFrame dma buffer size=%d,ptr=%x\n",
        bufHdlr->plane[0].size, bufHdlr->plane[0].phys);

    pOutQueue->Add(&bufHdlr);
    if(!bOutputStarted)
        bOutputStarted = OMX_TRUE;

    pThread->run_once();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE Process3::AddOutputFrame(OMX_BUFFERHEADERTYPE *bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_OMX_OUT))
        return OMX_ErrorBadParameter;

    LOG_DEBUG("Process3::AddOutputFrame len=%d,flags=%x,ts=%lld,p=%p\n",
        bufHdlr->nFilledLen,bufHdlr->nFlags,bufHdlr->nTimeStamp,bufHdlr->pBuffer);

    bufHdlr->nFilledLen = 0;
    pOutQueue->Add(&bufHdlr);
    if(!bOutputStarted)
        bOutputStarted = OMX_TRUE;

    pThread->run_once();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE Process3::GetOutputBuffer(DmaBufferHdr **bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_DMA_OUT))
        return OMX_ErrorBadParameter;

    if(pOutQueue->Size() > 0){
        pOutQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}

OMX_ERRORTYPE Process3::GetOutputBuffer(OMX_BUFFERHEADERTYPE **bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_OMX_OUT))
        return OMX_ErrorBadParameter;

    if(pOutQueue->Size() > 0){
        pOutQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}

OMX_ERRORTYPE Process3::GetOutputReturnBuffer(DmaBufferHdr **bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_DMA_OUT))
        return OMX_ErrorBadParameter;

    if(pOutReturnQueue->Size() > 0){
        pOutReturnQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}

OMX_ERRORTYPE Process3::GetOutputReturnBuffer(OMX_BUFFERHEADERTYPE **bufHdlr)
{
    if(bufHdlr == NULL || !(eBufMode & PROCESS3_OMX_OUT))
        return OMX_ErrorBadParameter;

    if(pOutReturnQueue->Size() > 0){
        pOutReturnQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}

OMX_ERRORTYPE Process3::SetCallbackFunc(PROCESS3_CALLBACKTYPE *callback, OMX_PTR appData)
{
    if(callback == NULL || appData == NULL)
        return OMX_ErrorBadParameter;

    pCallback = callback;
    pAppData = appData;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE Process3::Process(DmaBufferHdr *inBufHdlr, OMX_BUFFERHEADERTYPE *outBufHdlr)
{
    return OMX_ErrorNotImplemented;
}
OMX_ERRORTYPE Process3::Process(OMX_BUFFERHEADERTYPE *inBufHdlr, DmaBufferHdr *outBufHdlr)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE Process3::ConfigInput(PROCESS3_FORMAT* fmt)
{
    if(fmt == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_memcpy(&sInFormat, fmt, sizeof(PROCESS3_FORMAT));
    LOG_DEBUG("Process3::ConfigInput in w=%d,h=%d,stride=%d",sInFormat.width,sInFormat.height,sInFormat.stride);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE Process3::ConfigOutput(PROCESS3_FORMAT* fmt)
{
    if(fmt == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_memcpy(&sOutFormat, fmt, sizeof(PROCESS3_FORMAT));
    LOG_DEBUG("Process3::ConfigOutput in w=%d,h=%d,stride=%d",sOutFormat.width,sOutFormat.height,sOutFormat.stride);

    return OMX_ErrorNone;
}

Process3::~Process3()
{
    return;
}

OMX_BOOL Process3::OutputBufferAdded()
{
    return bOutputStarted;
}
OMX_BOOL Process3::InputBufferAdded()
{
    return bInputStarted;
}

