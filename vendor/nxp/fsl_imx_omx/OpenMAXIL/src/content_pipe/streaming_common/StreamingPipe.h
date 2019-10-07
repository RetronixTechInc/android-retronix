/**
 *  Copyright (c) 2012-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef STREAMING_PIPE_H
#define STREAMING_PIPE_H

#include "OMX_Types.h"
#include "OMX_ContentPipe.h"

#include "StreamingProtocol.h"
#include "StreamingCache.h"

#define STREAMING_PIPE_SUCESS 0

class StreamingPipe
{
private:
    StreamingCache *pCache;
    OMX_STRING szURL;
    OMX_U64 nLen;   /**< file length  */
    OMX_U64 nOffset;   /**< seek position */
    CPresult (*ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam);
    StreamingProtocol * pProtocol;

public:
    StreamingPipe(StreamingProtocol *protocol);
    virtual ~StreamingPipe();
    OMX_ERRORTYPE Init(CPuint nCacheSize, CPuint earlierDataPercentage);
    OMX_ERRORTYPE DeInit();
    CPresult CheckAvailableBytes(CPuint nBytesRequested, CP_CHECKBYTESRESULTTYPE *eResult);
    CPresult SetPosition(CPint64 nOffset, CP_ORIGINTYPE eOrigin);
    CPint64 GetPosition();
    CPuint Read(CPbyte *pData, CPuint nSize);
    CPresult ReadBuffer(CPbyte **ppBuffer, CPuint *nSize, CPbool bForbidCopy);
    CPresult ReleaseReadBuffer(CPbyte *pBuffer);
    CPresult Write(CPbyte *data, CPuint nSize);
    CPresult GetWriteBuffer(CPbyte **ppBuffer, CPuint nSize);
    CPresult WriteBuffer(CPbyte *pBuffer, CPuint nFilledSize);
    CPresult RegisterCallback(
        CPresult (*_ClientCallback)(CP_EVENTTYPE eEvent, CPuint iParam));

};

#endif
/* File EOF */

