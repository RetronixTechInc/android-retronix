/**
 *  Copyright (c) 2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "Ac3Dec.h"
#include "Ac3FrameParser.h"
//#undef LOG_DEBUG
//#define LOG_DEBUG printf

#define AC3D_FRAME_SIZE  1536
#define AC3_PUSH_MODE_LEN (3840+8)//max frame size + another frame header size

#ifndef OMX_STEREO_OUTPUT
#define AC3_MAX_CHANNELS 6
/* pcm channel layout for ac3 */
static uint32 ac3d_1channel_layout[] = {
    /* FC */
    UA_CHANNEL_FRONT_CENTER
};
static uint32 ac3d_2channel_layout[] = {
    /* FL,FR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT
};
static uint32 ac3d_3channel_layout[] = {
    /* FL,FR, FC */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER
    };
static uint32 ac3d_4channel_layout[] = {
    /* FL,FR,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 ac3d_5channel_layout[] = {
    /* FL,FR,FC,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
    };
static uint32 ac3d_6channel_layout[] = {
    /* FL,FR,FC,LFE,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_LFE,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 * ac3d_channel_layouts[] = {
    NULL,
    ac3d_1channel_layout,// 1
    ac3d_2channel_layout,// 2
    ac3d_3channel_layout,
    ac3d_4channel_layout,
    ac3d_5channel_layout,
    ac3d_6channel_layout
};
#endif

Ac3Dec::Ac3Dec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_decoder.ac3.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.ac3";
    codingType = OMX_AUDIO_CodingAC3;
    nPushModeInputLen = AC3_PUSH_MODE_LEN;
    outputPortBufferSize = 256*6*6*sizeof(int32);
    decoderLibName = "lib_ac3d_wrap_arm11_elinux_android.so";
    OMX_INIT_STRUCT(&Ac3Type, OMX_AUDIO_PARAM_AC3TYPE);
    Ac3Type.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    Ac3Type.nChannels = 2;
    Ac3Type.nSampleRate = 44100;
    LOG_DEBUG("Unia -> AC3");
}
OMX_ERRORTYPE Ac3Dec::AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioAc3:
            {
                OMX_AUDIO_PARAM_AC3TYPE *pAc3Type;
                pAc3Type = (OMX_AUDIO_PARAM_AC3TYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pAc3Type, OMX_AUDIO_PARAM_AC3TYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pAc3Type->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(pAc3Type, &Ac3Type, sizeof(OMX_AUDIO_PARAM_AC3TYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE Ac3Dec::AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioAc3:
            {
                OMX_AUDIO_PARAM_AC3TYPE *pAc3Type;
                pAc3Type = (OMX_AUDIO_PARAM_AC3TYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pAc3Type, OMX_AUDIO_PARAM_AC3TYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pAc3Type->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&Ac3Type, pAc3Type, sizeof(OMX_AUDIO_PARAM_AC3TYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE Ac3Dec::UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(index){
        case UNIA_SAMPLERATE:
            Ac3Type.nSampleRate = value;
            break;
        case UNIA_CHANNEL:
            Ac3Type.nChannels = value;
            break;
        case UNIA_BITRATE:
            Ac3Type.nBitRate = value;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE Ac3Dec::UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(value == NULL){
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    switch(index){
        case UNIA_SAMPLERATE:
            *value = Ac3Type.nSampleRate;
            break;
        case UNIA_CHANNEL:
            *value = Ac3Type.nChannels;
            break;
        case UNIA_BITRATE:
            *value = Ac3Type.nBitRate;
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
            table.size = AC3_MAX_CHANNELS;
            fsl_osal_memcpy(&table.channel_table,ac3d_channel_layouts,sizeof(ac3d_channel_layouts));
            fsl_osal_memcpy(value,&table,sizeof(CHAN_TABLE));
            break;
        #endif
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE Ac3Dec::AudioFilterCheckFrameHeader()
{
    OMX_ERRORTYPE ret = OMX_ErrorUndefined;
    OMX_U8 *pBuffer;
    OMX_U8 *pHeader;
    OMX_U32 nActuralLen;
    OMX_U32 nOffset=0;
    OMX_BOOL bFounded = OMX_FALSE;
    AUDIO_FRAME_INFO FrameInfo;
    fsl_osal_memset(&FrameInfo, 0, sizeof(AUDIO_FRAME_INFO));
    LOG_DEBUG("Ac3Dec AudioFilterCheckFrameHeader");

    do{
        AudioRingBuffer.BufferGet(&pBuffer, AC3_PUSH_MODE_LEN, &nActuralLen);
        LOG_LOG("Get stream length: %d\n", nActuralLen);

        if (nActuralLen < AC3_PUSH_MODE_LEN){
            ret = OMX_ErrorNone;
            break;
        }

        if(AFP_SUCCESS != Ac3CheckFrame(&FrameInfo, pBuffer, nActuralLen)){
            break;
        }

        if(FrameInfo.bGotOneFrame){
            bFounded = OMX_TRUE;
        }

        nOffset = FrameInfo.nConsumedOffset;

        if(nOffset < nActuralLen)
            LOG_LOG("buffer=%02x%02x%02x%02x",pBuffer[nOffset],pBuffer[nOffset+1],pBuffer[nOffset+2],pBuffer[nOffset+1]);

        LOG_LOG("Ac3 decoder skip %d bytes.\n", nOffset);
        AudioRingBuffer.BufferConsumered(nOffset);
        TS_Manager.Consumered(nOffset);

        if (bFounded == OMX_TRUE){
            ret = OMX_ErrorNone;
            break;
        }

    }while(0);

    return ret;
}
/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE Ac3DecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        Ac3Dec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(Ac3Dec, ());
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
