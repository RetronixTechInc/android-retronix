/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017-2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */
#include <fcntl.h>

#include "Mp3Dec.h"
#include "Mp3FrameParser.h"
//#undef LOG_DEBUG
//#define LOG_DEBUG printf

#define MP3D_FRAME_SIZE  1152
#define MP3_PUSH_MODE_LEN   (2048*4)
#define MP3_DECODER_DELAY 529 //samples

#define DSP_WRAPPER_LIB_NAME "lib_dsp_wrap_arm12_android.so"

Mp3Dec::Mp3Dec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_decoder.mp3.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.mp3";
    codingType = OMX_AUDIO_CodingMP3;
    nPushModeInputLen = MP3_PUSH_MODE_LEN;
    outputPortBufferSize = MP3D_FRAME_SIZE* 2*2;

    decoderLibName = "lib_mp3d_wrap_arm12_elinux_android.so";
    OMX_INIT_STRUCT(&Mp3Type, OMX_AUDIO_PARAM_MP3TYPE);
    Mp3Type.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    Mp3Type.nChannels = 2;
    Mp3Type.nSampleRate = 44100;
    Mp3Type.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    Mp3Type.eFormat = OMX_AUDIO_MP3StreamFormatMP1Layer3;
    LOG_DEBUG("Unia -> MP3");
    delayLeft = 0;
}
OMX_ERRORTYPE Mp3Dec::AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch (nParamIndex) {
        case OMX_IndexParamAudioMp3:
            {
                OMX_AUDIO_PARAM_MP3TYPE *pMp3Type;
                pMp3Type = (OMX_AUDIO_PARAM_MP3TYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pMp3Type, OMX_AUDIO_PARAM_MP3TYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pMp3Type->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(pMp3Type, &Mp3Type, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE Mp3Dec::AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch (nParamIndex) {
        case OMX_IndexParamAudioMp3:
            {
                OMX_AUDIO_PARAM_MP3TYPE *pMp3Type;
                pMp3Type = (OMX_AUDIO_PARAM_MP3TYPE*)pComponentParameterStructure;
                OMX_CHECK_STRUCT(pMp3Type, OMX_AUDIO_PARAM_MP3TYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;
                if (pMp3Type->nPortIndex != AUDIO_FILTER_INPUT_PORT)
                {
                    ret = OMX_ErrorBadPortIndex;
                    break;
                }
                fsl_osal_memcpy(&Mp3Type, pMp3Type, sizeof(OMX_AUDIO_PARAM_MP3TYPE));
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE Mp3Dec::UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(index){
        case UNIA_SAMPLERATE:
            Mp3Type.nSampleRate = value;
            break;
        case UNIA_CHANNEL:
            Mp3Type.nChannels = value;
            break;
        case UNIA_BITRATE:
            Mp3Type.nBitRate = value;
            break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE Mp3Dec::UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(value == NULL){
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    switch(index){
        case UNIA_SAMPLERATE:
            *value = Mp3Type.nSampleRate;
            break;
        case UNIA_CHANNEL:
            *value = Mp3Type.nChannels;
            break;
        case UNIA_BITRATE:
            *value = Mp3Type.nBitRate;
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
OMX_ERRORTYPE Mp3Dec::AudioFilterCheckFrameHeader()
{
    OMX_ERRORTYPE ret = OMX_ErrorUndefined;
    OMX_U8 *pBuffer;
    OMX_U8 *pHeader;
    OMX_U32 nActuralLen;
    OMX_U32 nOffset=0;
    OMX_BOOL bFounded = OMX_FALSE;
    AUDIO_FRAME_INFO FrameInfo;
    fsl_osal_memset(&FrameInfo, 0, sizeof(AUDIO_FRAME_INFO));
    LOG_LOG("Mp3Dec AudioFilterCheckFrameHeader");

    do{
        AudioRingBuffer.BufferGet(&pBuffer, MP3_PUSH_MODE_LEN, &nActuralLen);
        LOG_LOG("Get stream length: %d\n", nActuralLen);

        if (nActuralLen < MP3_PUSH_MODE_LEN && ePlayMode == DEC_FILE_MODE){
            ret = OMX_ErrorNone;
            break;
        }

        if(AFP_SUCCESS != Mp3CheckFrame(&FrameInfo, pBuffer, nActuralLen)){
            break;
        }

        if(FrameInfo.bGotOneFrame){
            bFounded = OMX_TRUE;
        }

        nOffset = FrameInfo.nConsumedOffset;

        if(nOffset < nActuralLen)
            LOG_LOG("buffer=%02x%02x%02x%02x",pBuffer[nOffset],pBuffer[nOffset+1],pBuffer[nOffset+2],pBuffer[nOffset+1]);

        LOG_LOG("Mp3 decoder skip %d bytes.\n", nOffset);
        AudioRingBuffer.BufferConsumered(nOffset);
        TS_Manager.Consumered(nOffset);

        if (bFounded == OMX_TRUE){
            ret = OMX_ErrorNone;
            break;
        }

    }while (0);

    return ret;
}
OMX_ERRORTYPE Mp3Dec::UniaDecoderParseFrame(OMX_U8* pBuffer,OMX_U32 len,UniaDecFrameInfo *info)
{
    OMX_U32 nActuralLen = 0;
    AUDIO_FRAME_INFO FrameInfo;

    if(pBuffer == NULL || info == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_memset(&FrameInfo, 0, sizeof(AUDIO_FRAME_INFO));

    if(AFP_SUCCESS == Mp3CheckFrame(&FrameInfo, pBuffer, len)){
        info->bGotOneFrame = FrameInfo.bGotOneFrame;
        info->nConsumedOffset = FrameInfo.nConsumedOffset;
        info->nHeaderCount = FrameInfo.nHeaderCount;
        info->nHeaderSize = FrameInfo.nHeaderSize;
        info->nFrameSize = FrameInfo.nFrameSize;
        info->nNextSize = FrameInfo.nNextFrameSize;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Mp3Dec::UniaDecoderGetDecoderProp(AUDIOFORMAT *formatType, OMX_BOOL *isHwBased)
{
    if (formatType)
        *formatType = MP3;
    if (isHwBased)
        *isHwBased = (fsl_osal_strcmp(decoderLibName, DSP_WRAPPER_LIB_NAME) == 0 ? OMX_TRUE : OMX_FALSE);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE Mp3Dec::AudioFilterHandleBOS()
{
    if(delayLeft == 0)
        delayLeft = MP3_DECODER_DELAY * Mp3Type.nChannels * sizeof(OMX_S16);

    // The decoder delay is 529 samples, so trim them off from the start of the first output buffer.
    // the process of 529 samples is to align with google's code and to pass cts testDecodeMp3Lame, etc.
    if(pOutBufferHdr->nFilledLen > delayLeft){
        pOutBufferHdr->nOffset += delayLeft;
        pOutBufferHdr->nFilledLen -= delayLeft;
        delayLeft = 0;

    }else {
        delayLeft -= pOutBufferHdr->nFilledLen;
        pOutBufferHdr->nOffset = 0;
        pOutBufferHdr->nFilledLen = 0;
    }

    //meet BOS, tran state to STATE_TO_ADD_ONE_FRAME.
    if(delayLeft == 0)
        return OMX_ErrorNone;
    else
        return OMX_ErrorNotReady;
}

OMX_ERRORTYPE Mp3Dec::AudioFilterHandleEOS()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 padding = 0;

    // pad the end of the stream with 529 samples, since that many samples
    // were trimmed off the beginning when decoding started
    padding = MP3_DECODER_DELAY * Mp3Type.nChannels * sizeof(OMX_S16);
    fsl_osal_memset(pOutBufferHdr->pBuffer + pOutBufferHdr->nOffset + pOutBufferHdr->nFilledLen, 2, padding);
    pOutBufferHdr->nFilledLen += padding;

    return ret;
}


/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE Mp3DecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        Mp3Dec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(Mp3Dec, ());
        if(obj == NULL)
            return OMX_ErrorInsufficientResources;

        base = (ComponentBase*)obj;
        ret = base->ConstructComponent(pHandle);
        if(ret != OMX_ErrorNone)
            return ret;

        return ret;
    }

    OMX_ERRORTYPE Mp3HwDecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        Mp3Dec *obj = NULL;
        ComponentBase *base = NULL;
        int fd = -1;

        fd = open("/vendor/firmware/imx/dsp/hifi4.bin", O_RDONLY, 0);
        if (fd < 0) {
            LOG_ERROR("Mp3HwDec check hardware support fail\n");
            return OMX_ErrorHardware;
        }
        else
            close(fd);

        obj = FSL_NEW(Mp3Dec, ());
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
