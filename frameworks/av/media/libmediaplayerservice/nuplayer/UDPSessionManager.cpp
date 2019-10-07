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

#define LOG_TAG "UDPSessionManager"

#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/MediaErrors.h>
#include "UDPSessionManager.h"
#include "GenericStreamSource.h"

namespace android {

NuPlayer::UDPSessionManager::UDPSessionManager(
    const char *uri,
    GenericStreamSource * pStreamSource)
    : SessionManager(uri, pStreamSource)
{
    return;
}

status_t NuPlayer::UDPSessionManager::parseHeader(const sp<ABuffer> & /*buffer */)
{
   return OK;
}

void NuPlayer::UDPSessionManager::enqueueFilledBuffer(const sp<ABuffer> &buffer)
{
    mFilledBufferQueue.push_back(buffer);
    mTotalDataSize += buffer->size();

    ALOGV("enqueueFilledBuffer queue count %zu", mFilledBufferQueue.size());

    return;
}

}

