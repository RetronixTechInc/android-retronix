/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "AudioFilter.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif
AudioFilter::AudioFilter()
{
    ePlayMode = DEC_FILE_MODE;
    bSendFirstPortSettingChanged = OMX_TRUE;
    bFirstOutput = OMX_TRUE;
    bConvertEnable = OMX_FALSE;
    pInBufferHdr = pOutBufferHdr = NULL;
}

OMX_ERRORTYPE AudioFilter::GetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch (nParamIndex) {
		case OMX_IndexParamAudioPcm:
            {
				OMX_BOOL bOutputPort = OMX_FALSE;
                OMX_AUDIO_PARAM_PCMMODETYPE *pPcmMode;
                pPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pPcmMode, OMX_AUDIO_PARAM_PCMMODETYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
				if (pPcmMode->nPortIndex == AUDIO_FILTER_OUTPUT_PORT)
				{
					bOutputPort = OMX_TRUE;
				}

				fsl_osal_memcpy(pPcmMode, &PcmMode,	sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			}
			LOG_DEBUG("PcmMode.nSamplingRate = %d\n", PcmMode.nSamplingRate);
			break;
		default:
			ret = AudioFilterGetParameter(nParamIndex, pComponentParameterStructure);
			break;
	}

	return ret;
}

OMX_ERRORTYPE AudioFilter::SetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((OMX_U32)nParamIndex) {
        case OMX_IndexParamAudioPortFormat:
            {
                OMX_AUDIO_PARAM_PORTFORMATTYPE *pPortFormat;
                pPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
				if (pPortFormat->nPortIndex > AUDIO_FILTER_PORT_NUMBER - 1)
				{
					ret = OMX_ErrorBadPortIndex;
					break;
				}
				fsl_osal_memcpy(&PortFormat[pPortFormat->nPortIndex], pPortFormat, \
						sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
			break;
        case OMX_IndexParamAudioPcm:
            {
                OMX_AUDIO_PARAM_PCMMODETYPE *pPcmMode;
                pPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pPcmMode, OMX_AUDIO_PARAM_PCMMODETYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
				fsl_osal_memcpy(&PcmMode, pPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
			}
			AudioFilterSetParameterPCM();
			break;
        case OMX_IndexParamDecoderPlayMode:
            {
                OMX_DECODE_MODE* pMode=(OMX_DECODE_MODE*)pComponentParameterStructure;
                ePlayMode=*pMode;
            }
            break;
        case OMX_IndexParamStandardComponentRole:
            {
            OMX_PARAM_COMPONENTROLETYPE *roleParams;
            roleParams = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
            SetRoleFormat((OMX_STRING)roleParams->cRole);
            //if (fsl_osal_strncmp((const char*)roleParams->cRole, role[0], OMX_MAX_STRINGNAME_SIZE - 1))
                //return OMX_ErrorUndefined;
            break;
            }
        case OMX_IndexParamAudioOutputConvert:
            {
                OMX_PARAM_AUDIO_OUTPUT_CONVERT *pParams = (OMX_PARAM_AUDIO_OUTPUT_CONVERT *)pComponentParameterStructure;
                bConvertEnable = pParams->bEnable;
                printf("AudioFilter SetParameter OMX_IndexParamAudioOutputConvert");
                break;
            }
        case OMX_IndexParamAudioSendFirstPortSettingChanged:
            {
                OMX_PARAM_AUDIO_SEND_FIRST_PORT_SETTING_CHANGED *pParams = (OMX_PARAM_AUDIO_SEND_FIRST_PORT_SETTING_CHANGED *)pComponentParameterStructure;
                bSendFirstPortSettingChanged = pParams->bEnable;
                printf("AudioFilter SetParameter OMX_IndexParamAudioSendFirstPortSettingChanged %d",bSendFirstPortSettingChanged);
                break;
            }
		default:
			ret = AudioFilterSetParameter(nParamIndex, pComponentParameterStructure);
			break;
	}

    return ret;
}

OMX_ERRORTYPE AudioFilter::AudioFilterSetParameterPCM()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    return ret;
}

OMX_ERRORTYPE AudioFilter::InstanceInit()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

	ret = AudioFilterInstanceInit();
	if (ret != OMX_ErrorNone)
	{
		LOG_ERROR("Audio decoder instance init fail.\n");
		return ret;
	}

	RINGBUFFER_ERRORTYPE BufferRet = RINGBUFFER_SUCCESS;
	BufferRet = AudioRingBuffer.BufferCreate(nPushModeInputLen, nRingBufferScale);
	if (BufferRet != RINGBUFFER_SUCCESS)
	{
		LOG_ERROR("Create ring buffer fail.\n");
		return OMX_ErrorInsufficientResources;
	}
	LOG_DEBUG("Ring buffer push mode input len: %d\n", nPushModeInputLen);

	AUDIO_TS_MANAGER_ERRORTYPE Ret = AUDIO_TS_MANAGER_SUCCESS;
	Ret = TS_Manager.Create();
	if (Ret != AUDIO_TS_MANAGER_SUCCESS)
	{
		LOG_ERROR("Create audio ts manager fail.\n");
		return OMX_ErrorInsufficientResources;
	}

	bReceivedEOS = OMX_FALSE;
	bFirstFrame = OMX_FALSE;
	bCodecInit = OMX_FALSE;
	bInstanceReset = OMX_FALSE;
	bDecoderEOS = OMX_FALSE;
	bDecoderInitFail = OMX_FALSE;
    bFirstInBuffer = OMX_TRUE;
    bFirstOutput = OMX_TRUE;
    nRequiredSize = 0;
    bStartTime = OMX_TRUE;
    nOutputBitPerSample = 16;
    pConvertBuffer = NULL;
    if(bConvertEnable)
        pConvertBuffer = (OMX_U8*)FSL_MALLOC(200000);//200k is large enough

	return ret;
}

