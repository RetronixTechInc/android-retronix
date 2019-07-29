/**
 *  Copyright (C) 2016 Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "FslExtractor"
#include <utils/Log.h>

#include "include/FslExtractor.h"
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AUtils.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>
#include <utils/String8.h>
#include <utils/RefBase.h>
#include <dlfcn.h>
#include <OMX_Video.h>
#include <OMX_Audio.h>
#include <OMX_Implement.h>
#include <cutils/properties.h>
namespace android {
#define MAX_USER_DATA_STRING_LENGTH 1024
#define MAX_FRAME_BUFFER_LENGTH 10000000
#define MAX_VIDEO_BUFFER_SIZE (512*1024)
#define MAX_AUDIO_BUFFER_SIZE (16*1024)
#define MAX_TEXT_BUFFER_SIZE (1024)
#define MAX_TRACK_COUNT 32

bool isForceUseGoogleAACCodec = false;

struct FslMediaSource : public MediaSource {
    FslMediaSource(
            const sp<FslExtractor> &extractor, size_t index, sp<MetaData>& metadata);

    virtual status_t start(MetaData *params);
    virtual status_t stop();

    virtual sp<MetaData> getFormat();

    virtual status_t read(
            MediaBuffer **buffer, const ReadOptions *options);

    bool started();
    void addMediaBuffer(MediaBuffer *buffer);
    bool full();
protected:
    virtual ~FslMediaSource();
private:
    sp<FslExtractor> mExtractor;
    size_t mSourceIndex;
    Mutex mLock;
    sp<MetaData> mFormat;
    List<MediaBuffer *> mPendingFrames;
    bool mStarted;
    bool mIsAVC;
    bool mIsHEVC;
    size_t mNALLengthSize;
    size_t mBufferSize;
    uint32 mFrameSent;

    void clearPendingFrames();
    FslMediaSource(const FslMediaSource &);
    FslMediaSource &operator=(const FslMediaSource &);
};

FslMediaSource::FslMediaSource(const sp<FslExtractor> &extractor, size_t index,sp<MetaData>& metadata)
    : mExtractor(extractor),
      mSourceIndex(index),
      mFormat(metadata)
{
    mStarted = false;
    const char *mime;
    CHECK(mFormat->findCString(kKeyMIMEType, &mime));

    mIsAVC = !strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_AVC);
    mIsHEVC = !strcasecmp(mime, MEDIA_MIMETYPE_VIDEO_HEVC);

    mNALLengthSize = 0;
    mBufferSize = 0;
    mFrameSent = 0;

    if (mIsAVC) {
        uint32_t type;
        const void *data;
        size_t size;
        if(!mFormat->findData(kKeyAVCC, &type, &data, &size))
            return;

        const uint8_t *ptr = (const uint8_t *)data;

        CHECK(size >= 7);
         CHECK_EQ((unsigned)ptr[0], 1u);  // configurationVersion == 1

        // The number of bytes used to encode the length of a NAL unit.
        mNALLengthSize = 1 + (ptr[4] & 3);
        ALOGD("FslMediaSource::FslMediaSource avc mNALLengthSize=%zu",mNALLengthSize);
    } else if (mIsHEVC) {
        uint32_t type;
        const void *data;
        size_t size;
        if(!mFormat->findData(kKeyHVCC, &type, &data, &size))
            return;

        const uint8_t *ptr = (const uint8_t *)data;

        CHECK(size >= 7);
        CHECK_EQ((unsigned)ptr[0], 1u);  // configurationVersion == 1

        mNALLengthSize = 1 + (ptr[21] & 3);
        ALOGD("FslMediaSource::FslMediaSource hevc mNALLengthSize=%zu",mNALLengthSize);
    }
}
FslMediaSource::~FslMediaSource()
{
    clearPendingFrames();
    mExtractor->ClearTrackSource(mSourceIndex);
    ALOGD("FslMediaSource::~FslMediaSource");

}
status_t FslMediaSource::start(MetaData * /* params */)
{
    mStarted = true;
    mExtractor->ActiveTrack(mSourceIndex);
    ALOGD("source start track %d",mSourceIndex);
    return OK;
}
void FslMediaSource::clearPendingFrames() {
    while (!mPendingFrames.empty()) {
        MediaBuffer *frame = *mPendingFrames.begin();
        mPendingFrames.erase(mPendingFrames.begin());

        frame->release();
        frame = NULL;
    }
    mBufferSize = 0;

}

status_t FslMediaSource::stop()
{
    clearPendingFrames();
    mStarted = false;
    mExtractor->DisableTrack(mSourceIndex);
    return OK;
}
sp<MetaData> FslMediaSource::getFormat()
{
    return mFormat;
}
static unsigned U24_AT(const uint8_t *ptr) {
    return ptr[0] << 16 | ptr[1] << 8 | ptr[2];
}
status_t FslMediaSource::read(
        MediaBuffer **out, const ReadOptions *options)
{
    status_t ret = OK;
    *out = NULL;
    uint32_t seekFlag = 0;
    //int64_t targetSampleTimeUs = -1ll;
    size_t srcSize = 0;
    size_t srcOffset = 0;
    int32_t i = 0;
    int64_t seekTimeUs;
    ReadOptions::SeekMode mode;
    int64_t outTs = 0;
    const char *containerMime = NULL;
    const char *mime = NULL;

    if (options && options->getSeekTo(&seekTimeUs, &mode)) {
        switch (mode) {
            case ReadOptions::SEEK_PREVIOUS_SYNC:
                seekFlag = SEEK_FLAG_NO_LATER;
                break;
            case ReadOptions::SEEK_NEXT_SYNC:
                seekFlag = SEEK_FLAG_NO_EARLIER;
                break;
            case ReadOptions::SEEK_CLOSEST_SYNC:
            case ReadOptions::SEEK_CLOSEST:
                seekFlag = SEEK_FLAG_NEAREST;
                break;
            default:
                seekFlag = SEEK_FLAG_NEAREST;
                break;
        }

        clearPendingFrames();

        sp<MetaData> meta = mExtractor->getMetaData();
        if(meta != NULL){
            meta->findCString(kKeyMIMEType, &containerMime);
            mFormat->findCString(kKeyMIMEType, &mime);

            if(mFrameSent < 10 && containerMime && !strcasecmp(containerMime, MEDIA_MIMETYPE_CONTAINER_FLV)
                        && mime && !strcasecmp(mime,MEDIA_MIMETYPE_VIDEO_SORENSON))
            {
                ALOGV("read first frame before seeking track, mFrameSent %d", mFrameSent);
                int64_t time = 0;
                int32_t j=0;
                ret = mExtractor->HandleSeekOperation(mSourceIndex,&time,seekFlag);
                while (mPendingFrames.empty()) {
                    status_t err = mExtractor->GetNextSample(mSourceIndex,false);
                    if (err != OK) {
                        clearPendingFrames();
                        return err;
                    }
                    j++;
                    if(j > 1 && OK != mExtractor->CheckInterleaveEos(mSourceIndex)){
                        ALOGE("get interleave eos");
                        return ERROR_END_OF_STREAM;
                    }
                }
                MediaBuffer *frame = *mPendingFrames.begin();
                frame->meta_data()->setInt64(kKeyTime, seekTimeUs);
            }

        }

        ret = mExtractor->HandleSeekOperation(mSourceIndex,&seekTimeUs,seekFlag);
    }

    while (mPendingFrames.empty()) {
        status_t err = mExtractor->GetNextSample(mSourceIndex,false);

        if (err != OK) {
            clearPendingFrames();

            return err;
        }
        i++;
        if(i > 1 && OK != mExtractor->CheckInterleaveEos(mSourceIndex)){
            ALOGE("get interleave eos");
            return ERROR_END_OF_STREAM;
        }
    }

    MediaBuffer *frame = *mPendingFrames.begin();
    mPendingFrames.erase(mPendingFrames.begin());

    *out = frame;
    mBufferSize -= frame->size();

    mFrameSent++;
    //frame->meta_data()->findInt64(kKeyTime, &outTs);
    ALOGV("FslMediaSource::read mSourceIndex=%d size=%d,time %lld",mSourceIndex,frame->size(),outTs);

    if(!mIsAVC && !mIsHEVC){
        return OK;
    }

    //convert to nal frame
    uint8_t *srcPtr =
        (uint8_t *)frame->data() + frame->range_offset();
    srcSize = frame->range_length();

    if(srcPtr[0] == 0x0 && srcPtr[1] == 0x0 && srcPtr[2] == 0x0 && srcPtr[3] == 0x1){
        return OK;
    }

    if(0 == mNALLengthSize)
        return OK;

    //replace the 4 bytes when nal length size is 4
    if(4 == mNALLengthSize){

        while(srcOffset + mNALLengthSize <= srcSize){
            size_t NALsize = U32_AT(srcPtr + srcOffset);

            srcPtr[srcOffset++] = 0;
            srcPtr[srcOffset++] = 0;
            srcPtr[srcOffset++] = 0;
            srcPtr[srcOffset++] = 1;

            //memcpy(&srcPtr[srcOffset], "\x00\x00\x00\x01", 4);
            srcOffset += NALsize;
        }
        if(srcOffset < srcSize){
            frame->release();
            frame = NULL;

            return ERROR_MALFORMED;
        }
        ALOGV("FslMediaSource::read 2 size=%d",srcSize);

        return OK;
    }

    //create a new MediaBuffer and copy all data from old buffer to new buffer.
    size_t dstSize = 0;
    MediaBuffer *buffer = NULL;
    uint8_t *dstPtr = NULL;
    //got the buffer size when pass is 0, then copy buffer when pass is 1
    for (int32_t pass = 0; pass < 2; pass++) {
        ALOGV("FslMediaSource::read pass=%d,begin",pass);
        size_t srcOffset = 0;
        size_t dstOffset = 0;
        while (srcOffset + mNALLengthSize <= srcSize) {
            size_t NALsize;
            switch (mNALLengthSize) {
                case 1: NALsize = srcPtr[srcOffset]; break;
                case 2: NALsize = U16_AT(srcPtr + srcOffset); break;
                case 3: NALsize = U24_AT(srcPtr + srcOffset); break;
                case 4: NALsize = U32_AT(srcPtr + srcOffset); break;
                default:
                    TRESPASS();
            }

            if (NALsize == 0) {
                frame->release();
                frame = NULL;

                return ERROR_MALFORMED;
            } else if (srcOffset + mNALLengthSize + NALsize > srcSize) {
                break;
            }

            if (pass == 1) {
                memcpy(&dstPtr[dstOffset], "\x00\x00\x00\x01", 4);

                memcpy(&dstPtr[dstOffset + 4],
                       &srcPtr[srcOffset + mNALLengthSize],
                       NALsize);
                ALOGV("FslMediaSource::read 3 copy %d",4+NALsize);
            }

            dstOffset += 4;  // 0x00 00 00 01
            dstOffset += NALsize;

            srcOffset += mNALLengthSize + NALsize;
        }

        if (srcOffset < srcSize) {
            // There were trailing bytes or not enough data to complete
            // a fragment.

            frame->release();
            frame = NULL;

            return ERROR_MALFORMED;
        }

        if (pass == 0) {
            dstSize = dstOffset;

            buffer = new MediaBuffer(dstSize);

            int64_t timeUs;
            CHECK(frame->meta_data()->findInt64(kKeyTime, &timeUs));
            int32_t isSync;
            CHECK(frame->meta_data()->findInt32(kKeyIsSyncFrame, &isSync));

            buffer->meta_data()->setInt64(kKeyTime, timeUs);
            buffer->meta_data()->setInt32(kKeyIsSyncFrame, isSync);

            dstPtr = (uint8_t *)buffer->data();
            ALOGV("FslMediaSource::read 3 size=%d,ts=%lld",dstSize,timeUs);
        }

    }
    frame->release();
    frame = NULL;
    *out = buffer;

    return OK;
}

