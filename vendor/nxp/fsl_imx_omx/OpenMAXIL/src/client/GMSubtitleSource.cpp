/**
 *  Copyright (c) 2013-2014, Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "GMSubtitleSource.h"
#include "GMPlayer.h"
//#undef LOG_DEBUG
//#define LOG_DEBUG printf

#define BUFFER_SIZE_3GP (1024)
#define BUFFER_SIZE_SRT (1024)

#define TIME2US(h,m,s,ms) (((OMX_TICKS)((h)*3600 + (m)*60 + (s))*1000 + (ms)) * 1000)

static GMSubtitleSource::SUBTITLE_TYPE GetSubtitleType(OMX_STRING type)
{
    GMSubtitleSource::SUBTITLE_TYPE type1;
    if(type && fsl_osal_strcmp(type, "3gpp") == 0) {
        type1 = GMSubtitleSource::TYPE_3GPP;
    }
    else if(type && fsl_osal_strcmp(type, "srt") == 0) {
        type1 = GMSubtitleSource::TYPE_SRT;
    }
    else
        type1 = GMSubtitleSource::TYPE_UNKNOWN;

    return type1;
}


GMInbandSubtitleSource::GMInbandSubtitleSource(GMPlayer *player)
{
    mAVPlayer = player;
    mType = TYPE_UNKNOWN;
    bReturnEndTime = OMX_FALSE;
    pBuffer = NULL;
    mEndTime = 0;
}

GMInbandSubtitleSource::~GMInbandSubtitleSource()
{
    if(pBuffer)
        FSL_FREE(pBuffer);
}

OMX_ERRORTYPE GMInbandSubtitleSource::SetType(OMX_STRING type)
{
    SUBTITLE_TYPE mType1;
    OMX_S32 nLen = 0;

    LOG_DEBUG("GMInbandSubtitleSource set type: %s\n", type);

    mType1 = GetSubtitleType(type);
    switch(mType1) {
        case TYPE_3GPP:
            nLen = BUFFER_SIZE_3GP;
            break;
        case TYPE_SRT:
            nLen = BUFFER_SIZE_SRT;
        default:
            break;
    }

    if(mType1 == TYPE_UNKNOWN)
        return OMX_ErrorNotImplemented;

    if(mType != mType1) {
        if(pBuffer)
            FSL_FREE(pBuffer);

        pBuffer = (OMX_U8*)FSL_MALLOC(nLen);
        if(pBuffer == NULL)
            return OMX_ErrorInsufficientResources;

        mType = mType1;
    }

    fsl_osal_memset(pBuffer, 0, nLen);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMInbandSubtitleSource::GetNextSubtitleSample(
        SUBTITLE_SAMPLE &sample,
        OMX_BOOL &bEOS)
{
    OMX_ERRORTYPE ret;
    OMX_TICKS duration = 0;
    if(bReturnEndTime == OMX_TRUE) {
        sample.pBuffer = NULL;
        sample.nFilledLen = 0;
        sample.nStartTime = mEndTime;
        bReturnEndTime = OMX_FALSE;
        return OMX_ErrorNone;
    }

    ret = mAVPlayer->GetSubtitleNextSample(&sample.pBuffer, &sample.nFilledLen, &sample.nStartTime, &duration);
    if(ret == OMX_ErrorNotReady)
        return ret;

    if(sample.pBuffer == NULL) {
        LOG_DEBUG("In band subtitle EOS.\n");
        bEOS = OMX_TRUE;
        return OMX_ErrorNone;
    }
    bEOS = OMX_FALSE;

#if 0
    {
        FILE *fp = fopen("/dump/sub.dat", "ab");
        if(fp) {
            fwrite(sample.pBuffer, 1, sample.nFilledLen, fp);
            fclose(fp);
        }
    }
#endif
    LOG_DEBUG("inband subtitle get next sample ts=%lld,dur=%lld",sample.nStartTime,duration);
    if(duration > 0){
        mEndTime = sample.nStartTime + duration;
    }else{
        mEndTime = sample.nStartTime + 500000;
    }
    bReturnEndTime = OMX_TRUE;

    if(mType == TYPE_3GPP || mType == TYPE_SRT)
        Convert3GPPToText(sample);

    return OMX_ErrorNone;
}

void GMInbandSubtitleSource::Convert3GPPToText(
        SUBTITLE_SAMPLE &sample)
{
    sample.fmt = TEXT;

    if(sample.nFilledLen >= BUFFER_SIZE_3GP) {
        sample.nFilledLen = BUFFER_SIZE_3GP - 1;
        LOG_ERROR("buffer size is not enough, need: %d, now: %d\n",
                BUFFER_SIZE_3GP, sample.nFilledLen);
    }

    fsl_osal_memset(pBuffer, 0, sample.nFilledLen + 1);

    OMX_S32 i, j = 0;
    for(OMX_S32 i=0; i<sample.nFilledLen; i++) {
        if(sample.pBuffer[i] != 0) {
            pBuffer[j] = sample.pBuffer[i];
            j++;
        }
    }

    sample.nFilledLen = j;
    if(j > 0)
        sample.pBuffer = pBuffer;
    else
        sample.pBuffer = NULL;

    return;
}
OMX_ERRORTYPE GMInbandSubtitleSource::SeekTo(OMX_TICKS pos)
{
    bReturnEndTime = OMX_FALSE;

    return OMX_ErrorNone;
}


GMOutbandSubtitleSource::GMOutbandSubtitleSource(
        GMPlayer *player,
        OMX_STRING type,
        CP_PIPETYPE *pipe,
        OMX_STRING url)
{
    mAVPlayer = player;
    mType = GetSubtitleType(type);
    mInit = OMX_FALSE;
    mSubtitleParser = NULL;
    if(mType == TYPE_SRT) {
        mSubtitleParser = FSL_NEW(SRTParser, (pipe, url));
        if(mSubtitleParser == NULL)
            return;
    }
    mInit = OMX_TRUE;
}

GMOutbandSubtitleSource::~GMOutbandSubtitleSource()
{
    if(mSubtitleParser)
        FSL_DELETE(mSubtitleParser);
}

OMX_ERRORTYPE GMOutbandSubtitleSource::Init()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(mInit != OMX_TRUE)
        return OMX_ErrorInsufficientResources;

    if(mSubtitleParser)
        ret = mSubtitleParser->Init();

    if(ret != OMX_ErrorNone)
        return ret;

    return OMX_ErrorNone;
}

void GMOutbandSubtitleSource::DeInit()
{
    if(mSubtitleParser)
        mSubtitleParser->DeInit();

    return;
}

OMX_ERRORTYPE GMOutbandSubtitleSource::SeekTo(OMX_TICKS pos)
{
    if(mSubtitleParser)
        return mSubtitleParser->SeekTo(pos);
    else
        return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE GMOutbandSubtitleSource::GetNextSubtitleSample(
        SUBTITLE_SAMPLE &sample,
        OMX_BOOL &bEOS)
{
    LOG_DEBUG("GMOutbandSubtitleSource::GetNextSubtitleSample\n");
    if(mSubtitleParser)
        return mSubtitleParser->GetOneSample(sample, bEOS);
    else
        return OMX_ErrorNotImplemented;
}


TextSource::TextSource(
        CP_PIPETYPE *hPipe,
        OMX_STRING url)
{
    mPipe = hPipe;
    mUrl = url;
    mHandle = NULL;
    mBuffer = NULL;
    mCache = NULL;
    mCacheRead = 0;
    mCacheFill = 0;
    mOffset =0;
}


OMX_ERRORTYPE TextSource::Init()
{
    CPresult ret = 0;
    ret = mPipe->Open(&mHandle, mUrl, CP_AccessRead);
    if(ret != 0) {
        LOG_ERROR("Can't open content: %s\n", mUrl);
        return OMX_ErrorUndefined;
    }

    mBuffer = (OMX_U8*)FSL_MALLOC(mBufferSize);
    if(mBuffer == NULL) {
        DeInit();
        return OMX_ErrorInsufficientResources;
    }

    mCache = (OMX_U8*)FSL_MALLOC(mCacheSize);
    if(mCache == NULL) {
        DeInit();
        return OMX_ErrorInsufficientResources;
    }
    mCacheRead = mCacheFill = 0;
    mOffset = 0;

    return OMX_ErrorNone;
}

void TextSource::DeInit()
{
    if(mBuffer)
        FSL_FREE(mBuffer);
    if(mCache)
        FSL_FREE(mCache);
    if(mHandle)
        mPipe->Close(mHandle);
}

OMX_ERRORTYPE TextSource::GetNextLine(
        TEXT_LINE &Line,
        OMX_BOOL &bEOS)
{
    OMX_U8 ch;
    OMX_S32 i = 0;
    fsl_osal_memset(mBuffer, 0, mBufferSize);
    while(1) {
        ch = GetChar();
        //printf("ch: %d\n", ch);
        if(ch == 0) {
            Line.pData = 0;
            bEOS = OMX_TRUE;
            return OMX_ErrorNone;
        }
        mBuffer[i++] = ch;
        //a line may end with: \n, \r, \r\n
        if(ch == '\n')
            break;
        else if( ch == '\r') {
            if('\n' != GetChar())
                mCacheRead--;
            mBuffer[i++] = '\n';
            break;
        }
    }

    if(mBuffer[0] == '\n' || mBuffer[0] == '\r')
        Line.isEmptyLine = OMX_TRUE;
    else
        Line.isEmptyLine = OMX_FALSE;
    Line.pData = mBuffer;
    Line.nDataLen = i;
    bEOS = OMX_FALSE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TextSource::SeekTo(OMX_S64 nPos)
{
    if(0 != mPipe->SetPosition(mHandle, nPos, CP_OriginBegin))
        return OMX_ErrorUndefined;
    mCacheRead = mCacheFill = 0;
    mOffset = nPos;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE TextSource::GetCurPosition(OMX_S64 &nPos)
{
    nPos = mOffset;
    return OMX_ErrorNone;
}

OMX_U8 TextSource::GetChar()
{
    //LOG_DEBUG("mCacheRead: %d, mCacheFill: %d\n", mCacheRead, mCacheFill);
    if(mCacheRead >= mCacheFill) {
        mPipe->GetPosition(mHandle, &mOffset);
        if(0 != mPipe->Read(mHandle, (CPbyte*)mCache, mCacheSize)) {
            LOG_DEBUG("End of text file.\n");
            return 0;
        }
        OMX_S64 nPos = 0;
        mPipe->GetPosition(mHandle, &nPos);
        mCacheFill = nPos - mOffset;
        mCacheRead = 0;
        LOG_DEBUG("Read text content len: %d\n", mCacheFill);
    }

    mOffset++;
    return (OMX_S8)mCache[mCacheRead++];
}

#define SRT_TIME_TABLE_SIZE (1024)
SRTParser::SRTParser(CP_PIPETYPE *hPipe, OMX_STRING url)
{
    mInit = OMX_FALSE;
    mState = SRT_STATE_SEQ_NUM;
    mBuffer = NULL;
    mSource = NULL;
    mTimeTable = NULL;
    mTimeTableEntrySize = 0;
    mTimeTableIndex = 0;
    mTimeTableSize = SRT_TIME_TABLE_SIZE;
    mStartTime = 0;
    mEndTime = 0;
    bReturnEndTime = OMX_FALSE;
    mBuffer = (OMX_U8*)FSL_MALLOC(BUFFER_SIZE_SRT);
    if(mBuffer == NULL) {
        LOG_ERROR("failed allocate buffer for SRTParser.\n");
        return;
    }
    mSource = FSL_NEW(TextSource, (hPipe, url));
    if(mSource == NULL) {
        LOG_ERROR("failed create TextSource.\n");
        return;
    }
    mTimeTable = (TIME_NODE*)FSL_MALLOC(mTimeTableSize * sizeof(TIME_NODE));
    if(mTimeTable == NULL) {
        LOG_ERROR("failed to create time table for srt.\n");
        return;
    }

    mInit = OMX_TRUE;
}

SRTParser::~SRTParser()
{
    if(mBuffer)
        FSL_FREE(mBuffer);
    if(mSource)
        FSL_DELETE(mSource);
    if(mTimeTable)
        FSL_DELETE(mTimeTable);
}

OMX_ERRORTYPE SRTParser::Init()
{
    if(mInit != OMX_TRUE)
        return OMX_ErrorInsufficientResources;

    OMX_ERRORTYPE ret = OMX_ErrorNone;
    ret = mSource->Init();
    if(ret != OMX_ErrorNone)
        return ret;

    bReturnEndTime = OMX_FALSE;
    mState = SRT_STATE_SEQ_NUM;

    mTimeTableEntrySize = 0;
    BuildSeekTable();

    return OMX_ErrorNone;
}

void SRTParser::DeInit()
{
    if(mSource)
        mSource->DeInit();
}

OMX_ERRORTYPE SRTParser::GetOneSample(
        GMSubtitleSource::SUBTITLE_SAMPLE &sample,
        OMX_BOOL &bEOS)
{
    sample.fmt = GMSubtitleSource::TEXT;
    sample.nFilledLen = 0;
    sample.pBuffer = NULL;

    if(bReturnEndTime == OMX_TRUE) {
        sample.nStartTime = mEndTime;
        bReturnEndTime = OMX_FALSE;
        return OMX_ErrorNone;
    }

    if(mTimeTableIndex >= mTimeTableEntrySize){
        LOG_DEBUG("SRT end of index table.\n");
        bEOS = OMX_TRUE;
        return OMX_ErrorNone;
    }


    TIME_NODE * pNode = mTimeTable + mTimeTableIndex;
    if(pNode->nOffset < 0)
        return OMX_ErrorUndefined;

    if(OMX_ErrorNone != mSource->SeekTo(pNode->nOffset))
        return OMX_ErrorUndefined;
    else
        mState = SRT_STATE_START_TIME;

    LOG_DEBUG("GetOneSample: index %d, offset %lld\n", mTimeTableIndex, pNode->nOffset);

    OMX_BOOL bQuit = OMX_FALSE;
    while(bQuit != OMX_TRUE) {
        TextSource::TEXT_LINE sLine;
        mSource->GetNextLine(sLine, bEOS);
        if(bEOS != OMX_FALSE) {
            LOG_DEBUG("SRT end of file.\n");
            return OMX_ErrorNone;
        }

        LOG_DEBUG("line: %s\n", (OMX_STRING)sLine.pData);

        int hour1, hour2, min1, min2, sec1, sec2, msec1, msec2;
        switch(mState) {
            case SRT_STATE_SEQ_NUM:
                {
                    if(sLine.isEmptyLine == OMX_TRUE)
                        break;

                    int seq_num = 0;
                    if(sscanf((OMX_STRING)sLine.pData, "%d", &seq_num) != 1) {
                        break;
                    }
                    LOG_DEBUG("seq_num: %d\n", seq_num);
                    mState = SRT_STATE_START_TIME;
                }
                break;
            case SRT_STATE_START_TIME:
                if(sscanf((OMX_STRING)sLine.pData, "%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d",
                            &hour1, &min1, &sec1, &msec1, &hour2, &min2, &sec2, &msec2) != 8) {
                    mState = SRT_STATE_SEQ_NUM;
                    break;
                }
                mStartTime = TIME2US(hour1, min1, sec1, msec1);
                mEndTime = TIME2US(hour2, min2, sec2, msec2);
                LOG_DEBUG("start time: %lld, end time: %lld\n", mStartTime, mEndTime);
                mState = SRT_STATE_DATA;
                break;
            case SRT_STATE_DATA:
                if(sLine.isEmptyLine == OMX_TRUE) {
                    sample.pBuffer = mBuffer;
                    sample.nStartTime = mStartTime;
                    mState = SRT_STATE_SEQ_NUM;
                    bQuit = OMX_TRUE;
                    bReturnEndTime = OMX_TRUE;
                    mTimeTableIndex++;
                    break;
                }
                fsl_osal_memcpy(mBuffer + sample.nFilledLen, sLine.pData, sLine.nDataLen);
                sample.nFilledLen += sLine.nDataLen;
                break;
            default:
                break;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SRTParser::SeekTo(OMX_TICKS nPos)
{
    OMX_S64 nOffset = 0;
    OMX_S32 low = 0;
    OMX_S32 high = mTimeTableEntrySize - 1;
    OMX_S32 mid = 0;

    while(low <= high) {
        mid = low + (high - low)/2;
        if(nPos < mTimeTable[mid].nTime)
            high = mid -1;
        else if(nPos > mTimeTable[mid].nTime)
            low = mid + 1;
        else {
            break;
        }
    }

    OMX_S32 index = mid;
    if(index > 1 && mTimeTable[index].nTime > nPos &&
        mTimeTable[index-1].nTime + mTimeTable[index-1].nDuration > nPos)
        index -= 1;

    nOffset = mTimeTable[index].nOffset;
    LOG_DEBUG("SRTParser want seek to %lld us, index is %d, actually to %lld, offset: %lld\n",
            nPos, index, mTimeTable[index].nTime, mTimeTable[index].nOffset);

    mSource->SeekTo(mTimeTable[index].nOffset);
    mTimeTableIndex = index;

    bReturnEndTime = OMX_FALSE;
    mState = SRT_STATE_START_TIME;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SRTParser::BuildSeekTable()
{
    TextSource::TEXT_LINE sLine;
    OMX_BOOL bEOS = OMX_FALSE;

    while(1) {
        OMX_S64 nPos = 0;
        mSource->GetCurPosition(nPos);
        mSource->GetNextLine(sLine, bEOS);
        if(bEOS != OMX_FALSE) {
            LOG_DEBUG("%s, EOS.\n", __FUNCTION__);
            break;
        }
        if(sLine.pData == NULL)
            continue;

        int hour1, hour2, min1, min2, sec1, sec2, msec1, msec2;
        if(sscanf((OMX_STRING)sLine.pData, "%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d",
                    &hour1, &min1, &sec1, &msec1, &hour2, &min2, &sec2, &msec2) != 8) {
            continue;
        }

        TIME_NODE *node = mTimeTable + mTimeTableEntrySize;
        node->nTime = TIME2US(hour1, min1, sec1, msec1);
        node->nOffset = nPos;
        node->nDuration= TIME2US(hour2,min2,sec2,msec2) - node->nTime;
        if(node->nDuration < 0)
            node->nDuration = 0;

        mTimeTableEntrySize++;
        if(mTimeTableEntrySize >= mTimeTableSize) {
            OMX_PTR temp = FSL_MALLOC(sizeof(TIME_NODE)*(mTimeTableSize + SRT_TIME_TABLE_SIZE));
            if(!temp) {
                LOG_ERROR("failed to allocate time table size to %d\n", mTimeTableSize + SRT_TIME_TABLE_SIZE);
                break;
            }
            fsl_osal_memcpy(temp, mTimeTable, mTimeTableEntrySize*sizeof(TIME_NODE));
            FSL_FREE(mTimeTable);
            mTimeTable = (TIME_NODE*)temp;
            mTimeTableSize += SRT_TIME_TABLE_SIZE;
        }
        LOG_DEBUG("[%d], time: %lld, offset: %lld\n", mTimeTableEntrySize, node->nTime, node->nOffset);
    }

    LOG_DEBUG("mTimeTableEntrySize: %d\n", mTimeTableEntrySize);

    OMX_BOOL needSort = OMX_FALSE;

    if(mTimeTableEntrySize > 1){
        for(OMX_U32 i = 0; i < mTimeTableEntrySize -2; i++){
            if(mTimeTable[i].nTime > mTimeTable[i+1].nTime){
                needSort = OMX_TRUE;
                break;
            }
        }
    }

    if(needSort == OMX_TRUE){
        for(OMX_U32 roundEnd = mTimeTableEntrySize - 1; roundEnd >0; roundEnd--){
            // search for biggest one from 0 to roundEnd
            OMX_U32 biggest = 0;
            for(OMX_U32 i = 1; i <= roundEnd; i++){
                if(mTimeTable[i].nTime > mTimeTable[biggest].nTime)
                    biggest = i;
            }
            if(biggest != roundEnd){
                // exchange biggest with roundEnd
                TIME_NODE node;
                fsl_osal_memcpy(&node, mTimeTable + biggest, sizeof(TIME_NODE));
                fsl_osal_memcpy(mTimeTable + biggest, mTimeTable + roundEnd, sizeof(TIME_NODE));
                fsl_osal_memcpy(mTimeTable + roundEnd, &node,  sizeof(TIME_NODE));
            }
        }
    }

    mSource->SeekTo(0);

    return OMX_ErrorNone;
}
