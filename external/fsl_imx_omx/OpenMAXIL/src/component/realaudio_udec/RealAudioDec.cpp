/**
 *  Copyright (c) 2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "RealAudioDec.h"
#undef LOG_DEBUG
#define LOG_DEBUG printf

#define REAL_AUDIO_PUSH_MODE_LEN (2240/8)//max frame size

RealAudioDec::RealAudioDec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_decoder.ra.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.ra";
    codingType = OMX_AUDIO_CodingRA;
    nPushModeInputLen = REAL_AUDIO_PUSH_MODE_LEN;
    outputPortBufferSize = 4096;
    decoderLibName = "lib_realad_wrap_arm11_elinux_android.so";
    OMX_INIT_STRUCT(&RealAudioType, OMX_AUDIO_PARAM_RATYPE);
    RealAudioType.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    RealAudioType.nChannels = 2;
    LOG_DEBUG("Unia -> REAL");
}
OMX_ERRORTYPE RealAudioDec::AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioRa:
            {
                OMX_AUDIO_PARAM_RATYPE *pRealAudioType;
                pRealAudioType = (OMX_AUDIO_PARAM_RATYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pRealAudioType, OMX_AUDIO_PARAM_RATYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pRealAudioType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(pRealAudioType, &RealAudioType, sizeof(OMX_AUDIO_PARAM_RATYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE RealAudioDec::AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioRa:
            {
                OMX_AUDIO_PARAM_RATYPE *pRealAudioType;
                pRealAudioType = (OMX_AUDIO_PARAM_RATYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pRealAudioType, OMX_AUDIO_PARAM_RATYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pRealAudioType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&RealAudioType, pRealAudioType, sizeof(OMX_AUDIO_PARAM_RATYPE));
                if(RealAudioType.nChannels > 2)
                    ret = OMX_ErrorBadParameter;
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE RealAudioDec::UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(index){
        case UNIA_SAMPLERATE:
            RealAudioType.nSamplingRate = value;
            break;
        case UNIA_CHANNEL:
            RealAudioType.nChannels= value;
            break;
        case UNIA_RA_FRAME_BITS:
            RealAudioType.nBitsPerFrame = value;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE RealAudioDec::UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(value == NULL){
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    switch(index){
        case UNIA_SAMPLERATE:
            *value = RealAudioType.nSamplingRate;
            LOG_DEBUG("UNIA_SAMPLERATE %d",*value);
            break;
        case UNIA_CHANNEL:
            *value = RealAudioType.nChannels;
            LOG_DEBUG("UNIA_CHANNEL %d",*value);
            break;
        case UNIA_FRAMED:
            *value = OMX_FALSE;
            break;
        case UNIA_RA_FRAME_BITS:
            *value = RealAudioType.nBitsPerFrame;
            LOG_DEBUG("UNIA_RA_FRAME_BITS %d",*value);
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE RealAudioDec::UniaDecoderCheckParameter()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(RealAudioType.nChannels > 2)
        ret = OMX_ErrorBadParameter;

    return ret;
}

/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE RaDecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        RealAudioDec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(RealAudioDec, ());
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
