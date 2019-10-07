/**
 *  Copyright (c) 2009-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "VorbisDec.h"
//#undef LOG_DEBUG
//#define LOG_DEBUG printf

#ifndef OMX_STEREO_OUTPUT
#define VORBIS_MAX_CHANNELS 6
/* pcm channel mask for vorbis*/
static uint32 vorbisd_1channel_layout[] = {
    /* FC */
    UA_CHANNEL_FRONT_CENTER
};
static uint32 vorbisd_2channel_layout[] = {
    /* FL,FR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT
};
static uint32 vorbisd_3channel_layout[] = {
    /* FL,FC,FR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER
};
static uint32 vorbisd_4channel_layout[] = {
    /* FL,FC,FR,BC */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 vorbisd_5channel_layout[] = {
    /* FL,FR,FC,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 vorbisd_6channel_layout[] = {
    /* FL,FR,FC,,LFE BL,BR*/
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_LFE,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 * vorbisd_channel_layouts[] = {
    NULL,
    vorbisd_1channel_layout, // 1
    vorbisd_2channel_layout, // 2
    vorbisd_3channel_layout,
    vorbisd_4channel_layout,
    vorbisd_5channel_layout,
    vorbisd_6channel_layout
};
#endif
VorbisDec::VorbisDec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_decoder.vorbis.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.vorbis";
    codingType = OMX_AUDIO_CodingVORBIS;
    outputPortBufferSize = (8192*2*2);
    decoderLibName = "lib_vorbisd_wrap_arm11_elinux_android.so";
    OMX_INIT_STRUCT(&VorbisType, OMX_AUDIO_PARAM_VORBISTYPE);
    VorbisType.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    VorbisType.nChannels = 2;
    VorbisType.nBitRate = 0;
    VorbisType.nMinBitRate = 0;
    VorbisType.nMaxBitRate = 0;
    VorbisType.nSampleRate = 0;
    VorbisType.nAudioBandWidth = 0;
    VorbisType.nQuality = 3;
    VorbisType.bManaged = OMX_FALSE;
    VorbisType.bDownmix = OMX_FALSE;
    LOG_DEBUG("Unia -> VORBIS");
}
OMX_ERRORTYPE VorbisDec::AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch (nParamIndex) {
        case OMX_IndexParamAudioVorbis:
            {
                OMX_AUDIO_PARAM_VORBISTYPE *pVorbisType;
                pVorbisType = (OMX_AUDIO_PARAM_VORBISTYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pVorbisType, OMX_AUDIO_PARAM_VORBISTYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pVorbisType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(pVorbisType, &VorbisType, sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE VorbisDec::AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch (nParamIndex) {
        case OMX_IndexParamAudioVorbis:
            {
                OMX_AUDIO_PARAM_VORBISTYPE *pVorbisType;
                pVorbisType = (OMX_AUDIO_PARAM_VORBISTYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pVorbisType, OMX_AUDIO_PARAM_VORBISTYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pVorbisType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&VorbisType, pVorbisType, sizeof(OMX_AUDIO_PARAM_VORBISTYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE VorbisDec::UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(index){
        case UNIA_SAMPLERATE:
            VorbisType.nSampleRate = value;
            break;
        case UNIA_CHANNEL:
            VorbisType.nChannels = value;
            break;
        case UNIA_BITRATE:
            VorbisType.nBitRate = value;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE VorbisDec::UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(value == NULL){
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    switch(index){
        case UNIA_SAMPLERATE:
            *value = VorbisType.nSampleRate;
            break;
        case UNIA_CHANNEL:
            *value = VorbisType.nChannels;
            break;
        case UNIA_BITRATE:
            *value = VorbisType.nBitRate;
            break;
        case UNIA_FRAMED:
            *value = OMX_TRUE;
            break;
        #ifndef OMX_STEREO_OUTPUT
        case UNIA_CHAN_MAP_TABLE:
            CHAN_TABLE table;
            fsl_osal_memset(&table,0,sizeof(table));
            table.size = VORBIS_MAX_CHANNELS;
            fsl_osal_memcpy(&table.channel_table,vorbisd_channel_layouts,sizeof(vorbisd_channel_layouts));
            fsl_osal_memcpy(value,&table,sizeof(CHAN_TABLE));
            break;
        #endif
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE VorbisDecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        VorbisDec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(VorbisDec, ());
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
