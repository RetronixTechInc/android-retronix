/**
 *  Copyright (c) 2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include <errno.h>
#include <string.h>
#include "AsyncWritePipe.h"
#include "fsl_osal.h"
#include "Mem.h"
#include "Log.h"
#include "Queue.h"
#include "OMX_Core.h"

#define ASW_PIPE_SUCESS 0

#define ASYNC_WRITE_BUFFER_LEN (10*1024*1024)

#define DATA_INFO_QUEUE_SIZE 1024

typedef void * FslFileHandle;

//#define DATA_VERIFY

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif


typedef struct _ASYNC_WRITE_PIPE
{
    CPint64 nUriLen;
    CPint64 nFakePos;  // fake position to report to caller of GetPosition
    CP_ACCESSTYPE eAccess;
    CPresult (*ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam);
    CP_PIPETYPE *pContentPipeImpl;
    FslFileHandle fileHandle;
    CPint64 beginPoint;
    CPint bufferWaterMark;

    CPhandle pAsyncWriteThread;
    fsl_osal_mutex lock;
    CPbool bStopWriteThread;

    CPbyte* pAsyncWriteBuffer;
    CPint nDataStartPos;
    CPint nFreeSpaceStartPos;
    CPint nBufferLen;
    CPint nDataLen;
    Queue *dataInfoQueue;

    CPbool noSpace;

}ASYNC_WRITE_PIPE;

typedef struct _DATA_INFO
{
    // for seek
    CPint64 nOffset;
    CP_ORIGINTYPE eOrigin;

    CPint   nLen;

    // debug
    CPint   nCheckSum;
}DATA_INFO;

static fsl_osal_ptr AsyncWriteThreadFunc(fsl_osal_ptr arg)
{
    ASYNC_WRITE_PIPE *hPipe = (ASYNC_WRITE_PIPE *) arg;
    DATA_INFO Info;
    QUEUE_ERRORTYPE retQ;
    CPresult result;

    while(!hPipe->bStopWriteThread || hPipe->nDataLen > 0){

        // blocking
        retQ = hPipe->dataInfoQueue->Get(&Info);

        if(retQ != QUEUE_SUCCESS){
            LOG_ERROR("AWP_thread dataInfoQueue Get failed %d!", retQ);
            continue;
        }

        // to seek
        if(Info.nOffset >= 0 && Info.eOrigin != CP_OriginMax){
            LOG_DEBUG("\t\t\tAWP_thread SetPo %lld/%d", Info.nOffset, Info.eOrigin);
            result = hPipe->pContentPipeImpl->SetPosition(hPipe->fileHandle, Info.nOffset, Info.eOrigin);
            if(result != ASW_PIPE_SUCESS){
                LOG_ERROR("AWP_thread call SetPosition fail!!! %x", result);
                if(result == KD_ENOSPC){
                    hPipe->noSpace = OMX_TRUE;
                }
            }
            continue;
        }

        // work with bStopWriteThread==TRUE
        if(Info.nLen <= 0){
            LOG_DEBUG("\t\t\tAWP_thread signaled to stop thread");
            continue;
        }

        fsl_osal_mutex_lock(hPipe->lock);

        if(Info.nLen > hPipe->nDataLen){
            LOG_ERROR("\t\t\tAWP_thread error: Info.nLen %d is bigger than dataLen in Buffer %d",Info.nLen, hPipe->nDataLen);
            fsl_osal_mutex_unlock(hPipe->lock);
            continue;
        }

        fsl_osal_mutex_unlock(hPipe->lock);

#ifdef DATA_VERIFY
        CPint checkSum = 0;
        for(int i = 0; i<10 && i<Info.nLen; i++)
            checkSum += *(hPipe->pAsyncWriteBuffer + ((hPipe->nDataStartPos + i)%hPipe->nBufferLen));

        if(checkSum != Info.nCheckSum)
            LOG_ERROR("AWP_thread checkSum error %x, required value %x", checkSum, Info.nCheckSum);
#endif

        do{

            OMX_U32 nLengthToConsume = 0;

            fsl_osal_mutex_lock(hPipe->lock);

            if(Info.nLen > hPipe->nBufferLen - hPipe->nDataStartPos){ // data is wrap around
                LOG_DEBUG("\t\t\tAWP_thread wrap around when getting data from buffer, Info.nLen %d, data at end %d", Info.nLen, hPipe->nBufferLen - hPipe->nDataStartPos);
                nLengthToConsume = hPipe->nBufferLen - hPipe->nDataStartPos;
            }
            else
                nLengthToConsume = Info.nLen;

            fsl_osal_mutex_unlock(hPipe->lock);

            // put Write() out of lock because it's time-consuming
            LOG_DEBUG("\t\t\tAWP_thread write %d", nLengthToConsume);
            result = hPipe->pContentPipeImpl->Write(hPipe->fileHandle,
                   (CPbyte*)hPipe->pAsyncWriteBuffer + hPipe->nDataStartPos, nLengthToConsume);
        	if(result != ASW_PIPE_SUCESS){
                LOG_ERROR("\t\t\tAWP_thread write to sharefd pipe fail: %x", result);
            }

            fsl_osal_mutex_lock(hPipe->lock);

            hPipe->nDataStartPos += nLengthToConsume;
            hPipe->nDataStartPos %= hPipe->nBufferLen;
            hPipe->nDataLen -= nLengthToConsume;

            fsl_osal_mutex_unlock(hPipe->lock);

            Info.nLen -= nLengthToConsume;

        }while(Info.nLen > 0);
    }

    /*
    if(hPipe->nDataLen > 0)
        LOG_ERROR("%s data not written completely when stopping thread ", __FUNCTION__);
        */

    return NULL;
}



