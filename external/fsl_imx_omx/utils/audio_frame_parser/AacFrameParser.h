/**
 *  Copyright (c) 2009-2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductors Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef AacFrameParser_h
#define AacFrameParser_h

#include "AudioFrameParser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAC_FRAME_HEAD_SIZE  7

//check frame function
AFP_RETURN AacCheckFrame(AUDIO_FRAME_INFO *pFrameInfo, fsl_osal_u8 *pBuffer, fsl_osal_u32 nBufferLen);

#ifdef __cplusplus
}
#endif

#endif//AacFrameParser_h
