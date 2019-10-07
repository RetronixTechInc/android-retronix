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

#define LOG_TAG "RTPSessionManager"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/MediaErrors.h>
#include "RTPSessionManager.h"
#include "GenericStreamSource.h"

namespace android {

static uint16_t U16_AT(const uint8_t *ptr)
{
    return ptr[0] << 8 | ptr[1];
}

static uint32_t U32_AT(const uint8_t *ptr)
{
    return ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];
}

NuPlayer::RTPSessionManager::RTPSessionManager(
    const char *uri,
    GenericStreamSource * pStreamSource)
    : SessionManager(uri, pStreamSource)
    ,mCurSeqNo(0)
{
    return;
}

status_t NuPlayer::RTPSessionManager::parseHeader(const sp<ABuffer> &buffer)
{
    ALOGV("RTPSessionManager::parseHeader size %zu", buffer->size());

    size_t size = buffer->size();
    if (size < 12) {
        // Too short to be a valid RTP header.
        return ERROR_MALFORMED;
    }

    const uint8_t *data = buffer->data();

    if ((data[0] >> 6) != 2) {
        // Unsupported version.
        return ERROR_UNSUPPORTED;
    }

    if (data[0] & 0x20) {
        // Padding present.

        size_t paddingLength = data[size - 1];

        if (paddingLength + 12 > size) {
            // If we removed this much padding we'd end up with something
            // that's too short to be a valid RTP header.
            return ERROR_MALFORMED;
        }

        size -= paddingLength;
    }

    int numCSRCs = data[0] & 0x0f;

    size_t payloadOffset = 12 + 4 * numCSRCs;

    if (size < payloadOffset) {
        // Not enough data to fit the basic header and all the CSRC entries.
        return ERROR_MALFORMED;
    }

    if (data[0] & 0x10) {
        // Header eXtension present.

        if (size < payloadOffset + 4) {
            // Not enough data to fit the basic header, all CSRC entries
            // and the first 4 bytes of the extension header.

            return ERROR_MALFORMED;
        }

        const uint8_t *extensionData = &data[payloadOffset];

        size_t extensionLength =
            4 * (extensionData[2] << 8 | extensionData[3]);

        if (size < payloadOffset + 4 + extensionLength) {
            return ERROR_MALFORMED;
        }

        payloadOffset += 4 + extensionLength;
    }

    uint32_t srcId = U32_AT(&data[8]);
    uint32_t rtpTime = U32_AT(&data[4]);
    uint16_t seqNo = U16_AT(&data[2]);

    sp<AMessage> meta = buffer->meta();
    meta->setInt32("ssrc", srcId);
    meta->setInt32("rtp-time", rtpTime);
    meta->setInt32("PT", data[1] & 0x7f);
    meta->setInt32("M", data[1] >> 7);

    mQueueTimeUs = rtpTime * 100 / 9;
    meta->setInt64("time",mQueueTimeUs);
    buffer->setInt32Data(seqNo);
    buffer->setRange(payloadOffset, size - payloadOffset);

    ALOGV("RTPSessionManager::parseHeader result size %zu", buffer->size());

    return OK;
}

bool NuPlayer::RTPSessionManager::checkDiscontinuity(const sp<ABuffer> &buffer, int64_t *timestamp)
{
    bool ret = false;
    int32_t seqNo = buffer->int32Data();
    if(mCurSeqNo != 0 && seqNo != 0 && mCurSeqNo != seqNo && (mCurSeqNo + 1) != seqNo){
        int32_t rtptime = 0;
        if(timestamp && buffer->meta()->findInt32("rtp-time", &rtptime))
            *timestamp = rtptime;
        ret = true;
        ALOGI("sequenceNo incorrect: current %d, new %d, ts %d", mCurSeqNo, seqNo, rtptime);
    }
    mCurSeqNo = seqNo;
    return ret;
}

void NuPlayer::RTPSessionManager::enqueueFilledBuffer(const sp<ABuffer> &buffer)
{
    ALOGV("RTPSessionManager::enqueueFilledBuffer, total %d", mTotalDataSize);

    int32_t newExtendedSeqNo = buffer->int32Data();

    // "0" means seqNo reaches upper limitation and restart from 0, shall append at end of queue
    if (mFilledBufferQueue.empty() || newExtendedSeqNo == 0) {
        mFilledBufferQueue.push_back(buffer);
        mTotalDataSize += buffer->size();
    }
    else{

        List<sp<ABuffer> >::iterator firstIt = mFilledBufferQueue.begin();
        List<sp<ABuffer> >::iterator it = --mFilledBufferQueue.end();
        for (;;) {
            int32_t extendedSeqNo = (*it)->int32Data();

            if (extendedSeqNo == newExtendedSeqNo) {
                ALOGE("enqueueFilledBuffer duplicated seqNo");
                return;
            }

            if (extendedSeqNo < newExtendedSeqNo) {
                // Insert new packet after the one at "it".
                mFilledBufferQueue.insert(++it, buffer);
                mTotalDataSize += buffer->size();
                break;
            }

            if (it == firstIt) {
                mFilledBufferQueue.insert(it, buffer);
                mTotalDataSize += buffer->size();
                break;
            }

            --it;
        }
    }

    ALOGV("after enqueue, total %d", mTotalDataSize);

}

}

