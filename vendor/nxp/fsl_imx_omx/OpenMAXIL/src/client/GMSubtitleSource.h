/**
 *  Copyright (c) 2013-2014, Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file GMSubtitleSource_h
 *  @brief Class definition of subtitle source
 *  @ingroup GraphManager
 */

#ifndef GMSubtitleSource_h
#define GMSubtitleSource_h

#include "fsl_osal.h"
#include "OMX_Core.h"
#include "OMX_ContentPipe.h"

class GMSubtitleSource {
    public:
        typedef enum {
            UNKNOWN,
            TEXT,
            BITMAP,
        }SUBTITLE_FMT;

        typedef struct {
            SUBTITLE_FMT fmt;
            OMX_U8 *pBuffer;
            OMX_S32 nFilledLen;
            OMX_TICKS nStartTime;
        } SUBTITLE_SAMPLE;

        typedef enum {
            TYPE_UNKNOWN,
            TYPE_3GPP,
            TYPE_XSUB,
            TYPE_SSA,
            TYPE_ASS,
            TYPE_SRT
        }SUBTITLE_TYPE;

        virtual OMX_ERRORTYPE GetNextSubtitleSample(SUBTITLE_SAMPLE &sample, OMX_BOOL &bEOS) = 0;
        virtual ~GMSubtitleSource() {};
};

class SubtitleParserBase {
    public:
        virtual ~SubtitleParserBase() {};
        virtual OMX_ERRORTYPE Init() = 0;
        virtual void DeInit() = 0;
        virtual OMX_ERRORTYPE GetOneSample(GMSubtitleSource::SUBTITLE_SAMPLE &sample, OMX_BOOL &bEOS) = 0;
        virtual OMX_ERRORTYPE SeekTo(OMX_TICKS nPos) = 0;
};

class TextSource {
    public:
        typedef struct {
            OMX_BOOL isEmptyLine;
            OMX_U8 *pData;
            OMX_U32 nDataLen;
        }TEXT_LINE;
        TextSource(CP_PIPETYPE *hPipe, OMX_STRING url);
        OMX_ERRORTYPE Init();
        void DeInit();
        OMX_ERRORTYPE GetNextLine(TEXT_LINE &Line, OMX_BOOL &bEOS);
        OMX_ERRORTYPE SeekTo(OMX_S64 nPos);
        OMX_ERRORTYPE GetCurPosition(OMX_S64 &nPos);
    private:
        static const OMX_U32 mBufferSize = 1024;
        static const OMX_U32  mCacheSize = 2048;
        CP_PIPETYPE *mPipe;
        OMX_STRING mUrl;
        CPhandle mHandle;
        OMX_U8 *mBuffer;
        OMX_U8 *mCache;
        OMX_U32 mCacheRead;
        OMX_U32 mCacheFill;
        OMX_S64 mOffset;

        OMX_U8 GetChar();
};

class SRTParser : public SubtitleParserBase {
    public:
        SRTParser(CP_PIPETYPE *hPipe, OMX_STRING url);
        virtual ~SRTParser();
        OMX_ERRORTYPE Init();
        void DeInit();
        OMX_ERRORTYPE GetOneSample(GMSubtitleSource::SUBTITLE_SAMPLE &sample, OMX_BOOL &bEOS);
        OMX_ERRORTYPE SeekTo(OMX_TICKS nPos);

    private:
        typedef enum {
            SRT_STATE_NULL,
            SRT_STATE_SEQ_NUM,
            SRT_STATE_START_TIME,
            SRT_STATE_END_TIME,
            SRT_STATE_DATA
        }SRT_STATE;
        typedef struct {
            OMX_TICKS nTime;
            OMX_S64 nOffset;
            OMX_S64 nDuration;
        }TIME_NODE;
        TextSource *mSource;
        OMX_BOOL mInit;
        OMX_U8 *mBuffer;
        SRT_STATE mState;
        OMX_TICKS mStartTime;
        OMX_TICKS mEndTime;
        OMX_BOOL bReturnEndTime;
        TIME_NODE *mTimeTable;
        OMX_U32 mTimeTableSize;
        OMX_U32 mTimeTableEntrySize;
        OMX_U32 mTimeTableIndex;

        OMX_ERRORTYPE BuildSeekTable();
};

class GMPlayer;

class GMInbandSubtitleSource : public GMSubtitleSource {
    public:
        GMInbandSubtitleSource(GMPlayer *player);
        OMX_ERRORTYPE SetType(OMX_STRING type);
        virtual OMX_ERRORTYPE GetNextSubtitleSample(SUBTITLE_SAMPLE &sample, OMX_BOOL &bEOS);
        virtual ~GMInbandSubtitleSource();
        OMX_ERRORTYPE SeekTo(OMX_TICKS nPos);

    private:
        GMPlayer *mAVPlayer;
        SUBTITLE_TYPE mType;
        OMX_U8 *pBuffer;
        OMX_BOOL bReturnEndTime;
        OMX_TICKS mEndTime;

        void Convert3GPPToText(SUBTITLE_SAMPLE &sample);
};

class GMOutbandSubtitleSource : public GMSubtitleSource {
    public:
        GMOutbandSubtitleSource(GMPlayer *player, OMX_STRING type, CP_PIPETYPE *pipe, OMX_STRING url);
        OMX_ERRORTYPE Init();
        void DeInit();
        OMX_ERRORTYPE SeekTo(OMX_TICKS pos);
        virtual OMX_ERRORTYPE GetNextSubtitleSample(SUBTITLE_SAMPLE &sample, OMX_BOOL &bEOS);
        virtual ~GMOutbandSubtitleSource();

    private:
        GMPlayer *mAVPlayer;
        SUBTITLE_TYPE mType;
        SubtitleParserBase *mSubtitleParser;
        OMX_BOOL mInit;
};

#endif
