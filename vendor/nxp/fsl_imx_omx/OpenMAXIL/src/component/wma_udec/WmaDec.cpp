/**
 *  Copyright (c) 2009-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "WmaDec.h"
#include "fsl_media_types.h"
//#undef LOG_DEBUG
//#define LOG_DEBUG printf
#define WMA_OUTPUT_PORT_SIZE 200000

#ifndef OMX_STEREO_OUTPUT
#define WMA_MAX_CHANNELS 8
/* pcm channel mask for wma*/
static uint32 wma10d_1channel_layout[] = {
    /* FC */
    UA_CHANNEL_FRONT_CENTER
};
static uint32 wma10d_2channel_layout[] = {
    /* FL,FR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
};
static uint32 wma10d_3channel_layout[] = {
    /* FL,FR,FC */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    };
static uint32 wma10d_4channel_layout[] = {
    /* FL,FR,BL,BR */
    UA_CHANNEL_FRONT_LEFT_CENTER,
    UA_CHANNEL_FRONT_RIGHT_CENTER,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 wma10d_5channel_layout[] = {
    /* FL,FR,FC,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
    };
static uint32 wma10d_6channel_layout[] = {
    /* FL,FR,FC,LFE,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_LFE,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};
static uint32 wma10d_7channel_layout[] = {
    /* FL,FR,FC,LFE,BL,BR,BC */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_LFE,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT,
    UA_CHANNEL_REAR_CENTER
};
static uint32 wma10d_8channel_layout[] = {
    /* FL,FR,FC,LFE,BL,BR,SL,SR  */
    UA_CHANNEL_FRONT_LEFT_CENTER,
    UA_CHANNEL_FRONT_RIGHT_CENTER,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_LFE,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT,
    UA_CHANNEL_SIDE_LEFT,
    UA_CHANNEL_SIDE_RIGHT
};
static uint32 * wma10d_channel_layouts[] = {
    NULL,
    wma10d_1channel_layout, // 1
    wma10d_2channel_layout, // 2
    wma10d_3channel_layout,
    wma10d_4channel_layout,
    wma10d_5channel_layout,
    wma10d_6channel_layout,
    wma10d_7channel_layout,
    wma10d_8channel_layout
};
#endif
WmaDec::WmaDec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_decoder.wma.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.wma";
    codingType = OMX_AUDIO_CodingWMA;
    outputPortBufferSize = WMA_OUTPUT_PORT_SIZE;
    decoderLibName = "lib_wma10d_wrap_arm12_elinux_android.so";

    OMX_INIT_STRUCT(&WmaType, OMX_AUDIO_PARAM_WMATYPE);
    OMX_INIT_STRUCT(&WmaTypeExt, OMX_AUDIO_PARAM_WMATYPE_EXT);

    WmaType.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    WmaType.nChannels = 2;
    WmaType.nSamplingRate = 44100;
    WmaType.nBitRate = 64000;
    WmaType.eFormat = OMX_AUDIO_WMAFormat9;

    WmaTypeExt.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    WmaTypeExt.nBitsPerSample = 16;

    LOG_DEBUG("Unia -> WMA");

}
OMX_ERRORTYPE WmaDec::AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioWma:
            {
                OMX_AUDIO_PARAM_WMATYPE *pWmaType;
                pWmaType = (OMX_AUDIO_PARAM_WMATYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pWmaType, OMX_AUDIO_PARAM_WMATYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pWmaType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(pWmaType, &WmaType, sizeof(OMX_AUDIO_PARAM_WMATYPE));
                break;
            }
    case OMX_IndexParamAudioWmaExt:
        {
            OMX_AUDIO_PARAM_WMATYPE_EXT *pWmaTypeExt;
            pWmaTypeExt = (OMX_AUDIO_PARAM_WMATYPE_EXT*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pWmaTypeExt, OMX_AUDIO_PARAM_WMATYPE_EXT, ret);
            if(ret != OMX_ErrorNone)
                break;
            if (pWmaTypeExt->nPortIndex != AUDIO_FILTER_INPUT_PORT)
            {
                ret = OMX_ErrorBadPortIndex;
                break;
            }
            fsl_osal_memcpy(pWmaTypeExt, &WmaTypeExt,sizeof(OMX_AUDIO_PARAM_WMATYPE_EXT));
            break;
        }

        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE WmaDec::AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexParamAudioWma:
            {
                OMX_AUDIO_PARAM_WMATYPE *pWmaType;
                pWmaType = (OMX_AUDIO_PARAM_WMATYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pWmaType, OMX_AUDIO_PARAM_WMATYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pWmaType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&WmaType, pWmaType, sizeof(OMX_AUDIO_PARAM_WMATYPE));
                break;
            }
        case OMX_IndexParamAudioWmaExt:
            {
                OMX_AUDIO_PARAM_WMATYPE_EXT *pWmaTypeExt;
                pWmaTypeExt = (OMX_AUDIO_PARAM_WMATYPE_EXT*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pWmaTypeExt, OMX_AUDIO_PARAM_WMATYPE_EXT, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pWmaTypeExt->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&WmaTypeExt, pWmaTypeExt, sizeof(OMX_AUDIO_PARAM_WMATYPE_EXT));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE WmaDec::UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(index){
        case UNIA_SAMPLERATE:
            WmaType.nSamplingRate = value;
            break;
        case UNIA_CHANNEL:
            WmaType.nChannels = value;
            break;
        case UNIA_BITRATE:
            WmaType.nBitRate = value;
            break;
        case UNIA_DEPTH:
            WmaTypeExt.nBitsPerSample = value;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE WmaDec::UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(value == NULL){
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    switch(index){
        case UNIA_SAMPLERATE:
            *value = WmaType.nSamplingRate;
            break;
        case UNIA_CHANNEL:
            *value = WmaType.nChannels;
            break;
        case UNIA_BITRATE:
            *value = WmaType.nBitRate;
            break;
        case UNIA_FRAMED:
            *value = OMX_TRUE;
            break;
        case UNIA_WMA_BlOCKALIGN:
            *value = WmaType.nBlockAlign;
            break;
        case UNIA_WMA_VERSION:
        {
            switch((int)WmaType.eFormat){
                case OMX_AUDIO_WMAFormat7:
                    *value = AUDIO_WMA1;
                    break;
                case OMX_AUDIO_WMAFormat8:
                    *value = AUDIO_WMA2;
                    break;
                case OMX_AUDIO_WMAFormat9:
                    *value = AUDIO_WMA3;
                    break;
                case OMX_AUDIO_WMAFormatLL:
                    *value = AUDIO_WMALL;
                    break;
                default:
                    LOG_ERROR("Unknown wma format!");
                    break;
            }
            break;
        }
        case UNIA_DEPTH:
            *value = WmaTypeExt.nBitsPerSample;
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
            table.size = WMA_MAX_CHANNELS;
            fsl_osal_memcpy(&table.channel_table,wma10d_channel_layouts,sizeof(wma10d_channel_layouts));
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
    OMX_ERRORTYPE WmaDecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        WmaDec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(WmaDec, ());
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
