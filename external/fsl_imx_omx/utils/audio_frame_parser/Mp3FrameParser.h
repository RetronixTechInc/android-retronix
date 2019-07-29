/**
 *  Copyright (c) 2009-2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductors Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef Mp3FrameParser_h
#define Mp3FrameParser_h

#include "AudioFrameParser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_FRAME_HEAD_SIZE  4

//check frame function
AFP_RETURN Mp3CheckFrame(AUDIO_FRAME_INFO *pFrameInfo, fsl_osal_u8 *pBuffer, fsl_osal_u32 nBufferLen);

#ifdef __cplusplus
}
#endif

#endif//AudioFrameParser_h