void FslMediaSource::addMediaBuffer(MediaBuffer *buffer)
{
    if(buffer == NULL)
        return;
    mBufferSize += buffer->size();

    mPendingFrames.push_back(buffer);
    return;
}
bool FslMediaSource::started()
{
    return mStarted;
}
bool FslMediaSource::full()
{
    if(mBufferSize > MAX_FRAME_BUFFER_LENGTH)
        return true;
    else
        return false;
}

struct FslDataSourceReader{
public:
    FslDataSourceReader(const sp<DataSource> &source);

    bool isStreaming() const;
    bool isLiveStreaming() const;
    bool AddBufferReadLimitation(uint32_t index,uint32_t size);
    uint32_t GetBufferReadLimitation(uint32_t index);
    sp<DataSource> mDataSource;
    Mutex mLock;
    int64_t mOffset;
    bool bStopReading;
    int64_t mLength;
private:

    bool mIsLiveStreaming;
    bool mIsStreaming;
    uint32_t mMaxBufferSize[MAX_TRACK_COUNT];

    FslDataSourceReader(const FslDataSourceReader &);
    FslDataSourceReader &operator=(const FslDataSourceReader &);
};
static FslFileHandle appFileOpen ( const uint8 * file_path, const uint8 * mode, void * context)
{
    if(NULL == mode || !context)
    {
        ALOGE("appLocalFileOpen: Invalid parameter\n");
        return NULL;
    }
    if(file_path)
        ALOGV("file_path=%s",file_path);

    ALOGV("appFileOpen");
    FslDataSourceReader *h = (FslDataSourceReader *)context;
    FslFileHandle sourceFileHandle =(FslFileHandle)&h->mDataSource;
    Mutex::Autolock autoLock(h->mLock);
    if(h->mDataSource->initCheck() == OK){
        ALOGV("appFileOpen success,sourceFileHandle=%p",sourceFileHandle);
        return sourceFileHandle;
    }else
        return NULL;
}
static uint32  appReadFile( FslFileHandle file_handle, void * buffer, uint32 nb, void * context)
{
    FslDataSourceReader *h = (FslDataSourceReader *)context;
    int ret = 0;

    if (!file_handle || !context || !buffer)
        return 0;

    Mutex::Autolock autoLock(h->mLock);
    if(h->bStopReading){
        return 0;
    }

    //ALOGV("appLocalReadFile nb %u",(unsigned int)nb);

    ret = h->mDataSource->readAt(h->mOffset, buffer, nb);

    //ALOGD("appReadFile at %lld nb %u, result %d", h->mOffset, (unsigned int)nb, ret);

    if(ret > 0)
    {
        h->mOffset += ret;
        return ret;
    }
    else
    {
        ALOGV("appLocalReadFile 0");
        return 0xffffffff;
    }
}


static int32   appSeekFile( FslFileHandle file_handle, int64 offset, int32 whence, void * context)
{

    FslDataSourceReader *h = (FslDataSourceReader *)context;
    int nContentPipeResult = 0;
    if (!file_handle || !context)
        return -1;

    Mutex::Autolock autoLock(h->mLock);
    //ALOGV("appSeekFile current location=%lld,offset %lld, whence %d",h->mOffset, offset, whence);

    switch(whence) {
        case SEEK_CUR:
        {
            if(h->mLength > 0 && h->mOffset + offset > h->mLength){
                nContentPipeResult = -1;
            }else
                h->mOffset += offset;
        }
        break;
        case SEEK_SET:
        {
            if(h->mLength > 0 && offset > h->mLength)
                nContentPipeResult = -1;
            else
                h->mOffset = offset;
        }
        break;
        case SEEK_END:
        {
            if(offset > 0 ||  h->mLength + offset < 0 || h->isLiveStreaming())
                nContentPipeResult = -1;
            else
                h->mOffset = h->mLength + offset;
        }
        break;
        default:
            nContentPipeResult = -1;
            break;
    }

    if( 0 == nContentPipeResult) /* success */
        return 0;

    ALOGV("appSeekFile fail %d", nContentPipeResult);
    return -1;
}
static int64  appGetCurrentFilePos( FslFileHandle file_handle, void * context)
{
    FslDataSourceReader *h = (FslDataSourceReader *)context;

    if (!file_handle || !context)
        return -1;

    return h->mOffset;
}

static int64   appFileSize( FslFileHandle file_handle, void * context)
{
    if (!file_handle || !context)
        return -1;

    FslDataSourceReader *h = (FslDataSourceReader *)context;

    ALOGV("appFileSize %lld", h->mLength);

    return h->mLength;
}

static int32 appFileClose( FslFileHandle file_handle, void * context)
{
    if (!file_handle || !context)
        return -1;
    FslDataSourceReader *h = (FslDataSourceReader *)context;

    Mutex::Autolock autoLock(h->mLock);
    file_handle = NULL;
    h->mOffset = 0;
    ALOGV("appFileClose");
    return 0;
}

static int64   appCheckAvailableBytes(FslFileHandle file_handle, int64 bytesRequested, void * context)
{
    if (!file_handle || !context)
        return 0;

    return bytesRequested;
}

static uint32  appGetFlag( FslFileHandle file_handle, void * context)
{
    uint32 flag = 0;

    if (!file_handle || !context)
        return 0;

    FslDataSourceReader *h = (FslDataSourceReader *)context;
    if(h->isLiveStreaming()){
        flag |= FILE_FLAG_NON_SEEKABLE;
        flag |= FILE_FLAG_READ_IN_SEQUENCE;
    }
    if(h->isStreaming())
        flag |= FILE_FLAG_READ_IN_SEQUENCE;

    ALOGV("appLocalGetFlag %x", flag);

    return flag;
}

static void *appCalloc(uint32 TotalNumber, uint32 TotalSize)
{
    void *PtrCalloc = NULL;

    if((0 == TotalSize)||(0==TotalNumber))
        ALOGW("\nWarning: ZERO size IN LOCAL CALLOC");

    PtrCalloc = malloc(TotalNumber*TotalSize);

    if (PtrCalloc == NULL) {

        ALOGE("\nError: MEMORY FAILURE IN LOCAL CALLOC");
        return NULL;
    }
    memset(PtrCalloc, 0, TotalSize*TotalNumber);
    return (PtrCalloc);
}

static void* appMalloc (uint32 TotalSize)
{

    void *PtrMalloc = NULL;

    if(0 == TotalSize)
        ALOGW("\nWarning: ZERO size IN LOCAL MALLOC");

    PtrMalloc = malloc(TotalSize);

    if (PtrMalloc == NULL) {

        ALOGE("\nError: MEMORY FAILURE IN LOCAL MALLOC");
    }
    return (PtrMalloc);
}
static void appFree (void *MemoryBlock)
{
    if(MemoryBlock)
        free(MemoryBlock);
}

static void * appReAlloc (void *MemoryBlock, uint32 TotalSize)
{
    void *PtrMalloc = NULL;

    if(0 == TotalSize)
        ALOGW("\nWarning: ZERO size IN LOCAL REALLOC");

    PtrMalloc = (void *)realloc(MemoryBlock, TotalSize);
    if (PtrMalloc == NULL) {
        ALOGE("\nError: MEMORY FAILURE IN LOCAL REALLOC");
    }

    return PtrMalloc;
}
static uint8* appRequestBuffer(   uint32 streamNum,
                                uint32 *size,
                                void ** bufContext,
                                void * parserContext)
{
    FslDataSourceReader *h;
    uint8 * dataBuf = NULL;
    ABuffer * buffer = NULL;
    uint32_t limitSize = 0;

    if (!size || !bufContext || !parserContext)
        return NULL;

    ALOGV("appRequestBuffer streamNum=%u",streamNum);

    h = (FslDataSourceReader *)parserContext;

    if((*size) >= 100000000){
        return NULL;
    }

    limitSize = h->GetBufferReadLimitation(streamNum);
    if((*size) > limitSize && limitSize > 0)
        *size = limitSize;

    buffer = new ABuffer((size_t)(*size));

    *bufContext = (void*)buffer;
    dataBuf = buffer->data();

    return dataBuf;
}


static void appReleaseBuffer(uint32 streamNum, uint8 * pBuffer, void * bufContext, void * parserContext)
{
    FslDataSourceReader *h;
    sp<ABuffer> buffer = NULL;

    ALOGV("appReleaseBuffer streamNum=%u",streamNum);

    if (!pBuffer || !bufContext || !parserContext)
        return;
    h = (FslDataSourceReader *)parserContext;
    buffer = (ABuffer *)(bufContext);
    buffer.clear();

    return;
}

FslDataSourceReader::FslDataSourceReader(const sp<DataSource> &source)
    :mDataSource(source),
    mOffset(0),
    mLength(0)
{
    off64_t size = 0;
    mIsLiveStreaming =
        (mDataSource->flags()
            & (DataSource::kWantsPrefetching
                | DataSource::kIsCachingDataSource))
        && mDataSource->getSize(&size) != OK;
    mIsStreaming = (mDataSource->flags() & DataSource::kIsCachingDataSource);

    if(!mIsLiveStreaming){
        status_t ret = mDataSource->getSize(&size);
        if(ret == OK)
            mLength = size;
        else if(ret > 0)
            mLength = ret;
    }else{
        mLength = 0;
    }

    ALOGV("FslDataSourceReader: mLength is %lld", mLength);

    bStopReading = false;
    memset(&mMaxBufferSize[0],0,MAX_TRACK_COUNT*sizeof(uint32_t));
}
bool FslDataSourceReader::isStreaming() const
{
    return mIsStreaming;
}
bool FslDataSourceReader::isLiveStreaming() const
{
    return mIsLiveStreaming;
}
bool FslDataSourceReader::AddBufferReadLimitation(uint32_t index,uint32_t size)
{
    if(index <= MAX_TRACK_COUNT){
        mMaxBufferSize[index] = size;
        return true;
    }else
        return false;
}
uint32_t FslDataSourceReader::GetBufferReadLimitation(uint32_t index)
{
    if(index <= MAX_TRACK_COUNT){
        return mMaxBufferSize[index];
    }else
        return 0;
}

