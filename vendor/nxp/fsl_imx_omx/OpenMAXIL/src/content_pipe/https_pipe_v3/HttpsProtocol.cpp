/**
 *  Copyright (c) 2012, 2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "HttpsProtocol.h"
#include "libavformat/http.h"
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
#include "fsl_osal.h"
#include "Mem.h"
#include "Log.h"
#include "OMX_Implement.h"

#define CONNECT_TIME_OUT (10*1000)
#define READ_TIME_OUT 5000
#define USE_READ_TO_IMPLEMENT_SEEK_MAX_LENGTH (20*1024)

extern URLProtocol ff_http_protocol;
extern URLProtocol ff_mmsh_protocol;

//#undef LOG_DEBUG
//#define LOG_DEBUG printf
//#undef LOG_INFO
//#define LOG_INFO printf


HttpsProtocol::HttpsProtocol(OMX_STRING url) : StreamingProtocol(url)
{
    if(szURL == NULL){
        LOG_ERROR("HttpsProtocol() url is NULL\n");
        return;
    }
    if(fsl_osal_strncmp(szURL, "http://", 7) == 0)
        pURLProtocol = &ff_http_protocol;
    else if(fsl_osal_strncmp(szURL, "mms://", 6) == 0)
        pURLProtocol = &ff_mmsh_protocol;
    else
        LOG_ERROR("HttpsProtocol() incorrect url prefix %s", szURL);
}

OMX_ERRORTYPE HttpsProtocol::Init()
{
    OMX_STRING URL, mURL, mHeader;
    URL = mURL = mHeader = NULL;

    if(pURLProtocol == NULL){
        LOG_ERROR("URL Protocol not assigned.\n");
        return OMX_ErrorUndefined;
    }

    av_register_all();
    //avio_set_interrupt_cb(abort_cb);

    OMX_S32 pos = UrlHeaderPos(szURL);
    if(pos > 0) {
        OMX_S32 url_len = fsl_osal_strlen(szURL) + 1;
        URL = (OMX_STRING)FSL_MALLOC(url_len);
        if(URL == NULL)
            return OMX_ErrorInsufficientResources;
        fsl_osal_strcpy(URL, szURL);
        URL[pos - 1] = '\0';
        mURL = URL;
        mHeader = URL + pos;
    }
    else
        mURL = szURL;

    LOG_INFO("mURL: %s, mHeader: %s\n", mURL, mHeader);

    uc = (URLContext*)FSL_MALLOC(sizeof(URLContext) + fsl_osal_strlen(mURL) + 1);
    if(uc == NULL) {
        FSL_FREE(URL);
        LOG_ERROR("Allocate for URLContext failed.\n");
        return OMX_ErrorInsufficientResources;
    }

    uc->filename = (char *) &uc[1];
    fsl_osal_strcpy(uc->filename, mURL);
    uc->flags = AVIO_RDONLY;
    uc->is_streamed = 0; /* default = not streamed */
    uc->max_packet_size = 0; /* default: stream file */
    uc->priv_data = NULL;
    uc->prot = pURLProtocol;
    uc->is_connected = 0;

    LOG_INFO("priv_data_size %d\n", uc->prot->priv_data_size);
    if (uc->prot->priv_data_size) {
    	LOG_DEBUG("allocate priv_data\n");
        uc->priv_data = FSL_MALLOC(uc->prot->priv_data_size);
        if(uc->priv_data == NULL) {
            FSL_FREE(uc);
            FSL_FREE(URL);
            return OMX_ErrorInsufficientResources;
        }
        fsl_osal_memset(uc->priv_data, 0, uc->prot->priv_data_size);
        if (uc->prot->priv_data_class) {
            *(const AVClass**)uc->priv_data = uc->prot->priv_data_class;
            av_opt_set_defaults(uc->priv_data);
        }
    }

    if(mHeader)
        ff_http_set_headers(uc, mHeader);

    FSL_FREE(URL);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE HttpsProtocol::Open()
{
    fsl_osal_timeval tv;

    LOG_INFO("HttpsProtocol::Open() %s\n", uc->filename);

    fsl_osal_systime(&tv);
    OMX_U64 nStartTime = (OMX_U64)tv.sec*1000 + (OMX_U64)tv.usec/1000;

    while(1){
        int err = pURLProtocol->url_open(uc, uc->filename, uc->flags);
        if(!err)
            break;
        if(GetStopRetrying(callbackArg) == OMX_TRUE){
            printf("HttpsProtocol::Connect() stop retrying\n");
            return OMX_ErrorTerminated;
        }

        fsl_osal_systime(&tv);
        OMX_U64 nCurTime = (OMX_U64)tv.sec*1000 + (OMX_U64)tv.usec/1000;
        OMX_S32 nCostTime = (OMX_S32)(nCurTime - nStartTime);
        if (nCostTime >= CONNECT_TIME_OUT) {
            LOG_ERROR("HttpsProtocol::Connect() timeout after %d ms.\n", nCostTime);
            return OMX_ErrorNetworkFail;
        }

        fsl_osal_sleep(10000);

        LOG_DEBUG("retry connect...");
    }

    uc->is_connected = 1;
    LOG_INFO("HttpsProtocol::Open success: %s\n", uc->filename);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE HttpsProtocol::Read(unsigned char* pBuffer, OMX_U32 nWant, OMX_BOOL * bStreamEOS, OMX_U64 offsetEnd, OMX_U32 *headerLen, OMX_U32 *nFilled)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_DEBUG("HttpsProtocol::Read() nWant %d, offsetEnd %lld\n", nWant, offsetEnd);

    int read = 0;
    *headerLen = 0;
    int fail_count = 0, max_fail_count_before_seek = 100;

    fsl_osal_timeval tv;
    fsl_osal_systime(&tv);
    OMX_U64 nStartTime = (OMX_U64)tv.sec*1000 + (OMX_U64)tv.usec/1000;

    while(nWant > 0){

        if(GetStopRetrying(callbackArg) == OMX_TRUE){
            printf("HttpsProtocol::Read() terminated\n");
            return OMX_ErrorTerminated;
        }

        read = pURLProtocol->url_read(uc, pBuffer, nWant);
        LOG_DEBUG("url_read result %d\n", read);

        if(GetStopRetrying(callbackArg) == OMX_TRUE){
            printf("HttpsProtocol::Read() terminated\n");
            return OMX_ErrorTerminated;
        }

        if(read > 0){
            if((OMX_U32)read > nWant){
                LOG_ERROR("HttpsProtocol::Read() read length is larger than wanted!");
                read = nWant;
            }
            pBuffer += read;
            nWant -= read;
            offsetEnd += read;
            *nFilled += read;
            fsl_osal_systime(&tv);
            nStartTime = (OMX_U64)tv.sec*1000 + (OMX_U64)tv.usec/1000;
            continue;
        }

        if(read == AVERROR_EOF || GetHandleError() == OMX_TRUE){
            printf("HttpsProtocol::Read() reach eos\n");
            *bStreamEOS = OMX_TRUE;
            return OMX_ErrorNone;
        }

        fsl_osal_systime(&tv);
        OMX_U64 nCurTime = (OMX_U64)tv.sec*1000 + (OMX_U64)tv.usec/1000;
        OMX_S32 nCostTime = (OMX_S32)(nCurTime - nStartTime);
        if (nCostTime < READ_TIME_OUT){
            fsl_osal_sleep(10000);
            continue;
        }

        printf("HttpsProtocol::Read() url_read fail and retry timeout (%d), do seek\n", nCostTime);

        OMX_S64 pos;
        while(1){
            pos = pURLProtocol->url_seek(uc, offsetEnd, SEEK_SET);
            if(pos > -600 && pos <= -400) // server refuses
                break;

            if(pos == (OMX_S64)offsetEnd) // success
                break;

            //printf("HttpsProtocol::Read() seek to %lld fail\n", offsetEnd);

            if(GetStopRetrying(callbackArg) == OMX_TRUE){
                printf("HttpsProtocol::Read() stop retrying seeking\n");
                return OMX_ErrorTerminated;
            }

            fsl_osal_sleep(10000);
        }

        fsl_osal_systime(&tv);
        nStartTime = (OMX_U64)tv.sec*1000 + (OMX_U64)tv.usec/1000;

    }

    return OMX_ErrorNone;
}


