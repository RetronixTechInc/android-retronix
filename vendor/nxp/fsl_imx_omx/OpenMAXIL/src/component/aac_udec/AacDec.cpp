/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */
#include <fcntl.h>

#include "AacDec.h"
#include "AacFrameParser.h"

#define AACD_FRAME_SIZE  1024
#define AAC_PUSH_MODE_LEN   (6*768+7)//max frame lenth + another frame header size
#define ADIF_FILE 0x41444946

#define DSP_WRAPPER_LIB_NAME "lib_dsp_wrap_arm12_android.so"

//#undef LOG_DEBUG
//#define LOG_DEBUG printf

#ifndef OMX_STEREO_OUTPUT
#define AAC_MAX_CHANNELS 8
/* pcm channgel layout for AAC*/
static uint32 aacd_1channel_layout[] = {
    /* FC */
    UA_CHANNEL_FRONT_CENTER
};

static uint32 aacd_2channel_layout[] = {
    /* FL,FR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT
};

static uint32 aacd_3channel_layout[] = {
    /* FL,FR,FC */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER
};

static uint32 aacd_4channel_layout[] = {
    /* FC,FCL,FCR,BC */
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_FRONT_LEFT_CENTER,
    UA_CHANNEL_FRONT_RIGHT_CENTER,
    UA_CHANNEL_REAR_CENTER
};

static uint32 aacd_5channel_layout[] = {
    /* FL,FR,FC,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT
};

static uint32 aacd_6channel_layout[] = {
    /* FL,FR,FC,LFE,BL,BR */
    UA_CHANNEL_FRONT_LEFT,
    UA_CHANNEL_FRONT_RIGHT,
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_LFE,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT,
};

static uint32 aacd_8channel_layout[] = {
    /* FC,LFE,,BL,BR,FCL,FCR,SL,SR */
    UA_CHANNEL_FRONT_CENTER,
    UA_CHANNEL_LFE,
    UA_CHANNEL_REAR_LEFT,
    UA_CHANNEL_REAR_RIGHT,
    UA_CHANNEL_FRONT_LEFT_CENTER,
    UA_CHANNEL_FRONT_RIGHT_CENTER,
    UA_CHANNEL_SIDE_LEFT,
    UA_CHANNEL_SIDE_RIGHT
};

static uint32 * aacd_channel_layouts[] = {
    NULL,
    aacd_1channel_layout, // 1
    aacd_2channel_layout, // 2
    aacd_3channel_layout,
    aacd_4channel_layout,
    aacd_5channel_layout,
    aacd_6channel_layout,
    NULL,
    aacd_8channel_layout,
};
#endif

