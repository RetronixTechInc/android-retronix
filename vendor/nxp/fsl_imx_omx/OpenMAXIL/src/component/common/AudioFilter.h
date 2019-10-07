/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file AudioFilter.h
 *  @brief Class definition of AudioFilter Component
 *  @ingroup AudioFilter
 */

#ifndef AudioFilter_h
#define AudioFilter_h

#include "ComponentBase.h"
#include "RingBuffer.h"
#include "AudioTSManager.h"

#define AUDIO_FILTER_INPUT_PORT 0
#define AUDIO_FILTER_OUTPUT_PORT 1
#define AUDIO_FILTER_PORT_NUMBER 2
#define CHECKFRAMEHEADERREADLEN 1024
#define AUDIO_MAXIMUM_ERROR_COUNT 100
#define DEFAULT_REQUIRED_SIZE 10

typedef enum {
    AUDIO_FILTER_SUCCESS,
    AUDIO_FILTER_EOS,
    AUDIO_FILTER_FAILURE,
    AUDIO_FILTER_NEEDMORE,
    AUDIO_FILTER_FATAL_ERROR,
}AUDIO_FILTERRETURNTYPE;

class AudioFilter : public ComponentBase {
    public:
        AudioFilter();
		RingBuffer AudioRingBuffer;
		AudioTSManager TS_Manager;
		OMX_BOOL bReceivedEOS;
	protected:
        OMX_ERRORTYPE ProcessDataBuffer();
		OMX_S32 nInputPortBufferSize;
		OMX_S32 nOutputPortBufferSize;
		OMX_BOOL bDecoderEOS;
		OMX_U32 nPushModeInputLen;
		OMX_U32 nRingBufferScale;
		OMX_TICKS TS_PerFrame;
		OMX_BOOL bInstanceReset;
        OMX_BOOL bFirstInBuffer;
        OMX_BUFFERHEADERTYPE *pOutBufferHdr;
        OMX_BUFFERHEADERTYPE *pInBufferHdr;
        OMX_AUDIO_PARAM_PCMMODETYPE PcmMode; /**< For output port */
        OMX_DECODE_MODE ePlayMode;
        OMX_U32 nRequiredSize;//set by subclass for audio ring buffer len
        OMX_BOOL bStartTime;
        OMX_U32  nOutputBitPerSample;
        OMX_BOOL bSendFirstPortSettingChanged;
        OMX_BOOL bConvertEnable;

    private:
        OMX_ERRORTYPE InstanceInit();
        OMX_ERRORTYPE InstanceDeInit();
        virtual OMX_ERRORTYPE AudioFilterInstanceInit() = 0;
        virtual OMX_ERRORTYPE AudioFilterCodecInit() = 0;
        virtual OMX_ERRORTYPE AudioFilterInstanceDeInit() = 0;
        OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        virtual OMX_ERRORTYPE AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure) = 0;
        virtual OMX_ERRORTYPE AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure) = 0;
        virtual OMX_ERRORTYPE AudioFilterSetParameterPCM();
		virtual OMX_ERRORTYPE AudioFilterInstanceReset();
		virtual OMX_ERRORTYPE AudioFilterCheckCodecConfig();
		virtual OMX_ERRORTYPE AudioFilterCheckFrameHeader();
        virtual OMX_ERRORTYPE PassParamToOutputWithoutDecode();
        virtual AUDIO_FILTERRETURNTYPE AudioFilterFrame() = 0;
		virtual OMX_ERRORTYPE AudioFilterReset() = 0;
		OMX_ERRORTYPE ProcessInputBufferFlag();
		OMX_ERRORTYPE ProcessInputDataBuffer();
		OMX_ERRORTYPE ProcessOutputDataBuffer();
        virtual OMX_ERRORTYPE ComponentReturnBuffer(OMX_U32 nPortIndex);
        virtual OMX_ERRORTYPE  SetRoleFormat(OMX_STRING role);
        virtual OMX_ERRORTYPE AudioFilterHandleBOS();
        virtual OMX_ERRORTYPE AudioFilterHandleEOS();
        OMX_ERRORTYPE FlushComponent(OMX_U32 nPortIndex);
        OMX_ERRORTYPE ConvertData(OMX_U8* pOut, OMX_U32 *nOutSize, OMX_U8 *pIn, OMX_U32 nInSize);
		OMX_AUDIO_PARAM_PORTFORMATTYPE PortFormat[AUDIO_FILTER_PORT_NUMBER];
		OMX_BOOL bFirstFrame;
		OMX_BOOL bCodecInit;
		OMX_BOOL bDecoderInitFail;
		OMX_U8 * pConvertBuffer;
        OMX_BOOL bFirstOutput;

};

#endif
/* File EOF */
