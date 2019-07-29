/**
 *  Copyright (c) 2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef UDPS_PROTOCOL_H
#define UDPS_PROTOCOL_H

#include "../streaming_common/StreamingProtocol.h"

class UdpsProtocol : public StreamingProtocol
{
public:
    UdpsProtocol(OMX_STRING url);
    virtual ~UdpsProtocol(){};
    virtual OMX_ERRORTYPE Read(unsigned char* pBuffer, OMX_U32 nWant, OMX_BOOL *streamEOS, OMX_U64 offsetEnd, OMX_U32 *headerLen, OMX_U32 *nFilled);
};

#endif

