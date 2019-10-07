/**
 *  Copyright (c) 2009-2012, Freescale Semiconductor Inc.,
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
#include "RingBuffer.h"

RingBuffer::RingBuffer()
{
	RingBufferPtr = NULL;
	Reserved = NULL;
	TotalConsumeLen = 0;
	nPrevOffset = 0;
	nPushModeInputLen = 0;
	nOneByteTime = 0;
	nRingBufferLen = 0;
	ReservedLen = 0;
	Begin = NULL;
	End = NULL;
	Consumered = NULL;
}

RINGBUFFER_ERRORTYPE RingBuffer::BufferCreate(fsl_osal_u32 nPushModeLen, fsl_osal_u32 nRingBufferScale)
{
    RINGBUFFER_ERRORTYPE ret = RINGBUFFER_SUCCESS;

	nPushModeInputLen = nPushModeLen;
	nRingBufferLen = nPushModeInputLen * nRingBufferScale;

	/** Create ring buffer for audio decoder input stream. */
	LOG_DEBUG("Ring buffer len: %d\n", nRingBufferLen);
	RingBufferPtr = (fsl_osal_u8 *)FSL_MALLOC(nRingBufferLen+8);
	if (RingBufferPtr == NULL)
	{
		LOG_ERROR("Can't get memory.\n");
		return RINGBUFFER_INSUFFICIENT_RESOURCES;
	}

	Reserved = (fsl_osal_u8 *)FSL_MALLOC(nPushModeInputLen);
	if (Reserved == NULL)
	{
		FSL_FREE(RingBufferPtr);
		LOG_ERROR("Can't get memory.\n");
		return RINGBUFFER_INSUFFICIENT_RESOURCES;
	}

	TotalConsumeLen = 0;
	ReservedLen = nPushModeInputLen;
	Begin = RingBufferPtr;
	End = RingBufferPtr;
	Consumered = RingBufferPtr;
	nPrevOffset = 0;

    return ret;
}

RINGBUFFER_ERRORTYPE RingBuffer::BufferReset()
{
	RINGBUFFER_ERRORTYPE ret = RINGBUFFER_SUCCESS;

	TotalConsumeLen = 0;
	Begin = RingBufferPtr;
	End = RingBufferPtr;
	Consumered = RingBufferPtr;
	nPrevOffset = 0;

	return ret;
}

RINGBUFFER_ERRORTYPE RingBuffer::BufferFree()
{
	RINGBUFFER_ERRORTYPE ret = RINGBUFFER_SUCCESS;

	LOG_DEBUG("Free ring buffer\n");
	FSL_FREE(RingBufferPtr);
	LOG_DEBUG("Free reserved buffer\n");
	FSL_FREE(Reserved);
	LOG_DEBUG("Ring buffer free finished.\n");

	return ret;
}

RINGBUFFER_ERRORTYPE RingBuffer::BufferAdd(fsl_osal_u8 *pBuffer, fsl_osal_u32 BufferLen, fsl_osal_u32 *pActualLen)
{
    RINGBUFFER_ERRORTYPE ret = RINGBUFFER_SUCCESS;

	fsl_osal_s32 DataLen = AudioDataLen();
	fsl_osal_s32 FreeBufferLen = nRingBufferLen - DataLen - 1;
	if (FreeBufferLen < (fsl_osal_s32)BufferLen)
	{
		*pActualLen = FreeBufferLen;
	}
	else
	{
		*pActualLen = BufferLen;
	}

	if (Begin + *pActualLen > RingBufferPtr + nRingBufferLen)
	{
		fsl_osal_u32 FirstSegmentLen = nRingBufferLen - (Begin - RingBufferPtr);
		fsl_osal_memcpy(Begin, pBuffer, FirstSegmentLen);
		fsl_osal_memcpy(RingBufferPtr, pBuffer + FirstSegmentLen, *pActualLen - FirstSegmentLen);
		Begin = RingBufferPtr + *pActualLen - FirstSegmentLen;
	}
	else
	{
		fsl_osal_memcpy(Begin, pBuffer, *pActualLen);
		Begin += *pActualLen;
	}

	LOG_LOG("nRingBufferLen = %d\t DataLen = %d\n", nRingBufferLen, DataLen);

    return ret;
}

