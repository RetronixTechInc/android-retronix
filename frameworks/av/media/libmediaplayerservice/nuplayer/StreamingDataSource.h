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

#ifndef STREAMING_DATA_SOURCE_H_

#define STREAMING_DATA_SOURCE_H_

#include "NuPlayer.h"

#include <stdio.h>

#include <media/DataSource.h>
#include <media/stagefright/MediaErrors.h>
#include <utils/threads.h>

namespace android {

struct SessionManager;

struct NuPlayer::StreamingDataSource : public DataSource {
public:
    StreamingDataSource(const char *uri);

    virtual status_t initCheck() const;

    virtual ssize_t readAt(off64_t offset, void *data, size_t size);
    virtual uint32_t flags() { return kIsLiveSource;}
    int64_t getLatency();

protected:
    virtual ~StreamingDataSource();

private:
    sp<SessionManager> mSessionManager;

    StreamingDataSource(const StreamingDataSource &);
    StreamingDataSource&operator=(const StreamingDataSource &);
};

}  // namespace android

#endif