OMX_ASW_PIPE_API CPresult AsyncWritePipe_Open(CPhandle* hContent, CPstring szURI, CP_ACCESSTYPE eAccess)
{
    ASYNC_WRITE_PIPE *hPipe = NULL;
    CPint fd = 0;
    CPint64 len = 0, offset = 0;
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    CPresult result;

    printf("AWP_Open uri: %s, access %x\n", szURI, eAccess);

    if(hContent == NULL || szURI == NULL || eAccess != CP_AccessAsyncWrite)
        return KD_EINVAL;

    hPipe = FSL_NEW(ASYNC_WRITE_PIPE, ());
    if(hPipe == NULL)
        return KD_EIO;

	hPipe->eAccess = eAccess;
    hPipe->fileHandle = NULL;
    hPipe->nFakePos = 0;
    hPipe->nUriLen = 0;
    hPipe->beginPoint = 0;
    hPipe->noSpace = OMX_FALSE;

    hPipe->pAsyncWriteThread = NULL;
    hPipe->lock = NULL;
    hPipe->bStopWriteThread = OMX_FALSE;

    hPipe->pAsyncWriteBuffer = NULL;
    hPipe->nBufferLen = 0;
    hPipe->nDataLen = 0;
    hPipe->nDataStartPos = hPipe->nFreeSpaceStartPos = 0;
    hPipe->dataInfoQueue = NULL;
    hPipe->bufferWaterMark = 0;

    if(fsl_osal_strncmp(szURI, "sharedfd://", fsl_osal_strlen("sharedfd://"))==0){
        if(sscanf(szURI, "sharedfd://%d:%lld:%lld", (int*)&fd, &offset, &len) == 3){
            hPipe->beginPoint = offset;
            hPipe->nUriLen = len;
            hPipe->nFakePos = 0;
        }

        ret = OMX_GetContentPipe((void**)&hPipe->pContentPipeImpl, (OMX_STRING)"SHAREDFD_PIPE");
        if(!hPipe->pContentPipeImpl){
            result = KD_EINVAL;
            goto error;
        }

        result = hPipe->pContentPipeImpl->Open(&hPipe->fileHandle, (char *)szURI, CP_AccessWrite);
        if(result != ASW_PIPE_SUCESS){
            goto error;
        }
    }
    else{
        LOG_ERROR("AWP_Open can't support uri: %s", szURI);
        result = KD_EINVAL;
        goto error;
    }

    hPipe->dataInfoQueue = FSL_NEW(Queue, ());
    if (!hPipe->dataInfoQueue)
    {
        result = KD_ENOMEM;
        goto error;
    }

    if (hPipe->dataInfoQueue->Create(DATA_INFO_QUEUE_SIZE, sizeof(DATA_INFO), E_FSL_OSAL_TRUE) != QUEUE_SUCCESS)
    {
        LOG_ERROR("AWP_Open failed to create dataInfoQueue.\n");
        result = KD_ENOMEM;
        goto error;
    }

    hPipe->pAsyncWriteBuffer = (CPbyte*)FSL_MALLOC(ASYNC_WRITE_BUFFER_LEN);
    if(!hPipe->pAsyncWriteBuffer){
        result = KD_ENOMEM;
        goto error;
    }
    fsl_osal_memset(hPipe->pAsyncWriteBuffer, 0, ASYNC_WRITE_BUFFER_LEN);
    hPipe->nBufferLen = ASYNC_WRITE_BUFFER_LEN;

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&hPipe->lock, fsl_osal_mutex_normal)) {
        LOG_ERROR("Failed create mutex for OMXPlayer.\n");
        result = KD_ENOMEM;
        goto error;
    }

    fsl_osal_thread_create(&hPipe->pAsyncWriteThread, NULL, AsyncWriteThreadFunc, (fsl_osal_ptr)hPipe);
    if(!hPipe->pAsyncWriteThread) {
        LOG_ERROR("AWP_Open Failed to create thread for async write.\n");
        result = KD_ENOMEM;
        goto error;
    }

    *hContent = hPipe;

    return ASW_PIPE_SUCESS;

