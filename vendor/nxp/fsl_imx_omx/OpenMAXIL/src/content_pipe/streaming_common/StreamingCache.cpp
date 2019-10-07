/**
 *  Copyright (c) 2012-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "StreamingCache.h"
#include "Mem.h"
#include "Log.h"
#include "OMX_Implement.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

#if 0
#undef LOG_INFO
#define LOG_INFO printf
#endif

void *CacheThreadFunction(void *arg)
{
    StreamingCache *h = (StreamingCache *) arg;
    OMX_U64 target = 0;
    LOG_INFO("StreamingCache thread entry\n");

    LOG_INFO("StreamingCache thread call protocol Init\n");
    OMX_ERRORTYPE ret = h->pProtocol->Init();
    if (ret != OMX_ErrorNone) {
        LOG_ERROR("StreamingCache thread protocol open fail %x.\n", ret);
        h->mProtocolError = ret;
        return NULL;
    }

    if(h->bStopThread == OMX_TRUE)
        goto bail;

    LOG_INFO("StreamingCache thread call protocol open");
    ret = h->pProtocol->Open();
    if(ret != OMX_ErrorNone){
        LOG_ERROR("StreamingCache thread protocol open fail %x", ret);
        h->mProtocolError = ret;
        goto bail;
    }

    if(h->bStopThread == OMX_TRUE)
        goto bail;

    h->mContentLength = h->pProtocol->GetContentLength();
    LOG_INFO("StreamingCache thread GetContentLength %lld", h->mContentLength);

    target = (h->mContentLength > 0) ? h->mContentLength/2 : 5*1024;
    ret = h->pProtocol->Seek(target, SEEK_SET, OMX_FALSE);
    if(ret == OMX_ErrorServerUnseekable)
        h->bSeekable = OMX_FALSE;

    printf("server seekable: %d", h->bSeekable);

    if(h->bStopThread == OMX_TRUE)
        goto bail;

    h->pProtocol->Seek(0, SEEK_SET, OMX_FALSE);

    if(h->bStopThread == OMX_TRUE)
        goto bail;

    while(OMX_ErrorNone == h->GetStreamData());

bail:

    h->pProtocol->Close();

    h->pProtocol->DeInit();

    LOG_INFO("StreamingCache thread exit\n");

    return NULL;
}

OMX_BOOL GetStopRetryingCallback(OMX_PTR handle)
{
    StreamingCache * pCache = (StreamingCache *)handle;

    if(pCache->bStopThread == OMX_TRUE || pCache->bReset == OMX_TRUE)
        return OMX_TRUE;

    return OMX_FALSE;
}

StreamingCache::StreamingCache()
{
    LOG_INFO("StreamingCache Constructor");

    mBlockSize = 2*1024;
    mBlockCnt = 0;
    mCacheBuffer = NULL;
    mCacheNodeRoot = NULL;
    mContentLength = mOffsetStart = mOffsetEnd = 0;
    mStartNode = mEndNode = mPlayNode = 0;
    pThread = NULL;
    bStopThread = OMX_FALSE;
    bStreamEOS = OMX_FALSE;
    bReset = OMX_FALSE;
    pProtocol = NULL;
    mutex = NULL;
    mProtocolError = OMX_ErrorNone;
    bStopRetry = OMX_FALSE;
    mEarlierDataMinBlockCnt = 0;
    mResetTargetOffset = 0;
    bSeekable = OMX_TRUE;
}

StreamingCache::~StreamingCache()
{
    LOG_INFO("StreamingCache Destructor");
}

OMX_ERRORTYPE StreamingCache::Init(StreamingProtocol * protocol, OMX_U32 nCacheSize, OMX_U32 earlierDataPercentage)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_S32 i;
    CacheNode *pNode = NULL;

    LOG_INFO("StreamingCache::Init() size %d\n", nCacheSize);

    if(protocol == NULL)
        return OMX_ErrorUndefined;

    if(nCacheSize < 1024 * 1024)
        return OMX_ErrorBadParameter;

    pProtocol = protocol;
    pProtocol->SetCallback(GetStopRetryingCallback, (OMX_PTR)this);

    mBlockCnt = nCacheSize/mBlockSize;
    mCacheBuffer = FSL_MALLOC(mBlockSize * mBlockCnt);
    if(mCacheBuffer == NULL) {
        LOG_ERROR("fail to allocate cache buffer, size %d\n", mBlockSize * mBlockCnt);
        ret = OMX_ErrorInsufficientResources;
        goto err;
    }

    mEarlierDataMinBlockCnt = mBlockCnt * earlierDataPercentage / 100;

    mCacheNodeRoot = (CacheNode*) FSL_MALLOC(mBlockCnt * sizeof(CacheNode));
    if(mCacheNodeRoot == NULL) {
        LOG_ERROR("StreamingCache::Init() fail to allocate memory for StreamingCache node.\n");
        ret = OMX_ErrorInsufficientResources;
        goto err;
    }

    pNode = mCacheNodeRoot;
    for(i=0; i<(OMX_S32)mBlockCnt; i++) {
        pNode->nOffset = 0;
        pNode->nLength = 0;
        pNode->pData = (OMX_PTR)((unsigned long)mCacheBuffer + i * mBlockSize);
        pNode->nHeaderLen = 0;
        pNode ++;
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&mutex, fsl_osal_mutex_default)) {
        LOG_ERROR("Failed create mutex for StreamingCache.\n");
        ret = OMX_ErrorInsufficientResources;
        goto err;
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_thread_create(&pThread, NULL, CacheThreadFunction, this)) {
        LOG_ERROR("Failed create Thread for StreamingCache.\n");
        ret = OMX_ErrorInsufficientResources;
        goto err;
    }

    LOG_INFO("StreamingCache::Init() ok");
    return OMX_ErrorNone;

err:
    LOG_ERROR("StreamingCache::Init() error, call DeInit...");

    DeInit();
    return ret;
}

OMX_ERRORTYPE StreamingCache::DeInit()
{
    LOG_INFO("StreamingCache::DeInit()\n");

    if(pThread != NULL) {
        bStopThread = OMX_TRUE;
        fsl_osal_thread_destroy(pThread);
        pThread = NULL;
    }

    if(mutex != NULL) {
        fsl_osal_mutex_destroy(mutex);
        mutex = NULL;
    }

    FSL_FREE(mCacheNodeRoot);
    FSL_FREE(mCacheBuffer);

    LOG_INFO("StreamingCache::DeInit() ok\n");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE StreamingCache::GetContentLength(OMX_U64 * length)
{
    LOG_DEBUG("StreamingCache::GetContentLength()\n");

    if(mProtocolError != OMX_ErrorNone)
        return mProtocolError;

    *length = mContentLength;
    return OMX_ErrorNone;
}

OMX_U32 StreamingCache::ReadAt(
        OMX_U64 nOffset,
        OMX_U32 nSize,
        OMX_PTR pBuffer)
{
    if(mProtocolError != OMX_ErrorNone)
        return 0;

    LOG_DEBUG("StreamingCache::ReadAt() want read data at [%lld:%d]\n", nOffset, nSize);

    CP_CHECKBYTESRESULTTYPE res = AvailableAt(nOffset, &nSize);
    if(CP_CheckBytesOk != res) {
        LOG_ERROR("StreamingCache::ReadAt() not enough data at [%lld:%d]\n", nOffset, nSize);
        return 0;
    }

    fsl_osal_mutex_lock(mutex);

    CacheNode *pNode = mCacheNodeRoot + mPlayNode;

    LOG_DEBUG("StreamingCache::ReadAt() read from node %d [%lld:%d]\n", mPlayNode, pNode->nOffset, pNode->nLength);

    if((nOffset + nSize) <= (pNode->nOffset + pNode->nLength)) {
        OMX_PTR src = (OMX_PTR)((unsigned long)pNode->pData + pNode->nHeaderLen + (nOffset - pNode->nOffset));
        fsl_osal_memcpy(pBuffer, src, nSize);
        //printf("1:Copy from cache node %d, nOffset=%lld, request=%d\n", mStartNode, nOffset, nSize);
    }
    else {
        OMX_PTR src = (OMX_PTR)((unsigned long)pNode->pData + pNode->nHeaderLen + (nOffset - pNode->nOffset));
        OMX_U32 left = pNode->nLength - (nOffset - pNode->nOffset);
        fsl_osal_memcpy(pBuffer, src, left);
        //printf("2:Copy from cache node %d, offset=%lld, request=%d\n", mStartNode, nOffset, nSize);
        //printf("2:Copy from cache node %d, offset=%lld, length=%d\n", mStartNode, nOffset, left);

        OMX_U64 offset = nOffset + left;
        OMX_U32 size = nSize - left;
        OMX_PTR dest = (OMX_PTR)((unsigned long)pBuffer + left);
        do {
            mPlayNode = (mPlayNode+1)%mBlockCnt;
            pNode = mCacheNodeRoot + mPlayNode;

            //LOG_DEBUG("Cache node [%d], [%lld:%d]\n", mStartNode, pNode->nOffset, pNode->nLength);

            OMX_U32 copy = size >= pNode->nLength ? pNode->nLength : size;
            fsl_osal_memcpy(dest, (OMX_U8*)pNode->pData + pNode->nHeaderLen, copy);
            //printf("2:Copy from cache node %d, offset=%lld, length=%d\n", mStartNode, nOffset, copy);
            offset += copy;
            size -= copy;
            dest = (OMX_PTR)((unsigned long)dest + copy);
        } while(size > 0);

        LOG_DEBUG("StreamingCache::ReadAt() ok, to node %d [%lld:%d]\n", mPlayNode, pNode->nOffset, pNode->nLength);
    }

    fsl_osal_mutex_unlock(mutex);

    return nSize;
}

/*
 * Required size will be modified to a smaller value when reaching end of file, so it needs a pointer type parameter
 */

