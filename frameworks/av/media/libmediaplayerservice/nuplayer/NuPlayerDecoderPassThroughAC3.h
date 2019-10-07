/**
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 */

#ifndef NUPLAYER_DECODER_PASS_THROUGH_AC3_H_
#define NUPLAYER_DECODER_PASS_THROUGH_AC3_H_

#include "NuPlayerDecoderPassThrough.h"
namespace android {

struct NuPlayer::DecoderPassThroughAC3 : public NuPlayer::DecoderPassThrough{
    DecoderPassThroughAC3(const sp<AMessage> &notify,
                       const sp<Source> &source,
                       const sp<Renderer> &renderer);

protected:

    virtual ~DecoderPassThroughAC3();
    bool enableOffload();
    int32_t getAudioOutputFlags();
    status_t parseAccessUnit(sp<ABuffer> *accessUnit);
    status_t getFrameLen(sp<ABuffer> *accessUnit,int32_t offset,int32_t *len);
    status_t getCacheSize(size_t *cacheSize,size_t * bufferSize);
private:
    sp<ABuffer> tempBuf;
};
}
#endif