typedef struct{
    const char* name;
    const char* mime;
}fsl_mime_struct;
fsl_mime_struct mime_table[]={
    {"mp4",MEDIA_MIMETYPE_CONTAINER_MPEG4},
    {"mkv",MEDIA_MIMETYPE_CONTAINER_MATROSKA},
    {"avi",MEDIA_MIMETYPE_CONTAINER_AVI},
    {"asf",MEDIA_MIMETYPE_CONTAINER_ASF},
    {"flv",MEDIA_MIMETYPE_CONTAINER_FLV},
    {"mpg2",MEDIA_MIMETYPE_CONTAINER_MPEG2TS},
    {"mpg2",MEDIA_MIMETYPE_CONTAINER_MPEG2PS},
    {"rm",MEDIA_MIMETYPE_CONTAINER_RMVB},
    {"mp3",MEDIA_MIMETYPE_AUDIO_MPEG},
    {"aac",MEDIA_MIMETYPE_AUDIO_AAC_ADTS},
    {"ape",MEDIA_MIMETYPE_AUDIO_APE},
    {"flac",MEDIA_MIMETYPE_AUDIO_FLAC}
};
typedef struct{
    uint32_t type;
    uint32_t subtype;
    const char* mime;
}codec_mime_struct;
codec_mime_struct video_mime_table[]={
    {VIDEO_H263,0,MEDIA_MIMETYPE_VIDEO_H263},
    {VIDEO_H264,0,MEDIA_MIMETYPE_VIDEO_AVC},
    {VIDEO_HEVC,0,MEDIA_MIMETYPE_VIDEO_HEVC},
    {VIDEO_MPEG2,0,MEDIA_MIMETYPE_VIDEO_MPEG2},
    {VIDEO_MPEG4,0,MEDIA_MIMETYPE_VIDEO_MPEG4},
    {VIDEO_JPEG,0,MEDIA_MIMETYPE_VIDEO_MJPEG},
    {VIDEO_MJPG,VIDEO_MJPEG_2000,NULL},
    {VIDEO_MJPG,VIDEO_MJPEG_FORMAT_B,NULL},
    {VIDEO_MJPG,0,MEDIA_MIMETYPE_VIDEO_MJPEG},
    {VIDEO_DIVX,VIDEO_DIVX3,MEDIA_MIMETYPE_VIDEO_DIV3},
    {VIDEO_DIVX,VIDEO_DIVX4,MEDIA_MIMETYPE_VIDEO_DIV4},
    {VIDEO_DIVX,VIDEO_DIVX5_6,MEDIA_MIMETYPE_VIDEO_DIVX},
    {VIDEO_DIVX,0,MEDIA_MIMETYPE_VIDEO_DIVX},
    {VIDEO_XVID,0,MEDIA_MIMETYPE_VIDEO_XVID},
    {VIDEO_WMV,VIDEO_WMV7,MEDIA_MIMETYPE_VIDEO_WMV},
    {VIDEO_WMV,VIDEO_WMV8,MEDIA_MIMETYPE_VIDEO_WMV},
    {VIDEO_WMV,VIDEO_WMV9,MEDIA_MIMETYPE_VIDEO_WMV9},
    {VIDEO_WMV,VIDEO_WMV9A,MEDIA_MIMETYPE_VIDEO_WMV9},
    {VIDEO_WMV,VIDEO_WVC1,MEDIA_MIMETYPE_VIDEO_WMV9},
    {VIDEO_REAL,0,MEDIA_MIMETYPE_VIDEO_REAL},
    {VIDEO_SORENSON_H263,0,MEDIA_MIMETYPE_VIDEO_SORENSON},
    {VIDEO_ON2_VP,VIDEO_VP8,MEDIA_MIMETYPE_VIDEO_VP8},
    {VIDEO_ON2_VP,VIDEO_VP9,MEDIA_MIMETYPE_VIDEO_VP9},
};
codec_mime_struct audio_mime_table[]={
    {AUDIO_MP3,0,MEDIA_MIMETYPE_AUDIO_MPEG},
    {AUDIO_VORBIS,0,MEDIA_MIMETYPE_AUDIO_VORBIS},
    {AUDIO_AAC,0,MEDIA_MIMETYPE_AUDIO_AAC},
    {AUDIO_MPEG2_AAC,0,MEDIA_MIMETYPE_AUDIO_AAC},
    {AUDIO_AC3,0,MEDIA_MIMETYPE_AUDIO_AC3},
    {AUDIO_EC3,0,MEDIA_MIMETYPE_AUDIO_EAC3},
    {AUDIO_WMA,0,MEDIA_MIMETYPE_AUDIO_WMA},
    {AUDIO_AMR,AUDIO_AMR_NB,MEDIA_MIMETYPE_AUDIO_AMR_NB},
    {AUDIO_AMR,AUDIO_AMR_WB,MEDIA_MIMETYPE_AUDIO_AMR_WB},
    {AUDIO_PCM,0,MEDIA_MIMETYPE_AUDIO_RAW},
    {AUDIO_REAL,REAL_AUDIO_RAAC,MEDIA_MIMETYPE_AUDIO_AAC},
    {AUDIO_REAL,REAL_AUDIO_SIPR,MEDIA_MIMETYPE_AUDIO_REAL},
    {AUDIO_REAL,REAL_AUDIO_COOK,MEDIA_MIMETYPE_AUDIO_REAL},
    {AUDIO_REAL,REAL_AUDIO_ATRC,MEDIA_MIMETYPE_AUDIO_REAL},
    {AUDIO_PCM_ALAW,0,MEDIA_MIMETYPE_AUDIO_G711_ALAW},
    {AUDIO_PCM_MULAW,0,MEDIA_MIMETYPE_AUDIO_G711_MLAW},
    {AUDIO_FLAC,0,MEDIA_MIMETYPE_AUDIO_FLAC},
    {AUDIO_OPUS,0,MEDIA_MIMETYPE_AUDIO_OPUS},
    {AUDIO_APE,0,MEDIA_MIMETYPE_AUDIO_APE},
};
FslExtractor::FslExtractor(const sp<DataSource> &source,const char *mime)
    : mDataSource(source),
    mReader(new FslDataSourceReader(mDataSource)),
    mMime(strdup(mime)),
    bInit(false),
    mFileMetaData(new MetaData)
{
    memset(&mLibName,0,255);
    mLibHandle = NULL;
    IParser = NULL;
    parserHandle = NULL;
    mFileMetaData->setCString(kKeyMIMEType, mime);
    currentVideoTs = 0;
    currentAudioTs = 0;
    mVideoActived = false;

    ALOGD("FslExtractor::FslExtractor mime=%s",mMime);
}
FslExtractor::~FslExtractor()
{
    if(parserHandle)
    {
        IParser->deleteParser(parserHandle);
        parserHandle = NULL;
    }

    if(IParser){
        delete IParser;
        IParser = NULL;
    }

    if(mReader != NULL){
        delete mReader;
        mReader = NULL;
    }

    if (mLibHandle != NULL) {
        dlclose(mLibHandle);
        mLibHandle = NULL;
    }
    free(mMime);
    mMime = NULL;
    ALOGD("FslExtractor::~FslExtractor");
}
status_t FslExtractor::Init()
{
    status_t ret = OK;

    if(mReader == NULL)
        return UNKNOWN_ERROR;
    ALOGD("FslExtractor::Init BEGIN");
    memset (&fileOps, 0, sizeof(FslFileStream));
    fileOps.Open = appFileOpen;
    fileOps.Read= appReadFile;
    fileOps.Seek = appSeekFile;
    fileOps.Tell = appGetCurrentFilePos;
    fileOps.Size= appFileSize;
    fileOps.Close = appFileClose;
    fileOps.CheckAvailableBytes = appCheckAvailableBytes;
    fileOps.GetFlag = appGetFlag;

    memset (&memOps, 0, sizeof(ParserMemoryOps));
    memOps.Calloc = appCalloc;
    memOps.Malloc = appMalloc;
    memOps.Free= appFree;
    memOps.ReAlloc= appReAlloc;

    outputBufferOps.RequestBuffer = appRequestBuffer;
    outputBufferOps.ReleaseBuffer = appReleaseBuffer;
    ret = CreateParserInterface();
    if(ret != OK){
        ALOGE("FslExtractor create parser failed");
        return ret;
    }

    ret = ParseFromParser();

    ALOGD("FslExtractor::Init ret=%d",ret);

    if(ret == OK)
        bInit = true;
    return ret;
}
size_t FslExtractor::countTracks()
{
    status_t ret = OK;
    if(!bInit){
        ret = Init();

        if(ret != OK)
            return 0;
    }

    return mTracks.size();
}
sp<MediaSource> FslExtractor::getTrack(size_t index)
{
    sp<FslMediaSource> source;
    sp<MetaData> meta;
    if (index >= mTracks.size()) {
        return NULL;
    }
    ALOGD("FslExtractor::getTrack index=%zu",index);

    if (index >= mTracks.size()) {
        return NULL;
    }
    TrackInfo *trackInfo = &mTracks.editItemAt(index);

    meta = trackInfo->mMeta;
    source = new FslMediaSource(this,index,meta);

    trackInfo->mSource = source;
    source->decStrong(this);
    ALOGE("getTrack source string cnt=%d",source->getStrongCount());

    return source;
}
sp<MetaData> FslExtractor::getTrackMetaData(
        size_t index, uint32_t flags) {

    if(!bInit){
        status_t ret = OK;
        ret = Init();

        if(ret != OK)
            return NULL;
    }
    if(flags){
        ;//
    }
    return mTracks.itemAt(index).mMeta;
}
sp<MetaData> FslExtractor::getMetaData()
{
    if(!bInit){
        status_t ret = OK;
        ret = Init();

        if(ret != OK)
            return NULL;
    }
    return mFileMetaData;
}
uint32_t FslExtractor::flags() const
{
    uint32_t x = CAN_PAUSE;
    if (!mReader->isLiveStreaming() && bSeekable) {
        x |= CAN_SEEK_BACKWARD | CAN_SEEK_FORWARD | CAN_SEEK;
    }

    return x;
}
status_t FslExtractor::GetLibraryName()
{
    const char * name = NULL;
    for (size_t i = 0; i < sizeof(mime_table) / sizeof(mime_table[0]); i++) {
        if (!strcmp((const char *)mMime, mime_table[i].mime)) {
            name = mime_table[i].name;
            break;
        }
    }
    if(name == NULL)
        return NAME_NOT_FOUND;

    strcpy(mLibName, "lib_");
    strcat(mLibName,name);
    strcat(mLibName,"_parser_arm11_elinux.3.0.so");

    ALOGD("GetLibraryName %s",mLibName);
    return OK;
}
status_t FslExtractor::CreateParserInterface()
{
    status_t ret = OK;
    int32 err = PARSER_SUCCESS;
    tFslParserQueryInterface  myQueryInterface;

    ret = GetLibraryName();
    if(ret != OK)
        return ret;

    do{
        mLibHandle = dlopen(mLibName, RTLD_NOW);
        if (mLibHandle == NULL){
            ret = UNKNOWN_ERROR;
            break;
        }
        ALOGD("load parser name %s",mLibName);
        myQueryInterface = (tFslParserQueryInterface)dlsym(mLibHandle, "FslParserQueryInterface");
        if(myQueryInterface == NULL){
            ret = UNKNOWN_ERROR;
            break;
        }

        IParser = new FslParserInterface;
        if(IParser == NULL){
            ret = UNKNOWN_ERROR;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_VERSION_INFO, (void **)&IParser->getVersionInfo);
        if(err)
            break;

        if(!IParser->getVersionInfo){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        // create & delete
        err = myQueryInterface(PARSER_API_CREATE_PARSER, (void **)&IParser->createParser);
        if(err)
            break;

        if(!IParser->createParser){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_CREATE_PARSER2, (void **)&IParser->createParser2);
        if(err)
            ALOGW("IParser->createParser2 not found");

        err = myQueryInterface(PARSER_API_DELETE_PARSER, (void **)&IParser->deleteParser);
        if(err)
            break;

        if(!IParser->deleteParser){
            err = PARSER_ERR_INVALID_API;
            break;
        }
        //index init
        err = myQueryInterface(PARSER_API_INITIALIZE_INDEX, (void **)&IParser->initializeIndex);
        if(err)
            break;

        //movie properties
        err = myQueryInterface(PARSER_API_IS_MOVIE_SEEKABLE, (void **)&IParser->isSeekable);
        if(err)
            break;

        if(!IParser->isSeekable){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_MOVIE_DURATION, (void **)&IParser->getMovieDuration);
        if(err)
            break;

        if(!IParser->getMovieDuration){
            err = PARSER_ERR_INVALID_API;
            break;
        }
        err = myQueryInterface(PARSER_API_GET_USER_DATA, (void **)&IParser->getUserData);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_META_DATA, (void **)&IParser->getMetaData);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_NUM_TRACKS, (void **)&IParser->getNumTracks);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_NUM_PROGRAMS, (void **)&IParser->getNumPrograms);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_PROGRAM_TRACKS, (void **)&IParser->getProgramTracks);
        if(err)
            break;

        if((!IParser->getNumTracks && !IParser->getNumPrograms)
            ||(IParser->getNumPrograms && !IParser->getProgramTracks))
        {
            ALOGE("Invalid API to get tracks or programs.");
            err = PARSER_ERR_INVALID_API;
            break;
        }

        //track properties
        err = myQueryInterface(PARSER_API_GET_TRACK_TYPE, (void **)&IParser->getTrackType);
        if(err)
            break;

        if(!IParser->getTrackType){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_TRACK_DURATION, (void **)&IParser->getTrackDuration);
        if(err)
            break;
        if(!IParser->getTrackDuration){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_LANGUAGE, (void **)&IParser->getLanguage);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_BITRATE, (void **)&IParser->getBitRate);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_DECODER_SPECIFIC_INFO, (void **)&IParser->getDecoderSpecificInfo);
        if(err)
            break;

        //video properties
        err = myQueryInterface(PARSER_API_GET_VIDEO_FRAME_WIDTH, (void **)&IParser->getVideoFrameWidth);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_VIDEO_FRAME_HEIGHT, (void **)&IParser->getVideoFrameHeight);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_VIDEO_FRAME_RATE, (void **)&IParser->getVideoFrameRate);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_VIDEO_FRAME_ROTATION, (void **)&IParser->getVideoFrameRotation);
        if(err){
            IParser->getVideoFrameRotation = NULL;
            err = PARSER_SUCCESS;
        }

        //audio properties
        err = myQueryInterface(PARSER_API_GET_AUDIO_NUM_CHANNELS, (void **)&IParser->getAudioNumChannels);
        if(err)
            break;
        if(!IParser->getAudioNumChannels){
            err = PARSER_ERR_INVALID_API;
            break;
        }
        err = myQueryInterface(PARSER_API_GET_AUDIO_SAMPLE_RATE, (void **)&IParser->getAudioSampleRate);
        if(err)
            break;
        if(!IParser->getAudioSampleRate){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_GET_AUDIO_BITS_PER_SAMPLE, (void **)&IParser->getAudioBitsPerSample);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_AUDIO_BLOCK_ALIGN, (void **)&IParser->getAudioBlockAlign);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_AUDIO_CHANNEL_MASK, (void **)&IParser->getAudioChannelMask);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_AUDIO_BITS_PER_FRAME, (void **)&IParser->getAudioBitsPerFrame);
        if(err)
            break;

        //subtitle properties
        err = myQueryInterface(PARSER_API_GET_TEXT_TRACK_WIDTH, (void **)&IParser->getTextTrackWidth);
        if(err)
            break;
        err = myQueryInterface(PARSER_API_GET_TEXT_TRACK_HEIGHT, (void **)&IParser->getTextTrackHeight);
        if(err)
            break;

        //track reading function
        err = myQueryInterface(PARSER_API_GET_READ_MODE, (void **)&IParser->getReadMode);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_SET_READ_MODE, (void **)&IParser->setReadMode);
        if(err)
            break;

        if(!IParser->getReadMode || !IParser->setReadMode){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_ENABLE_TRACK, (void **)&IParser->enableTrack);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_NEXT_SAMPLE, (void **)&IParser->getNextSample);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_NEXT_SYNC_SAMPLE, (void **)&IParser->getNextSyncSample);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_FILE_NEXT_SAMPLE, (void **)&IParser->getFileNextSample);
        if(err)
            break;

        err = myQueryInterface(PARSER_API_GET_FILE_NEXT_SYNC_SAMPLE, (void **)&IParser->getFileNextSyncSample);
        if(err)
            break;

        if(!IParser->getNextSample && !IParser->getFileNextSample){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        if(IParser->getFileNextSample && !IParser->enableTrack){
            err = PARSER_ERR_INVALID_API;
            break;
        }

        err = myQueryInterface(PARSER_API_SEEK, (void **)&IParser->seek);
        if(err)
            break;
        if(!IParser->seek){
            err = PARSER_ERR_INVALID_API;
            break;
        }

    }while(0);


    if(err){
        ALOGW("FslExtractor::CreateParserInterface parser err=%d",err);
        ret = UNKNOWN_ERROR;
    }

    if(ret == OK){
        ALOGD("FslExtractor::CreateParserInterface success");
    }else{
        if(mLibHandle)
            dlclose(mLibHandle);
        mLibHandle = NULL;

        if(IParser != NULL)
            delete IParser;
        IParser = NULL;
        ALOGW("FslExtractor::CreateParserInterface failed,ret=%d",ret);
    }

    return ret;
}
status_t FslExtractor::ParseFromParser()
{
    int32 err = (int32)PARSER_SUCCESS;
    uint32 flag = FLAG_H264_NO_CONVERT | FLAG_OUTPUT_PTS | FLAG_ID3_FORMAT_NON_UTF8;

    uint32 trackCnt = 0;
    bool bLive = mReader->isLiveStreaming();
    ALOGI("Core parser %s \n", IParser->getVersionInfo());

    if(IParser->createParser2){
        if(bLive){
            flag |= FILE_FLAG_NON_SEEKABLE;
            flag |= FILE_FLAG_READ_IN_SEQUENCE;
        }
        err = IParser->createParser2(flag,
                &fileOps,
                &memOps,
                &outputBufferOps,
                (void *)mReader,
                &parserHandle);
        ALOGD("createParser2 flag=%x,err=%d\n",flag,err);
    }else{
        err = IParser->createParser(bLive,
                &fileOps,
                &memOps,
                &outputBufferOps,
                (void *)mReader,
                &parserHandle);
        ALOGD("createParser flag=%x,err=%d\n",flag,err);
    }

    if(PARSER_SUCCESS !=  err)
    {
        ALOGE("fail to create the parser: %d\n", err);
        return UNKNOWN_ERROR;
    }
    if(mReader->isStreaming() || !strcasecmp(mMime, MEDIA_MIMETYPE_CONTAINER_MPEG2TS)
        || !strcasecmp(mMime, MEDIA_MIMETYPE_CONTAINER_MPEG2PS))
        mReadMode = PARSER_READ_MODE_FILE_BASED;
    else
        mReadMode = PARSER_READ_MODE_TRACK_BASED;

    err = IParser->setReadMode(parserHandle, mReadMode);
    if(PARSER_SUCCESS != err)
    {
        ALOGW("fail to set read mode to track mode\n");
        mReadMode = PARSER_READ_MODE_FILE_BASED;
        err = IParser->setReadMode(parserHandle, mReadMode);
        if(PARSER_SUCCESS != err)
        {
            ALOGE("fail to set read mode to file mode\n");
            return UNKNOWN_ERROR;
        }
    }

    if ((NULL == IParser->getNextSample && PARSER_READ_MODE_TRACK_BASED == mReadMode)
            || (NULL == IParser->getFileNextSample && PARSER_READ_MODE_FILE_BASED == mReadMode)){
        ALOGE("get next sample did not exist");
        return UNKNOWN_ERROR;
    }

    err = IParser->getNumTracks(parserHandle, &trackCnt);
    if(err)
        return UNKNOWN_ERROR;

    mNumTracks = trackCnt;
    if(IParser->initializeIndex){
        err = IParser->initializeIndex(parserHandle);
    }

    ALOGI("mReadMode=%d,mNumTracks=%u",mReadMode,mNumTracks);
    err = IParser->isSeekable(parserHandle,(bool *)&bSeekable);
    if(err)
        return UNKNOWN_ERROR;

    ALOGI("bSeekable %d", bSeekable);

    err = IParser->getMovieDuration(parserHandle, (uint64 *)&mMovieDuration);
    if(err)
        return UNKNOWN_ERROR;

    err = ParseMetaData();
    if(err)
        return UNKNOWN_ERROR;

    err = ParseMediaFormat();
    if(err)
        return UNKNOWN_ERROR;
    return OK;
}
status_t FslExtractor::ParseMetaData()
{
    struct KeyMap {
        uint32_t tag;
        UserDataID key;
    };
    const KeyMap kKeyMap[] = {
        { kKeyTitle, USER_DATA_TITLE },
        { kKeyGenre, USER_DATA_GENRE },
        { kKeyArtist, USER_DATA_ARTIST },
        { kKeyYear, USER_DATA_YEAR },
        { kKeyAlbum, USER_DATA_ALBUM },
        { kKeyComposer, USER_DATA_COMPOSER },
        { kKeyWriter, USER_DATA_MOVIEWRITER },
        { kKeyCDTrackNumber, USER_DATA_TRACKNUMBER },
        { kKeyLocation, USER_DATA_LOCATION},
        //{ (char *)"totaltracknumber", USER_DATA_TOTALTRACKNUMBER},
        { kKeyDiscNumber, USER_DATA_DISCNUMBER},
        { kKeyDate, USER_DATA_CREATION_DATE},
        { kKeyCompilation, USER_DATA_COMPILATION},
        { kKeyAlbumArtist, USER_DATA_ALBUMARTIST},
        { kKeyAuthor, USER_DATA_AUTHOR},
        { kKeyEncoderDelay,USER_DATA_AUD_ENC_DELAY},
        { kKeyEncoderPadding,USER_DATA_AUD_ENC_PADDING},
    };
    uint32_t kNumMapEntries = sizeof(kKeyMap) / sizeof(kKeyMap[0]);

    if (IParser->getMetaData){

        uint8 *metaData = NULL;
        uint32 metaDataSize = 0;
        UserDataFormat userDataFormat;

        for (uint32_t i = 0; i < kNumMapEntries; ++i) {
            userDataFormat = USER_DATA_FORMAT_UTF8;
            IParser->getMetaData(parserHandle, kKeyMap[i].key, &userDataFormat, &metaData, \
                &metaDataSize);

            if((metaData != NULL) && ((int)metaDataSize > 0) && USER_DATA_FORMAT_UTF8 == userDataFormat)
            {
                if(metaDataSize > MAX_USER_DATA_STRING_LENGTH)
                    metaDataSize = MAX_USER_DATA_STRING_LENGTH;

                mFileMetaData->setCString(kKeyMap[i].tag, (const char*)metaData);
                ALOGI("FslParser Key: %d\t format=%d,size=%d,Value: %s\n",
                    kKeyMap[i].key,userDataFormat,(int)metaDataSize,metaData);
            }else if((metaData != NULL) && ((int)metaDataSize > 0) && USER_DATA_FORMAT_INT_LE == userDataFormat){
                mFileMetaData->setInt32(kKeyMap[i].tag, *(int*)metaData);
                ALOGI("FslParser Key2: %d\t format=%d,size=%d,Value: %d\n",
                    kKeyMap[i].key,userDataFormat,(int)metaDataSize,*(int*)metaData);
            }
        }

        //capture fps
        userDataFormat = USER_DATA_FORMAT_FLOAT32_BE;
        IParser->getMetaData(parserHandle, USER_DATA_CAPTURE_FPS, &userDataFormat, &metaData, \
        &metaDataSize);
        if(4 == metaDataSize && metaData){
            char tmp[20] = {0};
            uint32 len = 0;
            uint32 value= 0;
            float data = 0.0;
            value += *metaData << 24;
            value += *(metaData+1) << 16;
            value += *(metaData+2) << 8;
            value += *(metaData+3);
            data = *(float *)&value;
            len = sprintf((char*)&tmp, "%f", data);
            ALOGI("get fps=%s,len=%u",tmp,len);
            mFileMetaData->setFloat(kKeyCaptureFramerate, data);
        }

        userDataFormat = USER_DATA_FORMAT_JPEG;
        IParser->getMetaData(parserHandle, USER_DATA_ARTWORK, &userDataFormat, &metaData, \
            &metaDataSize);
        if(metaData && metaDataSize)
        {
            mFileMetaData->setData(kKeyAlbumArt, MetaData::TYPE_NONE,metaData, metaDataSize);

        }

        metaData = NULL;
        metaDataSize = 0;
        //pssh
        userDataFormat = USER_DATA_FORMAT_UTF8;
        IParser->getMetaData(parserHandle, USER_DATA_PSSH, &userDataFormat, &metaData, \
            &metaDataSize);
        if(metaData && metaDataSize)
        {
            mFileMetaData->setData(kKeyPssh, 'pssh', metaData, metaDataSize);
        }
    }
    return OK;
}
status_t FslExtractor::ParseMediaFormat()
{
    uint32 trackCountToCheck = mNumTracks;
    uint32 programCount = 0;
    uint32 trackCountInOneProgram = 0;
    uint32_t index=0;
    uint32_t i = 0;
    uint32_t defaultProgram = 0;
    uint32 * pProgramTrackTable = NULL;
    int32 err = (int32)PARSER_SUCCESS;
    status_t ret = OK;
    MediaType trackType;
    uint32 decoderType;
    uint32 decoderSubtype;
    uint64 sSeekPosTmp = 0;
    ALOGD("FslExtractor::ParseMediaFormat BEGIN");
    if(IParser->getNumPrograms && IParser->getProgramTracks){
        err = IParser->getNumPrograms(parserHandle, &programCount);
        if(PARSER_SUCCESS !=  err || programCount == 0)
            return UNKNOWN_ERROR;

        err = IParser->getProgramTracks(parserHandle, defaultProgram, &trackCountInOneProgram, &pProgramTrackTable);
        if(PARSER_SUCCESS !=  err || trackCountInOneProgram == 0 || pProgramTrackTable == 0)
            return UNKNOWN_ERROR;

        trackCountToCheck = trackCountInOneProgram;
    }

    for(index = 0; index < trackCountToCheck; index ++){
        if(pProgramTrackTable)
            i = pProgramTrackTable[index];
        else
            i = index;

        err = IParser->getTrackType(  parserHandle,i,(uint32 *)&trackType,&decoderType,&decoderSubtype);
        if(err)
            continue;

        if(trackType == MEDIA_VIDEO)
            ret = ParseVideo(i,decoderType,decoderSubtype);
        else if(trackType == MEDIA_AUDIO)
            ret = ParseAudio(i,decoderType,decoderSubtype);
        else if(trackType == MEDIA_TEXT)
            ret = ParseText(i,decoderType,decoderSubtype);

        if(ret)
            continue;


        err = IParser->seek(parserHandle, i, &sSeekPosTmp, SEEK_FLAG_NO_LATER);
        if(err)
            return UNKNOWN_ERROR;
    }

    return OK;
}
status_t FslExtractor::ParseVideo(uint32 index, uint32 type,uint32 subtype)
{
    int32 err = (int32)PARSER_SUCCESS;
    uint32_t i = 0;
    const char* mime = NULL;
    uint64_t duration = 0;
    uint8 * decoderSpecificInfo = NULL;
    uint32 decoderSpecificInfoSize = 0;
    uint32 width = 0;
    uint32 height = 0;
    uint32 rotation = 0;
    uint32 rate = 0;
    uint32 scale = 0;
    uint32 fps = 0;
    uint32 bitrate = 0;
    size_t sourceIndex = 0;
    size_t max_size = 0;
    int64_t thumbnail_ts = -1;
    ALOGD("ParseVideo index=%u,type=%u,subtype=%u",index,type,subtype);
    for(i = 0; i < sizeof(video_mime_table)/sizeof(codec_mime_struct); i++){
        if (type == video_mime_table[i].type){
            if((video_mime_table[i].subtype > 0) && (subtype == (video_mime_table[i].subtype))){
                mime = video_mime_table[i].mime;
                break;
            }else if(video_mime_table[i].subtype == 0){
                mime = video_mime_table[i].mime;
                break;
            }
        }
    }

    if(mime == NULL)
        return UNKNOWN_ERROR;

    err = IParser->getTrackDuration(parserHandle, index,&duration);
    if(err)
        return UNKNOWN_ERROR;

    if(IParser->getDecoderSpecificInfo){
        err = IParser->getDecoderSpecificInfo(parserHandle, index, &decoderSpecificInfo, &decoderSpecificInfoSize);
        if(err)
            return UNKNOWN_ERROR;
    }
    err = IParser->getBitRate(parserHandle, index, &bitrate);
    if(err)
        return UNKNOWN_ERROR;

    err = IParser->getVideoFrameWidth(parserHandle, index, &width);
    if(err){
        return UNKNOWN_ERROR;
    }

    err = IParser->getVideoFrameHeight(parserHandle, index, &height);
    if(err){
        return UNKNOWN_ERROR;
    }

    err = IParser->getVideoFrameRate(parserHandle, index, &rate, &scale);
    if(err){
        return UNKNOWN_ERROR;
    }

    if(rate > 0 && scale > 0)
        fps = rate/scale;

    if(fps > 250)
        fps = 0;

    if(IParser->getVideoFrameRotation){
        err = IParser->getVideoFrameRotation(parserHandle,index,&rotation);
        if(err){
            return UNKNOWN_ERROR;
        }
    }
    ALOGI("ParseVideo width=%u,height=%u,fps=%u,rotate=%u",width,height,fps,rotation);

    sp<MetaData> meta = new MetaData;
    meta->setCString(kKeyMIMEType, mime);
    meta->setInt32(kKeyTrackID, index);

    if(decoderSpecificInfoSize > 0 && decoderSpecificInfo != NULL){
        ALOGI("video codec data size=%u",decoderSpecificInfoSize);
        if(type == VIDEO_H264){
            meta->setData(kKeyAVCC, 0, decoderSpecificInfo, decoderSpecificInfoSize);
            ALOGI("add avcc metadata for h264 video size=%u",decoderSpecificInfoSize);
        }else if(type == VIDEO_HEVC){
            //stagefright will check the first bytes, so modify to pass it.
            if(decoderSpecificInfo[0] != 1)
                decoderSpecificInfo[0] = 1;
            meta->setData(kKeyHVCC, 0, decoderSpecificInfo, decoderSpecificInfoSize);
            ALOGI("add hvcc metadata for hevc video size=%u",decoderSpecificInfoSize);
        }else if(type == VIDEO_MPEG4){
            addESDSFromCodecPrivate(meta,false,decoderSpecificInfo,decoderSpecificInfoSize);
            ALOGI("add esds metadata for mpeg4 video size=%u",decoderSpecificInfoSize);
        }else{
            meta->setData(kKeyCodecData, 0, decoderSpecificInfo, decoderSpecificInfoSize);
        }
    }else if(type == VIDEO_H264 || type == VIDEO_HEVC || type == VIDEO_MPEG4){
        thumbnail_ts = 0;
    }

    if(!strcmp(mMime, MEDIA_MIMETYPE_CONTAINER_MPEG2TS) || !strcmp(mMime, MEDIA_MIMETYPE_CONTAINER_MPEG2PS))
        thumbnail_ts = -1;

    if(0 == thumbnail_ts){
        meta->setInt32(kKeySpecialThumbnail, 1);
        thumbnail_ts = -1;
    }

    if(!strcmp(mMime, MEDIA_MIMETYPE_CONTAINER_FLV))
        thumbnail_ts = 0;

    if (type == VIDEO_H264) {
        // AVC requires compression ratio of at least 2, and uses
        // macroblocks
        max_size = ((width + 15) / 16) * ((height + 15) / 16) * 192;
    } else {
        // For all other formats there is no minimum compression
        // ratio. Use compression ratio of 1.
        max_size = width * height * 3 / 2;
    }
    if(0 == max_size)
        max_size = MAX_VIDEO_BUFFER_SIZE;
    max_size += 20;

    meta->setInt32(kKeyMaxInputSize, max_size);

    if(type == VIDEO_WMV){
        int32_t wmvType = 0;
        switch(subtype){
            case VIDEO_WMV7:
                wmvType = OMX_VIDEO_WMVFormat7;
                break;
            case VIDEO_WMV8:
                wmvType = OMX_VIDEO_WMVFormat8;
                break;
            case VIDEO_WMV9:
                wmvType = OMX_VIDEO_WMVFormat9;
                break;
            case VIDEO_WMV9A:
                wmvType = OMX_VIDEO_WMVFormat9a;
                break;
            case VIDEO_WVC1:
                wmvType = OMX_VIDEO_WMVFormatWVC1;
                break;
            default:
                break;
        }
        meta->setInt32(kKeySubFormat, wmvType);
    }

    meta->setInt32(kKeyBitRate, bitrate);
    meta->setInt32(kKeyWidth, width);
    meta->setInt32(kKeyHeight, height);
    meta->setInt64(kKeyDuration, duration);

    if(fps > 0)
        meta->setInt32(kKeyFrameRate, fps);
    if(rotation > 0)
        meta->setInt32(kKeyRotation, rotation);

    if(thumbnail_ts < 0)
        thumbnail_ts = duration > 5000000 ? 5000000:duration;

    meta->setInt64(kKeyThumbnailTime, thumbnail_ts);


    mTracks.push();
    sourceIndex = mTracks.size() - 1;
    TrackInfo *trackInfo = &mTracks.editItemAt(sourceIndex);
    trackInfo->mTrackNum = index;
    trackInfo->mExtractor = this;
    trackInfo->bCodecInfoSent = false;
    trackInfo->bPartial = false;
    trackInfo->buffer = NULL;
    trackInfo->outTs = 0;
    trackInfo->syncFrame = 0;
    trackInfo->mMeta = meta;
    trackInfo->mSource = NULL;
    trackInfo->max_input_size = max_size;
    trackInfo->type = MEDIA_VIDEO;
    trackInfo->bIsNeedConvert = false;
    mReader->AddBufferReadLimitation(index,max_size);

    ALOGI("add video track index=%u,source index=%zu,mime=%s",index,sourceIndex,mime);
    return OK;
}

