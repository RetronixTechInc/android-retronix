/**
 *  Copyright (c) 2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "IEC937Convert.h"

IEC937Convert::IEC937Convert()
{
    bInContext = OMX_FALSE;
    nPorts = AUDIO_FILTER_PORT_NUMBER;
    nInBufferSize = 0;
    nOutBufferSize = 0;
    eInCodingType = OMX_AUDIO_CodingUnused;
}

OMX_ERRORTYPE IEC937Convert::InitComponent()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    sPortDef.eDir = OMX_DirInput;
    sPortDef.eDomain = OMX_PortDomainAudio;
    sPortDef.format.audio.eEncoding = (OMX_AUDIO_CODINGTYPE)eInCodingType;
    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_TRUE;
    sPortDef.nBufferCountMin = 1;
    sPortDef.nBufferCountActual = 3;
    sPortDef.nBufferSize = nInBufferSize;
    ret = ports[AUDIO_FILTER_INPUT_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for port[%d] failed.\n", AUDIO_FILTER_INPUT_PORT);
        return ret;
    }

    sPortDef.nPortIndex = AUDIO_FILTER_OUTPUT_PORT;
    sPortDef.eDir = OMX_DirOutput;
    sPortDef.eDomain = OMX_PortDomainAudio;
    sPortDef.format.audio.eEncoding = OMX_AUDIO_CodingIEC937;
    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_TRUE;
    sPortDef.nBufferCountMin = 1;
    sPortDef.nBufferCountActual = 3;
    sPortDef.nBufferSize = nOutBufferSize;
    ret = ports[AUDIO_FILTER_OUTPUT_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for port[%d] failed.\n", 0);
        return ret;
    }

    nPushModeInputLen = nInBufferSize;
    nRingBufferScale = RING_BUFFER_SCALE;

    return ret;
}

OMX_ERRORTYPE IEC937Convert::DeInitComponent()
{
    return OMX_ErrorNone;
}

AUDIO_FILTERRETURNTYPE IEC937Convert::AudioFilterFrame()
{
    OMX_U8 *pBuffer = NULL;
    OMX_U32 nActuralLen = 0;

    pOutBufferHdr->nOffset = 0;
    pOutBufferHdr->nFilledLen = 0;
    AudioRingBuffer.BufferGet(&pBuffer, nPushModeInputLen, &nActuralLen);

    FRAME_INFO sFrameInfo = {0, 0};
    PARSE_FRAME_RET ParserFrameRet = ParseOneFrame(pBuffer, nActuralLen, &sFrameInfo);
    if(nActuralLen < nPushModeInputLen && ParserFrameRet != PARSE_FRAME_SUCCUSS) {
        AudioRingBuffer.BufferConsumered(nActuralLen);
        TS_Manager.Consumered(nActuralLen);
        LOG_DEBUG("IEC937Convert return EOS.\n");
        return AUDIO_FILTER_EOS;
    }
    if(ParserFrameRet == PARSE_FRAME_NOT_FOUND) {
        LOG_DEBUG("Not found one frame in %d data.\n", nActuralLen);
        AudioRingBuffer.BufferConsumered(nActuralLen);
        TS_Manager.Consumered(nActuralLen);
        return AUDIO_FILTER_FAILURE;
    }
    else if(ParserFrameRet == PARSE_FRAME_MORE_DATA) {
        LOG_DEBUG("Need more data to found one frame in %d data.\n", nActuralLen);
        if(sFrameInfo.nOffset > 0) {
            AudioRingBuffer.BufferConsumered(sFrameInfo.nOffset);
            TS_Manager.Consumered(sFrameInfo.nOffset);
        }
        return AUDIO_FILTER_FAILURE;
    }
    else {
        if(OMX_ErrorNone != IEC937Package(pBuffer + sFrameInfo.nOffset, sFrameInfo.nSize))
            return AUDIO_FILTER_FAILURE;
        AudioRingBuffer.BufferConsumered(sFrameInfo.nOffset + sFrameInfo.nSize);
        TS_Manager.TS_SetIncrease(GetFrameDuration());
    }

    return AUDIO_FILTER_SUCCESS;
}

#define IEC937_SYNC1            0xF872
#define IEC937_SYNC2            0x4E1F
#define IEC937_BURST_INFO_PAUSE 0x03

typedef struct {
    OMX_U16 pa;
    OMX_U16 pb;
    OMX_U16 pc;
    OMX_U16 pd;
} burst_struct;

OMX_ERRORTYPE IEC937Convert::IEC937Package(
        OMX_U8 *pBuffer,
        OMX_U32 nSize)
{
    if(nSize + 16 > nOutBufferSize) {
        LOG_ERROR("frame size is larger than a IEC937 burst size, burst size: %d, frame size: %d\n",
                nOutBufferSize, nSize);
        return OMX_ErrorInsufficientResources;
    }

    OMX_U8 *pOutBuffer = pOutBufferHdr->pBuffer;
    fsl_osal_memset(pOutBuffer, 0, pOutBufferHdr->nAllocLen);
    nSize = (nSize + 1) & (~1);
    burst_struct *burst = (burst_struct*)pOutBuffer;
    burst->pa = IEC937_SYNC1;
    burst->pb = IEC937_SYNC2;
    burst->pc = GetBurstType();
    burst->pd = nSize << 3;
    if(OMX_TRUE == NeedSwitchWord()) {
        OMX_U16 *pSrc = (OMX_U16*)pBuffer;
        OMX_U16 *pDest = (OMX_U16*)(pOutBuffer + 8);
        OMX_S32 count;
        for(count = 0; count < (OMX_S32)nSize/2; count++) {
            OMX_U16 word = pSrc[count];
            OMX_U16 byte1 = (word << 8) & 0xff00;
            OMX_U16 byte2 = (word >> 8) & 0x00ff;
            pDest[count] = byte1 | byte2;
        }
    }
    else
        fsl_osal_memcpy(pOutBuffer + 8, pBuffer, nSize);

    pOutBufferHdr->nFilledLen = nOutBufferSize;

    return OMX_ErrorNone;
}

/* File EOF */