AacDec::AacDec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_decoder.aac.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.aac";
    codingType = OMX_AUDIO_CodingAAC;
    nPushModeInputLen = AAC_PUSH_MODE_LEN;
    outputPortBufferSize = 6*AACD_FRAME_SIZE*2*4;
    decoderLibName = "lib_aacd_wrap_arm12_elinux_android.so";
    optionaDecoderlLibName = "lib_aacplusd_wrap_arm12_elinux_android.so";

    OMX_INIT_STRUCT(&AacType, OMX_AUDIO_PARAM_AACPROFILETYPE);
    AacType.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    AacType.nChannels = 2;
    AacType.nSampleRate = 44100;
    AacType.nAudioBandWidth = 0;
    AacType.nAACERtools = OMX_AUDIO_AACERNone;
    AacType.eAACProfile = OMX_AUDIO_AACObjectLC;
    AacType.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    AacType.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMax;
    LOG_DEBUG("Unia -> AAC");
    bFrameCheck = OMX_FALSE;
}
OMX_ERRORTYPE AacDec::AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch (nParamIndex) {
        case OMX_IndexParamAudioAac:
            {
                OMX_AUDIO_PARAM_AACPROFILETYPE *pAacType;
                pAacType = (OMX_AUDIO_PARAM_AACPROFILETYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pAacType, OMX_AUDIO_PARAM_AACPROFILETYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pAacType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(pAacType, &AacType, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE AacDec::AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch (nParamIndex) {
        case OMX_IndexParamAudioAac:
            {
                OMX_AUDIO_PARAM_AACPROFILETYPE *pAacType;
                pAacType = (OMX_AUDIO_PARAM_AACPROFILETYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pAacType, OMX_AUDIO_PARAM_AACPROFILETYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pAacType->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&AacType, pAacType, sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE AacDec::UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(index){
        case UNIA_SAMPLERATE:
            AacType.nSampleRate = value;
            break;
        case UNIA_CHANNEL:
            AacType.nChannels = value;
            break;
        case UNIA_BITRATE:
            AacType.nBitRate = value;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE AacDec::UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(value == NULL){
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    switch(index){
        case UNIA_SAMPLERATE:
            *value = AacType.nSampleRate;
            break;
        case UNIA_CHANNEL:
            *value = AacType.nChannels;
            break;
        case UNIA_BITRATE:
            *value = AacType.nBitRate;
            break;
        case UNIA_STREAM_TYPE:
            if(AacType.eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP2ADTS){
                *value = STREAM_ADTS;
            }else if(AacType.eAACStreamFormat == OMX_AUDIO_AACStreamFormatADIF){
                *value = STREAM_ADIF;
            }else if(AacType.eAACStreamFormat == OMX_AUDIO_AACStreamFormatRAW ||
                     AacType.eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4FF){
                *value = STREAM_RAW;
                frameInput = OMX_TRUE;
            }else{
                *value = STREAM_UNKNOW;
            }
            LOG_DEBUG("Get aac format = %d",AacType.eAACStreamFormat);
            break;
        case UNIA_FRAMED:
            *value = OMX_TRUE;
            break;
        #ifndef OMX_STEREO_OUTPUT
        case UNIA_CHAN_MAP_TABLE:
            CHAN_TABLE table;
            fsl_osal_memset(&table,0,sizeof(table));
            table.size = AAC_MAX_CHANNELS;
            fsl_osal_memcpy(&table.channel_table,aacd_channel_layouts,sizeof(aacd_channel_layouts));
            fsl_osal_memcpy(value,&table,sizeof(CHAN_TABLE));
            break;
        #endif
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
//add this function to check whether input data is raw data.
OMX_ERRORTYPE AacDec::AudioFilterCheckFrameHeader()
{
    OMX_U8 *pBuffer;
    OMX_U32 nActuralLen = 0;
    OMX_U32 nVal = 0;
    AUDIO_FRAME_INFO FrameInfo;
    fsl_osal_memset(&FrameInfo, 0, sizeof(AUDIO_FRAME_INFO));
    LOG_DEBUG("AacDec AudioFilterCheckFrameHeader");

    if(bFrameCheck){
        return OMX_ErrorNone;
    }

    do{
        AudioRingBuffer.BufferGet(&pBuffer, AAC_PUSH_MODE_LEN, &nActuralLen);
        LOG_LOG("Get stream length: %d\n", nActuralLen);

        if (nActuralLen < AAC_PUSH_MODE_LEN && ePlayMode == DEC_FILE_MODE)
            break;

        nVal = *pBuffer<<24|*(pBuffer+1)<<16|*(pBuffer+2)<<8|*(pBuffer+3);

        if(nVal == ADIF_FILE){
            LOG_DEBUG("ADIF_FILE");
            frameInput = OMX_FALSE;
            bFrameCheck = OMX_TRUE;
            AacType.eAACStreamFormat = OMX_AUDIO_AACStreamFormatADIF;
            break;
        }

        if(AFP_SUCCESS != AacCheckFrame(&FrameInfo, pBuffer, nActuralLen)){
            LOG_DEBUG("CHECK FAILED");
            break;
        }

        if(FrameInfo.bGotOneFrame){
            LOG_DEBUG("ADTS_FILE");
            AacType.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP2ADTS;
            frameInput = OMX_FALSE;
            bFrameCheck = OMX_TRUE;
            break;
        }

        AacType.eAACStreamFormat = OMX_AUDIO_AACStreamFormatRAW;
        frameInput = OMX_TRUE;
        bFrameCheck = OMX_TRUE;
        LOG_DEBUG("FrameInfo.nHeaderCount");
    }while(0);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE AacDec::UniaDecoderParseFrame(OMX_U8* pBuffer,OMX_U32 len,UniaDecFrameInfo *info)
{
    OMX_U32 nActuralLen = 0;
    AUDIO_FRAME_INFO FrameInfo;

    if(pBuffer == NULL || info == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_memset(&FrameInfo, 0, sizeof(AUDIO_FRAME_INFO));

    if(OMX_AUDIO_AACStreamFormatRAW != AacType.eAACStreamFormat){
        if(AFP_SUCCESS == AacCheckFrame(&FrameInfo, pBuffer, len)){
            info->bGotOneFrame = FrameInfo.bGotOneFrame;
            info->nConsumedOffset = FrameInfo.nConsumedOffset;
            info->nHeaderCount = FrameInfo.nHeaderCount;
            info->nHeaderSize = FrameInfo.nHeaderSize;
            info->nFrameSize = FrameInfo.nFrameSize;
            info->nNextSize = FrameInfo.nNextFrameSize;
        }
    }else{
        info->bGotOneFrame = E_FSL_OSAL_TRUE;
        info->nFrameSize = len;
        info->nNextSize = len/2;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AacDec::UniaDecoderGetDecoderProp(AUDIOFORMAT *formatType, OMX_BOOL *isHwBased)
{
    if (formatType)
        *formatType = AAC;
    if (isHwBased)
        *isHwBased = (fsl_osal_strcmp(decoderLibName, DSP_WRAPPER_LIB_NAME) == 0 ? OMX_TRUE : OMX_FALSE);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE AacDec::AudioFilterHandleEOS()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 padding = 0;

    // pad the end of the stream with one buffer of which the value are all 2(avoid gap/overlap),
    // since that the actual last buffer isn't sent by aac decoder.
    padding = AACD_FRAME_SIZE * AacType.nChannels * sizeof(OMX_S16);
    fsl_osal_memset(pOutBufferHdr->pBuffer + pOutBufferHdr->nOffset + pOutBufferHdr->nFilledLen, 2, padding);
    pOutBufferHdr->nFilledLen += padding;

    return ret;
}

/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE AacDecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        AacDec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(AacDec, ());
        if(obj == NULL)
            return OMX_ErrorInsufficientResources;

        base = (ComponentBase*)obj;
        ret = base->ConstructComponent(pHandle);
        if(ret != OMX_ErrorNone)
            return ret;

        return ret;
    }

    OMX_ERRORTYPE AacHwDecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        AacDec *obj = NULL;
        ComponentBase *base = NULL;
        int fd = -1;

        fd = open("/vendor/firmware/imx/dsp/hifi4.bin", O_RDONLY, 0);
        if (fd < 0) {
            LOG_ERROR("AacHwDec check hardware support fail\n");
            return OMX_ErrorHardware;
        }
        else
            close(fd);

        obj = FSL_NEW(AacDec, ());
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