CP_CHECKBYTESRESULTTYPE StreamingCache::AvailableAt(
        OMX_U64 nOffset,
        OMX_U32 * pSize)
{
    //if(bStreamEOS == OMX_TRUE)
    //    return AVAILABLE;

    OMX_U32 nSize = *pSize;
    CP_CHECKBYTESRESULTTYPE res = CP_CheckBytesOk;
    CacheNode *pNode;

    if(mProtocolError != OMX_ErrorNone)
        return CP_CheckBytesStreamIOFail;

    LOG_DEBUG("StreamingCache::AvailableAt() nOffset %lld, nSize %d\n", nOffset, nSize);

    OMX_U32 nTotal = mBlockCnt * mBlockSize;
    if(nSize > nTotal) {
        LOG_ERROR("StreamingCache::AvailableAt() requires %d larger than cache buffer size %d\n", nSize, nTotal);
        return CP_CheckBytesOutOfBuffers;
    }

    if(mContentLength > 0) {
        if(nOffset >= mContentLength){
            LOG_ERROR("StreamingCache::AvailableAt() requires offset %lld reach contentLength %lld\n", nOffset, mContentLength);
            return CP_CheckBytesAtEndOfStream;
        }

        // if requested size overflows stream length, just copy all available bytes
        OMX_U64 nLeft = mContentLength - nOffset;
        if(nSize > nLeft) {
            LOG_WARNING("StreamingCache::AvailableAt() want %d larger than left %lld\n", nSize, nLeft);
            nSize = nLeft;
            *pSize = nSize;
        }
    }

    fsl_osal_mutex_lock(mutex);

    if(nOffset < mOffsetStart || nOffset > mOffsetEnd) {
        if(bSeekable == OMX_FALSE){
            LOG_ERROR("############# StreamingCache::AvailableAt(), need to seek %lld (%lld-%lld) when stream is unseekable!!! ##################", nOffset, mOffsetStart, mOffsetEnd);
            res = CP_CheckBytesUnseekable;
        }
        else{
            LOG_DEBUG("StreamingCache::AvailableAt() reset cache to offset %lld origin %lld-%lld\n", nOffset, mOffsetStart, mOffsetEnd);
            mResetTargetOffset = nOffset;
            bReset = OMX_TRUE;
            res = CP_CheckBytesInsufficientBytes;
        }
        goto bail;
    }

    // no data in buffer
    if(mOffsetEnd == mOffsetStart){
        res = CP_CheckBytesInsufficientBytes;
        goto bail;
    }

    // most of the time, nOffset shall be in mPlayNode, don't need to search
    // if not in mPlayNode, move playNode to position corresponding to nOffset, search from startNode(including earlier data).
    // if playNode moves forward and buffer is full, this triggers cache thread to read new data from network.

    pNode = mCacheNodeRoot + mPlayNode;
    if(nOffset < pNode->nOffset || nOffset >= pNode->nOffset + pNode->nLength){
        OMX_U32 currentNode;
        for(currentNode = mStartNode ; currentNode != mEndNode; currentNode=(currentNode+1)%mBlockCnt){
            pNode = mCacheNodeRoot + currentNode;
            if(nOffset >= pNode->nOffset && nOffset < pNode->nOffset + pNode->nLength)
                break;
        }

        if(currentNode == mEndNode){
            LOG_ERROR("StreamingCache::AvailableAt() can't find correct node for nOffset %lld, offset %lld-%lld", nOffset, mOffsetStart, mOffsetEnd);
            res = CP_CheckBytesInsufficientBytes;
            goto bail;
        }

        mPlayNode = currentNode;
    }

    if(nOffset + nSize <= mOffsetEnd)
        res = CP_CheckBytesOk;
    else{
        LOG_DEBUG("StreamingCache::AvailableAt() return insufficientBytes, [%lld:%d], block %d-%d-%d, offset %lld-%lld", nOffset, nSize, mStartNode, mPlayNode, mEndNode, mOffsetStart, mOffsetEnd);
        res = CP_CheckBytesInsufficientBytes;
    }

bail:
    fsl_osal_mutex_unlock(mutex);
    return res;
}

