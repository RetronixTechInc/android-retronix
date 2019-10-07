/**
 *  Copyright (c) 2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "AC3ToIEC937.h"
#include "Ac3FrameParser.h"

#define IN_BUFFER_SIZE (5620)
#define FRAME_LEN (1536)
#define OUT_BUFFER_SIZE (FRAME_LEN*2*2)

AC3ToIEC937::AC3ToIEC937()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.audio_processor.ac3toiec937.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_processor.ac3toiec937";
    bInContext = OMX_FALSE;
    nInBufferSize = IN_BUFFER_SIZE;
    nOutBufferSize = OUT_BUFFER_SIZE;
    eInCodingType = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingAC3;
    OMX_INIT_STRUCT(&Ac3Type, OMX_AUDIO_PARAM_AC3TYPE);
    Ac3Type.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    Ac3Type.nChannels = 2;
    Ac3Type.nSampleRate = 48000;
    Ac3Type.nBitRate = 128*1024;
    bFirstFrame = OMX_TRUE;
    bSwitchWord = OMX_FALSE;

    OMX_INIT_STRUCT(&PcmMode, OMX_AUDIO_PARAM_PCMMODETYPE);
    PcmMode.nPortIndex = AUDIO_FILTER_OUTPUT_PORT;
    PcmMode.nChannels = 2;
    PcmMode.nSamplingRate = 48000;
    PcmMode.nBitPerSample = 16;
    PcmMode.bInterleaved = OMX_TRUE;
    PcmMode.eNumData = OMX_NumericalDataSigned;
    PcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
    PcmMode.eEndian = OMX_EndianLittle;
    PcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelNone;
}


OMX_ERRORTYPE AC3ToIEC937::AudioFilterGetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    if(nParamIndex == OMX_IndexParamAudioAc3)
        fsl_osal_memcpy(pStructure, &Ac3Type, sizeof(OMX_AUDIO_PARAM_AC3TYPE));
    else
        return OMX_ErrorUnsupportedIndex;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AC3ToIEC937::AudioFilterSetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    if(nParamIndex == OMX_IndexParamAudioAc3) {
        OMX_AUDIO_PARAM_AC3TYPE *pAc3Type;
        pAc3Type = (OMX_AUDIO_PARAM_AC3TYPE*)pStructure;
        Ac3Type.nBitRate = pAc3Type->nBitRate;
    }
    else
        return  OMX_ErrorUnsupportedIndex;

    return OMX_ErrorNone;
}

IEC937Convert::PARSE_FRAME_RET AC3ToIEC937::ParseOneFrame(
        OMX_U8 *pBuffer,
        OMX_U32 nSize,
        FRAME_INFO *pInfo)
{
    AUDIO_FRAME_INFO sFrameInfo;

    Ac3CheckFrame(&sFrameInfo, pBuffer, nSize);

    if(sFrameInfo.bGotOneFrame == E_FSL_OSAL_FALSE)
        return IEC937Convert::PARSE_FRAME_NOT_FOUND;
    else if(sFrameInfo.nConsumedOffset + sFrameInfo.nFrameSize > nSize) {
        pInfo->nOffset = sFrameInfo.nConsumedOffset;
        return IEC937Convert::PARSE_FRAME_MORE_DATA;
    }
    else {
        pInfo->nOffset = sFrameInfo.nConsumedOffset;
        pInfo->nSize = sFrameInfo.nFrameSize;

        if(pBuffer[pInfo->nOffset] == 0x0b && pBuffer[pInfo->nOffset + 1] == 0x77)
            bSwitchWord = OMX_TRUE;
        else
            bSwitchWord = OMX_FALSE;

        if(bFirstFrame) {
            Ac3Type.nSampleRate = sFrameInfo.nSamplingRate;
            Ac3Type.nBitRate = sFrameInfo.nBitRate;
            LOG_DEBUG("AC3 sample rate: %d, channels: %d\n", Ac3Type.nSampleRate, Ac3Type.nChannels);

            PcmMode.nSamplingRate = sFrameInfo.nSamplingRate;

            SendEvent(OMX_EventPortSettingsChanged, AUDIO_FILTER_OUTPUT_PORT, 0, NULL);
            bFirstFrame = OMX_FALSE;
        }
    }

    return IEC937Convert::PARSE_FRAME_SUCCUSS;
}

OMX_TICKS AC3ToIEC937::GetFrameDuration()
{
    return (OMX_TICKS)OMX_TICKS_PER_SECOND * (OMX_TICKS)FRAME_LEN / Ac3Type.nSampleRate;
}


/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE AC3ToIEC937Init(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        AC3ToIEC937 *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(AC3ToIEC937, ());
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