OMX_ERRORTYPE AudioFilter::InstanceDeInit()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

	ret = AudioFilterInstanceDeInit();
	if (ret != OMX_ErrorNone)
	{
		LOG_ERROR("Audio decoder instance de-init fail.\n");
		return ret;
	}

	LOG_DEBUG("Audio decoder instance de-init.\n");
	AudioRingBuffer.BufferFree();
	TS_Manager.Free();

    FSL_FREE(pConvertBuffer);
    return ret;
}

OMX_ERRORTYPE AudioFilter::AudioFilterCheckCodecConfig()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	pInBufferHdr->nFilledLen = 0;
	return ret;
}

OMX_ERRORTYPE AudioFilter::PassParamToOutputWithoutDecode()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    return ret;
}


OMX_ERRORTYPE AudioFilter::ProcessDataBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 ringBufferLen;
    LOG_LOG("Audio filter In: %d, Out: %d\n", ports[AUDIO_FILTER_INPUT_PORT]->BufferNum(), ports[AUDIO_FILTER_OUTPUT_PORT]->BufferNum());

    if(((ports[AUDIO_FILTER_INPUT_PORT]->BufferNum() == 0 && pInBufferHdr == NULL )
				|| pInBufferHdr != NULL) /**< Ring buffer full */
            && (ports[AUDIO_FILTER_OUTPUT_PORT]->BufferNum() == 0  && pOutBufferHdr == NULL))
        return OMX_ErrorNoMore;
	if(pOutBufferHdr == NULL && ports[AUDIO_FILTER_OUTPUT_PORT]->BufferNum() > 0) {
		ports[AUDIO_FILTER_OUTPUT_PORT]->GetBuffer(&pOutBufferHdr);
		pOutBufferHdr->nFlags = 0;
	}

    if(pInBufferHdr == NULL && ports[AUDIO_FILTER_INPUT_PORT]->BufferNum() > 0) {
		ports[AUDIO_FILTER_INPUT_PORT]->GetBuffer(&pInBufferHdr);
		if(pInBufferHdr != NULL) {
			ret = ProcessInputBufferFlag();
			if (ret != OMX_ErrorNone)
			{
				LOG_ERROR("Process input buffer flag fail.\n");
				return ret;
			}
		}
	}

	if(pInBufferHdr != NULL) {
		ret = ProcessInputDataBuffer();
		if (ret != OMX_ErrorNone)
		{
			LOG_ERROR("Process input data buffer fail.\n");
			return ret;
		}
	}

	if (bInstanceReset == OMX_TRUE)
	{
		bInstanceReset = OMX_FALSE;
		ret = AudioFilterInstanceReset();
		if (ret != OMX_ErrorNone)
		{
			LOG_ERROR("Audio filter instance reset fail.\n");
			bDecoderEOS = OMX_TRUE;
			SendEvent(OMX_EventError, ret, 0, NULL);
			return ret;
		}
	}

	if (bFirstFrame == OMX_TRUE)
	{
		ret = AudioFilterCheckFrameHeader();
		if (ret != OMX_ErrorNone)
		{
			LOG_ERROR("AudioFilterCheckFrameHeader fail.\n");
		}

	}

    //only 2 frames in audio ringbuffer for DEC_STREAM_MODE
    if(ePlayMode == DEC_FILE_MODE){
        ringBufferLen = nPushModeInputLen;
    }else if(nRequiredSize > 0){
        ringBufferLen = nRequiredSize;
    }else{
        ringBufferLen = nRequiredSize = AudioRingBuffer.AudioDataLen()+DEFAULT_REQUIRED_SIZE;
    }

    LOG_LOG("Audio Filter Ringbuffer data len: %d,required=%d\n", AudioRingBuffer.AudioDataLen(),nRequiredSize);
    if ((AudioRingBuffer.AudioDataLen() < ringBufferLen && bReceivedEOS == OMX_FALSE)
        || bDecoderEOS == OMX_TRUE)
    {
        LOG_DEBUG("Input buffer is not enough for filter.\n");
        if(ports[AUDIO_FILTER_INPUT_PORT]->BufferNum() > 0)
			return OMX_ErrorNone;
		else
			return OMX_ErrorNoMore;
	}

	if (bCodecInit == OMX_FALSE)
	{
		bCodecInit = OMX_TRUE;
		ret = AudioFilterCodecInit();
		if (ret != OMX_ErrorNone)
		{
			LOG_ERROR("Audio decoder codec init fail.\n");
			bDecoderInitFail = OMX_TRUE;
		}
	}

	if(pOutBufferHdr != NULL)
	{
		ret = ProcessOutputDataBuffer();
		if (ret != OMX_ErrorNone)
			LOG_ERROR("Process Output data buffer fail.\n");
	}

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AudioFilter::AudioFilterInstanceReset()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	return ret;
}

