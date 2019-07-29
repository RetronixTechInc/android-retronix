/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductors Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef AudioFrameParser_h
#define AudioFrameParser_h

#include "fsl_osal.h"

#ifdef __cplusplus
extern "C" {
#endif

//audio frame parser return value
#define AFP_RETURN fsl_osal_s32
#define AFP_SUCCESS 0
#define AFP_FAILED  1

//audio frame information
typedef struct AUDIO_FRAME_INFO {
    efsl_osal_bool bGotOneFrame;//true when get one frame and next frame's header
    fsl_osal_u32 nHeaderCount;
    fsl_osal_u32 nConsumedOffset;//frame header offset if get one frame
    fsl_osal_u32 nFrameSize;//frame size
    fsl_osal_u32 nBitRate;
    fsl_osal_u32 nSamplesPerFrame;
    fsl_osal_u32 nSamplingRate;
    fsl_osal_u32 nChannels;
    fsl_osal_u32 nNextFrameSize;//frame size
    fsl_osal_u32 nHeaderSize;
} AUDIO_FRAME_INFO;

typedef struct FRAME_INFO
{
    fsl_osal_u32 frm_size;
    fsl_osal_u32 b_rate;
    fsl_osal_u32 sampling_rate;
    fsl_osal_u32 sample_per_fr;
    fsl_osal_u32 layer;
    fsl_osal_u32 version;
    fsl_osal_u32 channels;
    fsl_osal_u32 header_size;
}FRAME_INFO;

typedef efsl_osal_bool (*IsValidHeader)(fsl_osal_u8 * pHeader,FRAME_INFO * Info);

AFP_RETURN CheckFrame(AUDIO_FRAME_INFO *pFrameInfo, fsl_osal_u8 *pBuffer, fsl_osal_u32 nBufferLen,fsl_osal_u32 nHeadSize,IsValidHeader fpIsValidHeader);

#ifdef __cplusplus
}
#endif

#endif//AudioFrameParser_h
