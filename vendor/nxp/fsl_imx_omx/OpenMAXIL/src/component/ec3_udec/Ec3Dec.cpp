/**
 *  Copyright (c) 2013, Freescale Semiconductor Inc.,
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "Ec3Dec.h"

#undef LOG_DEBUG
#define LOG_DEBUG printf

#define EC3_PUSH_MODE_LEN (6144+8)//max frame size + another frame header size

#ifndef OMX_STEREO_OUTPUT
#define EC3_MAX_CHANNELS 8
/* pcm channel layout for eac3 */
static uint32 eac3d_1channel_layout[] = {
    /* FC */
    UA_CHANNEL_FRONT_CENTER
};
static uint32 eac3d_2channel_layout[] = {
    /* FL,FR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT
};
static uint32 eac3d_3channel_layout[] = {
    /* FL,FR, FC */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER
    };
static uint32 eac3d_4channel_layout[] = {
    /* FL,FR,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 eac3d_5channel_layout[] = {
    /* FL,FR,FC,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
    };
static uint32 eac3d_6channel_layout[] = {
    /* FL,FR,FC,LFE,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_LFE,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 eac3d_8channel_layout[] = {
/* FL, FR, FC, LFE, BL, BR, SL, SR */
  UA_CHANNEL_FRONT_LEFT,
  UA_CHANNEL_FRONT_RIGHT,
  UA_CHANNEL_FRONT_CENTER,
  UA_CHANNEL_LFE,
  UA_CHANNEL_REAR_LEFT,
  UA_CHANNEL_REAR_RIGHT,
  UA_CHANNEL_SIDE_LEFT,
  UA_CHANNEL_SIDE_RIGHT,
};
static uint32 * eac3d_channel_layouts[] = {
    NULL,
    eac3d_1channel_layout,// 1
    eac3d_2channel_layout,// 2
    eac3d_3channel_layout,
    eac3d_4channel_layout,
    eac3d_5channel_layout,
    eac3d_6channel_layout,
    NULL,
    eac3d_8channel_layout
};
#endif
Ec3Dec::Ec3Dec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_decoder.eac3.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.eac3";
    codingType = OMX_AUDIO_CodingEC3;
    nPushModeInputLen = EC3_PUSH_MODE_LEN;
    outputPortBufferSize = 36864;//8*6*256*24/8
    decoderLibName = "lib_ddpd_wrap_arm12_elinux_android.so";
    OMX_INIT_STRUCT(&Ec3Type, OMX_AUDIO_PARAM_EC3TYPE);
    Ec3Type.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    Ec3Type.nChannels = 2;
    Ec3Type.nSampleRate = 44100;
    LOG_DEBUG("Unia -> EC3");
}
OMX_ERRORTYPE Ec3Dec::AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioEc3:
            {
                OMX_AUDIO_PARAM_EC3TYPE *pEc3Type;
                pEc3Type = (OMX_AUDIO_PARAM_EC3TYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pEc3Type, OMX_AUDIO_PARAM_EC3TYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pEc3Type->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(pEc3Type, &Ec3Type, sizeof(OMX_AUDIO_PARAM_EC3TYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE Ec3Dec::AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioEc3:
            {
                OMX_AUDIO_PARAM_EC3TYPE *pEc3Type;
                pEc3Type = (OMX_AUDIO_PARAM_EC3TYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pEc3Type, OMX_AUDIO_PARAM_EC3TYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pEc3Type->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&Ec3Type, pEc3Type, sizeof(OMX_AUDIO_PARAM_EC3TYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE Ec3Dec::UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(index){
        case UNIA_SAMPLERATE:
            Ec3Type.nSampleRate = value;
            break;
        case UNIA_CHANNEL:
            Ec3Type.nChannels = value;
            break;
        case UNIA_BITRATE:
            Ec3Type.nBitRate = value;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE Ec3Dec::UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(value == NULL){
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    switch(index){
        case UNIA_SAMPLERATE:
            *value = Ec3Type.nSampleRate;
            break;
        case UNIA_CHANNEL:
            *value = Ec3Type.nChannels;
            break;
        case UNIA_BITRATE:
            *value = Ec3Type.nBitRate;
            break;
        case UNIA_FRAMED:
            *value = OMX_TRUE;
            break;
        #ifdef OMX_STEREO_OUTPUT
        case UNIA_DOWNMIX_STEREO:
            *value = OMX_TRUE;
            break;
        #endif
        #ifndef OMX_STEREO_OUTPUT
        case UNIA_CHAN_MAP_TABLE:
            CHAN_TABLE table;
            fsl_osal_memset(&table,0,sizeof(table));
            table.size = EC3_MAX_CHANNELS;
            fsl_osal_memcpy(&table.channel_table,eac3d_channel_layouts,sizeof(eac3d_channel_layouts));
            fsl_osal_memcpy(value,&table,sizeof(CHAN_TABLE));
            LOG_DEBUG("SET MAP TABLE");
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
    OMX_ERRORTYPE Ec3DecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        Ec3Dec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(Ec3Dec, ());
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


