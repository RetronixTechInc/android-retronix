/**
 *  Copyright (c) 2012-2015, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "RtpsProtocol.h"
#include "fsl_osal.h"
#include "Log.h"
#include "OMX_Implement.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

extern URLProtocol ff_rtp_protocol;
#define FAIL_COUNT_LIMITATION 10

enum PayloadType {
    MP2T = 33,
    VLC_TS = 0x60,
};


RtpsProtocol::RtpsProtocol(OMX_STRING url) : StreamingProtocol(url)
{
    pURLProtocol = &ff_rtp_protocol;
    ssrc = 0;
    seqNum = 0;
}

OMX_ERRORTYPE RtpsProtocol::Read(unsigned char* pBuffer, OMX_U32 nWant, OMX_BOOL * streamEOS, OMX_U64 offsetEnd, OMX_U32 *headerLen, OMX_U32 *nFilled)
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
        //    printf("RtpsProtocol::Read %d\n", read);

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

        OMX_U8 version = (pBuffer[0] & 0xc0) >> 6;
        if(version != 2){
            printf("version number mismatch: %x, len %d\n", version, read);
            continue;
        }
        OMX_U8 payloadType = pBuffer[1] & 0x7f;
        if(payloadType != MP2T && payloadType != VLC_TS){
            printf("payloadType mismatch: %x, len %d\n", payloadType, read);
            continue;
        }

        if(ssrc == 0){ // first packet
            ssrc = pBuffer[8] << 24 | pBuffer[9] << 16 | pBuffer[10] << 8 | pBuffer[11];
            seqNum = pBuffer[2] << 8 | pBuffer[3];
            printf("ssrc %x, seq is %x", ssrc, seqNum);
        }
        else{ // following packets
            OMX_U32 currentSsrc = pBuffer[8] << 24 | pBuffer[9] << 16 | pBuffer[10] << 8 | pBuffer[11];
            if(currentSsrc != ssrc){
                printf("ssrc mismatch: %x, len %d\n", currentSsrc, read);
                continue;
            }
            OMX_U32 currentSeqNum = pBuffer[2] << 8 | pBuffer[3];
            OMX_U32 diff = (currentSeqNum + 0xffff - seqNum) % 0xffff;
            if(diff > 1)
                printf("seqence number error, prev: 0x%x, cur 0x%x, lost %d\n", seqNum, currentSeqNum, diff);

            seqNum = currentSeqNum;
        }

        break;
    }

    *nFilled = read;
    *headerLen = 12;
    *streamEOS = OMX_FALSE;

    return OMX_ErrorNone;
}