OMX_U32 StreamingCache::AvailableBytes(OMX_U64 nOffset)
{
    //printf("StreamingCache::AvailableBytes nOffset %lld, mOffsetEnd %lld\n", nOffset, mOffsetEnd);
    OMX_U32 bytes = 0;

    if(mProtocolError != OMX_ErrorNone)
        return 0;

    fsl_osal_mutex_lock(mutex);

    if(mOffsetEnd <= nOffset)
        bytes = 0;
    else
        bytes = mOffsetEnd - nOffset;

    fsl_osal_mutex_unlock(mutex);

    return bytes;
}

OMX_ERRORTYPE StreamingCache::GetStreamData()
{
    CacheNode *pNode = NULL;
    OMX_ERRORTYPE ret;
    OMX_U32 headerLen = 0;
    OMX_U32 nFilled = 0;
    OMX_U8 *pBuffer = 0;
    OMX_S32 nWant = 0;
    OMX_U32 sleepTime = 50000; // default sleep time in micro second when error occurs or eos

    if(bStopThread == OMX_TRUE)
        return OMX_ErrorNoMore;

    fsl_osal_mutex_lock(mutex);

    if(bReset == OMX_TRUE) {
        bReset = OMX_FALSE;
        bStreamEOS = OMX_FALSE;
        ResetCache(mResetTargetOffset);

        fsl_osal_mutex_unlock(mutex);
        ret = pProtocol->Seek(mOffsetStart, SEEK_SET, OMX_TRUE);
        fsl_osal_mutex_lock(mutex);

        if(ret != OMX_ErrorNone){
            // break from bStopThread or bReset, keep looping
            if(ret == OMX_ErrorTerminated){
                ret = OMX_ErrorNone;
                goto bail;
            }

            if(ret == OMX_ErrorServerUnseekable){
                LOG_ERROR("Shouldn't happen: only do reset when server is seekable");
                bSeekable = OMX_FALSE;
                ret = OMX_ErrorServerUnseekable;
                goto bail;
            }

            if(ret == OMX_ErrorUndefined){
                LOG_ERROR("Shouldn't happen: Undefined error when seek, go on reading data");
                ret = OMX_ErrorNone;
            }
            else{
                mProtocolError = ret;
                ret = OMX_ErrorNoMore;
                goto bail;
            }
        }
    }

    if(bStreamEOS != OMX_TRUE && mContentLength > 0 && mOffsetEnd >= (OMX_U64)mContentLength)
        bStreamEOS = OMX_TRUE;

    if(bStreamEOS == OMX_TRUE){
        ret = OMX_ErrorNone;
        goto bail;
    }

    if((mEndNode+1)%mBlockCnt == mStartNode){
        LOG_DEBUG("StreamingCache thread: buffer full and discard ealier data! node %d-%d-%d", mStartNode, mPlayNode, mEndNode);
        while((mPlayNode+mBlockCnt-mStartNode)%mBlockCnt > mEarlierDataMinBlockCnt){
            (mCacheNodeRoot + mStartNode)->nOffset = 0;
            (mCacheNodeRoot + mStartNode)->nLength = 0;
            mStartNode = (mStartNode+1)%mBlockCnt;
            mOffsetStart = (mCacheNodeRoot + mStartNode)->nOffset;
        }

        if((mEndNode+1)%mBlockCnt == mStartNode){
            LOG_DEBUG("StreamingCache thread: no space recycled for new data, waiting for mPlayNode moving forward");
            ret = OMX_ErrorNone;
            goto bail;
        }

    }


    // start reading one block
    pNode = mCacheNodeRoot + mEndNode;
    pNode->nOffset = mOffsetEnd;
    pNode->nLength = 0;
    pNode->nHeaderLen = 0;

    //LOG_DEBUG("StreamingCache thread: want Fill node %d at offset %d\n", mEndNode, mOffsetEnd);

    pBuffer = (OMX_U8*)pNode->pData;
    nWant = mBlockSize;
    if((mContentLength > 0) && (mOffsetEnd + nWant > (OMX_U64)mContentLength))
        nWant = mContentLength - mOffsetEnd;

    fsl_osal_mutex_unlock(mutex);
    ret = pProtocol->Read((unsigned char*)pBuffer, nWant, &bStreamEOS, mOffsetEnd, &headerLen, &nFilled);
    fsl_osal_mutex_lock(mutex);

    LOG_DEBUG("StreamingCache thread: protocol read: ret %x, offset %lld, nWant %d, eos %d, headerLen %d, nFilled %d\n", ret, mOffsetEnd, nWant, bStreamEOS, headerLen, nFilled);

#if 0
    if(nFilled>0){
        static int iiCnt=0;
        static FILE *pfTest = NULL;
        iiCnt ++;
        if (iiCnt==1) {
            pfTest = fopen("/data/autotest/dumpdata_udp_loop.ts", "wb");
            if(pfTest == NULL)
                printf("Unable to open dump file! \n");
        }
        if(iiCnt > 0 && pfTest != NULL) {
                //printf("dump data %d\n", n);
                fwrite(pBuffer + headerLen, sizeof(char), nFilled - headerLen, pfTest);
                fflush(pfTest);
                ////fclose(pfTest);
                //pfTest = NULL;
         }
    }
#endif

    if(ret != OMX_ErrorNone) {

        // break from bStopThread or bReset, keep looping
        if(ret == OMX_ErrorTerminated){
            ret = OMX_ErrorNone;
            goto bail;
        }

        LOG_ERROR("StreamingCache thread: protocol read fail, ret %x\n", ret);
        mProtocolError = ret;
        goto bail;
    }


    if(nFilled > 0) {
        pNode->nLength = nFilled - headerLen;
        pNode->nHeaderLen = headerLen;
        mOffsetEnd += nFilled - headerLen;
        LOG_DEBUG("StreamingCache thread: fill endNode %d-%d-%d [%lld:%d] offset %lld-%lld\n", mStartNode, mPlayNode, mEndNode, pNode->nOffset, pNode->nLength, mOffsetStart, mOffsetEnd);

        mEndNode = (mEndNode+1)%mBlockCnt;
    }

    if(mContentLength == 0 && bStreamEOS == OMX_TRUE)
        mContentLength = mOffsetEnd;

    sleepTime = 0;

bail:

    fsl_osal_mutex_unlock(mutex);

    // just need to sleep when something is wrong, to avoid this thread looping too fast and consuming all cpu time
    if(sleepTime > 0)
        fsl_osal_sleep(sleepTime);
    return ret;
}

OMX_ERRORTYPE StreamingCache::ResetCache(OMX_U64 nOffset)
{
    printf("StreamingCache::ResetCache() to %lld, node %d-%d-%d, offset %lld-%lld\n", nOffset, mStartNode, mPlayNode, mEndNode, mOffsetStart, mOffsetEnd);

    OMX_S32 i;
    for(i=0; i<(OMX_S32)mBlockCnt; i++) {
        mCacheNodeRoot[i].nOffset = 0;
        mCacheNodeRoot[i].nLength = 0;
        mCacheNodeRoot[i].nHeaderLen = 0;
    }

    mOffsetStart = mOffsetEnd = nOffset;
    mCacheNodeRoot[0].nOffset = nOffset;
    mStartNode = mEndNode = mPlayNode = 0;

    return OMX_ErrorNone;
}

OMX_BOOL StreamingCache::IsSeekable()
{
    return bSeekable;
}


