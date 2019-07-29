/**
 *  Copyright (c) 2012, 2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef HTTPS_PROTOCOL_H
#define HTTPS_PROTOCOL_H

#include "../streaming_common/StreamingProtocol.h"

class HttpsProtocol: public StreamingProtocol
{
private:
    virtual OMX_BOOL GetHandleError();
    virtual OMX_S32 UrlHeaderPos(OMX_STRING url);

public:
    HttpsProtocol(OMX_STRING url);
    virtual ~HttpsProtocol(){};
    virtual OMX_ERRORTYPE Init();
    virtual OMX_ERRORTYPE Open();
    virtual OMX_ERRORTYPE Seek(OMX_U64 offset, int whence, OMX_BOOL blocking);
    virtual OMX_ERRORTYPE Read(unsigned char* pBuffer, OMX_U32 nWant, OMX_BOOL * bStreamEOS, OMX_U64 offsetEnd, OMX_U32 *headerLen, OMX_U32 *nFilled);
};

#endif
