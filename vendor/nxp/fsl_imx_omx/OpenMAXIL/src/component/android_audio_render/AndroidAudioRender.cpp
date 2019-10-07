/**
 *  Copyright (c) 2010-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#if (ANDROID_VERSION <= ICS)
#include <AudioSystem.h>
#endif

#include "AndroidAudioRender.h"
#include <cutils/properties.h>

using namespace android;

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_WARNING
#define LOG_WARNING printf
#endif

#if (ANDROID_VERSION >= MARSH_MALLOW_600)
static unsigned int calculateMinFrameCount(OMX_U32 sampleRate)
{
    unsigned int afFrameCount, afSampleRate, afLatency, minBufCount;
    unsigned int defaultMinFrameCount = sampleRate * 500 / 1000;

    if (AudioSystem::getOutputFrameCount(&afFrameCount, AUDIO_STREAM_MUSIC) != NO_ERROR)
        return defaultMinFrameCount;

    if (AudioSystem::getOutputSamplingRate(&afSampleRate, AUDIO_STREAM_MUSIC) != NO_ERROR)
        return defaultMinFrameCount;

    if (AudioSystem::getOutputLatency(&afLatency, AUDIO_STREAM_MUSIC) != NO_ERROR)
        return defaultMinFrameCount;

    minBufCount = afLatency / ((1000 * afFrameCount) / afSampleRate);
    if (minBufCount < 2) {
        minBufCount = 2;
    }

    return minBufCount * sourceFramesNeededWithTimestretch(sampleRate, afFrameCount, afSampleRate, 2.0f);
}
#endif

AndroidAudioRender::AndroidAudioRender()
{
    LOG_DEBUG("%s", __FUNCTION__);
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_render.android.sw-based");
    ComponentVersion.s.nVersionMajor = 0x1;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x2;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_render.android";
    bInContext = OMX_FALSE;
    nPorts = NUM_PORTS;
    mAudioSink = NULL;
    bNativeDevice = OMX_FALSE;
    nWrited = 0;
    audioType = OMX_AUDIO_CodingPCM;
    bOpened = OMX_FALSE;
#if (ANDROID_VERSION >= MARSH_MALLOW_600)
    mPlaybackSettings = AUDIO_PLAYBACK_RATE_DEFAULT;
#endif

}

OMX_ERRORTYPE AndroidAudioRender::SelectDevice(OMX_PTR device)
{
    sp<MediaPlayerBase::AudioSink> *pSink;

    pSink = (sp<MediaPlayerBase::AudioSink> *)device;
    mAudioSink = *pSink;
    LOG_DEBUG("%s Set AudioSink %p\n", __FUNCTION__, &mAudioSink);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AndroidAudioRender::OpenDevice()
{
    status_t ret = NO_ERROR;

    LOG_DEBUG("AndroidAudioRender OpenDevice\n");

    if(mAudioSink == NULL) {
#ifdef RUN_WITH_GM
        /* this is used for run with oxmgm,
         * you need to define MediaPlayerService::AudioOutput
         * as public class in file MediaPlayerService.h */
        mAudioSink = FSL_NEW(MediaPlayerService::AudioOutput, ());
        if(mAudioSink == NULL) {
            LOG_ERROR("Failed to create AudioSink.\n");
            return OMX_ErrorInsufficientResources;
        }
        bNativeDevice = OMX_TRUE;
#else
        LOG_ERROR("Can't open mAudioSink device.\n");
        return OMX_ErrorHardware;
