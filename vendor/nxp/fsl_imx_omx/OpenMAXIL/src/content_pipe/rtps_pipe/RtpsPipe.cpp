/**
 *  Copyright (c) 2012-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "RtpsPipe.h"
#include "RtpsProtocol.h"
#include "../streaming_common/StreamingPipe.h"
#include "Mem.h"
#include "Log.h"
#include "OMX_Implement.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

OMX_STREAMING_PIPE_API CPresult RtpsPipe_Open(CPhandle* hContent, CPstring url, CP_ACCESSTYPE eAccess)
{
    OMX_ERRORTYPE err;
    StreamingPipe *hPipe;
    const CPuint CACHE_SIZE=10*1024*1024;
    const CPuint earlierDataPercentage = 0;

    LOG_DEBUG("%s hContent %p, url: %s\n", __FUNCTION__, hContent, url);

    if(hContent == NULL || url == NULL)
        return KD_EINVAL;

    if(eAccess != CP_AccessRead)
        return KD_EINVAL;

    RtpsProtocol *pProtocol = FSL_NEW(RtpsProtocol, (url));
    if(pProtocol == NULL){
        LOG_ERROR("[HttpsPipe] Failed to create RtpsProtocol for %s\n", url);
        return KD_EINVAL;
    }

	hPipe = FSL_NEW(StreamingPipe, (pProtocol));
    if(hPipe == NULL){
        LOG_ERROR("[HttpsPipe] Failed to create StreamingPipe for %s\n", url);
        FSL_DELETE(pProtocol);
        return KD_EINVAL;
    }

    err = hPipe->Init(CACHE_SIZE, earlierDataPercentage);
    if(err != OMX_ErrorNone){
        FSL_DELETE(hPipe);
        FSL_DELETE(pProtocol);
        return KD_EINVAL;
    }

	*hContent = hPipe;

    return STREAMING_PIPE_SUCESS;
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_Close(CPhandle hContent)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;
    LOG_DEBUG("%s\n", __FUNCTION__);

    hPipe->DeInit();

    FSL_DELETE(hPipe);

    return STREAMING_PIPE_SUCESS;
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_Create(CPhandle *hContent, CPstring szURI)
{
    return RtpsPipe_Open(hContent, szURI, CP_AccessRead);
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_CheckAvailableBytes(
        CPhandle hContent, CPuint nBytesRequested, CP_CHECKBYTESRESULTTYPE *eResult)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;
    CPresult res;

    if(hContent == NULL || eResult == NULL)
        return KD_EINVAL;

    LOG_DEBUG("%s request %d\n", __FUNCTION__, nBytesRequested);

    res = hPipe->CheckAvailableBytes(nBytesRequested, eResult);

    return res;
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_SetPosition(CPhandle hContent, CPint64 nOffset, CP_ORIGINTYPE eOrigin)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    LOG_DEBUG("%s offset %lld, origin %d\n", __FUNCTION__, nOffset, eOrigin);

    if(hContent == NULL)
        return KD_EINVAL;

    if(eOrigin == CP_OriginSeekable)
        return KD_EUNSEEKABLE;

    if((eOrigin != CP_OriginCur) || (nOffset < 0))
    {
        printf("invalid SetPostion offset %lld, type %d !\n", nOffset, eOrigin);
        return KD_EINVAL;
    }

    return hPipe->SetPosition(nOffset, eOrigin);
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_GetPosition(CPhandle hContent, CPint64 *pPosition)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || pPosition == NULL)
        return KD_EINVAL;

    LOG_DEBUG("%s\n", __FUNCTION__);

    *pPosition = hPipe->GetPosition();

    return STREAMING_PIPE_SUCESS;
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_Read(CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;
    CPint nActualRead = 0;

    if(hContent == NULL || pData == NULL || nSize == 0)
        return -KD_EINVAL;

    LOG_DEBUG("%s Want read %d \n", __FUNCTION__, nSize);

    nActualRead = hPipe->Read(pData, nSize);

    if(nActualRead <= 0)
        return -KD_EIO;

    return STREAMING_PIPE_SUCESS;
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_ReadBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint *nSize, CPbool bForbidCopy)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || ppBuffer == NULL || nSize == NULL)
        return -KD_EINVAL;

    return hPipe->ReadBuffer(ppBuffer,nSize,bForbidCopy);
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_ReleaseReadBuffer(CPhandle hContent, CPbyte *pBuffer)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || pBuffer == NULL)
        return -KD_EINVAL;

    return hPipe->ReleaseReadBuffer(pBuffer);
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_Write(CPhandle hContent, CPbyte *data, CPuint nSize)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || data == NULL || nSize == 0)
        return -KD_EINVAL;

    return hPipe->Write(data, nSize);
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_GetWriteBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint nSize)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || ppBuffer == NULL || nSize == 0)
        return -KD_EINVAL;

    return hPipe->GetWriteBuffer(ppBuffer, nSize);
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_WriteBuffer(CPhandle hContent, CPbyte *pBuffer, CPuint nFilledSize)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || pBuffer == NULL || nFilledSize == 0)
        return -KD_EINVAL;

    return hPipe->WriteBuffer(pBuffer, nFilledSize);
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_RegisterCallback(CPhandle hContent,
        CPresult (*ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam))
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || ClientCallback == NULL)
        return KD_EINVAL;

    return hPipe->RegisterCallback(ClientCallback);
}

OMX_STREAMING_PIPE_API CPresult RtpsPipe_Init(CP_PIPETYPE *pipe)
{
    if(pipe == NULL)
        return KD_EINVAL;

    pipe->Open = RtpsPipe_Open;
    pipe->Close = RtpsPipe_Close;
    pipe->Create = RtpsPipe_Create;
    pipe->CheckAvailableBytes = RtpsPipe_CheckAvailableBytes;
    pipe->SetPosition = RtpsPipe_SetPosition;
    pipe->GetPosition = RtpsPipe_GetPosition;
    pipe->Read = RtpsPipe_Read;
    pipe->ReadBuffer = RtpsPipe_ReadBuffer;
    pipe->ReleaseReadBuffer = RtpsPipe_ReleaseReadBuffer;
    pipe->Write = RtpsPipe_Write;
    pipe->GetWriteBuffer = RtpsPipe_GetWriteBuffer;
    pipe->WriteBuffer = RtpsPipe_WriteBuffer;
    pipe->RegisterCallback = RtpsPipe_RegisterCallback;

    return STREAMING_PIPE_SUCESS;
}


