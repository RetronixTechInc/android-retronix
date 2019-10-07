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

#ifndef GENERIC_STREAMSOURCE_H_

#define GENERIC_STREAMSOURCE_H_

#include "NuPlayer.h"
#include <media/IStreamSource.h>

namespace android {

struct IStreamListener;
struct AMessage;
struct SessionManager;

struct NuPlayer::GenericStreamSource : public IStreamSource{

    GenericStreamSource(const char *uri);

    virtual void setListener(const sp<IStreamListener> &listener);
    virtual void setBuffers(const Vector<sp<IMemory> > &buffers);
    virtual void onBufferAvailable(size_t index);

    int32_t outputFilledBuffer(const sp<ABuffer> &filledBuffer);
    virtual uint32_t flags() const;
    void issueCommand(
            IStreamListener::Command cmd, bool synchronous, const sp<AMessage> &extra);

protected:
    ~GenericStreamSource();
    virtual IBinder* onAsBinder(){return NULL;};

private:

    Mutex mLock;

    wp<IStreamListener> mListener;
    Vector<sp<IMemory>> mListenerBuffers;
    List<size_t> mEmptyBufferQueue;
    sp<SessionManager> mSessionManager;
    bool mLowLatency;
};

}

#endif
