/**
 *  Copyright (c) 2012, 2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "UdpsProtocol.h"
#include "fsl_osal.h"
#include "Log.h"
#include "OMX_Implement.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

extern URLProtocol ff_udp_protocol;
#define FAIL_COUNT_LIMITATION 10

UdpsProtocol::UdpsProtocol(OMX_STRING url) : StreamingProtocol(url)
{
    pURLProtocol = &ff_udp_protocol;
}

OMX_ERRORTYPE UdpsProtocol::Read(unsigned char* pBuffer, OMX_U32 nWant, OMX_BOOL * streamEOS, OMX_U64 offsetEnd, OMX_U32 *headerLen, OMX_U32 *nFilled)
{
    int read = 0;
    //static int count = 0;
    static int fail_count = 0;
    while(1){
        if(GetStopRetrying(callbackArg) == OMX_TRUE)
            return OMX_ErrorTerminated;

        read = pURLProtocol->url_read(uc, pBuffer, nWant);

        //count++;
        //if((count % 10) == 0)
        //    printf("UdpsProtocol::Read %d\n", read);

        if(read <= 0){
            fail_count++;
            if(fail_count > FAIL_COUNT_LIMITATION){
                printf("Reconnecting...\n");
                if(uc->is_connected){
                    pURLProtocol->url_close(uc);
                    uc->is_connected = 0;
                }
                int err = pURLProtocol->url_open(uc, uc->filename, uc->flags);
                if(err)
                    printf("Reconnect fail\n");
                else
                    uc->is_connected = 1;
                fail_count = 0;
            }
            fsl_osal_sleep(100000);
            continue;
        }

        fail_count = 0;

        break;
    }

    *nFilled = read;
    *headerLen = 0;
    *streamEOS = OMX_FALSE;
    return OMX_ErrorNone;
}




