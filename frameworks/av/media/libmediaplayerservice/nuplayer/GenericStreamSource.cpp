/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* Copyright (C) 2016 Freescale Semiconductor, Inc. */

#define LOG_TAG "GenericStreamSource"
#include <utils/Log.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ABuffer.h>
#include "GenericStreamSource.h"
#include "SessionManager.h"
#include <cutils/properties.h>


namespace android {

NuPlayer::GenericStreamSource::GenericStreamSource(const char *uri):
    IStreamSource(),
    mSessionManager(NULL)
{
    ALOGV("GenericStreamSource constructor %s, %p", uri, this);
    mSessionManager = SessionManager::createSessionManager(uri, this);
    mLowLatency = false;
    int value;
    value = property_get_int32("media.rtp_streaming.low_latency", 0);
    if(value & 0x01)
        mLowLatency = true;
}

NuPlayer::GenericStreamSource::~GenericStreamSource()
{
    ALOGV("GenericStreamSource destructor");
}

void NuPlayer::GenericStreamSource::setListener(const sp<IStreamListener> &listener)
{
    IStreamListener * pListener = static_cast<IStreamListener *>(listener.get());
    mListener = pListener;
}

void NuPlayer::GenericStreamSource::setBuffers(const Vector<sp<IMemory> > &buffers)
{
    mListenerBuffers = buffers;
}
uint32_t NuPlayer::GenericStreamSource::flags() const
{
    if(mLowLatency)
        return IStreamSource::kFlagKeepLowLatency;
    else
        return 0;
}
void NuPlayer::GenericStreamSource::onBufferAvailable(size_t index)
{
    ALOGV("GenericStreamSource::onBufferAvailable index %zu" , index);
    Mutex::Autolock autoLock(mLock);
    mEmptyBufferQueue.push_back(index);

    if(mSessionManager != NULL){
        sp<AMessage> mNotify = new AMessage(SessionManager::kWhatRequireData, mSessionManager.get());
        mNotify->post();
    }

}

int32_t NuPlayer::GenericStreamSource::outputFilledBuffer(const sp<ABuffer> &filledBuffer)
{
    if(mEmptyBufferQueue.empty())
        return -1;

    sp<IStreamListener> spListener(mListener.promote());
    if(spListener == NULL){
        ALOGE("StreamListener not exist!");
        return -1;
    }

    size_t index = 0;
    {
        Mutex::Autolock autoLock(mLock);
        index = *mEmptyBufferQueue.begin();
        mEmptyBufferQueue.erase(mEmptyBufferQueue.begin());
    }

    sp<IMemory> mem = mListenerBuffers.editItemAt(index);
    uint8_t * dst_data = (uint8_t *)mem->pointer();
    size_t dst_size = mem->size();

    //ALOGI("sendBufferToListenerdst dst size %d, src size %d", dst_size, filledBuffer->size());

    if(filledBuffer->size() <= dst_size){
        memcpy(dst_data, filledBuffer->data(), filledBuffer->size());
        spListener->queueBuffer(index, filledBuffer->size());
        return filledBuffer->size();
    }
    else{
        memcpy(dst_data, filledBuffer->data(), dst_size);
        spListener->queueBuffer(index, dst_size);
        return dst_size;
    }

}

void NuPlayer::GenericStreamSource::issueCommand(
        IStreamListener::Command cmd, bool synchronous, const sp<AMessage> &extra)
{
    sp<IStreamListener> spListener(mListener.promote());
    if(spListener == NULL){
        ALOGE("StreamListener not exist!");
        return;
    }

    spListener->issueCommand(cmd, synchronous, extra);
}


}

