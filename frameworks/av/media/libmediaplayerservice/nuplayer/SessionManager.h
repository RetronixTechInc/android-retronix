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

#ifndef SESSION_MANAGER_H_

#define SESSION_MANAGER_H_

#include "NuPlayer.h"

namespace android {

struct AMessage;
struct ANetworkSession;
struct GenericStreamSource;

struct NuPlayer::SessionManager : public AHandler {


public:
    SessionManager(const char *uri, GenericStreamSource * streamSource);
    static sp<SessionManager> createSessionManager(
        const char *uri,
        GenericStreamSource * pStreamSource);

    enum {
        kWhatNetworkNotify,
        kWhatRequireData,
    };
    ssize_t read(void * data, size_t size);
    int64_t getBufferedDuration();
protected:
    ~SessionManager();
    virtual void onMessageReceived(const sp<AMessage> &msg);
    virtual status_t parseHeader(const sp<ABuffer> &buffer) = 0;
    virtual void enqueueFilledBuffer(const sp<ABuffer> &buffer) = 0;
    virtual bool checkDiscontinuity(const sp<ABuffer> & /*buffer*/, int64_t * /*timestamp*/){return false;};

    List<sp<ABuffer>> mFilledBufferQueue;
    int32_t mTotalDataSize;
    int64_t mQueueTimeUs;
    int64_t mDequeueTimeUs;
private:

    GenericStreamSource * mDownStreamComp;
    sp<ANetworkSession> mNetSession;
    const char *mUri;
    int32_t mSessionID;
    AString mHost;
    int32_t mPort;
    sp<ALooper> mLooper;
    bool bufferingState;
    Mutex mLock;
    Condition mDataAvailableCond;
    bool mStarted;
    bool parseURI(const char *uri, AString *host, int32_t *port);
    void onNetworkNotify(const sp<AMessage> &msg);
    status_t initNetwork();
    bool init();
    bool deInit();
    void tryOutputFilledBuffers();
    void checkFlags(bool BufferIncreasing);
    status_t adjustAudioSinkBufferLen(int latencyMs);
};

}

#endif