status_t FslExtractor::ParseAudio(uint32 index, uint32 type,uint32 subtype)
{
    int32 err = (int32)PARSER_SUCCESS;
    uint32_t i = 0;
    uint32 bitrate = 0;
    uint32 channel = 0;
    uint32 samplerate = 0;
    uint32 bitPerSample = 0;
    uint32 bitsPerFrame = 0;
    uint32 audioBlockAlign = 0;
    uint32 audioChannelMask = 0;
    const char* mime = NULL;
    uint64_t duration = 0;
    uint8 * decoderSpecificInfo = NULL;
    uint32 decoderSpecificInfoSize = 0;
    uint8 language[8];
    size_t sourceIndex = 0;
    int32_t encoderDelay = 0;
    int32_t encoderPadding = 0;
    ALOGD("ParseAudio index=%u,type=%u,subtype=%u",index,type,subtype);
    for(i = 0; i < sizeof(audio_mime_table)/sizeof(codec_mime_struct); i++){
        if (type == audio_mime_table[i].type){
            if((audio_mime_table[i].subtype > 0) && (subtype == (audio_mime_table[i].subtype))){
                mime = audio_mime_table[i].mime;
                break;
            }else if(audio_mime_table[i].subtype == 0){
                mime = audio_mime_table[i].mime;
                break;
            }
        }
    }

    if(mime == NULL)
        return UNKNOWN_ERROR;

    err = IParser->getTrackDuration(parserHandle, index,&duration);
    if(err)
        return UNKNOWN_ERROR;

    if(IParser->getDecoderSpecificInfo){
        err = IParser->getDecoderSpecificInfo(parserHandle, index, &decoderSpecificInfo, &decoderSpecificInfoSize);
        if(err)
            return UNKNOWN_ERROR;
    }

    err = IParser->getBitRate(parserHandle, index, &bitrate);
    if(err)
        return UNKNOWN_ERROR;

    err = IParser->getAudioNumChannels(parserHandle, index, &channel);
    if(err)
        return UNKNOWN_ERROR;

    err = IParser->getAudioSampleRate(parserHandle, index, &samplerate);
    if(err)
        return UNKNOWN_ERROR;

    if(IParser->getAudioBitsPerSample){
        err = IParser->getAudioBitsPerSample(parserHandle, index, &bitPerSample);
        if(err)
            return UNKNOWN_ERROR;
    }
    if(IParser->getAudioBitsPerFrame){
        err = IParser->getAudioBitsPerFrame(parserHandle, index, &bitsPerFrame);
        if(err)
            return UNKNOWN_ERROR;
    }

    if(IParser->getAudioBlockAlign){
        err = IParser->getAudioBlockAlign(parserHandle, index, &audioBlockAlign);//wma & adpcm
        if(err)
            return UNKNOWN_ERROR;
    }

    if(IParser->getAudioChannelMask){
        err = IParser->getAudioChannelMask(parserHandle, index, &audioChannelMask);//not use
        if(err)
            return UNKNOWN_ERROR;
    }
    if(IParser->getLanguage) {
        err = IParser->getLanguage(parserHandle, index, &language[0]);
        ALOGI("audio track %u, lanuage: %s\n", index, language);
    }
    else
        strcpy((char*)&language, "unknown");

    sp<MetaData> meta = new MetaData;

    const char *containerMime;
    mFileMetaData->findCString(kKeyMIMEType, &containerMime);
    if(type == AUDIO_AAC && !strcmp(containerMime, MEDIA_MIMETYPE_CONTAINER_MPEG4)){
        int64_t fileSize = 0;
        mDataSource->getSize(&fileSize);

        // workaround for MA-8032, set isForceUseGoogleAACCodec to true, force to use google codecs,
        // to pass testDecodeM4a, testCodecResetsM4a & testAudioOnly, after codec is loaded, reset it to false.
        if(fileSize == 60053 && samplerate == 44100 && bitrate == 256000 && channel == 2)
           isForceUseGoogleAACCodec = true;
        else if(fileSize == 55118 && samplerate == 44100 && bitrate == 96000 && channel == 2)
           isForceUseGoogleAACCodec = true;
    }
    meta->setCString(kKeyMIMEType, mime);
    meta->setInt32(kKeyTrackID, index);

    if(decoderSpecificInfoSize > 0 && decoderSpecificInfo != NULL){
        ALOGI("audio codec data size=%u",decoderSpecificInfoSize);
        if(type == AUDIO_AAC){
            addESDSFromCodecPrivate(meta,true,decoderSpecificInfo,decoderSpecificInfoSize);
            ALOGI("add esds metadata for aac audio size=%u",decoderSpecificInfoSize);
        }else if(type == AUDIO_VORBIS){
            //TODO:
            //meta->setData(kKeyVorbisInfo, 0, decoderSpecificInfo, decoderSpecificInfoSize);
            if(OK != addVorbisCodecInfo(meta,decoderSpecificInfo,decoderSpecificInfoSize))
                ALOGE("add vorbis codec info error");
        }else if(type == AUDIO_OPUS){
            int64_t defaultValue = 0;
            meta->setData(kKeyOpusHeader, 0, decoderSpecificInfo, decoderSpecificInfoSize);
            meta->setInt64(kKeyOpusCodecDelay, defaultValue);
            meta->setInt64(kKeyOpusSeekPreRoll, defaultValue);
        }else{
            meta->setData(kKeyCodecData, 0, decoderSpecificInfo, decoderSpecificInfoSize);
        }
    }

    if(type == AUDIO_PCM && (subtype == AUDIO_PCM_S16BE || subtype == AUDIO_PCM_S24BE || subtype == AUDIO_PCM_S32BE ))
        meta->setInt32(kKeyIsEndianBig, 1);

    size_t max_size = MAX_AUDIO_BUFFER_SIZE;//16*1024
    if(type == AUDIO_APE) {
        max_size = 262144; //enlarge buffer size to 256*1024 for ape audio
        meta->setInt32(kKeyMaxInputSize, max_size);
    }

    if(type == AUDIO_WMA){
        int32_t wmaType = 0;
        switch(subtype){
            case AUDIO_WMA1:
                wmaType = OMX_AUDIO_WMAFormat7;
                break;
            case AUDIO_WMA2:
                wmaType = OMX_AUDIO_WMAFormat8;
                break;
            case AUDIO_WMA3:
                wmaType = OMX_AUDIO_WMAFormat9;
                break;
            case AUDIO_WMALL:
                wmaType = OMX_AUDIO_WMAFormatLL;
                break;
            default:
                break;
        }
        meta->setInt32(kKeySubFormat, wmaType);
        ALOGI("WMA subtype=%u",wmaType);
    }

    if(bitPerSample > 0)
        meta->setInt32(kKeyBitPerSample,bitPerSample);
    if(audioBlockAlign > 0)
        meta->setInt32(kKeyAudioBlockAlign,audioBlockAlign);
    if(bitsPerFrame > 0)
        meta->setInt32(kKeyBitsPerFrame,bitsPerFrame);
    if(type == AUDIO_AAC) {
        if(subtype == AUDIO_AAC_ADTS)
            meta->setInt32(kKeyIsADTS, true);
        else if (subtype == AUDIO_AAC_ADIF)
            meta->setInt32(kKeyIsADIF, true);
    }else if(bitrate > 0)
        meta->setInt32(kKeyBitRate, bitrate);

    if(type == AUDIO_AMR){
        if(subtype == AUDIO_AMR_NB){
            channel = 1;
            samplerate = 8000;
        }else if(subtype == AUDIO_AMR_WB){
            channel = 1;
            samplerate = 16000;
        }
    }

    if(type == AUDIO_AC3 && samplerate == 0)
        samplerate = 44100; // invalid samplerate will lead to findMatchingCodecs fail

    meta->setInt64(kKeyDuration, duration);
    meta->setInt32(kKeyChannelCount, channel);
    meta->setInt32(kKeySampleRate, samplerate);
    meta->setCString(kKeyMediaLanguage, (const char*)&language);

    if(mFileMetaData->findInt32(kKeyEncoderDelay,&encoderDelay))
        meta->setInt32(kKeyEncoderDelay, encoderDelay);
    if(mFileMetaData->findInt32(kKeyEncoderPadding,&encoderPadding))
        meta->setInt32(kKeyEncoderPadding, encoderPadding);

#if 0//test
    if(type == AUDIO_MP3) {
        meta->setInt32(kKeyEncoderDelay, 576);
        meta->setInt32(kKeyEncoderPadding, 1908);

    }
#endif
    ALOGI("ParseAudio channel=%d,sampleRate=%d,bitRate=%d,bitPerSample=%d,audioBlockAlign=%d",
        (int)channel,(int)samplerate,(int)bitrate,(int)bitPerSample,(int)audioBlockAlign);
    mTracks.push();
    sourceIndex = mTracks.size() - 1;
    TrackInfo *trackInfo = &mTracks.editItemAt(sourceIndex);
    trackInfo->mTrackNum = index;
    trackInfo->mExtractor = this;
    trackInfo->bCodecInfoSent = false;
    trackInfo->mMeta = meta;
    trackInfo->bPartial = false;
    trackInfo->buffer = NULL;
    trackInfo->outTs = 0;
    trackInfo->syncFrame = 0;
    trackInfo->mSource = NULL;
    trackInfo->max_input_size = max_size;
    trackInfo->type = MEDIA_AUDIO;
    trackInfo->bIsNeedConvert = (type == AUDIO_PCM && bitPerSample!= 16);
    mReader->AddBufferReadLimitation(index,max_size);
    ALOGI("add audio track index=%u,sourceIndex=%zu,mime=%s",index,sourceIndex,mime);
    return OK;
}
status_t FslExtractor::ParseText(uint32 index, uint32 type,uint32 subtype)
{

    int32 err = (int32)PARSER_SUCCESS;
    uint8 language[8];
    uint32 width = 0;
    uint32 height = 0;
    const char* mime = NULL;
    ALOGD("ParseText index=%u,type=%u,subtype=%u",index,type,subtype);
    switch(type){
        case TXT_3GP_STREAMING_TEXT:
        case TXT_QT_TEXT:
            mime = MEDIA_MIMETYPE_TEXT_3GPP;
            break;
        case TXT_SUBTITLE_TEXT:
            mime = MEDIA_MIMETYPE_TEXT_SRT;
            break;
        case TXT_SUBTITLE_SSA:
            mime = MEDIA_MIMETYPE_TEXT_SSA;
            break;
        case TXT_SUBTITLE_ASS:
            mime = MEDIA_MIMETYPE_TEXT_ASS;
            break;
        default:
            break;
    }
    if(mime == NULL)
        return UNKNOWN_ERROR;

    err = IParser->getTextTrackWidth(parserHandle,index,&width);
    if(err)
        return UNKNOWN_ERROR;

    err = IParser->getTextTrackHeight(parserHandle,index,&height);
    if(err)
        return UNKNOWN_ERROR;

    if(IParser->getLanguage) {
        err = IParser->getLanguage(parserHandle, index, &language[0]);
        ALOGI("test track %u, lanuage: %s\n", index, language);
    }
    else
        strcpy((char*)&language, "unknown");

    sp<MetaData> meta = new MetaData;
    meta->setCString(kKeyMIMEType, mime);
    meta->setInt32(kKeyWidth, width);
    meta->setInt32(kKeyHeight, height);
    meta->setCString(kKeyMediaLanguage, (const char*)&language);

    mTracks.push();
    TrackInfo *trackInfo = &mTracks.editItemAt(mTracks.size() - 1);
    trackInfo->mTrackNum = index;
    trackInfo->mExtractor = this;
    trackInfo->bCodecInfoSent = false;
    trackInfo->mMeta = meta;
    trackInfo->bPartial = false;
    trackInfo->buffer = NULL;
    trackInfo->outTs = 0;
    trackInfo->syncFrame = 0;
    trackInfo->mSource = NULL;
    trackInfo->max_input_size = MAX_TEXT_BUFFER_SIZE;
    trackInfo->type = MEDIA_TEXT;
    trackInfo->bIsNeedConvert = false;
    mReader->AddBufferReadLimitation(index,MAX_TEXT_BUFFER_SIZE);
    ALOGD("add text track");
    return OK;
}
int FslExtractor::bytesForSize(size_t size) {
    // use at most 28 bits (4 times 7)
    CHECK(size <= 0xfffffff);

    if (size > 0x1fffff) {
        return 4;
    } else if (size > 0x3fff) {
        return 3;
    } else if (size > 0x7f) {
        return 2;
    }
    return 1;
}
void FslExtractor::storeSize(uint8_t *data, size_t &idx, size_t size) {
    int numBytes = bytesForSize(size);
    idx += numBytes;

    data += idx;
    size_t next = 0;
    while (numBytes--) {
        *--data = (size & 0x7f) | next;
        size >>= 7;
        next = 0x80;
    }
}
void FslExtractor::addESDSFromCodecPrivate(
        const sp<MetaData> &meta,
        bool isAudio, const void *priv, size_t privSize) {

    int privSizeBytesRequired = bytesForSize(privSize);
    int esdsSize2 = 14 + privSizeBytesRequired + privSize;
    int esdsSize2BytesRequired = bytesForSize(esdsSize2);
    int esdsSize1 = 4 + esdsSize2BytesRequired + esdsSize2;
    int esdsSize1BytesRequired = bytesForSize(esdsSize1);
    size_t esdsSize = 1 + esdsSize1BytesRequired + esdsSize1;
    uint8_t *esds = new uint8_t[esdsSize];

    size_t idx = 0;
    esds[idx++] = 0x03;
    storeSize(esds, idx, esdsSize1);
    esds[idx++] = 0x00; // ES_ID
    esds[idx++] = 0x00; // ES_ID
    esds[idx++] = 0x00; // streamDependenceFlag, URL_Flag, OCRstreamFlag
    esds[idx++] = 0x04;
    storeSize(esds, idx, esdsSize2);
    esds[idx++] = isAudio ? 0x40   // Audio ISO/IEC 14496-3
                          : 0x20;  // Visual ISO/IEC 14496-2
    for (int i = 0; i < 12; i++) {
        esds[idx++] = 0x00;
    }
    esds[idx++] = 0x05;
    storeSize(esds, idx, privSize);
    memcpy(esds + idx, priv, privSize);

    meta->setData(kKeyESDS, 0, esds, esdsSize);

    delete[] esds;
    esds = NULL;

}
status_t FslExtractor::addVorbisCodecInfo(
        const sp<MetaData> &meta,
        const void *_codecPrivate, size_t codecPrivateSize) {

    size_t offset = 0;
    int32_t start1 = 0;
    int32_t start2 = 0;
    int32_t start3 = 0;

    if(_codecPrivate == NULL || codecPrivateSize < 6)
        return ERROR_MALFORMED;

    const uint8_t *ptr = (const uint8_t *)_codecPrivate;

    while(offset < codecPrivateSize-6){
        if((ptr[offset] == 0x01) && (memcmp(&ptr[offset+1],"vorbis",6)==0)){
            start1 = offset;
        }else if((ptr[offset] == 0x03) && (memcmp(&ptr[offset+1],"vorbis",6)==0)){
            start2 = offset;
        }else if((ptr[offset] == 0x05) && (memcmp(&ptr[offset+1],"vorbis",6)==0)){
            start3 = offset;
        }
        offset ++;
    }

    if(!(start2 > start1 && start3 > start2))
        return ERROR_MALFORMED;

    meta->setData(kKeyVorbisInfo, 0, &ptr[start1], start2 - start1);

    meta->setData(
            kKeyVorbisBooks, 0, &ptr[start3],
            codecPrivateSize - start3);

    ALOGI("addVorbisCodecInfo SUCCESS");
    return OK;
}
status_t FslExtractor::ActiveTrack(uint32 index)
{
    uint64 seekPos = 0;
    Mutex::Autolock autoLock(mLock);
    bool seek = true;

    TrackInfo *trackInfo = &mTracks.editItemAt(index);
    if(trackInfo == NULL)
        return UNKNOWN_ERROR;
    trackInfo->bCodecInfoSent = false;
    if(trackInfo->type == MEDIA_VIDEO){
        seekPos = currentVideoTs;
        mVideoActived = true;
    }else if(trackInfo->type == MEDIA_AUDIO)
        seekPos = currentAudioTs;
    else if(currentVideoTs > 0)
        seekPos = currentVideoTs;
    else
        seekPos = currentAudioTs;

    IParser->enableTrack(parserHandle,trackInfo->mTrackNum, TRUE);

    if(trackInfo->type == MEDIA_TEXT || trackInfo->type == MEDIA_AUDIO){
        if(isTrackModeParser())
            seek = true;
        else
            seek = false;
    }

    if(seek)
        IParser->seek(parserHandle, trackInfo->mTrackNum, &seekPos, SEEK_FLAG_NO_LATER);

    ALOGD("start track %d",trackInfo->mTrackNum);
    return OK;
}
status_t FslExtractor::DisableTrack(uint32 index)
{
    Mutex::Autolock autoLock(mLock);
    TrackInfo *trackInfo = &mTracks.editItemAt(index);
    if(trackInfo == NULL)
        return UNKNOWN_ERROR;

    if(trackInfo->type == MEDIA_VIDEO){
        mVideoActived = false;
    }

    IParser->enableTrack(parserHandle,trackInfo->mTrackNum, FALSE);
    ALOGD("close track %d",trackInfo->mTrackNum);
    return OK;
}
status_t FslExtractor::HandleSeekOperation(uint32_t index,int64_t * ts,uint32_t flag)
{
    TrackInfo *pInfo = NULL;
    int64_t target;
    bool seek = true;
    if(ts == NULL)
        return UNKNOWN_ERROR;

    target = *ts;
    pInfo = &mTracks.editItemAt(index);

    if(pInfo == NULL)
        return UNKNOWN_ERROR;

    if(mReadMode == PARSER_READ_MODE_FILE_BASED){
        if(pInfo->type == MEDIA_AUDIO && mVideoActived){
            seek = false;
            //for track mode parser, it must seek audio track.
            if(isTrackModeParser())
                seek = true;
        }else if(pInfo->type == MEDIA_TEXT && mVideoActived){
            seek = false;
        }
    }

    if(seek){
        IParser->seek(parserHandle, pInfo->mTrackNum, (uint64*)ts, flag);
        //clear temp buffer

        if(pInfo->buffer != NULL){
            pInfo->buffer.clear();
            pInfo->buffer = NULL;
        }
        ALOGD("HandleSeekOperation do seek index=%d",index);
    }

    pInfo->bPartial = false;

    if(pInfo->type == MEDIA_VIDEO)
        currentVideoTs = target;
    else if(pInfo->type == MEDIA_AUDIO)
        currentAudioTs = target;

    ALOGD("HandleSeekOperation index=%d,ts=%lld,flag=%x",index,*ts,flag);
    return OK;
}
status_t FslExtractor::GetNextSample(uint32_t index,bool is_sync)
{
    int32 err = (int32)PARSER_SUCCESS;
    void * buffer_context = NULL;
    uint64 ts = 0;
    uint64 duration = 0;
    uint8 *tmp = NULL;
    uint32 datasize = 0;
    uint32 sampleFlag = 0;
    uint32 track_num_got = 0;
    uint32 direction = 0;

    TrackInfo *pInfo = NULL;
    Mutex::Autolock autoLock(mLock);

    pInfo = &mTracks.editItemAt(index);
    track_num_got = pInfo->mTrackNum;
    pInfo = NULL;

    ALOGV("GetNextSample readmode=%u index=%u BEGIN",mReadMode,track_num_got);
    do{

        if(mReadMode == PARSER_READ_MODE_TRACK_BASED){
            if (is_sync)
            {
                err = IParser->getNextSyncSample(parserHandle,
                        direction,
                        track_num_got,
                        &tmp,
                        &buffer_context,
                        &datasize,
                        (uint64 *)&ts,
                        &duration,
                        (uint32 *)&sampleFlag);
            }
            else
            {
                err = IParser->getNextSample(parserHandle,
                        track_num_got,
                        &tmp,
                        &buffer_context,
                        &datasize,
                        (uint64 *)&ts,
                        &duration,
                        (uint32 *)&sampleFlag);
            }
        }else{
            if (is_sync)
            {
                err = IParser->getFileNextSyncSample(parserHandle,
                        direction,
                        &track_num_got,
                        &tmp,
                        &buffer_context,
                        &datasize,
                        (uint64 *)&ts,
                        &duration,
                        (uint32 *)&sampleFlag);
            }
            else
            {
                err = IParser->getFileNextSample(parserHandle,
                        &track_num_got,
                        &tmp,
                        &buffer_context,
                        &datasize,
                        (uint64 *)&ts,
                        &duration,
                        (uint32 *)&sampleFlag);
            }
        }

        if(PARSER_NOT_READY == err){
            return WOULD_BLOCK;
        }

        ALOGV("GetNextSample err %d get track num=%u ts=%lld,size=%u,flag=%x",err, track_num_got,ts,datasize,sampleFlag);

        if(PARSER_SUCCESS != err){
            if(err == PARSER_READ_ERROR)
                return ERROR_IO;
            else
                return ERROR_END_OF_STREAM;
        }

        pInfo = &mTracks.editItemAt(index);
        if(pInfo->mTrackNum != track_num_got){

            size_t trackCount = mTracks.size();
            for (size_t index = 0; index < trackCount; index++) {
                pInfo = &mTracks.editItemAt(index);
                if(pInfo->mTrackNum == track_num_got)
                    break;
                pInfo = NULL;
            }
            if(pInfo == NULL)
                continue;
        }

        if(tmp && buffer_context) {
            sp<ABuffer> buffer = pInfo->buffer;

            if(sampleFlag & FLAG_SAMPLE_NOT_FINISHED)
                pInfo->bPartial = true;

            if(pInfo->bPartial){

                if(buffer == NULL){
                    buffer = (ABuffer *)buffer_context;
                    pInfo->outTs = ts;
                    pInfo->syncFrame = (sampleFlag & FLAG_SYNC_SAMPLE);
                    buffer->setRange(0,datasize);
                    pInfo->buffer = buffer;
                    ALOGV("bPartial first buffer");
                }else {
                    sp<ABuffer> lastBuf = buffer;
                    sp<ABuffer> currBuf = (ABuffer *)buffer_context;
                    size_t tempLen = lastBuf->size();
                    buffer = new ABuffer(tempLen + (size_t)datasize);
                    memcpy(buffer->data(),lastBuf->data(),tempLen);
                    memcpy(buffer->data()+tempLen,currBuf->data(),currBuf->size());
                    lastBuf.clear();
                    currBuf.clear();
                    pInfo->buffer = buffer;
                    ALOGV("bPartial second buffer,");
                }

                if(!(sampleFlag & FLAG_SAMPLE_NOT_FINISHED))
                    pInfo->bPartial = false;
            }
            else{
                buffer = (ABuffer *)buffer_context;
                pInfo->outTs = ts;
                pInfo->syncFrame = (sampleFlag & FLAG_SYNC_SAMPLE);
                pInfo->buffer = buffer;
                buffer->setRange(0,datasize);
            }
        }else {
            // mpg2 parser often send an empty buffer as the last partial frame.
            if(pInfo->bPartial && !(sampleFlag & FLAG_SAMPLE_NOT_FINISHED))
                pInfo->bPartial = false;
        }

    }while((sampleFlag & FLAG_SAMPLE_NOT_FINISHED) && (pInfo->buffer->size() < pInfo->max_input_size));

    if(pInfo && pInfo->buffer != NULL ){
        sp<FslMediaSource> source = pInfo->mSource;
        bool add = false;
        if(source != NULL  && source->started()){
            add = true;
            if(pInfo->type == MEDIA_AUDIO){
                if(pInfo->outTs >= 0 && pInfo->outTs < currentAudioTs && mVideoActived == true){
                    ALOGV("drop audio after seek ts=%lld,audio_ts=%lld",pInfo->outTs,currentAudioTs);
                    add = false;
                }
            }
        }
        if(add){
            if(pInfo->bIsNeedConvert) {
                int32_t bitPerSample = 16;
                pInfo->mMeta->findInt32(kKeyBitPerSample, &bitPerSample);
                sp<ABuffer> buffer = pInfo->buffer;
                sp<ABuffer> tmp = new ABuffer(2 * buffer->size());
                convertPCMData(buffer, tmp, bitPerSample);
                pInfo->buffer = tmp;
                buffer.clear();
            }

            MediaBuffer *mbuf = new MediaBuffer(pInfo->buffer);
            mbuf->meta_data()->setInt64(kKeyTime, pInfo->outTs);
            mbuf->meta_data()->setInt32(kKeyIsSyncFrame, pInfo->syncFrame);
            ALOGV("addMediaBuffer ts=%lld,size=%d",pInfo->outTs,pInfo->buffer->size());

            source->addMediaBuffer(mbuf);
            if(pInfo->type == MEDIA_VIDEO)
                currentVideoTs = pInfo->outTs;
            else if(pInfo->type == MEDIA_AUDIO)
                currentAudioTs = pInfo->outTs;

        }

        pInfo->buffer.clear();
        pInfo->buffer = NULL;
    }

    //check for get subtitle track in file mode, avoid interleave
    pInfo = &mTracks.editItemAt(index);
    if(pInfo->type == MEDIA_TEXT && pInfo->mTrackNum != track_num_got)
        return WOULD_BLOCK;

    return OK;
}
status_t FslExtractor::CheckInterleaveEos(__unused uint32_t index)
{
    bool bTrackFull = false;

    if(mReadMode == PARSER_READ_MODE_TRACK_BASED)
        return OK;

    if(mTracks.size() < 2)
        return OK;

    for(size_t i = 0; i < mTracks.size(); i++){
        TrackInfo *pInfo = &mTracks.editItemAt(i);
        sp<FslMediaSource> source = pInfo->mSource;
        if(source->started() && source->full()){
            bTrackFull = true;
            ALOGE("get a full track");
            break;
        }
    }
    if(bTrackFull)
        return ERROR_END_OF_STREAM;
    else
        return OK;
}
status_t FslExtractor::ClearTrackSource(uint32_t index)
{
    if (index >= mTracks.size()) {
        return UNKNOWN_ERROR;
    }
    TrackInfo *trackInfo = &mTracks.editItemAt(index);
    if(trackInfo){
        trackInfo->mSource = NULL;
    }
    return OK;
}

