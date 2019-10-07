/*
 * Copyright (C) 2010 The Android Open Source Project
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
/* Copyright (C) 2015 Freescale Semiconductor, Inc. */
#ifndef STREAMING_SOURCE_H_

#define STREAMING_SOURCE_H_

#include "NuPlayer.h"
#include "NuPlayerSource.h"

namespace android {

struct ABuffer;
struct ATSParser;
struct AnotherPacketSource;

struct NuPlayer::StreamingSource : public NuPlayer::Source {

    // Continues drop should wait below time as pipeline isn't refresh.
    static const int RESUME_DROP_CHECK = 100000;
    // Remain data to avoid under run after drop.
    static const int REMAIN_DATA_AFTER_DROP = 50000;
    // Defautl drop threshold.
    static const int LATENCY_THRESHOLD_DEFAULT = 120000;
    // Maximum drop threshold.
    static const int LATENCY_THRESHOLD_MAX = 2000000;
    // Discard one time media data within this peroid.
    static const int QUALITY_CATIRIA = 5*60*1000000;
    // The invertal to statistic lowest latency.
    static const int STATISTIC_PERIOD = 5000000;
    // Drop video threshold.
    static const int DROP_VIDEO_THRESHOLD = 1000000;
    // Reset statistic lowest latency value.
    static const int LOWEST_LATENCY_INIT = 60000000;

    StreamingSource(
            const sp<AMessage> &notify,
            const sp<IStreamSource> &source);

    virtual status_t getBufferingSettings(
            BufferingSettings* buffering /* nonnull */) override;
    virtual status_t setBufferingSettings(const BufferingSettings& buffering) override;

    virtual void prepareAsync();
    virtual void start();

    virtual status_t feedMoreTSData();

    virtual status_t dequeueAccessUnit(bool audio, sp<ABuffer> *accessUnit);

    virtual bool isRealTime() const;

    virtual bool isAVCReorderDisabled() const;
    virtual void setRenderPosition(int64_t positionUs);
protected:
    virtual ~StreamingSource();

    virtual void onMessageReceived(const sp<AMessage> &msg);

    virtual sp<AMessage> getFormat(bool audio);

private:
    enum {
        kWhatReadBuffer,
    };
    sp<IStreamSource> mSource;
    status_t mFinalResult;
    sp<NuPlayerStreamListener> mStreamListener;
    sp<ATSParser> mTSParser;

    int64_t mLatencyLowest;
    int64_t mStasticPeroid;
    int64_t mLatencyThreshold;
    int64_t mTunnelRenderLatency;
    int64_t mPositionUs;
    int64_t mAnchorTimeRealUs;
    int64_t mDropEndTimeUs;
    int64_t mPipeLineLatencyUs;
    int64_t mResumeCheckTimeUs;
    int64_t mPrevDropTimeUs;
    bool mBuffering;
    Mutex mBufferingLock;
    sp<ALooper> mLooper;

    void setError(status_t err);
    sp<AnotherPacketSource> getSource(bool audio);
    bool haveSufficientDataOnAllTracks();
    status_t postReadBuffer();
    void onReadBuffer();

    bool discardMediaDate(bool audio, int64_t timeUs, sp<ABuffer> *accessUnit);
    DISALLOW_EVIL_CONSTRUCTORS(StreamingSource);
};

}  // namespace android

#endif  // STREAMING_SOURCE_H_
