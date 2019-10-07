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

//#define LOG_NDEBUG 0
#define LOG_TAG "SessoinManager"
#include <utils/Log.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ANetworkSession.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/MediaKeys.h>
#include "SessionManager.h"
#include "RTPSessionManager.h"
#include "UDPSessionManager.h"
#include "GenericStreamSource.h"
#include "ATSParser.h"
#include <cutils/properties.h>

#define KB * 1024
#define MB * 1024 * 1024

#define HIGH_WATERMARK (5 MB)
#define LOW_WATERMARK (0 KB)//this value should be 0 to keep latency lowest.

// if shift_point is 200KB, AVC_MP31_1280x720_AACLC_44.1kHz_2ch.ts will has no video output
#define SHIFT_POINT (500 KB)

const static int64_t WAIT_DATA_TIMEOUT = 10; // seconds

#define DISPLAY_CACHED_DATA_SIZE

#define MIN(a, b) ((a) < (b)) ? (a) : (b)

namespace android {

NuPlayer::SessionManager::SessionManager(const char *uri, GenericStreamSource *streamSource)
    :mTotalDataSize(0),
    mDownStreamComp(streamSource),
    mNetSession(NULL),
    mUri(uri),
    mSessionID(0),
    mPort(0),
    mLooper(new ALooper),
    bufferingState(true)
{
    ALOGI("constructor");
    mStarted = false;
    mQueueTimeUs = 0;
    mDequeueTimeUs = 0;
}

NuPlayer::SessionManager::~SessionManager()
{
    ALOGI("destructor");

    mDataAvailableCond.signal();

    deInit();

    mFilledBufferQueue.clear();
}

bool NuPlayer::SessionManager::parseURI(const char *uri, AString *host, int32_t *port)
{
    host->clear();
    *port = 0;

    // skip "rtp://" and "udp://"
    host->setTo(&uri[6]);

    const char *colon = strchr(host->c_str(), ':');

    if (colon == NULL) {
        return false;
    }

    char *end;
    unsigned long x = strtoul(colon + 1, &end, 10);

    if (end == colon + 1 || *end != '\0' || x >= 65536) {
        return false;
    }

    *port = x;

    size_t colonOffset = colon - host->c_str();
    host->erase(colonOffset, host->size() - colonOffset);

    ALOGI("SessionManager::parseURI(), host %s, port %d", host->c_str(), *port);

    return true;
}

bool NuPlayer::SessionManager::init()
{
    mLooper->setName("SessoinManager");
    mLooper->start();
    mLooper->registerHandler(this);

    if(initNetwork() != OK){
        ALOGE("initialize network %s fail!", mUri);
        return false;
    }

    int pcmLen = 60;
    adjustAudioSinkBufferLen(pcmLen);

    return true;
}

bool NuPlayer::SessionManager::deInit()
{
    if((mNetSession != NULL) && (mSessionID != 0)){
        mNetSession->stop();
        mNetSession->destroySession(mSessionID);
        mSessionID = 0;
    }

    if (mLooper != NULL) {
        mLooper->unregisterHandler(id());
        mLooper->stop();
    }

    int pcmLen = 500;
    adjustAudioSinkBufferLen(pcmLen);

    return true;
}
status_t NuPlayer::SessionManager::adjustAudioSinkBufferLen(int latencyMs)
{
    char value[10];
    if(latencyMs < 0 || latencyMs > 5000)
        return -1;

    memset(value, 0, sizeof(value));
    snprintf(value, sizeof(value), "%d", latencyMs);
    property_set("media.stagefright.audio.sink", value);
    ALOGI("adjustAudioSinkBufferLen latencyMs=%d",latencyMs);
    return 0;
}
status_t NuPlayer::SessionManager::initNetwork()
{
    ALOGV("SessionManager::init");
    uint32_t MAX_RETRY_TIMES = 20;
    uint32_t retry = 0;

    mNetSession = new ANetworkSession;
    status_t ret = mNetSession->start();
    if(ret != OK)
        return ret;

    sp<AMessage> notify = new AMessage(kWhatNetworkNotify, this);

    CHECK_EQ(mSessionID, 0);

    if(!parseURI(mUri, &mHost, &mPort))
        return -1;

    for (;;) {
        ret = mNetSession->createUDPSession(
                mPort,
                mHost.c_str(),
                0,
                notify,
                &mSessionID);

        if (ret == OK) {
            break;
        }

        mNetSession->destroySession(mSessionID);
        mSessionID = 0;

        if(++retry > MAX_RETRY_TIMES)
            return ret;

        usleep(500000);
        ALOGI("createUDPSession fail, retry...");

    }

    ALOGI("UDP session created, id %d", mSessionID);

    return OK;
}

void NuPlayer::SessionManager::onNetworkNotify(const sp<AMessage> &msg) {
    int32_t reason;
    CHECK(msg->findInt32("reason", &reason));

    switch (reason) {
        case ANetworkSession::kWhatError:
        {
            ALOGV("SessionManager::onNetworkNotify(): error");

            int32_t sessionID;
            CHECK(msg->findInt32("sessionID", &sessionID));

            int32_t err;
            CHECK(msg->findInt32("err", &err));

            int32_t errorOccuredDuringSend;
            CHECK(msg->findInt32("send", &errorOccuredDuringSend));

            AString detail;
            CHECK(msg->findString("detail", &detail));

            ALOGE("An error occurred during %s in session %d "
                    "(%d, '%s' (%s)).",
                    errorOccuredDuringSend ? "send" : "receive",
                    sessionID,
                    err,
                    detail.c_str(),
                    strerror(-err));

            mNetSession->destroySession(sessionID);

            if (sessionID == mSessionID) {
                mSessionID = 0;
            } else
                ALOGE("Incorrect session ID in error msg!");

            break;
        }

        case ANetworkSession::kWhatDatagram:
        {
            ALOGV("SessionManager::onNetworkNotify(): data");

            sp<ABuffer> data;
            CHECK(msg->findBuffer("data", &data));
            if(parseHeader(data) != OK){
                ALOGE("parseHeader fail, ignore this packet");
                break;
            }

            {
                Mutex::Autolock autoLock(mLock);
                enqueueFilledBuffer(data);
                checkFlags(true);
            }

            ALOGV("after enqueue, total size is %d", mTotalDataSize);

            if(mDownStreamComp)
                tryOutputFilledBuffers();

            break;
        }

        case ANetworkSession::kWhatClientConnected:
        {
            int32_t sessionID;
            CHECK(msg->findInt32("sessionID", &sessionID));

            ALOGI("Received notify of session %d connected", sessionID);
            break;
        }
    }
}

void NuPlayer::SessionManager::tryOutputFilledBuffers()
{
    //Mutex::Autolock autoLock(mLock);

    while(!mFilledBufferQueue.empty() && !bufferingState){
        sp<ABuffer> src = *mFilledBufferQueue.begin();

        int64_t timestamp = 0;
        if(checkDiscontinuity(src, &timestamp)){
            ALOGI("send DISCONTINUITY...");

            sp<AMessage> response = new AMessage;
            response->setInt64("timeUs", timestamp);
            mDownStreamComp->issueCommand(IStreamListener::DISCONTINUITY, false, response);

        }

        //update dequeued time
        src->meta()->findInt64("time", &mDequeueTimeUs);

        int64_t bufferedTs = getBufferedDuration();

        if(bufferedTs > 0){
            sp<AMessage> extra = new AMessage;
            extra->setInt32(
                    kIStreamListenerKeyDiscontinuityMask,
                    ATSParser::DISCONTINUITY_ABSOLUTE_TIME);
            extra->setInt64("tunnel-render-latency", bufferedTs);

            mDownStreamComp->issueCommand(
                    IStreamListener::DISCONTINUITY,
                    false /* synchronous */,
                    extra);
        }

        int32_t ret = mDownStreamComp->outputFilledBuffer(src);
        ALOGV("tryOutputFilledBuffers, ret %d, src size %zu", ret, src->size());
        if(ret >= (int32_t)src->size()){
            mFilledBufferQueue.erase(mFilledBufferQueue.begin());
            CHECK_GE(mTotalDataSize, (int32_t)src->size());
            mTotalDataSize -= src->size();
            ALOGV("after output, total size is %d", mTotalDataSize);
        }
        else if(ret <= 0){
            // no more empty buffer space
            break;
        }
        else{
            // part of this buffer is outputed
            src->setRange(src->offset() + ret, src->size() - ret);
            CHECK_GE(mTotalDataSize, ret);
            mTotalDataSize -= ret;
            ALOGV("after output, total size is %d", mTotalDataSize);
        }

        checkFlags(false);
    }

    return;
}

void NuPlayer::SessionManager::checkFlags(bool BufferIncreasing)
{
    if(BufferIncreasing){
        if(bufferingState){
            if(mStarted){
                bufferingState = false;
                mDataAvailableCond.signal();
                ALOGV("--- switch outof bufferingState");
            }else if(!mStarted && mTotalDataSize > SHIFT_POINT){
                ALOGV("--- switch outof bufferingState start");
                bufferingState = false;
                mDataAvailableCond.signal();
                mStarted = true;
            }
        }

        if(mTotalDataSize > HIGH_WATERMARK){
            ALOGI("--- cache overflow (%d KB), discard all ---", (int32_t)mTotalDataSize/1024);
            mFilledBufferQueue.clear();
            mTotalDataSize = 0;
        }
    }
    else{
        if(mTotalDataSize <= LOW_WATERMARK && !bufferingState){
            ALOGV("--- switch into bufferingState");
            bufferingState = true;
        }
    }

#ifdef DISPLAY_CACHED_DATA_SIZE
    // print mTotalDataSize every 2 seconds
    static int prev_sec = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if((prev_sec == 0) || (tv.tv_sec - prev_sec >= 2)){
        //ALOGI("==== cached data %d KB , time %lld====", mTotalDataSize/1024,mQueueTimeUs-mDequeueTimeUs);
        prev_sec = tv.tv_sec;
    }
#endif

}

#define USE_CONDITION_WAIT

ssize_t NuPlayer::SessionManager::read(void * data, size_t size)
{

    size_t offset = 0;

#ifndef USE_CONDITION_WAIT
    struct timeval time_start;
    gettimeofday(&time_start, NULL);
#endif

    ALOGV("to read size %zu", size);

    while(offset < size)
    {

#ifdef USE_CONDITION_WAIT
            {
                Mutex::Autolock autoLock(mLock);
                if(bufferingState){
                    ALOGV("BufferingState, wait for data...");
                    status_t err = mDataAvailableCond.waitRelative(mLock, WAIT_DATA_TIMEOUT * 1000000000LL);
                    if (err != OK) {
                        ALOGE("Wait for data timeout!");
                        return -1;
                    }
                }
            }
#else
            if(bufferingState){
                usleep(100000);

                struct timeval time_now;
                gettimeofday(&time_now, NULL);
                if(time_now.tv_sec - time_start.tv_sec > WAIT_DATA_TIMEOUT){
                    ALOGE("Waiting for data timeout!");
                    break;
                }
                continue;
            }
#endif
            mLock.lock();

            if(mFilledBufferQueue.empty()){
                mLock.unlock();
                continue;
            }

            sp<ABuffer> src = *mFilledBufferQueue.begin();
            CHECK(src != NULL);

            checkDiscontinuity(src, NULL);

            //ALOGV("require size %d, available in buffer %d", size - offset, src->size());
            src->meta()->findInt64("time", &mDequeueTimeUs);

            size_t copy = MIN(src->size(), size - offset);
            memcpy((uint8_t *)data + offset, src->data(), copy);
            offset += copy;
            mTotalDataSize -= copy;

            if(copy == src->size()){
                // finish copying one buffer
                mFilledBufferQueue.erase(mFilledBufferQueue.begin());
            }
            else{
                src->setRange(src->offset() + copy, src->size() - copy);
            }

            checkFlags(false);

            mLock.unlock();
    }

    ALOGV("read result %zu", offset);

    return offset;
}
int64_t NuPlayer::SessionManager::getBufferedDuration()
{
    if(mQueueTimeUs > 0 && mDequeueTimeUs > 0){
        return mQueueTimeUs - mDequeueTimeUs;
    }
    return 0;
}
void NuPlayer::SessionManager::onMessageReceived(
        const sp<AMessage> &msg) {

    switch (msg->what()) {
        case kWhatNetworkNotify:
        {
            onNetworkNotify(msg);
            break;
        }
        case kWhatRequireData:
        {
            ALOGV("receive message kWhatRequireData");
            if(mDownStreamComp)
                tryOutputFilledBuffers();
            break;
        }
        default:
        {
            TRESPASS();
        }
    }
}

sp<NuPlayer::SessionManager> NuPlayer::SessionManager::createSessionManager(
    const char *uri,
    GenericStreamSource * pStreamSource)
{
    sp<NuPlayer::SessionManager> smg = NULL;

    if(!strncasecmp(uri, "rtp://", 6))
        smg = new RTPSessionManager(uri, pStreamSource);
    else if(!strncasecmp(uri, "udp://", 6))
        smg = new UDPSessionManager(uri, pStreamSource);

    if(smg != NULL && smg->init())
        return smg;

    return NULL;
}


}

