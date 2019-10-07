/**
 *  Copyright (c) 2009-2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file RingBuffer.h
 *  @brief Class definition of ring buffer
 *  @ingroup RingBuffer
 */


#ifndef RingBuffer_h
#define RingBuffer_h

#include "fsl_osal.h"
#include "Queue.h"

#define RING_BUFFER_SCALE 2

typedef enum {
    RINGBUFFER_SUCCESS,
    RINGBUFFER_FAILURE,
    RINGBUFFER_INSUFFICIENT_RESOURCES
}RINGBUFFER_ERRORTYPE;

class RingBuffer {
	public:
		RingBuffer();
		RINGBUFFER_ERRORTYPE BufferCreate(fsl_osal_u32 nPushModeLen, fsl_osal_u32 nRingBufferScale = RING_BUFFER_SCALE);
        RINGBUFFER_ERRORTYPE BufferFree();
		RINGBUFFER_ERRORTYPE BufferReset();
		RINGBUFFER_ERRORTYPE BufferAdd(fsl_osal_u8 *pBuffer, fsl_osal_u32 BufferLen, fsl_osal_u32 *pActualLen);
		RINGBUFFER_ERRORTYPE BufferAddZeros(fsl_osal_u32 BufferLen, fsl_osal_u32 *pActualLen);
		fsl_osal_u32 AudioDataLen();
		RINGBUFFER_ERRORTYPE BufferGet(fsl_osal_u8 **ppBuffer, fsl_osal_u32 BufferLen, fsl_osal_u32 *pActualLen);
		/** Set consumered point for ring buffer. */
		RINGBUFFER_ERRORTYPE BufferConsumered(fsl_osal_u32 ConsumeredLen);
		fsl_osal_u32 nPrevOffset;
	private:
		fsl_osal_u32 nPushModeInputLen;
		fsl_osal_s64 TotalConsumeLen;
        fsl_osal_u32 nOneByteTime;
		fsl_osal_u8 *RingBufferPtr;
		fsl_osal_u32 nRingBufferLen; /**< Should at least RING_BUFFER_SCALE * PUSH model input buffer length */
		fsl_osal_u8 *Reserved;
		fsl_osal_u32 ReservedLen;   /**< 1/RING_BUFFER_SCALE length of ring buffer length,
								 used for the end of ring buffer data */
		fsl_osal_u8 *Begin;
		fsl_osal_u8 *End;
		fsl_osal_u8 *Consumered;
};

#endif
/* File EOF */