#endif

    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AndroidAudioRender::CloseDevice()
{
    LOG_DEBUG("AndroidAudioRender CloseDevice.\n");

    if(mAudioSink == NULL)
        return OMX_ErrorBadParameter;

    mAudioSink->stop();
    mAudioSink->close();
    nWrited = 0;
    bOpened = OMX_FALSE;
    if(bNativeDevice == OMX_TRUE) {
        LOG_DEBUG("%s, clear mAudioSink.\n", __FUNCTION__);
        mAudioSink.clear();
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE AndroidAudioRender::GetChannelMask(OMX_U32* mask)
{
    OMX_U32 i = 0;
    *mask = 0;
    if(PcmMode.nChannels > OMX_AUDIO_MAXCHANNELS){
        return OMX_ErrorBadParameter;
    }

    for(i = 0; i < PcmMode.nChannels; i++){

        switch(PcmMode.eChannelMapping[i]){
            case OMX_AUDIO_ChannelLF:
                *mask |= AUDIO_CHANNEL_OUT_FRONT_LEFT;
                break;
            case OMX_AUDIO_ChannelRF:
                *mask |= AUDIO_CHANNEL_OUT_FRONT_RIGHT;
                break;
            case OMX_AUDIO_ChannelCF:
                *mask |= AUDIO_CHANNEL_OUT_FRONT_CENTER;
                break;
            case OMX_AUDIO_ChannelLS:
                *mask |= AUDIO_CHANNEL_OUT_SIDE_LEFT;
                break;
            case OMX_AUDIO_ChannelRS:
                *mask |= AUDIO_CHANNEL_OUT_SIDE_RIGHT;
                break;
            case OMX_AUDIO_ChannelLFE:
                *mask |= AUDIO_CHANNEL_OUT_LOW_FREQUENCY;
                break;
            case OMX_AUDIO_ChannelCS:
                *mask |= AUDIO_CHANNEL_OUT_BACK_CENTER;
                break;
            case OMX_AUDIO_ChannelLR:
                *mask |= AUDIO_CHANNEL_OUT_BACK_LEFT;
                break;
            case OMX_AUDIO_ChannelRR:
                *mask |= AUDIO_CHANNEL_OUT_BACK_RIGHT;
                break;
            default:
                break;
        }

    }
    return OMX_ErrorNone;
}
OMX_ERRORTYPE AndroidAudioRender::SetDevice()
{
    status_t status = NO_ERROR;
    OMX_U32 nSamplingRate;
    OMX_U32 bufferCount;

    if(mAudioSink == NULL)
        return OMX_ErrorBadParameter;

    nChannelsOut = PcmMode.nChannels;
    nBitPerSampleOut = PcmMode.nBitPerSample;
#ifdef OMX_STEREO_OUTPUT
    if (nChannelsOut > 2)
        nChannelsOut = 2;
#endif

    if (nBitPerSampleOut > 16)
        nBitPerSampleOut = 16;

    LOG_DEBUG("SetDevice: SampleRate: %d, Channels: %d, formats: %d, nClockScale %x\n",
            PcmMode.nSamplingRate, nChannelsOut, nBitPerSampleOut, nClockScale);

    nSamplingRate = (OMX_U64)PcmMode.nSamplingRate * nClockScale / Q16_SHIFT;


    switch(nBitPerSampleOut) {
        case 8:
            #if (ANDROID_VERSION <= HONEY_COMB)
                format = AudioSystem::PCM_8_BIT;
            #elif (ANDROID_VERSION == ICS)
                format = AUDIO_FORMAT_PCM_SUB_8_BIT;
            #elif (ANDROID_VERSION >= JELLY_BEAN_42)
                format = AUDIO_FORMAT_PCM_8_BIT;
            #endif
            break;
        case 16:
            #if (ANDROID_VERSION <= HONEY_COMB)
                format = AudioSystem::PCM_16_BIT;
            #elif (ANDROID_VERSION == ICS)
                format = AUDIO_FORMAT_PCM_SUB_16_BIT;
            #elif (ANDROID_VERSION >= JELLY_BEAN_42)
                format = AUDIO_FORMAT_PCM_16_BIT;
            #endif
            break;
        default:
            return OMX_ErrorBadParameter;
    }

    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = IN_PORT;
    ports[IN_PORT]->GetPortDefinition(&sPortDef);
    audioType = sPortDef.format.audio.eEncoding;
    if(audioType == OMX_AUDIO_CodingIEC937)
        format = AUDIO_FORMAT_IEC937;

#if (ANDROID_VERSION <= ICS)
    if (bOpened == OMX_TRUE) {
        mAudioSink->stop();
        mAudioSink->close();
        bOpened = OMX_FALSE;
        nWrited = 0;
    }

    if(NO_ERROR != mAudioSink->open(nSamplingRate, nChannelsOut, \
                format, DEFAULT_AUDIOSINK_BUFFERCOUNT)) {
        LOG_ERROR("Failed to open AudioOutput device.\n");
        return OMX_ErrorHardware;
    }
    LOG_DEBUG("buffersize: %d, frameSize: %d, frameCount: %d\n",
            mAudioSink->bufferSize(), mAudioSink->frameSize(), mAudioSink->frameCount());

    nBufferSize = mAudioSink->bufferSize() * PcmMode.nChannels \
                  * PcmMode.nBitPerSample / nChannelsOut / nBitPerSampleOut;
    nSampleSize = mAudioSink->frameSize() * PcmMode.nChannels \
                  * PcmMode.nBitPerSample / nChannelsOut / nBitPerSampleOut;

    bOpened == OMX_TRUE;


#elif (ANDROID_VERSION >= JELLY_BEAN_42)
    if(audioType == OMX_AUDIO_CodingIEC937)
        format = AUDIO_FORMAT_PCM_16_BIT;

    #if (ANDROID_VERSION >= JELLY_BEAN_43)
    unsigned
    #endif
        int afFrameCount, afSampleRate;

    if (AudioSystem::getOutputFrameCount(&afFrameCount, AUDIO_STREAM_MUSIC) != NO_ERROR)
        return OMX_ErrorUndefined;

    if (AudioSystem::getOutputSamplingRate(&afSampleRate, AUDIO_STREAM_MUSIC) != NO_ERROR)
        return OMX_ErrorUndefined;

#if (ANDROID_VERSION >= MARSH_MALLOW_600)
    int frameCount = calculateMinFrameCount(PcmMode.nSamplingRate);
    int framePerBuffer = PcmMode.nSamplingRate * afFrameCount / afSampleRate;
    bufferCount = (frameCount + framePerBuffer - 1) / framePerBuffer;
#else
    bufferCount = DEFAULT_AUDIOSINK_BUFFERCOUNT;
    char value[PROPERTY_VALUE_MAX];
    if (property_get("ro.kernel.qemu", value, 0))
        bufferCount  = 12;  // to prevent systematic buffer underrun for emulator

    int frameCount = (nSamplingRate*afFrameCount*bufferCount)/afSampleRate;
#endif

    int frameSize;
    if (audio_is_linear_pcm(format)) {
        frameSize = nChannelsOut * audio_bytes_per_sample(format);
    } else {
        frameSize = sizeof(uint8_t);
    }

    nBufferSize = frameCount * frameSize * PcmMode.nChannels \
                  * PcmMode.nBitPerSample / nChannelsOut / nBitPerSampleOut;
    nSampleSize = frameSize * PcmMode.nChannels \
                  * PcmMode.nBitPerSample / nChannelsOut / nBitPerSampleOut;

    // clockscale changed during playback, reopen audiosink
    if (bOpened == OMX_TRUE) {
        OMX_U32 nSamplingRate;

        LOG_DEBUG("%s, reopen audio sink, SampleRate: %d, Channels: %d, nClockScale %x\n",
                __FUNCTION__, PcmMode.nSamplingRate, nChannelsOut, nClockScale);

        mAudioSink->stop();
        mAudioSink->close();
        nWrited = 0;

        // Need get from decoder.
        OMX_U32 channelMask = 0;
        GetChannelMask(&channelMask);


        nSamplingRate = (OMX_U64)PcmMode.nSamplingRate * nClockScale / Q16_SHIFT;

        audio_output_flags_t flags = (audioType == OMX_AUDIO_CodingIEC937) ? AUDIO_OUTPUT_FLAG_DIRECT : AUDIO_OUTPUT_FLAG_NONE;

        if(NO_ERROR != mAudioSink->open(nSamplingRate, nChannelsOut, \
                    channelMask, format, DEFAULT_AUDIOSINK_BUFFERCOUNT, NULL, NULL, flags)) {
            LOG_ERROR("Failed to open AudioOutput device.\n");
            return OMX_ErrorHardware;
        }

    }

#endif

#if (ANDROID_VERSION >= MARSH_MALLOW_600)
    nPeriodSize = nBufferSize/nSampleSize/bufferCount;
#else
    nPeriodSize = nBufferSize/nSampleSize/DEFAULT_AUDIOSINK_BUFFERCOUNT;
#endif

    if(audioType == OMX_AUDIO_CodingIEC937)
        nFadeInFadeOutProcessLen = 0;
    else {
        nFadeInFadeOutProcessLen = nSamplingRate * FADEPROCESSTIME / 1000 * nSampleSize;
    }

    return OMX_ErrorNone;
}

OMX_U32 AndroidAudioRender::DataLenOut2In(OMX_U32 nLength)
{
    OMX_U32 nActualLen = nLength;

    if(PcmMode.nBitPerSample == 24)
        nActualLen = nActualLen / (nBitPerSampleOut>>3) * (PcmMode.nBitPerSample>>3);
#ifdef OMX_STEREO_OUTPUT
    if (PcmMode.nChannels > 2)
        nActualLen = nActualLen / nChannelsOut * PcmMode.nChannels;
#endif

    return nActualLen;
}

OMX_BOOL AndroidAudioRender::IsNeedConvertData()
{
    OMX_BOOL bConvert = OMX_FALSE;

#if (ANDROID_VERSION >= LOLLIPOP_50)

#ifdef OMX_STEREO_OUTPUT
    if (PcmMode.nChannels > 2)
        bConvert = OMX_TRUE;
#endif

    if(PcmMode.nBitPerSample == 8 || PcmMode.nBitPerSample == 24)
        bConvert = OMX_TRUE;
#endif

    return bConvert;
}


OMX_ERRORTYPE AndroidAudioRender::ConvertData(OMX_U8* pOut, OMX_U32 *nOutSize, OMX_U8 *pIn, OMX_U32 nInSize)
{
    if(NULL == pOut || NULL == pIn)
        return OMX_ErrorUndefined;

#ifdef OMX_STEREO_OUTPUT
	if (PcmMode.nChannels > 2)
	{
		OMX_U32 i, j, Len;

		Len = nInSize / (PcmMode.nBitPerSample>>3) / PcmMode.nChannels;
		nInSize = Len * (PcmMode.nBitPerSample>>3) * nChannelsOut;

		switch(PcmMode.nBitPerSample)
		{
			case 8:
				{
					OMX_S8 *pSrc = (OMX_S8 *)pIn, *pDst = (OMX_S8 *)pOut;
					for (i = 0; i < Len; i ++)
					{
						*pDst++ = *pSrc++;
						*pDst++ = *pSrc++;
						pSrc += PcmMode.nChannels - 2;
					}
				}
				break;
			case 16:
				{
					OMX_S16 *pSrc = (OMX_S16 *)pIn, *pDst = (OMX_S16 *)pOut;
					for (i = 0; i < Len; i ++)
					{
						*pDst++ = *pSrc++;
						*pDst++ = *pSrc++;
						pSrc += PcmMode.nChannels - 2;
					}
				}
				break;
			case 24:
				{
					OMX_U8 *pSrc = (OMX_U8 *)pIn, *pDst = (OMX_U8 *)pOut;
					for (i = 0; i < Len; i ++)
					{
						*pDst++ = *pSrc++;
						*pDst++ = *pSrc++;
						*pDst++ = *pSrc++;
						*pDst++ = *pSrc++;
						*pDst++ = *pSrc++;
						*pDst++ = *pSrc++;
						pSrc += (PcmMode.nChannels - 2)*3;
					}
				}
				break;
		}
	}
#endif

	if(PcmMode.nBitPerSample == 8) {
		// Convert to U8
		OMX_S32 i,Len;
		OMX_U8 *pDst =(OMX_U8 *)pOut;
		OMX_S8 *pSrc =(OMX_S8 *)pIn;
		Len = nInSize;
		for(i=0;i<Len;i++) {
				*pDst++ = *pSrc++ + 128;
		}
	} else if(PcmMode.nBitPerSample == 24) {
		OMX_S32 i,j,Len;
		OMX_U8 *pDst =(OMX_U8 *)pOut;
		OMX_U8 *pSrc =(OMX_U8 *)pIn;
		Len = nInSize / (PcmMode.nBitPerSample>>3) / nChannelsOut;
		for(i=0;i<Len;i++) {
			for(j=0;j<(OMX_S32)PcmMode.nChannels;j++) {
				pDst[0] = pSrc[1];
				pDst[1] = pSrc[2];
				pDst+=2;
				pSrc+=3;
			}
		}
		nInSize = Len * (nBitPerSampleOut>>3) * nChannelsOut;
	}

    *nOutSize = nInSize;

    return OMX_ErrorNone;
}


OMX_ERRORTYPE AndroidAudioRender::WriteDevice(OMX_U8 *pBuffer, OMX_U32 nActuralLen, OMX_U32 *nConsumedLen)
{
    if(mAudioSink == NULL)
        return OMX_ErrorBadParameter;

    LOG_LOG("AndroidAudioRender write: %d\n", nActuralLen);

#if (ANDROID_VERSION >= LOLLIPOP_50)
    OMX_S32 ret;

    ret = mAudioSink->write(pBuffer, nActuralLen, false);
    *nConsumedLen = (ret > 0 ? ret : 0);
#else
    mAudioSink->write(pBuffer, nActuralLen);
    *nConsumedLen = nActuralLen;
#endif

    if(nWrited == 0){
        mAudioSink->start();
        LOG_DEBUG("WRITE DEVICE START");
    }

#if (ANDROID_VERSION >= LOLLIPOP_50)
    nWrited += DataLenOut2In(*nConsumedLen);
#else
    nWrited += *nConsumedLen;
#endif

    if(*nConsumedLen < nActuralLen)
        return OMX_ErrorNotComplete;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AndroidAudioRender::DrainDevice()
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE AndroidAudioRender::DeviceDelay(OMX_U32 *nDelayLen)
{
    OMX_U32 nPSize = nPeriodSize * nSampleSize;

    if(nWrited > nBufferSize + nPSize)
        *nDelayLen = nBufferSize + nPSize;
    else
        *nDelayLen = nWrited;

    //in ResetDevice(), flush() doesn't work sometimes for passthrough, so mPosition from getTimestamp doesn't reset to 0
    if(audioType == OMX_AUDIO_CodingIEC937)
        return OMX_ErrorNone;

#if (ANDROID_VERSION >= LOLLIPOP_50)
    if(nWrited > (nBufferSize + nPSize) * 2)
        *nDelayLen = (nBufferSize + nPSize) * 2;
    else
        *nDelayLen = nWrited;

    AudioTimestamp ts;
    status_t res = mAudioSink->getTimestamp(ts);
    if (res == OK) {
        OMX_U32 numFramesPlayed = ts.mPosition;
        OMX_U64 numFramesPlayedAt = ts.mTime.tv_sec * 1000000LL + ts.mTime.tv_nsec / 1000;
        OMX_U64 nowUs = systemTime(SYSTEM_TIME_MONOTONIC) / 1000ll;
        static const int64_t kStaleTimestamp100ms = 100000;

        //LOG_DEBUG("sampleSize %d, sampleingRate %d, nBufferSize %d, nPSize %d", nSampleSize, PcmMode.nSamplingRate, nBufferSize, nPSize);

        //After pausing, the MixerThread may go idle and numFramesPlayedAt will be stale
        const int64_t timestampAge = nowUs - numFramesPlayedAt;
        if (timestampAge > kStaleTimestamp100ms)
            numFramesPlayedAt = nowUs - kStaleTimestamp100ms;

        LOG_DEBUG("Writed frame %d, numFramesPlayed: %d, nowUs %lld, numFramesPlayedAt %lld",
            nWrited/nSampleSize, numFramesPlayed, nowUs, numFramesPlayedAt);

        OMX_S32 delay = nWrited - numFramesPlayed * nSampleSize - (nowUs - numFramesPlayedAt) * nSampleSize * PcmMode.nSamplingRate / OMX_TICKS_PER_SECOND ;

        LOG_DEBUG("%s, %d, %3d ms", __FUNCTION__, delay, delay * OMX_TICKS_PER_SECOND / nSampleSize / PcmMode.nSamplingRate / 1000);

        if(delay >= 0)
            *nDelayLen = delay;
        else
            LOG_ERROR("delay incorrect: %d", delay);

    }
#endif

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AndroidAudioRender::ResetDevice()
{
    if(mAudioSink == NULL)
        return OMX_ErrorBadParameter;

    OMX_STATETYPE State;
    GetState(&State);
    LOG_DEBUG("AndroidAudioRender::ResetDevice, state %d", State);

    //audio sink resets it's internal mPosition when flush() is called in pause state.
    //mPosition is used with nWrited to calculate device delay. nWrited is initialized to 0 when seeking, so
    //mPosition shall also be reset to 0 .
    if(State == OMX_StateExecuting)
        mAudioSink->pause();
    mAudioSink->flush();
    if(State == OMX_StateExecuting)
        mAudioSink->start();

    if(audioType == OMX_AUDIO_CodingIEC937) {
        mAudioSink->stop();
    }

#if (ANDROID_VERSION >= LOLLIPOP_50)

    if (bOpened == OMX_FALSE) {
        OMX_U32 channelMask = 0;
        OMX_U32 nFrameCount = 0;
        GetChannelMask(&channelMask);

        LOG_DEBUG("ResetDevice Open AudioSink: SampleRate: %d, Channels: %d, nClockScale %x nFrameCount %d\n",
                PcmMode.nSamplingRate, nChannelsOut, nClockScale, nFrameCount);

        OMX_U32 nSamplingRate = (OMX_U64)PcmMode.nSamplingRate * nClockScale / Q16_SHIFT;

        audio_output_flags_t flags = (audioType == OMX_AUDIO_CodingIEC937) ? AUDIO_OUTPUT_FLAG_DIRECT : AUDIO_OUTPUT_FLAG_NONE;
#if (ANDROID_VERSION >= MARSH_MALLOW_600)
        nFrameCount = calculateMinFrameCount(PcmMode.nSamplingRate);

        if(NO_ERROR != mAudioSink->open(nSamplingRate, nChannelsOut, \
                    channelMask, format, 0, NULL, NULL, flags, NULL, false, nFrameCount)) {
#else
        if(NO_ERROR != mAudioSink->open(nSamplingRate, nChannelsOut, \
                    channelMask, format, 0, NULL, NULL, flags)) {
#endif
            LOG_ERROR("Failed to open AudioOutput device.\n");
            return OMX_ErrorHardware;
        }
        bOpened = OMX_TRUE;

        //audio sink resets it's internal mPosition when flush() is called in pause state.
        //mPosition is used with nWrited to calculate device delay. nWrited is initialized to 0 when start playing, so
        //mPosition shall also be reset to 0 .
        mAudioSink->pause();
        mAudioSink->flush();

        mAudioSink->start();
    }

#endif

    nWrited = 0;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE AndroidAudioRender::SetPlaybackRate(OMX_PTR rate)
{

#if (ANDROID_VERSION >= MARSH_MALLOW_600)

    AudioPlaybackRate newRate;

    if(rate) {
        newRate = *(AudioPlaybackRate*)rate;
        if (newRate.mSpeed == 0.f) {
            newRate.mSpeed = mPlaybackSettings.mSpeed;
            mPlaybackSettings = newRate;
            return OMX_ErrorNone;
        }
    }
    else {
        mPlaybackSettings.mSpeed = (float)nClockScale / (float)Q16_SHIFT;
        newRate = mPlaybackSettings;
    }

    if(mAudioSink != NULL) {
        status_t err = mAudioSink->setPlaybackRate(newRate);
        if (err != OK) {
            LOG_ERROR("AndroidAudioRender setPlaybackRate fail\n");
            return OMX_ErrorUndefined;
        }
        mPlaybackSettings = newRate;
    }

    return OMX_ErrorNone;

#else

    return SetDevice();

#endif
}

OMX_ERRORTYPE AndroidAudioRender::AudioRenderDoExec2Pause()
{
    LOG_DEBUG("AndroidAudioRender::AudioRenderDoExec2Pause");
    if(mAudioSink == NULL)
        return OMX_ErrorBadParameter;

	if (bSendEOS == OMX_TRUE) {
#if (ANDROID_VERSION == FROYO)
		OMX_S64 latency = mAudioSink->latency() * 1000;
		printf("Add latency: %lld\n", latency);
		fsl_osal_sleep(latency);
#endif
        LOG_DEBUG("AndroidAudioRender::DoExec2Pause stop");
		mAudioSink->stop();
	} else {
	    LOG_DEBUG("AndroidAudioRender::DoExec2Pause pause");
		mAudioSink->pause();
	}
    return OMX_ErrorNone;
}

OMX_ERRORTYPE AndroidAudioRender::AudioRenderDoPause2Exec()
{
    if(mAudioSink == NULL)
        return OMX_ErrorBadParameter;
    LOG_DEBUG("AndroidAudioRender::AudioRenderDoPause2Exec");

    if (bOpened == OMX_TRUE)
        mAudioSink->start();
    return OMX_ErrorNone;
}


/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE AndroidAudioRenderInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        AndroidAudioRender *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(AndroidAudioRender, ());
        if(obj == NULL)
            return OMX_ErrorInsufficientResources;

        base = (ComponentBase*)obj;
        ret = base->ConstructComponent(pHandle);
        if(ret != OMX_ErrorNone)
            return ret;

        return ret;
    }
}

/* File EOF */
