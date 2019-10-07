/**
 *  Copyright (c) 2012, 2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "StreamingProtocol.h"
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
#include "fsl_osal.h"
#include "Mem.h"
#include "Log.h"
#include "OMX_Implement.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

StreamingProtocol::StreamingProtocol(OMX_STRING url)
{
    pURLProtocol = NULL;
    szURL = url;
    uc = NULL;
    GetStopRetrying = NULL;
    callbackArg = NULL;
}

OMX_ERRORTYPE StreamingProtocol::Init()
{
    OMX_STRING mURL, mHeader;
    mURL = mHeader = NULL;

    if(pURLProtocol == NULL){
        LOG_ERROR("%s URL Protocol not assigned.\n", __FUNCTION__);
        return OMX_ErrorUndefined;
    }

    av_register_all();

    mURL = szURL;

    LOG_DEBUG("%s mURL: %s, mHeader: %s\n",__FUNCTION__, mURL, mHeader);

    uc = (URLContext*)FSL_MALLOC(sizeof(URLContext) + fsl_osal_strlen(mURL) + 1);
    if(uc == NULL) {
        LOG_ERROR("%s Allocate for URLContext failed.\n", __FUNCTION__);
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

    LOG_DEBUG("%s priv_data_size %d\n",__FUNCTION__, uc->prot->priv_data_size);
    if (uc->prot->priv_data_size) {
    	LOG_DEBUG("%s allocate priv_data\n", __FUNCTION__);
        uc->priv_data = FSL_MALLOC(uc->prot->priv_data_size);
        if(uc->priv_data == NULL) {
            FSL_FREE(uc);
            return OMX_ErrorInsufficientResources;
        }
        fsl_osal_memset(uc->priv_data, 0, uc->prot->priv_data_size);
        if (uc->prot->priv_data_class) {
            *(const AVClass**)uc->priv_data = uc->prot->priv_data_class;
            av_opt_set_defaults(uc->priv_data);
        }
    }

    return OMX_ErrorNone;
}


OMX_ERRORTYPE StreamingProtocol::DeInit()
{
    LOG_DEBUG("%s\n", __FUNCTION__);

    if(uc != NULL) {
        if (uc->prot->priv_data_size && uc->priv_data)
            FSL_FREE(uc->priv_data);
        FSL_FREE(uc);
    }
    return OMX_ErrorNone;
}

/*
 * return value:
    0  - no url_seek interface,
         url_seek returns fail

    >0 - content length
 */

OMX_U64 StreamingProtocol::GetContentLength()
{
    OMX_S64 len = 0;
#if 0
    //workaround for MediaPlayerFlakyNetworkTest CTS case
    if (fsl_osal_strncmp(szURL, "http://localhost", 16) == 0) {
        if (fsl_osal_strstr (szURL, "video")) {
            return 0;
        }
    }
#endif

    if(pURLProtocol->url_seek){
        len = pURLProtocol->url_seek(uc, 0, AVSEEK_SIZE);
        if(len < 0){
            LOG_ERROR("%s url_seek AVSEEK_SIZE fail", __FUNCTION__);
            len = 0;
        }
    }
    return (OMX_U64)len;
}

void StreamingProtocol::SetCallback(GetStopRetryingFunction f, OMX_PTR handle)
{
    GetStopRetrying = f;
    callbackArg = handle;
}

OMX_ERRORTYPE StreamingProtocol::Seek(OMX_U64 offset, int whence, OMX_BOOL blocking)
{
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE StreamingProtocol::Open()
{
    LOG_DEBUG("Open %s\n", uc->filename);

    int err = pURLProtocol->url_open(uc, uc->filename, uc->flags);
    if (err) {
        LOG_ERROR("%s url_open fail (%s)\n",__FUNCTION__, uc->filename);
        return OMX_ErrorNetworkFail;
    }

    uc->is_connected = 1;
    LOG_DEBUG("%s url_open success (%s)\n",__FUNCTION__, uc->filename);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE StreamingProtocol::Close()
{
    LOG_DEBUG("%s\n", __FUNCTION__);

    if(uc != NULL) {
        LOG_DEBUG("%s call url_close()\n", __FUNCTION__);

        if (uc->is_connected) {
            pURLProtocol->url_close(uc);
            uc->is_connected = 0;
        }
    }
    return OMX_ErrorNone;
}


