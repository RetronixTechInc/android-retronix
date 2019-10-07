/**
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 */

//#define LOG_NDEBUG 0

#define LOG_TAG "NuPlayerDecoderPassThroughDDP"
#include <utils/Log.h>
#include <inttypes.h>

#include "NuPlayerDecoderPassThroughDDP.h"

#include "NuPlayerRenderer.h"
#include "NuPlayerSource.h"

#include <media/ICrypto.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaDefs.h>

#include "ATSParser.h"

namespace android {

#define AC3_FRAME_HEAD_SIZE  8
#define NFSCOD      3   /* # defined sample rates */
#define NDATARATE   38  /* # defined data rates */
#define AC3D_FRAME_SIZE 1536

static const int16_t ac3_frame_size[NFSCOD][NDATARATE] =
{   {   64, 64, 80, 80, 96, 96, 112, 112,
    128, 128, 160, 160, 192, 192, 224, 224,
    256, 256, 320, 320, 384, 384, 448, 448,
    512, 512, 640, 640, 768, 768, 896, 896,
    1024, 1024, 1152, 1152, 1280, 1280 },
{   69, 70, 87, 88, 104, 105, 121, 122,
    139, 140, 174, 175, 208, 209, 243, 244,
    278, 279, 348, 349, 417, 418, 487, 488,
    557, 558, 696, 697, 835, 836, 975, 976,
    1114, 1115, 1253, 1254, 1393, 1394 },
{   96, 96, 120, 120, 144, 144, 168, 168,
    192, 192, 240, 240, 288, 288, 336, 336,
    384, 384, 480, 480, 576, 576, 672, 672,
    768, 768, 960, 960, 1152, 1152, 1344, 1344,
    1536, 1536, 1728, 1728, 1920, 1920 }
};
NuPlayer::DecoderPassThroughDDP::DecoderPassThroughDDP(
        const sp<AMessage> &notify,
        const sp<Source> &source,
        const sp<Renderer> &renderer)
    : DecoderPassThrough(notify,source,renderer){
    tempBuf = NULL;
    mFrameSize = 0;
    ALOGW_IF(renderer == NULL, "expect a non-NULL renderer");
}
NuPlayer::DecoderPassThroughDDP::~DecoderPassThroughDDP()
{
    if(tempBuf != NULL){
        tempBuf.clear();
        tempBuf = NULL;
    }
}
bool NuPlayer::DecoderPassThroughDDP::enableOffload()
{
    return false;
}
int32_t NuPlayer::DecoderPassThroughDDP::getAudioOutputFlags()
{
    ALOGV("getAudioOutputFlags AUDIO_OUTPUT_FLAG_DIRECT");
    return AUDIO_OUTPUT_FLAG_DIRECT;
}
//assume one buffer in one unit
status_t NuPlayer::DecoderPassThroughDDP::parseAccessUnit(sp<ABuffer> *accessUnit)
{

    sp<ABuffer> src = *accessUnit;
    sp<ABuffer> tar;
    int64_t timeUs;
    uint16_t header1=0xF872;
    uint16_t header2=0x4E1F;
    uint16_t header3=0x15;
    uint16_t header4;
    int32_t src_offset = 0;
    int32_t copy_offset = 0;
    int32_t copy_size = 0;
    int32_t tar_offset = 0;
    uint32_t fromSize = src->size();
    uint8_t* src_ptr= NULL;
    uint8_t* tar_ptr= NULL;
    int32_t frameCnt = 0;
    uint32_t blockNum = 0;
    uint32_t outputBufferCnt = 0;
    int32_t frameLen = 0;

    //skip empty buffer
    if(fromSize == 0)
        return OK;

    ALOGV("parseAccessUnit input buffer size=%d",fromSize);

    if(tempBuf == NULL){
        tempBuf = src;
    }else if(tempBuf != NULL){
        uint8_t* temp_in_ptr= NULL;
        int32_t lastBuf_offset = tempBuf->offset();
        int32_t tempBufSize = tempBuf->size();
        temp_in_ptr = src->data();
        src = new ABuffer(tempBufSize + fromSize);
        ALOGV("last buffer offset=%d,size=%d",lastBuf_offset,tempBufSize);
        memcpy(src->data(),tempBuf->data(),tempBufSize);
        memcpy(src->data()+tempBufSize,temp_in_ptr,fromSize);
        fromSize = src->size();
        ALOGV("left buffer size=%d,new input buffer size=%d",tempBufSize,fromSize);
        if((*accessUnit)->meta()->findInt64("timeUs", &timeUs)){
            src->meta()->setInt64("timeUs",timeUs);
        }
        (*accessUnit).clear();
        tempBuf.clear();
        tempBuf = NULL;
    }

    while(src_offset < (int32_t)fromSize){

        if(OK != getFrameLen(&src,src_offset,&frameLen)){
            src_offset += 1;
            ALOGE("can't find DDP header");
            continue;
        }else{
            ALOGV("src_offset=%d,frameLen=%d",src_offset,frameLen);
        }
        if(src_offset + frameLen > (int32_t)fromSize){
            ALOGV("frame len is larger than current buffer, src_offset=%d,frameLen=%d,size=%d",src_offset,frameLen,fromSize);
            break;
        }
        src_offset += frameLen;
        frameCnt++;
        blockNum += mNumBlocks;

    }

    //wait until get 6 audio blocks
    if(blockNum < 6){
        ALOGV("blockNum =%d, wait another frame",blockNum);
        if(tempBuf != NULL)
            tempBuf.clear();
        tempBuf = src;
        return -EWOULDBLOCK;
    }

    outputBufferCnt = blockNum/6;

    #define OUTPUT_BUFFER_SIZE (6144*4)
    tar = new ABuffer(OUTPUT_BUFFER_SIZE*outputBufferCnt);
    if(tar == NULL)
        return BAD_VALUE;

    memset(tar->data(),0,OUTPUT_BUFFER_SIZE*outputBufferCnt);

    copy_offset = 0;
    copy_size = src_offset;

    if(outputBufferCnt > 1)
        copy_size = frameLen;

    if(copy_size+8 > OUTPUT_BUFFER_SIZE)
        return BAD_VALUE;

    tar_ptr = tar->data();
    src_ptr = src->data();

    while(outputBufferCnt > 0 && copy_offset < src_offset){
        tar_ptr += tar_offset;
        src_ptr += copy_offset;
        header4 = copy_size * 8;

        memcpy(tar_ptr,&header1,sizeof(uint16_t));
        memcpy(tar_ptr+sizeof(uint16_t),&header2,sizeof(uint16_t));
        memcpy(tar_ptr+2*sizeof(uint16_t),&header3,sizeof(uint16_t));
        memcpy(tar_ptr+3*sizeof(uint16_t),&header4,sizeof(uint16_t));

        tar_ptr += 8;

        //need switch byte order for each uint16
        if(src_ptr[0] == 0x0b && src_ptr[1] == 0x77){
            int32_t i;
            uint16_t * ptr = (uint16_t*)src_ptr;
            uint16_t * ptr2 = (uint16_t*)tar_ptr;
            for(i = 0; i < (int32_t)copy_size/2; i++) {
                uint16_t word = ptr[i];
                uint16_t byte1 = (word << 8) & 0xff00;
                uint16_t byte2 = (word >> 8) & 0x00ff;
                ptr2[i] = byte1 | byte2;
            }
        }else{
            memcpy(tar_ptr,src_ptr,copy_size);
        }

        outputBufferCnt --;
        tar_offset += OUTPUT_BUFFER_SIZE-8;
        copy_offset += copy_size;
        ALOGV("copy %d, total copy %d, total=%d",copy_size,copy_offset,fromSize);
 
    }

    if(src->meta()->findInt64("timeUs", &timeUs)){
        tar->meta()->setInt64("timeUs",timeUs);
        ALOGV("parseAccessUnit ts=%" PRId64 "",timeUs);
    }

    if(src_offset < (int32_t)fromSize){
        sp<ABuffer> temp2 = new ABuffer(fromSize - src_offset);
        memcpy(temp2->data(),src->data()+src_offset,fromSize - src_offset);
        src.clear();
        src = NULL;
        tempBuf = temp2;
        ALOGV("left size=%d", fromSize - src_offset);
    }else{
        src.clear();
        src = NULL;
        tempBuf.clear();
        tempBuf = NULL;
    }

    *accessUnit = tar;
    #if 0
    FILE * pfile;
    pfile = fopen("/data/misc/media/pcm","ab");
    if(pfile){
        fwrite(tar->data(),1,tar->size(),pfile);
        fclose(pfile);
    }
    #endif
    return OK;
}
status_t NuPlayer::DecoderPassThroughDDP::getFrameLen(sp<ABuffer> *accessUnit,int32_t offset,int32_t *len)
{
    int32_t fscod = 0;
    int32_t frmsizecod = 0;
    int fscod2 = -1;
    int bsid = 0;
    mNumBlocks = 0;
    uint8_t * pHeader = NULL;
    bool bigEndian = true;//big enadian for most case

    if(accessUnit == NULL || len == NULL)
        return BAD_VALUE;

    sp<ABuffer> from = *accessUnit;
    if(from->size() < 8 || offset + 8 > (int32_t)from->size())
        return BAD_VALUE;
    pHeader = from->data()+offset;

    //big endian, stream order
    if(pHeader[0] == 0x0b && pHeader[1] == 0x77){
        frmsizecod = ((pHeader[2] & 0x7) << 8) + pHeader[3];
        fscod = pHeader[4] >> 6;
        bsid = (pHeader[5] >> 3) & 0x1f;
    }else if(pHeader[0] == 0x77 && pHeader[1] == 0x0b){
        frmsizecod = ((pHeader[3] & 0x7) << 8) + pHeader[2];
        fscod = pHeader[5] >> 6;
        bsid = (pHeader[4] >> 3) & 0x1f;
        bigEndian = false;
    }else
        return BAD_VALUE;

    //ac3
    if(bsid <=8){

        if (fscod >= NFSCOD || frmsizecod >= NDATARATE){
            ALOGE("getFrameLen NOT AC3,fscod=%d,framesizecode=%d",fscod,frmsizecod);
            return BAD_VALUE;
        }
        //samplerate = ac3_sampling_rate[fscod];
        *len = 2 * ac3_frame_size[fscod][frmsizecod];
        ALOGV("getFrameLen fscod = %d len=%d",fscod,*len);
    //ddp
    }else if(bsid > 10 && bsid <= 16){
        if(bigEndian){
            if(fscod == 0x3){
                mNumBlocks = 6;
                fscod2 = (pHeader[4] & 0x3f) >> 4;
            }else
                mNumBlocks = (pHeader[4] & 0x3f) >> 4;
        }else{
            if(fscod == 0x3){
                mNumBlocks = 6;
                fscod2 = (pHeader[5] & 0x3f) >> 4;
            }else
                mNumBlocks = (pHeader[5] & 0x3f) >> 4;
        }

        if(3 == mNumBlocks)
            mNumBlocks = 6;
        else if(mNumBlocks < 3)
            mNumBlocks += 1;

        *len = (frmsizecod+1)*2;
        ALOGV("numBlocks = %zu fscod = %d len=%d",mNumBlocks,fscod,*len);

    }

    return OK;
}
status_t NuPlayer::DecoderPassThroughDDP::getCacheSize(size_t *cacheSize,size_t * bufferSize)
{
    if(cacheSize == NULL || bufferSize == NULL)
        return BAD_VALUE;
    *cacheSize = 10000;
    *bufferSize = 0;
    ALOGV("getCacheSize");
    return OK;
}
}