/*
    currently only SEEK_SET is used
 */

OMX_ERRORTYPE HttpsProtocol::Seek(OMX_U64 targetOffset, int whence, OMX_BOOL blocking)
{
    LOG_DEBUG("HttpsProtocol::Seek() to %lld , whence %d\n", targetOffset, whence);

    OMX_S64 location;

    while(1){

        if(GetStopRetrying(callbackArg) == OMX_TRUE){
            printf("HttpsProtocol::Seek() terminated\n");
            return OMX_ErrorTerminated;
        }

        location = pURLProtocol->url_seek(uc, targetOffset, SEEK_SET);

        // simulate unseekable server
        //return OMX_ErrorServerUnseekable;

        if(location == (OMX_S64)targetOffset){
            LOG_DEBUG("HttpsProtocol::Seek() ok");
            return OMX_ErrorNone;
        }

        if(location > -600 && location <= -400) // server refuses
            return OMX_ErrorServerUnseekable;

        if(location == 0) // server not support range request
            return OMX_ErrorServerUnseekable;

        if(GetStopRetrying(callbackArg) == OMX_TRUE){
            printf("HttpsProtocol::Seek() terminated\n");
            return OMX_ErrorTerminated;
        }

        if(blocking == OMX_FALSE){
            LOG_ERROR("HttpsProtocol::Seek() fail, ret %lld, request %lld", location, targetOffset);
            return OMX_ErrorUndefined;
        }

        if(location > 0
            && targetOffset > (OMX_U64)location
            && targetOffset - (OMX_U64)location < USE_READ_TO_IMPLEMENT_SEEK_MAX_LENGTH)
        {

            printf("HttpsProtocol::Seek() use read to move file ptr to target position");
            OMX_U8 buffer[512];
            fsl_osal_memset(buffer, 0, 512);
            OMX_S32 diff = targetOffset - (OMX_S64)location;

            while(diff > 0) {
                OMX_S32 read = pURLProtocol->url_read(uc, (unsigned char*)buffer, diff > 512 ? 512 : diff);
                printf("HttpsProtocol::Seek() read %d, diff %d", read, diff);
                if(read > 0){
                    diff -= read;
                }

                if(GetStopRetrying(callbackArg) == OMX_TRUE){
                    printf("HttpsProtocol::Seek() stop retrying\n");
                    return OMX_ErrorTerminated;
                }
                fsl_osal_sleep(10000);
            }

        }

        LOG_DEBUG("HttpsProtocol::Seek() url_seek fail and retry, ret : %lld", location);

        // retry when:
        // location < 0 and not in range -600 ~ -400
        // location > targetOffset
        // location > 0 and targetOffset - location > MAX_LENGTH

        fsl_osal_sleep(10000);

    }

    return OMX_ErrorNone;
}

OMX_BOOL HttpsProtocol::GetHandleError()
{
    if(GetContentLength() == 0 && (fsl_osal_strncmp(szURL, "http://localhost", 16) == 0))
        return OMX_TRUE;
    else
        return OMX_FALSE;
}

OMX_S32 HttpsProtocol::UrlHeaderPos(OMX_STRING url)
{
    OMX_S32 len = fsl_osal_strlen(url) + 1;
    OMX_S32 i;
    OMX_BOOL bHasHeader = OMX_FALSE;
    for(i=0; i<len; i++) {
        if(url[i] == '\n') {
            bHasHeader = OMX_TRUE;
            break;
        }
    }

    if(bHasHeader == OMX_TRUE)
        return i+1;
    else
        return 0;
}

