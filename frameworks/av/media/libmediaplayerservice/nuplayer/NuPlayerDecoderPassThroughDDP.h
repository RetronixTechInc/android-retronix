/**
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 */

#ifndef NUPLAYER_DECODER_PASS_THROUGH_DDP_H_
#define NUPLAYER_DECODER_PASS_THROUGH_DDP_H_

#include "NuPlayerDecoderPassThrough.h"
namespace android {

struct NuPlayer::DecoderPassThroughDDP : public NuPlayer::DecoderPassThrough{
    DecoderPassThroughDDP(const sp<AMessage> &notify,
                       const sp<Source> &source,
                       const sp<Renderer> &renderer);

protected:

    virtual ~DecoderPassThroughDDP();
    bool enableOffload();
    int32_t getAudioOutputFlags();
    status_t parseAccessUnit(sp<ABuffer> *accessUnit);
    status_t getFrameLen(sp<ABuffer> *accessUnit,int32_t offset,int32_t *len);
    status_t getCacheSize(size_t *cacheSize,size_t * bufferSize);
private:
    size_t mFrameSize;
    size_t mNumBlocks;
    sp<ABuffer> tempBuf;
};
}
#endif