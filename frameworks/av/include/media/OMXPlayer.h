/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copyright 2009-2015 Freescale Semiconductor, Inc. */

#ifndef ANDROID_OMXPLAYER_H
#define ANDROID_OMXPLAYER_H

#include <utils/Errors.h>
#include <media/MediaPlayerInterface.h>
#include <private/media/VideoFrame.h>

namespace android {

class ISurfaceTexture;

class OMXPlayer : public MediaPlayerInterface
{
public:
                        OMXPlayer(int nMediaType = 0);
    virtual             ~OMXPlayer();

    virtual status_t    initCheck();
    virtual status_t    setDataSource(const sp<IMediaHTTPService> &httpService, const char *url, const KeyedVector<String8, String8> *headers);
    virtual status_t    setDataSource(int fd, int64_t offset, int64_t length);
    virtual status_t    setVideoSurfaceTexture(const sp<IGraphicBufferProducer>& surfaceTexture);
    virtual status_t    prepare();
    virtual status_t    prepareAsync();
    virtual status_t    start();
    virtual status_t    stop();
    virtual status_t    pause();
    virtual bool        isPlaying();
    virtual status_t    setSyncSettings(const AVSyncSettings &sync, float videoFpsHint);
    virtual status_t    getSyncSettings(AVSyncSettings *sync, float *videoFps);
    virtual status_t    setPlaybackSettings(const AudioPlaybackRate &rate);
    virtual status_t    getPlaybackSettings(AudioPlaybackRate *rate);

    virtual status_t    seekTo(int msec);
    virtual status_t    getCurrentPosition(int *msec);
    virtual status_t    getDuration(int *msec);
    virtual status_t    reset();
    virtual status_t    setLooping(int loop);
    virtual player_type playerType() { return OMX_PLAYER; }
    virtual status_t    suspend();
    virtual status_t    resume();
    virtual status_t    invoke(const Parcel& request, Parcel *reply);
    virtual status_t    getMetadata(const SortedVector<media::Metadata::Type>& ids, Parcel *records);

    virtual status_t    setAudioEffect(int iBandIndex, int iBandFreq, int iBandGain);
    virtual status_t    setAudioEqualizer(bool isEnable);
    virtual status_t    captureCurrentFrame(VideoFrame** pvframe);
    virtual status_t    setVideoCrop(int top,int left, int bottom, int right);
    virtual int         getAudioTrackCount();
    virtual char*       getAudioTrackName(int index);
    virtual int         getCurrentAudioTrackIndex();
    virtual status_t    selectAudioTrack(int index);
    virtual status_t    setPlaySpeed(int speed);
    virtual status_t    setParameter(int key, const Parcel &request);
    virtual status_t    getParameter(int key, Parcel *reply);

    status_t            ProcessEvent(int eventID, void* Eventpayload);
    status_t            ProcessAsyncCmd();
    status_t            PropertyCheck();

private:
    typedef enum {
        MSG_NONE,
        MSG_PREPAREASYNC,
        MSG_START,
        MSG_SEEKTO,
        MSG_PAUSE,
        MSG_RESUME,
        MSG_SETSURFACE,
    }MSGTYPE;

    typedef struct {
        MSGTYPE type;
        int data;
    }ASYNC_COMMAND;

    void                *player;
    status_t            mInit;
    sp<ANativeWindow>   mNativeWindow;
    int                 mSharedFd;
    bool                bLoop;
    char                *contentURI;
    char                *subtitleURI;
    MSGTYPE             msg;
    int                 msgData;
    void                *sem;
    void                *pPCmdThread;
    bool                bStopPCmdThread;
    void                *pPCheckThread;
    bool                bStopPCheckThread;
    bool                bTvOut;
    bool                bDualDisplay;
    bool                bFB0Display;
    bool                bSuspend;
    int                 sLeft;
    int                 sRight;
    int                 sTop;
    int                 sBottom;
    int                 sRot;
    int                 mMediaType;
    status_t            setVideoDispRect(int top,int left, int bottom, int right);
    status_t            getSurfaceRegion(int *top,int *left, int *bottom, int *right, int *rot);
    status_t            CheckSurfaceRegion();
    status_t            CheckDualDisplaySetting();
    status_t            CheckTvOutSetting();
    status_t            CheckFB0DisplaySetting();
    status_t            DoSeekTo(int msec);
    status_t            setVideoScalingMode(int32_t mode);
    status_t            getTrackInfo(Parcel *reply);
    status_t            EnQueueCommand(ASYNC_COMMAND * pCmd);
    status_t            DoSetVideoSurfaceTexture();

    bool                qdFlag;
    bool                bNetworkFail;
    void                *cmdQueue;
    bool                bPlaying;
    void                *queueLock;
    
};

typedef enum {
    URL_SUPPORT = 0,
    URL_NOT_SUPPORT,
    URL_UNKNOWN,
}URL_TYPE;

class OMXPlayerType
{
public:
                        OMXPlayerType();
    virtual             ~OMXPlayerType();
    int                 IsSupportedContent(char *url);
    URL_TYPE            IsSupportedUrl(const char *url);
};

}; // namespace android

#endif // ANDROID_OMXPLAYER_H
