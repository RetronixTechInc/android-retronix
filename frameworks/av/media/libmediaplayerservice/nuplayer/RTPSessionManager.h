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

#ifndef RTP_SESSION_MANAGER_H_

#define RTP_SESSION_MANAGER_H_

#include "SessionManager.h"

namespace android {

struct GenericStreamSource;

struct NuPlayer::RTPSessionManager : public SessionManager{
    public:
        RTPSessionManager(const char *uri, GenericStreamSource * pStreamSource);

    protected:
        ~RTPSessionManager(){};
        virtual status_t parseHeader(const sp<ABuffer> &buffer);
        virtual void enqueueFilledBuffer(const sp<ABuffer> &buffer);
        virtual bool checkDiscontinuity(const sp<ABuffer> &buffer, int64_t *timestamp);

    private:

        int32_t mCurSeqNo;

};

}

#endif

