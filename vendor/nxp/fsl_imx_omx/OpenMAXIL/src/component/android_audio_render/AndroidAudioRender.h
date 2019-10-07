/**
 *  Copyright (c) 2010-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file AndroidAudioRender.h
 *  @brief Class definition of AndroidAudioRender Component
 *  @ingroup AndroidAudioRender
 */

#ifndef AndroidAudioRender_h
#define AndroidAudioRender_h

#if (ANDROID_VERSION <= ICS)
#include <Errors.h>
#include <MediaPlayerService.h>
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
#include <utils/Errors.h>
#endif

#include <AudioTrack.h>
#include <MediaPlayerInterface.h>
#include "ComponentBase.h"
#include "AudioRender.h"

namespace android {

class AndroidAudioRender : public AudioRender {
    public:
        AndroidAudioRender();
    private:
        OMX_ERRORTYPE SelectDevice(OMX_PTR device);
        OMX_ERRORTYPE OpenDevice();
        OMX_ERRORTYPE CloseDevice();
        OMX_ERRORTYPE SetDevice();
        OMX_ERRORTYPE ResetDevice();
        OMX_ERRORTYPE DrainDevice();
        OMX_ERRORTYPE DeviceDelay(OMX_U32 *nDelayLen);
        OMX_ERRORTYPE WriteDevice(OMX_U8 *pBuffer, OMX_U32 nActuralLen, OMX_U32 *nConsumedLen);
        OMX_ERRORTYPE ConvertData(OMX_U8* pOut, OMX_U32 *nOutSize, OMX_U8 *pIn, OMX_U32 nInSize);
        OMX_BOOL IsNeedConvertData();
        OMX_U32 DataLenOut2In(OMX_U32 nLength);
        OMX_ERRORTYPE SetPlaybackRate(OMX_PTR rate);
        OMX_ERRORTYPE AudioRenderDoExec2Pause();
        OMX_ERRORTYPE AudioRenderDoPause2Exec();
        OMX_ERRORTYPE GetChannelMask(OMX_U32* mask);
        OMX_BOOL bNativeDevice;
        OMX_BOOL bOpened;
        sp<MediaPlayerBase::AudioSink> mAudioSink;

        OMX_AUDIO_CODINGTYPE audioType;
        OMX_U32 nWrited;
        OMX_U32 nBufferSize;
        OMX_U32 nChannelsOut;
        OMX_U32 nBitPerSampleOut;
#if (ANDROID_VERSION >= MARSH_MALLOW_600)
        AudioPlaybackRate mPlaybackSettings;
#endif
#if (ANDROID_VERSION <= ICS)
        OMX_S32 format;
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
        audio_format_t format;
#endif
};

}

#endif
/* File EOF */
