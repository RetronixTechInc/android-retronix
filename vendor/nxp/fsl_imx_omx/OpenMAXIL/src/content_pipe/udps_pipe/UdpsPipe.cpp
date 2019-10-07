/**
 *  Copyright (c) 2012-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "UdpsPipe.h"
#include "UdpsProtocol.h"
#include "../streaming_common/StreamingPipe.h"
#include "Mem.h"
#include "Log.h"
#include "OMX_Implement.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

#if 0
#undef LOG_INFO
#define LOG_INFO printf
#endif


OMX_STREAMING_PIPE_API CPresult UdpsPipe_Open(CPhandle* hContent, CPstring url, CP_ACCESSTYPE eAccess)
{
    OMX_ERRORTYPE err;
    StreamingPipe *hPipe;
    const CPuint CACHE_SIZE = 10*1024*1024;
    const CPuint earlierDataPercentage = 0;

    LOG_INFO("UdpsPipe_Open hContent url: %s\n", url);

    if(hContent == NULL || url == NULL)
        return KD_EINVAL;

    if(eAccess != CP_AccessRead)
        return KD_EINVAL;

    UdpsProtocol *pProtocol = FSL_NEW(UdpsProtocol, (url));
    if(pProtocol == NULL){
        LOG_ERROR("[HttpsPipe] Failed to create UdpsProtocol for %s\n", url);
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

    LOG_INFO("UdpsPipe_Open ok, hContent %p\n", hPipe);

    return STREAMING_PIPE_SUCESS;
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_Close(CPhandle hContent)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;
    LOG_INFO("UdpsPipe_Close hContent %p\n", hContent);

    hPipe->DeInit();

    FSL_DELETE(hPipe);

    LOG_INFO("UdpsPipe_Close ok\n");

    return STREAMING_PIPE_SUCESS;
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_Create(CPhandle *hContent, CPstring szURI)
{
    LOG_DEBUG("%s hContent %p\n", __FUNCTION__, hContent);
    return UdpsPipe_Open(hContent, szURI, CP_AccessRead);
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_CheckAvailableBytes(
        CPhandle hContent, CPuint nBytesRequested, CP_CHECKBYTESRESULTTYPE *eResult)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;
    CPresult res;

    LOG_DEBUG("%s hContent %p, request %d, eResult %p\n", __FUNCTION__, hContent, nBytesRequested, eResult);

    if(hContent == NULL || eResult == NULL)
        return KD_EINVAL;

    res = hPipe->CheckAvailableBytes(nBytesRequested, eResult);

    return res;
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_SetPosition(CPhandle hContent, CPint64 offset, CP_ORIGINTYPE eOrigin)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    LOG_DEBUG("%s hContent %p, offset %lld, origin %d\n", __FUNCTION__, hContent, offset, eOrigin);

    if(hContent == NULL)
        return KD_EINVAL;

    if(eOrigin == CP_OriginSeekable)
        return KD_EUNSEEKABLE;

    if((eOrigin != CP_OriginCur) || (offset < 0))
    {
        printf("invalid SetPostion offset %lld, type %d !\n", offset, eOrigin);
        return KD_EINVAL;
    }

    return hPipe->SetPosition(offset, eOrigin);
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_GetPosition(CPhandle hContent, CPint64 *pPosition)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    LOG_DEBUG("%s hContent %p pPosition %p\n", __FUNCTION__, hContent, pPosition);

    if(hContent == NULL || pPosition == NULL)
        return KD_EINVAL;

    *pPosition = hPipe->GetPosition();

    return STREAMING_PIPE_SUCESS;
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_Read(CPhandle hContent, CPbyte *pData, CPuint nSize)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;
    CPint nActualRead = 0;

    LOG_DEBUG("%s hContent %p, pData %p, Want read %d \n", __FUNCTION__, hContent, pData, nSize);

    if(hContent == NULL || pData == NULL || nSize == 0)
        return -KD_EINVAL;

    nActualRead = hPipe->Read(pData, nSize);

    if(nActualRead <= 0)
        return -KD_EIO;

    return STREAMING_PIPE_SUCESS;
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_ReadBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint *nSize, CPbool bForbidCopy)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || ppBuffer == NULL || nSize == NULL)
        return -KD_EINVAL;

    return hPipe->ReadBuffer(ppBuffer,nSize,bForbidCopy);
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_ReleaseReadBuffer(CPhandle hContent, CPbyte *pBuffer)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || pBuffer == NULL)
        return -KD_EINVAL;

    return hPipe->ReleaseReadBuffer(pBuffer);
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_Write(CPhandle hContent, CPbyte *data, CPuint nSize)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || data == NULL || nSize == 0)
        return -KD_EINVAL;

    return hPipe->Write(data, nSize);
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_GetWriteBuffer(CPhandle hContent, CPbyte **ppBuffer, CPuint nSize)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || ppBuffer == NULL || nSize == 0)
        return -KD_EINVAL;

    return hPipe->GetWriteBuffer(ppBuffer, nSize);
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_WriteBuffer(CPhandle hContent, CPbyte *pBuffer, CPuint nFilledSize)
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || pBuffer == NULL || nFilledSize == 0)
        return -KD_EINVAL;

    return hPipe->WriteBuffer(pBuffer, nFilledSize);
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_RegisterCallback(CPhandle hContent,
        CPresult (*ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam))
{
    StreamingPipe *hPipe = (StreamingPipe *)hContent;

    if(hContent == NULL || ClientCallback == NULL)
        return KD_EINVAL;

    return hPipe->RegisterCallback(ClientCallback);
}

OMX_STREAMING_PIPE_API CPresult UdpsPipe_Init(CP_PIPETYPE *pipe)
{
    if(pipe == NULL)
        return KD_EINVAL;

    pipe->Open = UdpsPipe_Open;
    pipe->Close = UdpsPipe_Close;
    pipe->Create = UdpsPipe_Create;
    pipe->CheckAvailableBytes = UdpsPipe_CheckAvailableBytes;
    pipe->SetPosition = UdpsPipe_SetPosition;
    pipe->GetPosition = UdpsPipe_GetPosition;
    pipe->Read = UdpsPipe_Read;
    pipe->ReadBuffer = UdpsPipe_ReadBuffer;
    pipe->ReleaseReadBuffer = UdpsPipe_ReleaseReadBuffer;
    pipe->Write = UdpsPipe_Write;
    pipe->GetWriteBuffer = UdpsPipe_GetWriteBuffer;
    pipe->WriteBuffer = UdpsPipe_WriteBuffer;
    pipe->RegisterCallback = UdpsPipe_RegisterCallback;

    return STREAMING_PIPE_SUCESS;
}


