/**
 *  Copyright (c) 2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef RTPS_PROTOCOL_H
#define RTPS_PROTOCOL_H

#include "../streaming_common/StreamingProtocol.h"

class RtpsProtocol: public StreamingProtocol
{
private:
    OMX_U32 ssrc;
    OMX_U32 seqNum;

public:
    RtpsProtocol(OMX_STRING url);
    virtual ~RtpsProtocol(){};
    virtual OMX_ERRORTYPE Read(unsigned char* pBuffer, OMX_U32 nWant, OMX_BOOL *streamEOS, OMX_U64 offsetEnd, OMX_U32 *headerLen, OMX_U32 *nFilled);
};

#endif

