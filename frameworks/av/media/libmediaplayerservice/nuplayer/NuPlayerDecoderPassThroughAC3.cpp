/**
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "NuPlayerDecoderPassThroughAC3"
#include <utils/Log.h>
#include <inttypes.h>

#include "NuPlayerDecoderPassThroughAC3.h"

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
static const int32_t ac3_sampling_rate[NFSCOD] = {48000,44100,32000};
NuPlayer::DecoderPassThroughAC3::DecoderPassThroughAC3(
        const sp<AMessage> &notify,
        const sp<Source> &source,
        const sp<Renderer> &renderer)
    : DecoderPassThrough(notify,source,renderer){
    ALOGW_IF(renderer == NULL, "expect a non-NULL renderer");
    tempBuf = NULL;
}
NuPlayer::DecoderPassThroughAC3::~DecoderPassThroughAC3()
{
    if(tempBuf != NULL){ 
        tempBuf.clear();
        tempBuf = NULL;
    }
}
bool NuPlayer::DecoderPassThroughAC3::enableOffload()
{
    return false;
}
int32_t NuPlayer::DecoderPassThroughAC3::getAudioOutputFlags()
{
    ALOGV("getAudioOutputFlags AUDIO_OUTPUT_FLAG_DIRECT");
    return AUDIO_OUTPUT_FLAG_DIRECT;
}
status_t NuPlayer::DecoderPassThroughAC3::parseAccessUnit(sp<ABuffer> *accessUnit)
{

    sp<ABuffer> src = *accessUnit;
    sp<ABuffer> tar;
    int64_t timeUs;
    uint16_t header1=0xF872;
    uint16_t header2=0x4E1F;
    uint16_t header3=0x01;
    uint16_t header4;
    int32_t src_offset = 0;
    int32_t tar_offset = 0;
    int32_t frameCnt = 0;
    uint32_t fromSize = src->size();
    uint8_t* src_ptr= NULL;
    uint8_t* tar_ptr= NULL;

    //skip empty buffer
    if(fromSize == 0)
        return OK;

    if(tempBuf != NULL){
        uint8_t* temp_in_ptr= NULL;
        int32_t lastBuf_offset = tempBuf->offset();
        int32_t tempBufSize = tempBuf->size();
        temp_in_ptr = src->data();
        src = new ABuffer(tempBufSize + fromSize);
        ALOGV("last buffer offset=%d,size=%d",lastBuf_offset,tempBufSize);
        memcpy(src->data(),tempBuf->data(),tempBufSize);
        memcpy(src->data()+tempBufSize,temp_in_ptr,fromSize);
        fromSize = src->size();
        ALOGV("copy left buffer size=%d,new buffer size=%d",tempBufSize,fromSize);
        if((*accessUnit)->meta()->findInt64("timeUs", &timeUs)){
            src->meta()->setInt64("timeUs",timeUs);
        }
        (*accessUnit).clear();
        tempBuf.clear();
        tempBuf = NULL;
    }
    while(src_offset < (int32_t)fromSize){
        int32_t frameLen = 0;
        if(OK != getFrameLen(&src,src_offset,&frameLen)){
            src_offset += 1;
            continue;
        }else{
            ALOGV("src_offset=%d",src_offset);
        }
        if(src_offset + frameLen > (int32_t)fromSize){
            ALOGV("src_offset=%d,frameLen=%d,size=%d",src_offset,frameLen,fromSize);
            break;
        }
        src_offset += frameLen;
        frameCnt++;
    }
    if(frameCnt == 0){
        if(tempBuf != NULL)
            tempBuf.clear();
        tempBuf = src;
        return -EWOULDBLOCK;
    }

    ALOGV("frameCnt=%d",frameCnt);
    //uint8_t first = *src->data();

    #define OUTPUT_BUFFER_SIZE (6144)
    tar = new ABuffer(OUTPUT_BUFFER_SIZE*frameCnt);
    if(tar == NULL)
        return BAD_VALUE;

    memset(tar->data(),0,OUTPUT_BUFFER_SIZE*frameCnt);

    src_offset = 0;
    
    while(src_offset < (int32_t)fromSize){
        int32_t frameLen = 0;
        if(OK != getFrameLen(&src,src_offset,&frameLen)){
            src_offset += 1;
            continue;
        }else{
            ALOGV("src_offset=%d",src_offset);
        }
        src_ptr = src->data()+src_offset;
        if(src_offset + frameLen > (int32_t)fromSize){
            ALOGV("src_offset=%d,len=%d,fromSize=%d",src_offset ,frameLen,fromSize);
            tempBuf = src;
            tempBuf->setRange(src_offset, fromSize - src_offset);
            break;
        }
        header4 = frameLen * 8;
        tar_ptr = tar->data()+tar_offset;
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
            for(i = 0; i < frameLen/2; i++) {
                uint16_t word = ptr[i];
                uint16_t byte1 = (word << 8) & 0xff00;
                uint16_t byte2 = (word >> 8) & 0x00ff;
                ptr2[i] = byte1 | byte2;
            }
        }else{
            memcpy(tar_ptr,src_ptr,frameLen);
        }

        ALOGV("parseAccessUnit,offset=%d,frameLen=%d,totalSrcSize=%d",src_offset,frameLen,fromSize);
        src_offset += frameLen;
        tar_offset += OUTPUT_BUFFER_SIZE;
        if(src_offset + frameLen > (int32_t)fromSize){
            ALOGV("src_offset=%d,len=%d,fromSize=%d",src_offset ,frameLen,fromSize);
            tempBuf = src;
            tempBuf->setRange(src_offset, fromSize - src_offset);
            break;
        }
    }
    if(src->meta()->findInt64("timeUs", &timeUs)){
        tar->meta()->setInt64("timeUs",timeUs);
        ALOGV("parseAccessUnit ts=%" PRId64 "",timeUs);
    }

    if(tempBuf == NULL){
        src.clear();
    }

    *accessUnit = tar;

    return OK;
}
status_t NuPlayer::DecoderPassThroughAC3::getFrameLen(sp<ABuffer> *accessUnit,int32_t offset,int32_t *len)
{
    int32_t fscod = 0;
    int32_t frmsizecod = 0;
    int32_t acmod=0;
    int32_t samplerate=0;
    int32_t framesize=0;
    uint8_t * pHeader = NULL;
    bool bigEndian = false;
    if(accessUnit == NULL || len == NULL)
        return BAD_VALUE;
    sp<ABuffer> from = *accessUnit;
    if(from->size() < 8 || offset + 8 > (int32_t)from->size())
        return BAD_VALUE;
    pHeader = from->data()+offset;
    if ((pHeader[0] == 0x0b && pHeader[1] == 0x77) || (pHeader[0] == 0x77 && pHeader[1] == 0x0b)){
        ;
    }else{
        return BAD_VALUE;
    }
    if(pHeader[0] == 0x0b && pHeader[1] == 0x77){
        fscod = pHeader[4] >> 6;
        frmsizecod = pHeader[4] & 0x3f;
        acmod = pHeader[6] >> 5;
        bigEndian = true;
    }
    else if(pHeader[0] == 0x77 && pHeader[1] == 0x0b){
        fscod = pHeader[5] >> 6;
        frmsizecod = pHeader[5] & 0x3f;
        acmod = pHeader[7] >> 5;
    }
    if (fscod>=NFSCOD || frmsizecod>=NDATARATE){
        ALOGE("getFrameLen NOT AC3");
        return BAD_VALUE;
    }
    samplerate = ac3_sampling_rate[fscod];
    framesize = 2 * ac3_frame_size[fscod][frmsizecod];
    *len = framesize;

    return OK;
}
status_t NuPlayer::DecoderPassThroughAC3::getCacheSize(size_t *cacheSize,size_t * bufferSize)
{
    if(cacheSize == NULL || bufferSize == NULL)
        return BAD_VALUE;
    *cacheSize = 10000;
    *bufferSize = 0;
    ALOGV("getCacheSize");
    return OK;
}
}