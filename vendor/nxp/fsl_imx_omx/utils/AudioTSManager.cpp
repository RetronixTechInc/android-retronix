/**
 *  Copyright (c) 2012, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include <stdio.h>
#include <dlfcn.h>
#include "Mem.h"
#include "Log.h"
#include "AudioTSManager.h"

AudioTSManager::AudioTSManager()
{
	TS_Queue = NULL;
	CurrentTS = 0;
    PreTS = -1;
    bHaveTS = E_FSL_OSAL_FALSE;
	TotalConsumeLen = 0;
	TotalReceivedLen = 0;
    nOneByteTime = 0;
}

AUDIO_TS_MANAGER_ERRORTYPE AudioTSManager::Create()
{
    AUDIO_TS_MANAGER_ERRORTYPE ret = AUDIO_TS_MANAGER_SUCCESS;

	/** Create queue for TS. */
	TS_Queue = FSL_NEW(Queue, ());
	if (TS_Queue == NULL)
	{
		LOG_ERROR("Can't get memory.\n");
		return AUDIO_TS_MANAGER_INSUFFICIENT_RESOURCES;
	}

	if (TS_Queue->Create(TS_QUEUE_SIZE, sizeof(TS_QUEUE), E_FSL_OSAL_FALSE) != QUEUE_SUCCESS)
	{
		FSL_DELETE(TS_Queue);
		LOG_ERROR("Can't create audio ts queue.\n");
		return AUDIO_TS_MANAGER_INSUFFICIENT_RESOURCES;
	}

	CurrentTS = 0;
    PreTS = -1;
	bHaveTS = E_FSL_OSAL_FALSE;
	TotalConsumeLen = 0;
	TotalReceivedLen = 0;

    return ret;
}

AUDIO_TS_MANAGER_ERRORTYPE AudioTSManager::SetOneByteTime(fsl_osal_u32 OneByteTime)
{
	AUDIO_TS_MANAGER_ERRORTYPE ret = AUDIO_TS_MANAGER_SUCCESS;

    nOneByteTime = OneByteTime;

	return ret;
}

AUDIO_TS_MANAGER_ERRORTYPE AudioTSManager::Reset()
{
	AUDIO_TS_MANAGER_ERRORTYPE ret = AUDIO_TS_MANAGER_SUCCESS;

    if(TS_Queue){
    	while (TS_Queue->Size() > 0)
    	{
    		TS_QUEUE TS_Item;

    		if (TS_Queue->Get(&TS_Item) != QUEUE_SUCCESS)
    		{
    			LOG_ERROR("Can't get audio TS item.\n");
    			return AUDIO_TS_MANAGER_FAILURE;
    		}
    	}
    }

	CurrentTS = 0;
    PreTS = -1;
	bHaveTS = E_FSL_OSAL_FALSE;
	TotalConsumeLen = 0;
	TotalReceivedLen = 0;

	return ret;
}

AUDIO_TS_MANAGER_ERRORTYPE AudioTSManager::Free()
{
	AUDIO_TS_MANAGER_ERRORTYPE ret = AUDIO_TS_MANAGER_SUCCESS;

	LOG_DEBUG("TS queue free\n");
	if(TS_Queue != NULL)
		TS_Queue->Free();
	LOG_DEBUG("TS queue delete\n");
	FSL_DELETE(TS_Queue);

	return ret;
}

