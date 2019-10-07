/**
 *  Copyright (c) 2009-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file AudioRender.h
 *  @brief Class definition of AudioRender Component
 *  @ingroup AudioRender
 */

#ifndef AudioRender_h
#define AudioRender_h

#include "ComponentBase.h"
#include "RingBuffer.h"
#include "AudioTSManager.h"
#include "FadeInFadeOut.h"

#define NUM_PORTS 2
#define IN_PORT 0
#define CLK_PORT 1

#define FADEPROCESSTIME 100  /**< 100 ms for fade process */


class AudioRender : public ComponentBase {
    protected:
        OMX_AUDIO_PARAM_PCMMODETYPE PcmMode;
		OMX_U32 nPeriodSize;
		OMX_U32 nSampleSize;
        OMX_U32 nFadeInFadeOutProcessLen;
        OMX_U32 nClockScale;
		OMX_BOOL bSendEOS;
    private:
        OMX_ERRORTYPE InitComponent();
        OMX_ERRORTYPE DeInitComponent();
        OMX_ERRORTYPE InstanceInit();
        OMX_ERRORTYPE InstanceDeInit();
        OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
		OMX_ERRORTYPE DoLoaded2Idle();
		OMX_ERRORTYPE DoIdle2Loaded();
		OMX_ERRORTYPE DoExec2Pause();
		OMX_ERRORTYPE DoPause2Exec();
		OMX_ERRORTYPE DoExec2Idle();
		OMX_ERRORTYPE ComponentReturnBuffer(OMX_U32 nPortIndex);
		OMX_ERRORTYPE FlushComponent(OMX_U32 nPortIndex);
		OMX_ERRORTYPE ProcessDataBuffer();
		OMX_ERRORTYPE ProcessInputBufferFlag();
		OMX_ERRORTYPE ClockGetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
		OMX_ERRORTYPE ClockSetConfig(OMX_INDEXTYPE nParamIndex, \
				OMX_TIME_CONFIG_TIMESTAMPTYPE *pTimeStamp);
		OMX_ERRORTYPE ProcessInputDataBuffer();
		OMX_ERRORTYPE ProcessOutputDataBuffer();
        OMX_ERRORTYPE ProcessClkBuffer();
        OMX_ERRORTYPE SyncTS(OMX_TICKS nTimeStamp);
        OMX_ERRORTYPE RenderData();
        OMX_ERRORTYPE RenderPeriod();

		/** Audio render device related */
        virtual OMX_ERRORTYPE SelectDevice(OMX_PTR device);
        virtual OMX_ERRORTYPE OpenDevice() = 0;
        virtual OMX_ERRORTYPE CloseDevice() = 0;
        virtual OMX_ERRORTYPE SetDevice() = 0;
        virtual OMX_ERRORTYPE ResetDevice() = 0;
        virtual OMX_ERRORTYPE DrainDevice() = 0;
		virtual OMX_ERRORTYPE DeviceDelay(OMX_U32 *nDelayLen) = 0;
        virtual OMX_ERRORTYPE WriteDevice(OMX_U8 *pBuffer, OMX_U32 nActuralLen, OMX_U32 *nConsumedLen = NULL) = 0;
        virtual OMX_ERRORTYPE ConvertData(OMX_U8* pOut, OMX_U32 *nOutSize, OMX_U8 *pIn, OMX_U32 nInSize);
        virtual OMX_BOOL IsNeedConvertData();
        virtual OMX_U32 DataLenOut2In(OMX_U32 nLength);
        virtual OMX_ERRORTYPE SetPlaybackRate(OMX_PTR rate);

		virtual OMX_ERRORTYPE AudioRenderDoExec2Pause();
		virtual OMX_ERRORTYPE AudioRenderDoPause2Exec();

		RingBuffer AudioRenderRingBuffer;
		AudioTSManager TS_Manager;
		FadeInFadeOut AudioRenderFadeInFadeOut;
		OMX_AUDIO_CONFIG_VOLUMETYPE Volume;
		OMX_AUDIO_CONFIG_MUTETYPE Mute;
        OMX_BUFFERHEADERTYPE *pInBufferHdr;
        OMX_BUFFERHEADERTYPE *pInBufferHdrBak;
		OMX_TIME_CONFIG_TIMESTAMPTYPE StartTime;
		OMX_TIME_CONFIG_TIMESTAMPTYPE ReferTime;
		OMX_U32 nWriteOutLen;
        OMX_U32 nAudioDataWriteThreshold;
		OMX_BOOL bClockRuningFlag;
		OMX_BOOL bHeardwareError;
		OMX_BOOL bReceivedEOS;
		OMX_BOOL bLiveMode;
		OMX_TICKS CurrentTS;
        OMX_U32 nDiscardData;
        OMX_U32 nAddZeros;
		OMX_U32 nDNSeScale;
		OMX_S64 nTotalConsumeredLen;
		OMX_S64 nTotalReceivedLen;
		OMX_TICKS audioIntervalThreshold;
        OMX_PLAYBACK_MODE playbackMode;
        OMX_BOOL bNeedConvertData;
        OMX_U8* pBufferBak;
        OMX_U32 nBufferBakSize;
        OMX_U32 nDataSize;
        OMX_U32 nDataOffset;
};

#endif
/* File EOF */
