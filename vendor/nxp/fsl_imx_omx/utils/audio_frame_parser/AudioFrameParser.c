/**
 *  Copyright (c) 2010-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "AudioFrameParser.h"
#include "Mem.h"
#include "Log.h"

AFP_RETURN CheckFrame(AUDIO_FRAME_INFO *pFrameInfo, fsl_osal_u8 *pBuffer, fsl_osal_u32 nBufferLen,fsl_osal_u32 nHeadSize,IsValidHeader fpIsValidHeader)
{
    AFP_RETURN ret = AFP_SUCCESS;

    fsl_osal_u32 nOffset = 0;
    fsl_osal_u32 nFrameOffset = 0;
    fsl_osal_u32 nFirstFrameSize = 0;
    fsl_osal_u32 nFirstHeaderSize = 0;
    fsl_osal_u32 nHeaderCount = 0;
    FRAME_INFO Info;
    fsl_osal_u8* pHeader = NULL;

    if(!pFrameInfo || !pBuffer){
        return AFP_FAILED;
    }

    fsl_osal_memset(pFrameInfo, 0, sizeof(AUDIO_FRAME_INFO));

    pFrameInfo->bGotOneFrame = E_FSL_OSAL_FALSE;
    pFrameInfo->nHeaderCount = 0;
    pFrameInfo->nConsumedOffset = 0;
    pFrameInfo->nFrameSize = 0;
    pFrameInfo->nBitRate = 0;
    pFrameInfo->nSamplesPerFrame = 0;

    fsl_osal_memset(&Info, 0x0, sizeof(FRAME_INFO));
    LOG_LOG("CheckFrame start nBufferLen=%d",nBufferLen);

    while(nOffset + nHeadSize <= nBufferLen){

        pHeader = pBuffer + nOffset;

        if(!fpIsValidHeader(pHeader,&Info)){
            nOffset++;
            continue;
        }
        //check the frame size
        if(0 == Info.frm_size){
            nOffset++;
            continue;
        }

        nHeaderCount ++;
        nFrameOffset = nOffset;
        nFirstFrameSize = Info.frm_size;
        nFirstHeaderSize = Info.header_size;

        LOG_LOG("CheckFrame nOffset=%d,frm_size=%d",nOffset,Info.frm_size);

        if(Info.frm_size + nOffset == nBufferLen){
            printf("CheckFrame frame size=%d,buffer len=%d",Info.frm_size,nBufferLen);
            pFrameInfo->bGotOneFrame = E_FSL_OSAL_TRUE;
            pFrameInfo->nNextFrameSize = 0;
            nHeaderCount ++;
            break;
        }else if(Info.frm_size + nOffset + nHeadSize < nBufferLen){
            nOffset += Info.frm_size;
        }else{
            nOffset++;
            continue;
        }

        pHeader = pBuffer + nOffset;

        if(fpIsValidHeader(pHeader,&Info)){
            pFrameInfo->bGotOneFrame = E_FSL_OSAL_TRUE;
            pFrameInfo->nNextFrameSize = Info.frm_size;
            nHeaderCount ++;
            break;
        }else{
            nHeaderCount --;
            nOffset = nFrameOffset+1;
            continue;
        }
    }

    if(nFirstFrameSize > nHeadSize){
        pFrameInfo->nFrameSize = nFirstFrameSize;
        pFrameInfo->nHeaderSize = nFirstHeaderSize;
    }else{
        pFrameInfo->nFrameSize = 0;
    }

    if(nHeaderCount > 0){
        pFrameInfo->nConsumedOffset = nFrameOffset;
    }else{
        pFrameInfo->nConsumedOffset = nBufferLen;
        pFrameInfo->nFrameSize = 0;
    }

    pFrameInfo->nBitRate = Info.b_rate;
    pFrameInfo->nSamplesPerFrame = Info.sample_per_fr;
    pFrameInfo->nSamplingRate = Info.sampling_rate;
    pFrameInfo->nChannels = Info.channels;
    pFrameInfo->nHeaderCount = nHeaderCount;
    LOG_DEBUG("CheckFrame parse one frame,bGotOneFrame=%d,nFrameCount=%d,nConsumedOffset=%d,nFrameSize=%d,\
samplerate=%d,bitrate=%d,channel=%d,samplePerFrame=%d",
        pFrameInfo->bGotOneFrame,
        pFrameInfo->nHeaderCount,
        pFrameInfo->nConsumedOffset,
        pFrameInfo->nFrameSize,
        pFrameInfo->nSamplingRate,
        pFrameInfo->nBitRate,
        pFrameInfo->nChannels,
        pFrameInfo->nSamplesPerFrame
    );

    return ret;
}
