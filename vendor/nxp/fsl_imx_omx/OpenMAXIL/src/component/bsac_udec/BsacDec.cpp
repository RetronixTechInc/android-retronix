/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */

#include <fcntl.h>
#include "BsacDec.h"
//#undef LOG_DEBUG
//#define LOG_DEBUG printf

#define BSACD_FRAME_SIZE  1152
#define BSAC_PUSH_MODE_LEN   (2048*4)
#define DSP_WRAPPER_LIB_NAME "lib_dsp_wrap_arm12_android.so"

BsacDec::BsacDec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_decoder.bsac.hw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.bsac";
    codingType = OMX_AUDIO_CodingBSAC;
    nPushModeInputLen = BSAC_PUSH_MODE_LEN;
    outputPortBufferSize = BSACD_FRAME_SIZE* 2*2;

    OMX_INIT_STRUCT(&BsacType, OMX_AUDIO_PARAM_BSACTYPE);
    BsacType.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    BsacType.nChannels = 2;
    BsacType.nSampleRate = 44100;
    LOG_DEBUG("Unia -> BSAC");
}
OMX_ERRORTYPE BsacDec::AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioBsac:
            {
                OMX_AUDIO_PARAM_BSACTYPE *pBsacType;
                pBsacType = (OMX_AUDIO_PARAM_BSACTYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pBsacType, OMX_AUDIO_PARAM_BSACTYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pBsacType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(pBsacType, &BsacType, sizeof(OMX_AUDIO_PARAM_BSACTYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE BsacDec::AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioBsac:
            {
                OMX_AUDIO_PARAM_BSACTYPE *pBsacType;
                pBsacType = (OMX_AUDIO_PARAM_BSACTYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pBsacType, OMX_AUDIO_PARAM_BSACTYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pBsacType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&BsacType, pBsacType, sizeof(OMX_AUDIO_PARAM_BSACTYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE BsacDec::UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(index){
        case UNIA_SAMPLERATE:
            BsacType.nSampleRate = value;
            break;
        case UNIA_CHANNEL:
            BsacType.nChannels = value;
            break;
        case UNIA_BITRATE:
            BsacType.nBitRate = value;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE BsacDec::UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(value == NULL){
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    switch(index){
        case UNIA_SAMPLERATE:
            *value = BsacType.nSampleRate;
            break;
        case UNIA_CHANNEL:
            *value = BsacType.nChannels;
            break;
        case UNIA_BITRATE:
            *value = BsacType.nBitRate;
            break;
        case UNIA_FRAMED:
            *value = OMX_TRUE;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}

OMX_ERRORTYPE BsacDec::UniaDecoderGetDecoderProp(AUDIOFORMAT *formatType, OMX_BOOL *isHwBased)
{
    if (formatType)
        *formatType = BSAC;
    if (isHwBased)
        *isHwBased = (fsl_osal_strcmp(decoderLibName, DSP_WRAPPER_LIB_NAME) == 0 ? OMX_TRUE : OMX_FALSE);
    return OMX_ErrorNone;
}


/**< C style functions to expose entry point for the shared library */
extern "C" {

    OMX_ERRORTYPE BsacHwDecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        BsacDec *obj = NULL;
        ComponentBase *base = NULL;
        int fd = -1;

        fd = open("/vendor/firmware/imx/dsp/hifi4.bin", O_RDONLY, 0);
        if (fd < 0) {
            LOG_ERROR("BsacHwDec check hardware support fail\n");
            return OMX_ErrorHardware;
        }
        else
            close(fd);

        obj = FSL_NEW(BsacDec, ());
        if(obj == NULL)
            return OMX_ErrorInsufficientResources;

        obj->SetDecLibName(DSP_WRAPPER_LIB_NAME);
        base = (ComponentBase*)obj;
        ret = base->ConstructComponent(pHandle);
        if(ret != OMX_ErrorNone)
            return ret;

        return ret;
    }

}

/* File EOF */
