/**
 *  Copyright (c) 2012-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef STREAMING_CACHE_H
#define STREAMING_CACHE_H

#include "fsl_osal.h"
#include "OMX_Core.h"
#include "avformat.h"
#include "StreamingProtocol.h"
#include "OMX_ContentPipe.h"

typedef enum SC_ERROR_TYPE{
 AVAILABLE,
 NOT_AVAILABLE,
 NETWORK_FAIL,
}SC_ERROR_TYPE;

class StreamingCache {
    public:
        StreamingCache();
        virtual ~StreamingCache();
        OMX_ERRORTYPE Init(StreamingProtocol * protocol, OMX_U32 nCacheSize, OMX_U32 earlierDataPercentage);
        OMX_ERRORTYPE DeInit();
        OMX_ERRORTYPE GetContentLength(OMX_U64 * length);
        OMX_U32 ReadAt(OMX_U64 nOffset, OMX_U32 nSize, OMX_PTR pBuffer);
        CP_CHECKBYTESRESULTTYPE AvailableAt(OMX_U64 nOffset, OMX_U32 * pSize);
        OMX_U32 AvailableBytes(OMX_U64 nOffset);
        OMX_ERRORTYPE GetStreamData();
        OMX_BOOL IsSeekable();

    friend OMX_BOOL GetStopRetryingCallback(OMX_PTR pHandle);
    friend void *CacheThreadFunction(void *arg);

    private:
        typedef struct{
            OMX_U64 nOffset;
            OMX_U32 nLength;
            OMX_PTR pData;
            OMX_U32 nHeaderLen;
        } CacheNode;

        OMX_U32 mBlockSize;
        OMX_U32 mBlockCnt;
        OMX_PTR mCacheBuffer;
        CacheNode *mCacheNodeRoot;
        OMX_U64 mContentLength;
        OMX_U64 mOffsetEnd;
        OMX_U64 mOffsetStart;
        OMX_U32 mStartNode;
        OMX_U32 mEndNode;
        OMX_U32 mPlayNode;
        OMX_U32 mEarlierDataMinBlockCnt;
        OMX_PTR pThread;
        OMX_BOOL bStopThread;
        OMX_BOOL bStreamEOS;
        OMX_BOOL bReset;
        fsl_osal_mutex mutex;
        StreamingProtocol * pProtocol;

        OMX_ERRORTYPE mProtocolError;
        OMX_BOOL bStopRetry;
        OMX_U64 mResetTargetOffset;
        OMX_BOOL bSeekable;

        OMX_ERRORTYPE ResetCache(OMX_U64 nOffset);
};

#endif
/* File EOF */