RINGBUFFER_ERRORTYPE RingBuffer::BufferAddZeros(fsl_osal_u32 BufferLen, fsl_osal_u32 *pActualLen)
{
    RINGBUFFER_ERRORTYPE ret = RINGBUFFER_SUCCESS;

	fsl_osal_u32 DataLen = AudioDataLen();
	fsl_osal_u32 FreeBufferLen = nRingBufferLen - DataLen - 1;
	if (FreeBufferLen < BufferLen)
	{
		*pActualLen = FreeBufferLen;
	}
	else
	{
		*pActualLen = BufferLen;
	}

	if (Begin + *pActualLen > RingBufferPtr + nRingBufferLen)
	{
		fsl_osal_u32 FirstSegmentLen = nRingBufferLen - (Begin - RingBufferPtr);
		fsl_osal_memset(Begin, 0, FirstSegmentLen);
		fsl_osal_memset(RingBufferPtr, 0, *pActualLen - FirstSegmentLen);
		Begin = RingBufferPtr + *pActualLen - FirstSegmentLen;
	}
	else
	{
		fsl_osal_memset(Begin, 0, *pActualLen);
		Begin += *pActualLen;
	}

	LOG_LOG("nRingBufferLen = %d\t DataLen = %d\n", nRingBufferLen, DataLen);

    return ret;
}


fsl_osal_u32 RingBuffer::AudioDataLen()
{
	fsl_osal_s32 DataLen = Begin - End;
	if (DataLen < 0)
	{
		DataLen = nRingBufferLen - (End - Begin);
	}

    return (fsl_osal_u32)DataLen;
}

RINGBUFFER_ERRORTYPE RingBuffer::BufferGet(fsl_osal_u8 **ppBuffer, fsl_osal_u32 BufferLen, fsl_osal_u32 *pActualLen)
{
    RINGBUFFER_ERRORTYPE ret = RINGBUFFER_SUCCESS;
	fsl_osal_s32 DataLen = AudioDataLen();
	if (DataLen < (fsl_osal_s32)BufferLen)
	{
		*pActualLen = DataLen;
	}
	else
	{
		*pActualLen = BufferLen;
	}
	if (ReservedLen < *pActualLen)
	{
		LOG_WARNING("Reserved buffer is too short.\n");
		*pActualLen = ReservedLen;
	}

	if (End + *pActualLen > RingBufferPtr + nRingBufferLen)
	{
		fsl_osal_u32 FirstSegmentLen = nRingBufferLen - (End - RingBufferPtr);
		fsl_osal_memcpy(Reserved, End, FirstSegmentLen);
		fsl_osal_memcpy(Reserved + FirstSegmentLen, RingBufferPtr, *pActualLen - FirstSegmentLen);
		*ppBuffer = Reserved;
	}
	else
	{
		*ppBuffer = End;
	}

	LOG_LOG("nRingBufferLen = %d\t DataLen = %d\n", nRingBufferLen, DataLen);
    return ret;
}

RINGBUFFER_ERRORTYPE RingBuffer::BufferConsumered(fsl_osal_u32 ConsumeredLen)
{
    RINGBUFFER_ERRORTYPE ret = RINGBUFFER_SUCCESS;
	fsl_osal_s32 DataLen = AudioDataLen();
	if (DataLen < (fsl_osal_s32)ConsumeredLen)
	{
		LOG_ERROR("Ring buffer consumer point set error.\n");
		return RINGBUFFER_FAILURE;
	}

	if (End + ConsumeredLen > RingBufferPtr + nRingBufferLen)
	{
		End += ConsumeredLen - nRingBufferLen;
	}
	else
	{
		End += ConsumeredLen;
	}

	TotalConsumeLen += ConsumeredLen;
	LOG_LOG("nRingBufferLen = %d\t DataLen = %d\n", nRingBufferLen, DataLen);
	return ret;
}

/* File EOF */