error:

    LOG_ERROR("AWP_Open fail result %d", result);


    if(hPipe){

        if(hPipe->lock)
            fsl_osal_mutex_destroy(hPipe->lock);

        FSL_FREE(hPipe->pAsyncWriteBuffer);

        if(hPipe->dataInfoQueue)
            hPipe->dataInfoQueue->Free();
        FSL_DELETE(hPipe->dataInfoQueue);

        if(hPipe->pContentPipeImpl && hPipe->fileHandle)
            hPipe->pContentPipeImpl->Close(hPipe->fileHandle);

    }

    FSL_FREE(hPipe);

    return result;
}


OMX_ASW_PIPE_API CPresult AsyncWritePipe_Close(CPhandle hContent)
{
    ASYNC_WRITE_PIPE *hPipe = (ASYNC_WRITE_PIPE *)hContent;

    LOG_DEBUG("AWP_Close bufferWaterMark %d, buffer dataLen %d", hPipe->bufferWaterMark, hPipe->nDataLen);

    if(!hContent)
        return KD_EINVAL;

    if(hPipe->pAsyncWriteThread) {
        hPipe->bStopWriteThread = OMX_TRUE;

        DATA_INFO info;
        info.nOffset = -1L;
        info.eOrigin = CP_OriginMax;
        info.nLen = -1;
        hPipe->dataInfoQueue->Add(&info);

        fsl_osal_thread_destroy(hPipe->pAsyncWriteThread);
    }

    if(hPipe->lock)
        fsl_osal_mutex_destroy(hPipe->lock);

    FSL_FREE(hPipe->pAsyncWriteBuffer);

    if(hPipe->dataInfoQueue)
        hPipe->dataInfoQueue->Free();

    FSL_DELETE(hPipe->dataInfoQueue);

    if(hPipe->pContentPipeImpl && hPipe->fileHandle)
        hPipe->pContentPipeImpl->Close(hPipe->fileHandle);

    FSL_DELETE(hPipe);

    return ASW_PIPE_SUCESS;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_Create(CPhandle *hContent, CPstring szURI)
{
    LOG_DEBUG("AWP_Create uri %s", szURI);
    return AsyncWritePipe_Open(hContent, szURI, CP_AccessAsyncWrite);
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_CheckAvailableBytes(
        CPhandle hContent, CPuint nBytesRequested, CP_CHECKBYTESRESULTTYPE *eResult)
{
    ASYNC_WRITE_PIPE *hPipe = (ASYNC_WRITE_PIPE *)hContent;

    if(hContent == NULL || eResult == NULL)
        return KD_EINVAL;

    //LOG_DEBUG("AWP_CheckAvailableBytes");

    *eResult =  CP_CheckBytesOk;
    return ASW_PIPE_SUCESS;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_SetPosition(
        CPhandle  hContent, CPint64 nOffset, CP_ORIGINTYPE eOrigin)
{
    ASYNC_WRITE_PIPE *hPipe = (ASYNC_WRITE_PIPE *)hContent;

    if(!hContent)
        return KD_EINVAL;


    switch(eOrigin)
    {
        case CP_OriginBegin:
            if(nOffset > hPipe->nUriLen){
                LOG_DEBUG("AWP_SetPo extend %lld->%lld) ", hPipe->nUriLen, nOffset);

                if(hPipe->noSpace == OMX_TRUE){
                    printf("don't extend file because of no space");
                    return KD_ENOSPC;
                }

                hPipe->nUriLen = nOffset;
            }
            else{
                LOG_DEBUG("AWP_SetPo set nFakePos to %lld/%d", nOffset, eOrigin);
                hPipe->nFakePos = nOffset;
            }
            break;
        case CP_OriginCur:
            if(hPipe->nFakePos + nOffset > hPipe->nUriLen){
                LOG_DEBUG("AWP_SetPo extend to %lld, current UriLen %lld ", hPipe->nFakePos + nOffset, hPipe->nUriLen);
                if(hPipe->noSpace == OMX_TRUE){
                    printf("don't extend file because of no space");
                    return KD_ENOSPC;
                }
                hPipe->nUriLen = hPipe->nFakePos + nOffset;
            }
            else{
                LOG_DEBUG("AWP_SetPo increase nFakePos by %lld/%d", nOffset, eOrigin);
                hPipe->nFakePos += nOffset;
            }
            break;
        case CP_OriginEnd:
            if(hPipe->nUriLen + nOffset < 0){
                LOG_ERROR("AWP_SetPo set to invalid pos of CP_OriginEnd, UriLen %lld, nOffset %lld", hPipe->nUriLen, nOffset);
                return KD_EINVAL;
            }

            if(nOffset > 0){
                LOG_DEBUG("AWP_SetPo extend to %lld, current urllen %lld ", hPipe->nUriLen + nOffset, hPipe->nUriLen);
                if(hPipe->noSpace == OMX_TRUE){
                    printf("don't extend file because of no space");
                    return KD_ENOSPC;
                }
                hPipe->nUriLen += nOffset;
            }
            else{
                LOG_DEBUG("AWP_SetPo set nFakePos to end %lld/%d", nOffset, eOrigin);
                hPipe->nFakePos = hPipe->nUriLen + nOffset;
            }
            break;
        default:
            LOG_ERROR("AWP_SetPo invalid parameter %d", eOrigin);
            return KD_EINVAL;
    }

    DATA_INFO info;
    info.nOffset = nOffset;
    info.eOrigin = eOrigin;
    info.nLen = 0;

    hPipe->dataInfoQueue->Add(&info);

    return ASW_PIPE_SUCESS;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_GetPosition(CPhandle hContent, CPint64 *pPosition)
{
    ASYNC_WRITE_PIPE *hPipe = (ASYNC_WRITE_PIPE *)hContent;

    if(!hContent || !pPosition)
        return KD_EINVAL;

    LOG_DEBUG("AWP_GetPo %lld\n", hPipe->nFakePos);

    *pPosition = hPipe->nFakePos;

    return ASW_PIPE_SUCESS;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_Read(CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    return -KD_EINVAL;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_ReadBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint *nSize, CPbool bForbidCopy)
{
    return KD_EINVAL;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_ReleaseReadBuffer(CPhandle hContent, CPbyte *pBuffer)
{
    return KD_EINVAL;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_Write(CPhandle hContent, CPbyte *data, CPuint nSize)
{
    ASYNC_WRITE_PIPE *hPipe = (ASYNC_WRITE_PIPE *)hContent;
    OMX_S32 nActualWritten = 0;
    CPint freeSpaceSize, freeSpaceAtEnd;
    CPbyte *pSrc = data;
    CPuint nWriteSize = nSize;

    //LOG_DEBUG("AWP_Write %p/%d", data, nSize);
    LOG_DEBUG("AWP_Write %d", nSize);

    if(hContent == NULL || data == NULL || nSize == 0)
        return -KD_EINVAL;
    if(hPipe->eAccess != CP_AccessAsyncWrite )
        return -KD_EINVAL;

    fsl_osal_mutex_lock(hPipe->lock);

    freeSpaceSize = hPipe->nBufferLen - hPipe->nDataLen;
    if(nSize > (CPuint)freeSpaceSize){
        LOG_ERROR("AWP_Write buffer overflow! data to write %d, freeSpaceSize %d", nSize, freeSpaceSize);

        // waiting for all data tobe output to file, then no one is reading buffer when reallocating.
        while(hPipe->dataInfoQueue->Size() > 0){
            fsl_osal_sleep(5000);
        }

        CPint newLength = nSize/(1024*1024)*(1024*1024) + (1024*1024) ;
        LOG_ERROR("Reallocate asyc write buffer to %d", newLength);

        CPbyte * newBuffer = (CPbyte*)FSL_MALLOC(newLength);
        if(!newBuffer){
            fsl_osal_mutex_unlock(hPipe->lock);
            LOG_ERROR("Reallocate asyc write buffer fail");
    		return KD_ENOMEM;
        }
        fsl_osal_memset(newBuffer, 0, newLength);

        FSL_FREE(hPipe->pAsyncWriteBuffer);
        hPipe->pAsyncWriteBuffer = newBuffer;
        hPipe->nBufferLen = newLength;
        hPipe->nDataLen = 0;
        hPipe->nDataStartPos = hPipe->nFreeSpaceStartPos = 0;

    }

    DATA_INFO info;
    info.nOffset = -1L;
    info.eOrigin = CP_OriginMax;
    info.nLen = nSize;
    info.nCheckSum = 0;

#ifdef DATA_VERIFY
    for(int i=0; i<10 && i<(int)nSize; i++)
        info.nCheckSum += *(pSrc+i);
    //LOG_DEBUG("AWP_Write checksum %x", info.nCheckSum);
#endif

    if(hPipe->nDataLen == 0)
        hPipe->nFreeSpaceStartPos = hPipe->nDataStartPos = 0;

    freeSpaceAtEnd = hPipe->nBufferLen - hPipe->nFreeSpaceStartPos;
    if(nWriteSize > (CPuint)freeSpaceAtEnd){
        LOG_ERROR("AWP_Write data wrap around, DataStart %d, FreeSpaceStart %d, DataLen %d" , hPipe->nDataStartPos, hPipe->nFreeSpaceStartPos, hPipe->nDataLen);
        fsl_osal_memcpy(hPipe->pAsyncWriteBuffer + hPipe->nFreeSpaceStartPos, pSrc, freeSpaceAtEnd);
        nWriteSize -= freeSpaceAtEnd;
        pSrc += freeSpaceAtEnd;
        hPipe->nFreeSpaceStartPos = 0;
        hPipe->nDataLen += freeSpaceAtEnd;
    }

    fsl_osal_memcpy(hPipe->pAsyncWriteBuffer + hPipe->nFreeSpaceStartPos, pSrc, nWriteSize);
    hPipe->nFreeSpaceStartPos += nWriteSize;
    hPipe->nFreeSpaceStartPos %= hPipe->nBufferLen;
    hPipe->nDataLen += nWriteSize;
    hPipe->bufferWaterMark = (hPipe->bufferWaterMark > hPipe->nDataLen) ? hPipe->bufferWaterMark : hPipe->nDataLen;

    fsl_osal_mutex_unlock(hPipe->lock);

    hPipe->dataInfoQueue->Add(&info);
    hPipe->nFakePos += nSize;
    if(hPipe->nFakePos > hPipe->nUriLen){
        //LOG_DEBUG("AWP_Write increase UriLen %lld to %lld", hPipe->nUriLen, hPipe->nFakePos);
        hPipe->nUriLen = hPipe->nFakePos;
    }

    return ASW_PIPE_SUCESS;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_GetWriteBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint nSize)
{
    return KD_EINVAL;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_WriteBuffer(CPhandle hContent, CPbyte *pBuffer, CPuint nFilledSize)
{
    return KD_EINVAL;
}

OMX_ASW_PIPE_API CPresult AsyncWritePipe_RegisterCallback(CPhandle hContent,
        CPresult (*ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam))
{
    ASYNC_WRITE_PIPE *hPipe = NULL;

    if(hContent == NULL || ClientCallback == NULL)
        return KD_EINVAL;

    hPipe = (ASYNC_WRITE_PIPE *)hContent;

    hPipe->ClientCallback = ClientCallback;

    return ASW_PIPE_SUCESS;
}

/*! This method is the entry point function to this content pipe implementation. This function
  is registered with core during content pipe registration (Will happen at OMX_Init).
  Later when OMX_GetContentPipe is called for this implementation this entry point function
  is called, and content pipe function pointers are assigned with the implementation function
  pointers.

  @param [InOut] pipe
  pipe is the handle to the content pipe that is passed.

  @return OMX_ERRORTYPE
  This will denote the success or failure of the method call.
  */
OMX_ASW_PIPE_API CPresult AsyncWritePipe_Init(CP_PIPETYPE *pipe)
{
    if(pipe == NULL)
        return KD_EINVAL;

    pipe->Open = AsyncWritePipe_Open;
    pipe->Close = AsyncWritePipe_Close;
    pipe->Create = AsyncWritePipe_Create;
    pipe->CheckAvailableBytes = AsyncWritePipe_CheckAvailableBytes;
    pipe->SetPosition = AsyncWritePipe_SetPosition;
    pipe->GetPosition = AsyncWritePipe_GetPosition;
    pipe->Read = AsyncWritePipe_Read;
    pipe->ReadBuffer = AsyncWritePipe_ReadBuffer;
    pipe->ReleaseReadBuffer = AsyncWritePipe_ReleaseReadBuffer;
    pipe->Write = AsyncWritePipe_Write;
    pipe->GetWriteBuffer = AsyncWritePipe_GetWriteBuffer;
    pipe->WriteBuffer = AsyncWritePipe_WriteBuffer;
    pipe->RegisterCallback = AsyncWritePipe_RegisterCallback;

    return ASW_PIPE_SUCESS;
}

/*EOF*/




