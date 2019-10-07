/*
 * Copyright (C) 2009 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "StreamingDataSource"
#include <utils/Log.h>

#include <media/stagefright/foundation/ADebug.h>
#include <StreamingDataSource.h>
#include "SessionManager.h"

namespace android {

NuPlayer::StreamingDataSource::StreamingDataSource(const char *uri)
    :mSessionManager(0)
{
    ALOGI("constructor %s", uri);
    mSessionManager = SessionManager::createSessionManager(uri, 0);
}


NuPlayer::StreamingDataSource::~StreamingDataSource()
{
    ALOGI("destructor");
}

status_t NuPlayer::StreamingDataSource::initCheck() const
{
    return (mSessionManager != 0) ? OK : NO_INIT;
}

ssize_t NuPlayer::StreamingDataSource::readAt(off64_t /*offset*/, void *data, size_t size) {
    if (mSessionManager == 0) {
        return NO_INIT;
    }

    ssize_t ret = mSessionManager->read(data,size);
#if 0
    if(offset % 5000 == 0)
        ALOGI("readAt get %d from sessionManager, offset %lld", ret, offset);
#endif
    return ret;
}
int64_t NuPlayer::StreamingDataSource::getLatency()
{
    return mSessionManager->getBufferedDuration();
}
}// namespace android
