/**
 *  Copyright (c) 2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file AudioTSManager.h
 *  @brief Class definition of audio time stamp manager.
 *  @ingroup AudioTSManager
 */


#ifndef AudioTSManager_h
#define AudioTSManager_h

#include "fsl_osal.h"
#include "Queue.h"

#define TS_QUEUE_SIZE (1024*20)

typedef enum {
    AUDIO_TS_MANAGER_SUCCESS,
    AUDIO_TS_MANAGER_FAILURE,
    AUDIO_TS_MANAGER_INSUFFICIENT_RESOURCES
}AUDIO_TS_MANAGER_ERRORTYPE;

typedef struct _TS_QUEUE{
    fsl_osal_s64 ts;
	fsl_osal_s64 AudioTSManagerBegin;
}TS_QUEUE;

class AudioTSManager {
	public:
		AudioTSManager();
		AUDIO_TS_MANAGER_ERRORTYPE Create();
        AUDIO_TS_MANAGER_ERRORTYPE SetOneByteTime(fsl_osal_u32 OneByteTime);
        AUDIO_TS_MANAGER_ERRORTYPE Free();
		AUDIO_TS_MANAGER_ERRORTYPE Reset();
		AUDIO_TS_MANAGER_ERRORTYPE TS_Add(fsl_osal_s64 ts, fsl_osal_u32 BufferLen);
		/** Set TS increase after decode one of audio data */
		AUDIO_TS_MANAGER_ERRORTYPE TS_SetIncrease(fsl_osal_s64 ts);
		/** Get TS for output audio data of audio decoder. The output TS is calculated for
		 every frame of audio data */
		AUDIO_TS_MANAGER_ERRORTYPE TS_Get(fsl_osal_s64 *ts);
		fsl_osal_u32 GetFrameLen();
		/** Set consumered point for ring buffer. */
		AUDIO_TS_MANAGER_ERRORTYPE Consumered(fsl_osal_u32 ConsumeredLen);
	private:
		Queue *TS_Queue;
		fsl_osal_s64 CurrentTS;
		fsl_osal_s64 PreTS;
		efsl_osal_bool bHaveTS;
		fsl_osal_s64 TotalConsumeLen;
		fsl_osal_s64 TotalReceivedLen;
        fsl_osal_u32 nOneByteTime;
};

#endif
/* File EOF */