OMX_ERRORTYPE AudioFilter::ProcessInputBufferFlag()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
        {
            LOG_DEBUG("received audio codec config data.\n");
            AudioFilterCheckCodecConfig();
            return OMX_ErrorNone;
        }

	if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTIME)
	{
		AudioRingBuffer.BufferReset();
		TS_Manager.Reset();
		AudioFilterReset();
		bReceivedEOS = OMX_FALSE;
		bDecoderEOS = OMX_FALSE;
        bFirstFrame = OMX_TRUE;
        LOG_DEBUG("Audio Filter received STARTTIME.\n");
	}

	if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
	{
		bReceivedEOS = OMX_TRUE;
		LOG_DEBUG("Audio Filter %s received EOS.\n", role[0]);
	}

    if (OMX_TRUE == bFirstInBuffer && (pInBufferHdr->nFlags & OMX_BUFFERFLAG_EOS) && (pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTIME))
    {
        if (OMX_ErrorNone == PassParamToOutputWithoutDecode())
            SendEvent(OMX_EventPortSettingsChanged, AUDIO_FILTER_OUTPUT_PORT, 0, NULL);
    }

    if (bFirstInBuffer == OMX_TRUE)
        bFirstInBuffer = OMX_FALSE;

	LOG_DEBUG_INS("FilledLen = %d, TimeStamp = %lld\n", \
			pInBufferHdr->nFilledLen, pInBufferHdr->nTimeStamp);
    TS_Manager.TS_Add(pInBufferHdr->nTimeStamp, pInBufferHdr->nFilledLen - \
            pInBufferHdr->nOffset);

	return ret;
}