AUDIO_TS_MANAGER_ERRORTYPE AudioTSManager::TS_Add(fsl_osal_s64 ts, fsl_osal_u32 BufferLen)
{
	AUDIO_TS_MANAGER_ERRORTYPE ret = AUDIO_TS_MANAGER_SUCCESS;
	TS_QUEUE TS_Item;

    do {
        // Core parser EOS.
        // Core parser will output same audio time stamp if the audio data in
        // same trunk.
        if (ts < 0 \
                || (ts == 0 && BufferLen == 0) \
                || ts == PreTS)
            break;

        TS_Item.ts = ts;
        PreTS = ts;
        /** Should add TS first after received buffer from input port */
        TS_Item.AudioTSManagerBegin = TotalReceivedLen;
        LOG_LOG("TS: %lld\t AudioTSManagerBegin: %lld\n", TS_Item.ts, \
                TS_Item.AudioTSManagerBegin);

        if (TS_Queue->Add(&TS_Item) != QUEUE_SUCCESS) {
            LOG_ERROR("Can't add TS item to audio ts queue. Queue size: %d, max queue size: %d \n", TS_Queue->Size(), TS_QUEUE_SIZE);
            ret = AUDIO_TS_MANAGER_FAILURE;
            break;
        }
    }while (0);

	TotalReceivedLen += BufferLen;

	return ret;
}

AUDIO_TS_MANAGER_ERRORTYPE AudioTSManager::TS_Get(fsl_osal_s64 *ts)
{
	AUDIO_TS_MANAGER_ERRORTYPE ret = AUDIO_TS_MANAGER_SUCCESS;
	*ts = CurrentTS;
	LOG_DEBUG_INS("Get CurrentTS = %lld\n", CurrentTS);
	return ret;
}

AUDIO_TS_MANAGER_ERRORTYPE AudioTSManager::TS_SetIncrease(fsl_osal_s64 ts)
{
    AUDIO_TS_MANAGER_ERRORTYPE ret = AUDIO_TS_MANAGER_SUCCESS;
	if (bHaveTS == E_FSL_OSAL_FALSE) {
		CurrentTS += ts;
	}
	bHaveTS = E_FSL_OSAL_FALSE;
	LOG_DEBUG_INS("Set CurrentTS = %lld\n", CurrentTS);
	return ret;
}

fsl_osal_u32 AudioTSManager::GetFrameLen()
{
	TS_QUEUE TS_Item;
	fsl_osal_u32 nFrameLen = 0;

	if (TS_Queue->Access(&TS_Item, 1) != QUEUE_SUCCESS) {
		LOG_WARNING("Can't get audio TS item.\n");
		return TotalReceivedLen - TotalConsumeLen;
	}

	nFrameLen = TS_Item.AudioTSManagerBegin - TotalConsumeLen;
	LOG_LOG("TS_Item.RingBufferBegin: %lld\n", TS_Item.AudioTSManagerBegin);
	LOG_LOG("TotalConsumeLen: %lld\n", TotalConsumeLen);
	LOG_LOG("Frame Len: %d\n", nFrameLen);

	return nFrameLen;
}

AUDIO_TS_MANAGER_ERRORTYPE AudioTSManager::Consumered(fsl_osal_u32 ConsumeredLen)
{
    AUDIO_TS_MANAGER_ERRORTYPE ret = AUDIO_TS_MANAGER_SUCCESS;

	if (TotalReceivedLen < TotalConsumeLen + ConsumeredLen) {
		LOG_ERROR("audio ts manager consumer point set error.\n");
		return AUDIO_TS_MANAGER_FAILURE;
	}

	TotalConsumeLen += ConsumeredLen;

	if (TS_Queue->Size() < 1)
		return ret;

	/** Adjust current TS */
	TS_QUEUE TS_Item;

	while (1) {
		if (TS_Queue->Access(&TS_Item, 1) != QUEUE_SUCCESS) {
			LOG_DEBUG("Can't get audio TS item.\n");
			return AUDIO_TS_MANAGER_SUCCESS;
		}

        if (TotalConsumeLen >= TS_Item.AudioTSManagerBegin) {
            CurrentTS = TS_Item.ts;
            if (nOneByteTime) {
                CurrentTS += (TotalConsumeLen - TS_Item.AudioTSManagerBegin) \
                             * nOneByteTime;
            }
            bHaveTS = E_FSL_OSAL_TRUE;

            if (TS_Queue->Get(&TS_Item) != QUEUE_SUCCESS) {
                LOG_ERROR("Can't get audio TS item.\n");
                return AUDIO_TS_MANAGER_FAILURE;
            }
        }
		else
			break;
	}

	return ret;
}

/* File EOF */
