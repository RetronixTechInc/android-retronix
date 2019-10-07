/**
 *  Copyright (c) 2010-2014 Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include <OMXPlayer.h>
#include <gui/Surface.h>
#include <private/media/VideoFrame.h>
#include <cutils/properties.h>
#include <inttypes.h>

#include <fcntl.h>
#include <linux/mxcfb.h>

#include "OMX_GraphManager.h"
#undef OMX_MEM_CHECK
#include "Mem.h"
#include "Log.h"
#include "OMX_Common.h"
#include "Queue.h"

using namespace android;
using android::Surface;

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_WARNING
#define LOG_WARNING printf
#endif

// should keep align with enum OMXPlayer::MSGTYPE
static const char * CmdName[] = {
    "MSG_NONE",
    "MSG_PREPAREASYNC",
    "MSG_START",
    "MSG_SEEKTO",
    "MSG_PAUSE",
    "MSG_RESUME",
    "MSG_SETSURFACE",
};


#define COMMAND_QUEUE_SIZE 100

static int eventhandler(void* context, GM_EVENT EventID, void* Eventpayload)
{
    OMXPlayer *player = (OMXPlayer*)context;

    player->ProcessEvent(EventID, Eventpayload);

    return 1;
}

static fsl_osal_ptr ProcessAsyncCmdThreadFunc(fsl_osal_ptr arg)
{
    OMXPlayer *player = (OMXPlayer*)arg;

    while(NO_ERROR == player->ProcessAsyncCmd());

    return NULL;
}

OMXPlayer::OMXPlayer(int nMediaType)
{
    OMX_GraphManager* gm = NULL;

    LOG_DEBUG("OMXPlayer constructor.\n");

    player = NULL;
    mInit = NO_ERROR;
    mNativeWindow = NULL;
    mSharedFd = -1;
    bLoop = false;
    pPCmdThread = NULL;
    bStopPCmdThread = false;
    msg = MSG_NONE;
    sem = NULL;
    contentURI = NULL;
    mMediaType = nMediaType;
    qdFlag = false;
    cmdQueue = NULL;
    bPlaying = false;
    queueLock = NULL;

    LogInit(-1, NULL);

    gm = OMX_GraphManagerCreate();
    if(gm == NULL) {
        LOG_ERROR("Failed to create GMPlayer.\n");
        mInit = NO_MEMORY;
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_sem_init(&sem, 0, 0)) {
        LOG_ERROR("Failed create Semphore for OMXPlayer.\n");
        mInit = NO_MEMORY;
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&queueLock, fsl_osal_mutex_recursive)) {
        LOG_ERROR("Failed create mutex for OMXPlayer.\n");
        mInit = NO_MEMORY;
    }

    cmdQueue = FSL_NEW(Queue, ());
    if (cmdQueue == NULL)
    {
        LOG_ERROR("Failed to alloc queue for async commad.\n");
        mInit = NO_MEMORY;
    }

    if (((Queue*)cmdQueue)->Create(COMMAND_QUEUE_SIZE, sizeof(ASYNC_COMMAND), E_FSL_OSAL_FALSE) != QUEUE_SUCCESS)
    {
        Queue* pCmdQueue = (Queue *)cmdQueue;
        FSL_DELETE(pCmdQueue);
        LOG_ERROR("Failed to create queue for async commad.\n");
        mInit = NO_MEMORY;
    }

    fsl_osal_thread_create(&pPCmdThread, NULL, ProcessAsyncCmdThreadFunc, (fsl_osal_ptr)this);
    if(pPCmdThread == NULL) {
        LOG_ERROR("Failed to create thread for async commad.\n");
        mInit = NO_MEMORY;
    }

#if 0
    if(mInit != NO_ERROR)
        OMXPlayer::~OMXPlayer();
#endif

    LOG_DEBUG("OMXPlayer Construct ok, gm player: %p\n", gm);
    player = (void*)gm;
}

OMXPlayer::~OMXPlayer()
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;

    LOG_DEBUG("OMXPlayer destructor %p.\n", gm);

    if(pPCmdThread != NULL) {
        bStopPCmdThread = OMX_TRUE;
        fsl_osal_sem_post(sem);
        fsl_osal_thread_destroy(pPCmdThread);
        pPCmdThread = NULL;
        bStopPCmdThread = OMX_FALSE;
    }

    if(sem != NULL) {
        fsl_osal_sem_destroy(sem);
        sem = NULL;
    }

    FSL_FREE(contentURI);

    if(gm != NULL) {
        gm->deleteIt(gm);
        player = NULL;
    }

    if (mSharedFd >= 0) {
        close(mSharedFd);
        mSharedFd = -1;
    }

    fsl_osal_mutex_lock((fsl_osal_mutex)queueLock);

    if(cmdQueue != NULL){
        Queue* pCmdQueue = (Queue *)cmdQueue;
        pCmdQueue->Free();
        FSL_DELETE(pCmdQueue);
        cmdQueue = NULL;
    }
    fsl_osal_mutex_unlock((fsl_osal_mutex)queueLock);

    fsl_osal_mutex_destroy(queueLock);
    queueLock = NULL;

    LogDeInit();

    LOG_DEBUG("OMXPlayer destructor %p ok.\n", gm);

}

status_t OMXPlayer::initCheck()
{
    return mInit;
}

status_t OMXPlayer::setDataSource(
        const sp<IMediaHTTPService> &httpService,
        const char *url,
        const KeyedVector<String8, String8> *headers)
{
    LOG_DEBUG("OMXPlayer set data source %s\n", url);

    qdFlag = false;

    if (mSharedFd >= 0) {
        close(mSharedFd);
        mSharedFd = -1;
    }

    OMX_S32 len = fsl_osal_strlen(url) + 1;

    if(headers) {
        OMX_S32 i;
        for(i=0; i<(int)headers->size(); i++)
            len += fsl_osal_strlen(headers->keyAt(i).string()) + fsl_osal_strlen(headers->valueAt(i).string()) + 4;
        len += 1;
    }

    contentURI = (char*)FSL_MALLOC(len);
    if(contentURI == NULL)
        return UNKNOWN_ERROR;

    fsl_osal_strcpy(contentURI, url);

    if(headers) {
        char *pHeader = contentURI + fsl_osal_strlen(url);
        pHeader[0] = '\n';
        pHeader += 1;
        OMX_S32 i;
        for(i=0; i<(int)headers->size(); i++) {
            OMX_S32 keyLen = fsl_osal_strlen(headers->keyAt(i).string());
            fsl_osal_memcpy(pHeader, (OMX_PTR)headers->keyAt(i).string(), keyLen);
            pHeader += keyLen;
            pHeader[0] = ':';
            pHeader[1] = ' ';
            pHeader += 2;
            OMX_S32 valueLen = fsl_osal_strlen(headers->valueAt(i).string());
            fsl_osal_memcpy(pHeader, (OMX_PTR)headers->valueAt(i).string(), valueLen);
            pHeader += valueLen;
            pHeader[0] = '\r';
            pHeader[1] = '\n';
            pHeader += 2;
        }
        pHeader[0] = '\0';
    }

    LOG_DEBUG("OMXPlayer contentURI: %s\n", contentURI);

    return NO_ERROR;
}

status_t OMXPlayer::setDataSource(
        int fd,
        int64_t offset,
        int64_t length)
{
    LOG_DEBUG("OMXPlayer set data source, fd:%d, offset: %lld, length: %lld.\n",
            fd, offset, length);

    if(offset != 0 && (length == 17681 || length == 16130)){
        // for quadrant test
        qdFlag = true;
    }
    else
        qdFlag = false;

    if (mSharedFd >= 0) {
        close(mSharedFd);
        mSharedFd = -1;
    }

    contentURI = (char*)FSL_MALLOC(128);
    if(contentURI == NULL)
        return UNKNOWN_ERROR;

    mSharedFd = dup(fd);
    sprintf(contentURI, "sharedfd://%d:%" PRId64 ":%" PRId64 ":%d",  mSharedFd, offset, length, mMediaType);
    LOG_DEBUG("OMXPlayer contentURI: %s\n", contentURI);

    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    if(gm == NULL)
        return UNKNOWN_ERROR;

    if(OMX_TRUE != gm->preload(gm, contentURI, fsl_osal_strlen(contentURI))) {
        LOG_DEBUG("OMXPlayer::setDataSourcce() preload contentURI failed: %s\n", contentURI);
        gm->stop(gm);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}


status_t OMXPlayer::setVideoSurfaceTexture(const sp<IGraphicBufferProducer>& bufferProducer)
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;

    LOG_DEBUG("OMXPlayer setVideoSurfaceTexture\n");

    if(gm == NULL)
        return BAD_VALUE;

    if (bufferProducer != NULL)
        mNativeWindow = FSL_NEW(Surface, (bufferProducer));
    else
        mNativeWindow = NULL;

    fsl_osal_mutex_lock((fsl_osal_mutex)queueLock);

    if(msg == MSG_PREPAREASYNC){
        fsl_osal_mutex_unlock((fsl_osal_mutex)queueLock);
        DoSetVideoSurfaceTexture();
    }
    else{
        ASYNC_COMMAND cmd = {MSG_SETSURFACE, 0};
        EnQueueCommand(&cmd);
        fsl_osal_mutex_unlock((fsl_osal_mutex)queueLock);
    }
    return NO_ERROR;

}

status_t OMXPlayer::DoSetVideoSurfaceTexture()
{
    LOG_DEBUG("OMXPlayer::DoSetVideoSurfaceTexture\n");
    OMX_GraphManager* gm = (OMX_GraphManager*)player;

    if (mNativeWindow != NULL) {
        if(gm != NULL){
            gm->setVideoSurface(gm, &mNativeWindow, OMX_VIDEO_SURFACE_TEXTURE);
            if(strncmp(contentURI, "sharedfd://", 11) != 0) {
                GM_STREAMINFO sStreamInfo;
                if(gm->getStreamInfo(gm, GM_VIDEO_STREAM_INDEX, &sStreamInfo))
                    sendEvent(MEDIA_SET_VIDEO_SIZE, sStreamInfo.streamFormat.video_info.nFrameWidth, sStreamInfo.streamFormat.video_info.nFrameHeight);
            }
        }
    } else {
        if(gm != NULL)
            gm->setVideoSurface(gm, NULL, OMX_VIDEO_SURFACE_TEXTURE);
    }
    return NO_ERROR;
}

status_t OMXPlayer::prepare()
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    char value[PROPERTY_VALUE_MAX];
    int i;
    OMX_VIDEO_RENDER_TYPE videoRender = OMX_VIDEO_RENDER_SURFACE;

    LOG_DEBUG("OMXPlayer prepare.\n");

    if(gm == NULL)
        return BAD_VALUE;

    gm->registerEventHandler(gm, this, eventhandler);
    gm->setAudioSink(gm, &mAudioSink);

    if(OMX_TRUE != gm->setVideoRenderType(gm, videoRender)) {
        LOG_DEBUG("to check SetVideoRenderType %d... \n", videoRender);
        gm->stop(gm);
        return UNKNOWN_ERROR;
    }

    fsl_osal_memset(value, 0, PROPERTY_VALUE_MAX);
    property_get("rw.HTTP_PROXY", value, "");
    //printf("rw.HTTP_PROXY [%s]\n", value);
    if(fsl_osal_strlen(value) > 0){
        if(fsl_osal_strncasecmp(value,"http://",7)){
            char *temp = (char *)FSL_MALLOC(fsl_osal_strlen(value) + 8);
            fsl_osal_memset(temp,0,fsl_osal_strlen(value) + 8);
            fsl_osal_strcpy(temp,"http://");
            fsl_osal_strcpy(temp + 7, value);
            setenv("http_proxy", temp, 1);
            FSL_FREE(temp);
        }
        else
            setenv("http_proxy", value, 1);
    }
    else
        unsetenv("http_proxy");

    if(OMX_TRUE != gm->load(gm, contentURI, fsl_osal_strlen(contentURI))) {
        LOG_DEBUG("OMXPlayer::Prepare() load contentURI fail %s\n", contentURI);
        gm->stop(gm);
        return UNKNOWN_ERROR;
    }

    GM_STREAMINFO sStreamInfo;
    if(gm->getStreamInfo(gm, GM_VIDEO_STREAM_INDEX, &sStreamInfo)) {
        sendEvent(MEDIA_SET_VIDEO_SIZE, sStreamInfo.streamFormat.video_info.nFrameWidth, sStreamInfo.streamFormat.video_info.nFrameHeight);
    }


    LOG_DEBUG("OMXPlayer prepare finished\n");

    return NO_ERROR;
}

status_t OMXPlayer::prepareAsync()
{
    LOG_DEBUG("OMXPlayer prepareAsync.\n");

    ASYNC_COMMAND cmd = {MSG_PREPAREASYNC, 0};
    EnQueueCommand(&cmd);

    return NO_ERROR;
}

status_t OMXPlayer::start()
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    GM_STATE State = GM_STATE_NULL;
	OMX_U32 nCount = 0;

    LOG_DEBUG("OMXPlayer start.\n");

    if(gm == NULL)
        return BAD_VALUE;

    gm->getState(gm, &State);

    if(msg == MSG_PREPAREASYNC || State == GM_STATE_LOADED) {
        LOG_DEBUG("OMXPlayer start send MSG_START\n");
        ASYNC_COMMAND cmd = {MSG_START, 0};
        EnQueueCommand(&cmd);

    }
    else{
        LOG_DEBUG("OMXPlayer start send MSG_RESUME\n");
        ASYNC_COMMAND cmd = {MSG_RESUME, 0};
        EnQueueCommand(&cmd);
    }

    LOG_DEBUG("OMXPlayer start finished.\n");

    bPlaying = true;

    return NO_ERROR;
}

status_t OMXPlayer::stop()
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;

    LOG_DEBUG("OMXPlayer stop %p.\n", gm);

    if(gm == NULL)
        return BAD_VALUE;

    { // clean up messages in queue, except prepare, set surface
        ASYNC_COMMAND tempCmd;
        fsl_osal_u32 i;

        fsl_osal_mutex_lock((fsl_osal_mutex)queueLock);

        Queue *pCmdQueue = (Queue*) cmdQueue;
        if(pCmdQueue != NULL && pCmdQueue->Size() >= 1){
            for(i = pCmdQueue->Size(); i >= 1; i--){
                if(pCmdQueue->Access(&tempCmd, i) == QUEUE_SUCCESS){
                    if(tempCmd.type == MSG_SEEKTO || tempCmd.type == MSG_START || tempCmd.type == MSG_PAUSE || tempCmd.type == MSG_RESUME)
                        pCmdQueue->Get(&tempCmd, i);
                }
            }
        }

        fsl_osal_mutex_unlock((fsl_osal_mutex)queueLock);
    }

    gm->stop(gm);

    LOG_DEBUG("OMXPlayer stop %p ok.\n", gm);

    return NO_ERROR;
}

status_t OMXPlayer::pause()
{

    LOG_DEBUG("OMXPlayer pause\n");

    ASYNC_COMMAND cmd = {MSG_PAUSE, 0};
    EnQueueCommand(&cmd);

    bPlaying = false;

    return NO_ERROR;

}

bool OMXPlayer::isPlaying()
{
    LOG_DEBUG("OMXPlayer isPlaying.\n");

    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    if(gm == NULL)
        return false;

    return bPlaying;
}

status_t OMXPlayer::seekTo(int msec)
{
    LOG_DEBUG("OMXPlayer seekTo %d\n", msec/1000);

    ASYNC_COMMAND cmd = {MSG_SEEKTO, msec};
    EnQueueCommand(&cmd);

    LOG_DEBUG("OMXPlayer seekTo exit\n");

    return NO_ERROR;
}

status_t OMXPlayer::getCurrentPosition(int *msec)
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    OMX_TICKS ts = 0;

    if(gm == NULL)
        return BAD_VALUE;

    gm->getPosition(gm, &ts);
    *msec = ts / 1000;

    //LOG_DEBUG("OMXPlayer Cur: %lld\n", ts);

    return NO_ERROR;
}

status_t OMXPlayer::getDuration(int *msec)
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    OMX_TICKS ts = 0;

    // let quadrant have a proper score
    if(qdFlag)
        fsl_osal_sleep(0);

    if(gm == NULL)
        return BAD_VALUE;

    gm->getDuration(gm, &ts);
    *msec = ts / 1000;

    LOG_DEBUG("OMXPlayer Dur: %lld\n", ts);

    return NO_ERROR;
}

status_t OMXPlayer::reset()
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;

    LOG_DEBUG("OMXPlayer Reset %p.\n", gm);

    stop();

    sendEvent(MEDIA_STOPPED);

    if (mSharedFd >= 0) {
        close(mSharedFd);
        mSharedFd = -1;
    }

    return NO_ERROR;
}

status_t OMXPlayer::setLooping(int loop)
{
    bLoop = loop > 0 ? true : false;
    return NO_ERROR;
}

status_t OMXPlayer::setVideoScalingMode(int32_t mode) {
    LOG_DEBUG("OMXPlayer::setVideoScalingMode: %d\n", mode);

    OMX_GraphManager* gm = (OMX_GraphManager*)player;

    if(gm != NULL)
        gm->setVideoScalingMode(gm, mode);

    return OK;
}

status_t OMXPlayer::getTrackInfo(Parcel *reply) {

    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    if(gm == NULL)
        return BAD_VALUE;
    OMX_U32 nVideoTrackNum = gm->GetVideoTrackNum(gm);
    OMX_U32 nAudioTrackNum = gm->GetAudioTrackNum(gm);
    OMX_U32 nSubtitleTrackNum = gm->GetSubtitleTrackNum(gm);
    reply->writeInt32(nVideoTrackNum + nAudioTrackNum + nSubtitleTrackNum);

    OMX_S32 fields = 3;
    OMX_U32 i;
    for (i= 0; i < nVideoTrackNum; i ++) {
        GM_VIDEO_TRACK_INFO sInfo;
        if(!gm->GetVideoTrackInfo(gm,i,&sInfo))
            continue;

        reply->writeInt32(fields); // 2 fields
        reply->writeInt32(MEDIA_TRACK_TYPE_VIDEO);
        reply->writeString16(String16((const char*)sInfo.mime));
        reply->writeString16(String16((const char*)sInfo.lanuage));
        LOG_DEBUG("OMXPlayer::getTrackInfo: video mime=%s,lang=%s\n",sInfo.mime,sInfo.lanuage);
    }

    for (i= 0; i < nAudioTrackNum; i ++) {
        GM_AUDIO_TRACK_INFO sInfo;
        if(!gm->GetAudioTrackInfo(gm,i,&sInfo))
            continue;
        reply->writeInt32(fields); // 2 fields
        reply->writeInt32(MEDIA_TRACK_TYPE_AUDIO);
        reply->writeString16(String16((const char*)sInfo.mime));
        reply->writeString16(String16((const char*)sInfo.lanuage));
        LOG_DEBUG("OMXPlayer::getTrackInfo: audio mime=%s,lang=%s\n",sInfo.mime,sInfo.lanuage);
    }

    for (i = 0; i < nSubtitleTrackNum; i ++) {
        GM_SUBTITLE_TRACK_INFO sInfo;
        if(!gm->GetSubtitleTrackInfo(gm,i,&sInfo))
            continue;

        reply->writeInt32(fields); // 2 fields
        reply->writeInt32(MEDIA_TRACK_TYPE_TIMEDTEXT);
        reply->writeString16(String16((const char*)sInfo.mime));
        reply->writeString16(String16((const char*)sInfo.lanuage));
        LOG_DEBUG("OMXPlayer::getTrackInfo: subtitle mime=%s,lang=%s\n",sInfo.mime,sInfo.lanuage);
    }

    return OK;
}

status_t OMXPlayer::invoke(
        const Parcel& request,
        Parcel *reply)
{
    if (NULL == reply) {
        return android::BAD_VALUE;
    }
    int32_t methodId;
    status_t ret = request.readInt32(&methodId);
    if (ret != android::OK) {
        return ret;
    }

    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    if(gm == NULL)
        return BAD_VALUE;

    switch(methodId) {
        case INVOKE_ID_SET_VIDEO_SCALING_MODE:
        {
            int mode = request.readInt32();
            gm->setVideoScalingMode(gm, mode);
            break;
        }

        case INVOKE_ID_GET_TRACK_INFO:
        {
            getTrackInfo(reply);
            break;
        }

        default:
        {
            return BAD_VALUE;
        }
    }

    // It will not reach here.
    return OK;
}

status_t OMXPlayer::getMetadata(
        const SortedVector<media::Metadata::Type>& ids,
        Parcel *records)
{
    using media::Metadata;

    Metadata metadata(records);

    metadata.appendBool(
            Metadata::kPauseAvailable,
            true);

    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    bool seekable = false;
    if(gm)
        seekable = gm->isSeekable(gm);

    metadata.appendBool(
            Metadata::kSeekBackwardAvailable,
            seekable);

    metadata.appendBool(
            Metadata::kSeekForwardAvailable,
            seekable);

    metadata.appendBool(
            Metadata::kSeekAvailable,
            seekable);

    return NO_ERROR;
}


status_t OMXPlayer::ProcessEvent(int eventID, void* Eventpayload)
{
    GM_EVENT EventID = (GM_EVENT) eventID;

    switch(EventID) {
        case GM_EVENT_EOS:
            LOG_DEBUG("OMXPlayer Received GM_EVENT_EOS.\n");
            if(true != bLoop) {
                fsl_osal_sleep(1000); //force OS sheduling
                LOG_DEBUG ("Pause as EOS.\n");
                pause();
                LOG_DEBUG("OMXPlayer send MEDIA_PLAYBACK_COMPLETE");
                sendEvent(MEDIA_PLAYBACK_COMPLETE);
            }
            else
                DoSeekTo(0);

            break;
        case GM_EVENT_BW_BOS:
            LOG_DEBUG("OMXPlayer Received GM_EVENT_BW_BOS.\n");
            sendEvent(MEDIA_PLAYBACK_COMPLETE);
            break;
        case GM_EVENT_FF_EOS:
            LOG_DEBUG("OMXPlayer Received GM_EVENT_FF_EOS.\n");
            sendEvent(MEDIA_PLAYBACK_COMPLETE);
            break;
        case GM_EVENT_CORRUPTED_STREM:
            sendEvent(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN);
            break;
        case GM_EVENT_BUFFERING_UPDATE:
            if(Eventpayload == NULL)
                break;
            LOG_DEBUG("Buffering update to: %d\n", (*((OMX_S32 *)Eventpayload)));
            sendEvent(MEDIA_BUFFERING_UPDATE, (*((OMX_S32 *)Eventpayload)));
            break;

        case GM_EVENT_RENDERING_START:
            sendEvent(MEDIA_INFO, MEDIA_INFO_RENDERING_START);
            break;

        case GM_EVENT_SET_SURFACE:
            {
            LOG_DEBUG("OMXPlayer Received GM_EVENT_SET_SURFACE.\n");
            GM_STATE State;
            OMX_GraphManager* gm = (OMX_GraphManager*)player;

            gm->getState(gm, &State);
            if(State == GM_STATE_STOP)
                break;
            mNativeWindow = (Eventpayload == NULL) ? NULL : *((ANativeWindow **)Eventpayload);
            ASYNC_COMMAND cmd = {MSG_SETSURFACE, 0};
            EnQueueCommand(&cmd);
            }
            break;
        case GM_EVENT_SET_VIDEO_SIZE:
            {
            OMX_GraphManager* gm = (OMX_GraphManager*)player;
            if (mNativeWindow != NULL && gm != NULL) {
                GM_STREAMINFO sStreamInfo;
                if(gm->getStreamInfo(gm, GM_VIDEO_STREAM_INDEX, &sStreamInfo))
                    sendEvent(MEDIA_SET_VIDEO_SIZE, sStreamInfo.streamFormat.video_info.nFrameWidth, sStreamInfo.streamFormat.video_info.nFrameHeight);
                LOG_DEBUG("OMX player send GM_EVENT_SET_VIDEO_SIZE");
            }
            }
            break;

        default:
            break;
    }

    return NO_ERROR;
}

status_t OMXPlayer::ProcessAsyncCmd()
{
    status_t ret = NO_ERROR;
    OMX_GraphManager* gm = (OMX_GraphManager*)player;
    ASYNC_COMMAND cmd;
    QUEUE_ERRORTYPE retQ;
    GM_STATE State = GM_STATE_NULL;

    fsl_osal_sem_wait(sem);

    fsl_osal_mutex_lock((fsl_osal_mutex)queueLock);
    retQ = ((Queue *)cmdQueue)->Get(&cmd);
    if(retQ == QUEUE_SUCCESS)
        msg = cmd.type;
    else
        msg = MSG_NONE;
    fsl_osal_mutex_unlock((fsl_osal_mutex)queueLock);

    if(bStopPCmdThread == OMX_TRUE)
        return UNKNOWN_ERROR;

    if(retQ == QUEUE_NOT_READY)
        return NO_ERROR;

    if(retQ != QUEUE_SUCCESS){
        LOG_ERROR("ProcessAsyncCmd command queue error %d!", retQ);
        return UNKNOWN_ERROR;
    }

    if(cmd.type == MSG_SETSURFACE)
        LOG_DEBUG("ProcessAsyncCmd %s/%p", CmdName[cmd.type], (mNativeWindow != NULL) ? &mNativeWindow : 0);
    else
        LOG_DEBUG("ProcessAsyncCmd %s/%d", CmdName[cmd.type], cmd.data);

    switch(msg) {
        case MSG_PREPAREASYNC:
            ret = prepare();
            if(ret != NO_ERROR)
                sendEvent(MEDIA_ERROR, ret);
            else
                sendEvent(MEDIA_PREPARED);
            break;

        case MSG_SETSURFACE:
            DoSetVideoSurfaceTexture();

            break;

        case MSG_RESUME:
            if(gm == NULL)
                break;
            gm->getState(gm, &State);

            if(State == GM_STATE_PAUSE) {
                gm->resume(gm);
                sendEvent(MEDIA_STARTED);
            }

            else if(State == GM_STATE_EOS) {
                DoSeekTo(0);
                //sometimes seek generates eos events, need time to handle them and enter pause&eos state
                fsl_osal_sleep(100000);
                gm->resume(gm);
            }
            else
                LOG_ERROR("incorrect state when doing MSG_RESUME %d", State);

            break;

        case MSG_START:
            gm->start(gm);
            sendEvent(MEDIA_STARTED);
            break;

        case MSG_SEEKTO:
            DoSeekTo(cmd.data);
            //seek generates eos events if network fail, need wait some time to
            // let GMPlayer handle them in SysEventHandler and enter eos state
            fsl_osal_sleep(100000);
            // when quadrant is playing, seek complete event is sent by media player to apk to increase performance, no need to sent it here.
            if(!qdFlag)
                sendEvent(MEDIA_SEEK_COMPLETE);
            break;

        case MSG_PAUSE:
            if(gm == NULL)
                break;

            gm->pause(gm);
            sendEvent(MEDIA_PAUSED);

            break;

        case MSG_NONE:
        default:
            break;
    }

    msg = MSG_NONE;

    return NO_ERROR;
}

status_t OMXPlayer::DoSeekTo(int msec)
{
    OMX_GraphManager* gm = (OMX_GraphManager*)player;

    if(gm == NULL)
        return BAD_VALUE;

    LOG_DEBUG("OMXPlayer DoSeekTo %d\n", msec/1000);

    gm->seek(gm, OMX_TIME_SeekModeFast, (OMX_TICKS)msec*1000);

    LOG_DEBUG("OMXPlayer DoSeekTo %d ok\n", msec/1000);

	return NO_ERROR;
}

status_t OMXPlayer::setParameter(int key, const Parcel &request)
{
	return NO_ERROR;
}

status_t OMXPlayer::getParameter(int key, Parcel *reply)
{
	return NO_ERROR;
}


status_t OMXPlayer::EnQueueCommand(ASYNC_COMMAND * pCmd)
{

    ASYNC_COMMAND tempCmd;
    fsl_osal_u32 i;
    QUEUE_ERRORTYPE ret = QUEUE_SUCCESS;

    fsl_osal_mutex_lock((fsl_osal_mutex)queueLock);

    Queue *pCmdQueue = (Queue*) cmdQueue;
    if(pCmd == NULL || pCmdQueue == NULL){
        fsl_osal_mutex_unlock((fsl_osal_mutex)queueLock);
        return BAD_VALUE;
    }

    if(pCmd->type == MSG_SETSURFACE)
        LOG_DEBUG("EnQueueCommand %s/%p queue size %d", CmdName[pCmd->type], (mNativeWindow != NULL) ? &mNativeWindow : 0, pCmdQueue->Size());
    else
        LOG_DEBUG("EnQueueCommand %s/%d queue size %d", CmdName[pCmd->type], pCmd->data, pCmdQueue->Size());


    switch(pCmd->type){
        case MSG_PREPAREASYNC:
        case MSG_START:
            ret = pCmdQueue->Add(pCmd);
            break;

        case MSG_SEEKTO:
        case MSG_SETSURFACE:
            for(i = 1; i <= pCmdQueue->Size(); i++){
                if(pCmdQueue->Access(&tempCmd, i) == QUEUE_SUCCESS){
                    if(tempCmd.type == pCmd->type)
                        break;
                }
            }
            if(i <= pCmdQueue->Size())
                pCmdQueue->Get(&tempCmd, i);

            ret = pCmdQueue->Add(pCmd);

            // when prepare is followed by setsurface , adjust their sequence because prepare need a valid surface
            if(pCmd->type == MSG_SETSURFACE && pCmdQueue->Size() > 1){
                if(pCmdQueue->Access(&tempCmd, pCmdQueue->Size() - 1) == QUEUE_SUCCESS){
                    if(tempCmd.type == MSG_PREPAREASYNC){
                        LOG_DEBUG("adjust sequence of setsurface and prepare");
                        pCmdQueue->Get(&tempCmd, pCmdQueue->Size() - 1);
                        ASYNC_COMMAND cmd = {MSG_PREPAREASYNC, 0};
                        ret = pCmdQueue->Add(&cmd);
                    }
                }
            }

            break;

        case MSG_PAUSE:
        case MSG_RESUME:
            for(i = 1; i <= pCmdQueue->Size(); i++){
                if(pCmdQueue->Access(&tempCmd, i) == QUEUE_SUCCESS){
                    if(tempCmd.type == MSG_PAUSE || tempCmd.type == MSG_RESUME)
                        break;
                }
            }
            if(i <= pCmdQueue->Size())
                pCmdQueue->Get(&tempCmd, i);

            ret = pCmdQueue->Add(pCmd);
            break;

        default:
            LOG_ERROR("EnQueueCommand: uknown command %d!", pCmd->type);
            ret = QUEUE_FAILURE;
            break;
    }


    fsl_osal_mutex_unlock((fsl_osal_mutex)queueLock);

    if(ret == QUEUE_SUCCESS)
        fsl_osal_sem_post(sem);
    else
        LOG_ERROR("EnQueueCommand queue add fail %d, queue size %d", ret, pCmdQueue->Size());

    return NO_ERROR;
}


OMXPlayerType::OMXPlayerType()
{
}

OMXPlayerType::~OMXPlayerType()
{
}

URL_TYPE OMXPlayerType::IsSupportedUrl(const char *url)
{
    if (!fsl_osal_strncasecmp(url, "udp://", 6) \
            || !fsl_osal_strncasecmp(url, "rtp://", 6))
        return URL_SUPPORT;

    return URL_NOT_SUPPORT;
}