OMX_ERRORTYPE AudioFilter::ProcessInputDataBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
	OMX_U32 nActuralLen;

    if(ePlayMode == DEC_STREAM_MODE && nRequiredSize > 0 && AudioRingBuffer.AudioDataLen() > nRequiredSize)
        return ret;

	/** Process audio data */
	AudioRingBuffer.BufferAdd(pInBufferHdr->pBuffer + pInBufferHdr->nOffset,
                pInBufferHdr->nFilledLen,
				&nActuralLen);

	if (nActuralLen < pInBufferHdr->nFilledLen)
	{
        pInBufferHdr->nOffset += nActuralLen;
        pInBufferHdr->nFilledLen -= nActuralLen;
	}
	else
	{
        pInBufferHdr->nOffset = 0;
        pInBufferHdr->nFilledLen = 0;
        ports[AUDIO_FILTER_INPUT_PORT]->SendBuffer(pInBufferHdr);
        pInBufferHdr = NULL;
	}

    return ret;
}

OMX_ERRORTYPE AudioFilter::AudioFilterCheckFrameHeader()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    return ret;
}


OMX_ERRORTYPE AudioFilter::ProcessOutputDataBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (bFirstFrame == OMX_TRUE) {
        TS_Manager.Consumered(0);
		TS_Manager.TS_SetIncrease(0);
    }
	TS_Manager.TS_Get(&pOutBufferHdr->nTimeStamp);

	AUDIO_FILTERRETURNTYPE DecodeRet;

	if (bDecoderInitFail == OMX_TRUE)
	{
		AudioRingBuffer.BufferReset();
		TS_Manager.Reset();
		bReceivedEOS = OMX_TRUE;
		DecodeRet = AUDIO_FILTER_EOS;
	}
	else
	{
		DecodeRet = AudioFilterFrame();
		if (DecodeRet == AUDIO_FILTER_FAILURE)
		{
			LOG_WARNING("Decode frame fail.\n");
                     fsl_osal_sleep(4000);
                     return OMX_ErrorNone;
		}else if(DecodeRet == AUDIO_FILTER_FATAL_ERROR){
            AudioRingBuffer.BufferReset();
            TS_Manager.Reset();
            bReceivedEOS = OMX_TRUE;
            DecodeRet = AUDIO_FILTER_EOS;
        }
	}


	if (DecodeRet == AUDIO_FILTER_EOS && bReceivedEOS == OMX_TRUE && AudioRingBuffer.AudioDataLen() == 0)
	{
	    //if buffer is both the first and the last one, and is empty, send out EOS event directly.
	    if((bFirstOutput && pOutBufferHdr->nFilledLen == 0) || OMX_ErrorNone == AudioFilterHandleEOS()) {
		    LOG_DEBUG("Audio Filter %s send EOS, len %d\n", role[0], pOutBufferHdr->nFilledLen);
		    pOutBufferHdr->nFlags |= OMX_BUFFERFLAG_EOS;
		    bDecoderEOS = OMX_TRUE;
        }
	}

	if (bFirstFrame == OMX_TRUE)
	{
		bFirstFrame = OMX_FALSE;
		pOutBufferHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME;
	}
	pOutBufferHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
	pOutBufferHdr->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

    if(DecodeRet == AUDIO_FILTER_NEEDMORE){
        return OMX_ErrorNone;
    }

    if(bFirstOutput) {
        if(OMX_ErrorNone == AudioFilterHandleBOS())
            bFirstOutput = OMX_FALSE;
    }

    if(bConvertEnable && pConvertBuffer != NULL){
        OMX_U32 outSize = 0;
        if(PcmMode.nBitPerSample != 16){
            nOutputBitPerSample = PcmMode.nBitPerSample;
            PcmMode.nBitPerSample = 16;
        }
        if(nOutputBitPerSample != 16 && OMX_ErrorNone == ConvertData(pConvertBuffer,&outSize,pOutBufferHdr->pBuffer,pOutBufferHdr->nFilledLen)){
            //output buffer size will reduce after call the convert function.
            fsl_osal_memcpy(pOutBufferHdr->pBuffer,pConvertBuffer,outSize);
            pOutBufferHdr->nFilledLen = outSize;
        }
    }

    ports[AUDIO_FILTER_OUTPUT_PORT]->SendBuffer(pOutBufferHdr);
    pOutBufferHdr = NULL;

    return OMX_ErrorNone;
}
OMX_ERRORTYPE AudioFilter::ConvertData(OMX_U8* pOut, OMX_U32 *nOutSize, OMX_U8 *pIn, OMX_U32 nInSize)
{
    if(NULL == pOut || NULL == pIn)
        return OMX_ErrorUndefined;

    if(nOutputBitPerSample == 8) {
        // Convert to U16
        OMX_S32 i,Len;
        OMX_U16 *pDst =(OMX_U16 *)pOut;
        OMX_S8 *pSrc =(OMX_S8 *)pIn;
        Len = nInSize;
        for(i=0;i<Len;i++) {
            *pDst++ = (OMX_U16)(*pSrc++) << 8;
        }
        nInSize *= 2;
    } else if(nOutputBitPerSample == 24) {
        OMX_S32 i,j,Len;
        OMX_U8 *pDst =(OMX_U8 *)pOut;
        OMX_U8 *pSrc =(OMX_U8 *)pIn;
        Len = nInSize / (nOutputBitPerSample>>3) / PcmMode.nChannels;
        for(i=0;i<Len;i++) {
            for(j=0;j<(OMX_S32)PcmMode.nChannels;j++) {
                pDst[0] = pSrc[1];
                pDst[1] = pSrc[2];
                pDst+=2;
                pSrc+=3;
            }
        }
        nInSize = Len * (16>>3) * PcmMode.nChannels;
    }

    *nOutSize = nInSize;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE AudioFilter::ComponentReturnBuffer(
        OMX_U32 nPortIndex)
{
	bDecoderEOS = OMX_FALSE;
	if(nPortIndex == AUDIO_FILTER_INPUT_PORT && pInBufferHdr != NULL) {
        ports[AUDIO_FILTER_INPUT_PORT]->SendBuffer(pInBufferHdr);
        pInBufferHdr = NULL;
    }

    if(nPortIndex == AUDIO_FILTER_OUTPUT_PORT && pOutBufferHdr != NULL) {
        ports[AUDIO_FILTER_OUTPUT_PORT]->SendBuffer(pOutBufferHdr);
        pOutBufferHdr = NULL;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AudioFilter::FlushComponent(
        OMX_U32 nPortIndex)
{
    if(nPortIndex == AUDIO_FILTER_INPUT_PORT && pInBufferHdr != NULL) {
        ports[AUDIO_FILTER_INPUT_PORT]->SendBuffer(pInBufferHdr);
        pInBufferHdr = NULL;
    }

    if(nPortIndex == AUDIO_FILTER_OUTPUT_PORT && pOutBufferHdr != NULL) {
        ports[AUDIO_FILTER_OUTPUT_PORT]->SendBuffer(pOutBufferHdr);
        pOutBufferHdr = NULL;
    }

	bReceivedEOS = OMX_FALSE;
    bDecoderEOS = OMX_FALSE;
    bFirstOutput = OMX_TRUE;
    bFirstFrame = OMX_TRUE;
	AudioRingBuffer.BufferReset();
	TS_Manager.Reset();
    AudioFilterReset();
	LOG_DEBUG("Clear ring buffer.\n");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE  AudioFilter::SetRoleFormat(OMX_STRING role)
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE AudioFilter::AudioFilterHandleBOS()
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE AudioFilter::AudioFilterHandleEOS()
{
    return OMX_ErrorNone;
}


/* File EOF */
