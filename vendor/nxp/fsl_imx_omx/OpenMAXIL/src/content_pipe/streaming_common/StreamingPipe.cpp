/**
 *  Copyright (c) 2012-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "StreamingPipe.h"
#include "StreamingCache.h"
#include "Log.h"
#include "Mem.h"
#include "OMX_Implement.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

#if 0
#undef LOG_INFO
#define LOG_INFO printf
#endif


StreamingPipe::StreamingPipe(StreamingProtocol * protocol)
{
    LOG_INFO("StreamingPipe Constructor");

    nOffset = 0;
    pCache = NULL;
    nLen = 0;
    ClientCallback = NULL;
    pProtocol = protocol;
    szURL = NULL;
}

StreamingPipe::~StreamingPipe()
{
    LOG_INFO("StreamingPipe Destructor");
}

OMX_ERRORTYPE StreamingPipe::Init(CPuint nCacheSize, CPuint earlierDataPercentage)
{
    LOG_INFO("StreamingPipe::Init");

    pCache = FSL_NEW(StreamingCache, ());
    if(pCache == NULL) {
        LOG_ERROR("StreamingPipe::Init fail to New StreamingCache .\n");
        return OMX_ErrorInsufficientResources;
    }

    OMX_ERRORTYPE err = pCache->Init(pProtocol, nCacheSize, earlierDataPercentage);
    if(OMX_ErrorNone != err) {
        LOG_ERROR("StreamingPipe::Init fail to initialize cache\n");
        FSL_DELETE(pCache);
        pCache = NULL;
        return err;
    }

    LOG_INFO("StreamingPipe::Init OK");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE StreamingPipe::DeInit()
{
    LOG_INFO("StreamingPipe::DeInit");

    pCache->DeInit();

    FSL_DELETE(pCache);

    FSL_DELETE(pProtocol);

    LOG_INFO("StreamingPipe::DeInit ok");

    return OMX_ErrorNone;
}

CPresult StreamingPipe::CheckAvailableBytes(
        CPuint nBytesRequested, CP_CHECKBYTESRESULTTYPE *eResult)
{
    if(nBytesRequested == (CPuint)-1) {
        OMX_U32 nAvailable = pCache->AvailableBytes(nOffset);
        *eResult = (CP_CHECKBYTESRESULTTYPE) nAvailable;
        return STREAMING_PIPE_SUCESS;
    }

    OMX_ERRORTYPE err = pCache->GetContentLength(&nLen);
    if(err != OMX_ErrorNone){
        *eResult = CP_CheckBytesStreamIOFail;
        return STREAMING_PIPE_SUCESS;
    }

    if(nLen > 0 && nOffset >= nLen) {
        *eResult = CP_CheckBytesAtEndOfStream;
        LOG_ERROR("StreamingPipe::CheckAvailableBytes() data is EOS at [%lld:%d], len %lld\n", nOffset, nBytesRequested, nLen);
        return STREAMING_PIPE_SUCESS;
    }

    LOG_DEBUG("StreamingPipe::CheckAvailableBytes() nOffset %lld, request %d\n", nOffset, nBytesRequested);

    *eResult = pCache->AvailableAt(nOffset, &nBytesRequested);
    return STREAMING_PIPE_SUCESS;

}

CPresult StreamingPipe::SetPosition(CPint64 offset, CP_ORIGINTYPE eOrigin)
{

    LOG_DEBUG("StreamingPipe::SetPosition() to: %lld, type: %d\n", offset, eOrigin);

    if(eOrigin == CP_OriginSeekable)
        return (pCache->IsSeekable() == OMX_TRUE) ? STREAMING_PIPE_SUCESS : KD_EUNSEEKABLE;

    switch(eOrigin)
    {
        case CP_OriginBegin:
            nOffset = offset;
            break;
        case CP_OriginCur:
            nOffset += offset;
            break;
        case CP_OriginEnd:
            nOffset = nLen + offset;
            break;
        default:
            return KD_EINVAL;
    }

    return STREAMING_PIPE_SUCESS;
}

CPint64 StreamingPipe::GetPosition()
{
    return nOffset;
}

CPuint StreamingPipe::Read(CPbyte *pData, CPuint nSize)
{
    CPuint nActualRead = 0;

    if(pData == NULL || nSize == 0)
        return 0;

    //printf("StreamingPipe::Read want %d at offset %d\n", nSize, nOffset);

    nActualRead = pCache->ReadAt(nOffset, nSize, pData);

    if(nActualRead != nSize)
        LOG_ERROR("StreamingPipe::Read want %d, actual read %d\n",nSize, nActualRead);

    nOffset += nActualRead;

    return nActualRead;
}

CPresult StreamingPipe::ReadBuffer(CPbyte **ppBuffer, CPuint *nSize, CPbool bForbidCopy)
{
    return KD_EINVAL;
}

CPresult StreamingPipe::ReleaseReadBuffer(CPbyte *pBuffer)
{
    return KD_EINVAL;
}

CPresult StreamingPipe::Write(CPbyte *data, CPuint nSize)
{
    return KD_EINVAL;
}

CPresult StreamingPipe::GetWriteBuffer(CPbyte **ppBuffer, CPuint nSize)
{
    return KD_EINVAL;
}

CPresult StreamingPipe::WriteBuffer(CPbyte *pBuffer, CPuint nFilledSize)
{
    return KD_EINVAL;
}

CPresult StreamingPipe::RegisterCallback(
        CPresult (*_ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam))
{

    if(ClientCallback == NULL)
        return KD_EINVAL;

    ClientCallback = _ClientCallback;

    return STREAMING_PIPE_SUCESS;
}