bool FslExtractor::isTrackModeParser()
{
    if(!strcmp(mMime, MEDIA_MIMETYPE_CONTAINER_MPEG4) || !strcmp(mMime, MEDIA_MIMETYPE_CONTAINER_AVI))
        return true;
    else
        return false;
}
status_t FslExtractor::convertPCMData(sp<ABuffer> inBuffer, sp<ABuffer> outBuffer, int32_t bitPerSample)
{
    if(bitPerSample == 8) {
        // Convert 8-bit unsigned samples to 16-bit signed.
        ssize_t numBytes = inBuffer->size();

        int16_t *dst = (int16_t *)outBuffer->data();
        const uint8_t *src = (const uint8_t *)inBuffer->data();

        while (numBytes-- > 0) {
            *dst++ = ((int16_t)(*src) - 128) * 256;
            ++src;
        }
        outBuffer->setRange(0, 2 * inBuffer->size());

    }else if (bitPerSample == 24) {
        // Convert 24-bit signed samples to 16-bit signed.
        const uint8_t *src = (const uint8_t *)inBuffer->data();
        int16_t *dst = (int16_t *)outBuffer->data();
        size_t numSamples = inBuffer->size() / 3;
        for (size_t i = 0; i < numSamples; i++) {
            int32_t x = (int32_t)(src[0] | src[1] << 8 | src[2] << 16);
            x = (x << 8) >> 8;  // sign extension
            x = x >> 8;
            *dst++ = (int16_t)x;
            src += 3;
        }
        outBuffer->setRange(0, 2 * numSamples);
    }

    return OK;
}

}// namespace android
