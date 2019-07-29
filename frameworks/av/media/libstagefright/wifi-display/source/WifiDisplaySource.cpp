/*
 * Copyright 2012, The Android Open Source Project
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "WifiDisplaySource"
#include <utils/Log.h>

#include "WifiDisplaySource.h"
#include "PlaybackSession.h"
#include "Parameters.h"
#include "rtp/RTPSender.h"

#include <binder/IServiceManager.h>
#include <gui/IGraphicBufferProducer.h>
#include <media/IHDCP.h>
#include <media/IMediaPlayerService.h>
#include <media/IRemoteDisplayClient.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ParsedMessage.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/Utils.h>
#include <arpa/inet.h>
#include <cutils/properties.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>

#include <ctype.h>

#define UIBC_SCROLL_MOUSE_NOTCH 0x4000
#define UIBC_SCROLL_MOUSE_UP    0x2000
#define UIBC_SCROLL_NUMBER_MASK 0x1FFF
#define UIBC_LATENCY_THRESHOLD  10

namespace android {

// static
const int64_t WifiDisplaySource::kReaperIntervalUs;
const int64_t WifiDisplaySource::kTeardownTriggerTimeouSecs;
const int64_t WifiDisplaySource::kPlaybackSessionTimeoutSecs;
const int64_t WifiDisplaySource::kPlaybackSessionTimeoutUs;
const AString WifiDisplaySource::sUserAgent = MakeUserAgent();

WifiDisplaySource::WifiDisplaySource(
        const String16 &opPackageName,
        const sp<ANetworkSession> &netSession,
        const sp<IRemoteDisplayClient> &client,
        const char *path)
    : mOpPackageName(opPackageName),
      mState(INITIALIZED),
      mNetSession(netSession),
      mClient(client),
      mSessionID(0),
      mUibcSessionID(0),
      mUibcTouchFd(-1),
      mUibcKeyFd(-1),
      mAbs_x_min(-1),
      mAbs_x_max(-1),
      mAbs_y_min(-1),
      mAbs_y_max(-1),
      mStopReplyID(NULL),
      mChosenRTPPort(-1),
      mUsingPCMAudio(false),
      mClientSessionID(0),
      mReaperPending(false),
      mNextCSeq(1),
      resolutionWidth(0),
      resolutionHeigh(0),
      mResolution_RealW(0),
      mResolution_RealH(0),
      mResolution_NativeW(0),
      mResolution_NativeH(0),
      uibc_calc_data_Ss(0),
      uibc_calc_data_Wbs(0),
      uibc_calc_data_Hbs(0),
      mUibcMouseID(-1),
      mUsingHDCP(false),
      mIsHDCP2_0(false),
      mHDCPPort(0),
      mHDCPInitializationComplete(false),
      mSetupTriggerDeferred(false),
      mPlaybackSessionEstablished(false) {
    if (path != NULL) {
        mMediaPath.setTo(path);
    }
    DisplayInfo mainDpyInfo;
    VideoFormats::ResolutionType type;
    size_t err = 0;
    size_t index;
    sp<IBinder> mainDpy = SurfaceComposerClient::getBuiltInDisplay(
            ISurfaceComposer::eDisplayIdMain);
    err = SurfaceComposerClient::getDisplayInfo(mainDpy, &mainDpyInfo);
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: unable to get display characteristics\n");
        return;
    }
    mSupportedSourceVideoFormats.ConvertDpyInfo2Resolution(mainDpyInfo, type, index);
    ALOGI("Main display is %dx%d @%.2ffps, so pick best resolution type=%d, index=%d\n",
            mainDpyInfo.w, mainDpyInfo.h, mainDpyInfo.fps,
            type, index);

    mResolution_RealW = mainDpyInfo.w;
    mResolution_RealH = mainDpyInfo.h;

    mOrientation = mainDpyInfo.orientation;
    uint8_t sysOrientation = 0;
    char sysOriprop[PROPERTY_VALUE_MAX];
    property_get("ro.sf.hwrotation", sysOriprop, NULL);
    ALOGD("sendrolon system orientation is %s", sysOriprop);
    if (!strcmp("0", sysOriprop) || !strcmp("180", sysOriprop)) {
        sysOrientation = DISPLAY_ORIENTATION_0;
    } else if (!strcmp("90", sysOriprop) || !strcmp("270", sysOriprop)) {
        sysOrientation = DISPLAY_ORIENTATION_90;
    } else {
        sysOrientation = DISPLAY_ORIENTATION_0;
    }

    if ((sysOrientation & 0x1) ^ (mOrientation & 0x1)) {
        size_t tmp = mResolution_RealW;
        mResolution_RealW = mResolution_RealH;
        mResolution_RealH = tmp;
    }

    mSupportedSourceVideoFormats.disableAll();

    mSupportedSourceVideoFormats.setNativeResolution(
            type, index);

    // Enable all resolutions up to NativeResolution
    mSupportedSourceVideoFormats.enableResolutionUpto(
            type, index,
            VideoFormats::PROFILE_CHP,  // Constrained High Profile
            VideoFormats::LEVEL_32);    // Level 3.2

}

uint8_t WifiDisplaySource::getOrientation() {
    DisplayInfo mainDpyInfo;
    int err;
    sp<IBinder> mainDpy = SurfaceComposerClient::getBuiltInDisplay(
    ISurfaceComposer::eDisplayIdMain);
    err = SurfaceComposerClient::getDisplayInfo(mainDpy, &mainDpyInfo);
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: unable to get display characteristics\n");
        return -1;
    }
    return mainDpyInfo.orientation;

}

WifiDisplaySource::~WifiDisplaySource() {
}

static status_t PostAndAwaitResponse(
        const sp<AMessage> &msg, sp<AMessage> *response) {
    status_t err = msg->postAndAwaitResponse(response);

    if (err != OK) {
        return err;
    }

    if (response == NULL || !(*response)->findInt32("err", &err)) {
        err = OK;
    }

    return err;
}

status_t WifiDisplaySource::start(const char *iface) {
    CHECK_EQ(mState, INITIALIZED);

    sp<AMessage> msg = new AMessage(kWhatStart, this);
    msg->setString("iface", iface);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t WifiDisplaySource::startUibc(const int32_t port) {

    status_t err = OK;
    sp<AMessage> notify = new AMessage(kWhatUIBCNotify, this);
    ALOGI("source will create uibc channel");
    err = mNetSession->createTCPDatagramSession(
            mInterfaceAddr,
            port,
            notify,
            &mUibcSessionID);
   return err;
}

status_t WifiDisplaySource::stop() {
    sp<AMessage> msg = new AMessage(kWhatStop, this);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t WifiDisplaySource::pause() {
    sp<AMessage> msg = new AMessage(kWhatPause, this);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

status_t WifiDisplaySource::resume() {
    sp<AMessage> msg = new AMessage(kWhatResume, this);

    sp<AMessage> response;
    return PostAndAwaitResponse(msg, &response);
}

#define DEV_DIR "/dev/input"
#define sizeof_bit_array(bits)  ((bits + 7) / 8)
#define test_bit(bit, array)    (array[bit/8] & (1<<(bit%8)))
#define ABS_MT_POSITION_X 0x35
#define ABS_MT_POSITION_Y 0x36
struct input_event {
    struct timeval time;
    __u16 type;
    __u16 code;
    __s32 value;
};
int WifiDisplaySource::write_event(int fd, int type, int code, int value)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    if(write(fd, &event, sizeof(event)) < (int)sizeof(event)) {
        ALOGI("write event failed[%d]: %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}

int WifiDisplaySource::calc_uibc_parameter(size_t width, size_t height)
{
    //scan and select touch input device
    //struct input_absinfo absinfo;
    float this_w, this_h, dw, dh, vWidth = (float) width, vHeight = (float) height;

    this_w = mResolution_RealW;
    this_h = mResolution_RealH;

    dw = mResolution_NativeW / vWidth;
    dh = mResolution_NativeH / vHeight;


    uibc_calc_data_Ss = (dw > dh) ? dh : dw;
    ALOGI("uibc_para Ss1 = %f vWidth = %f vHeight = %f", uibc_calc_data_Ss, vWidth, vHeight);

    dw =  vWidth / this_w;
    dh =  vHeight / this_h;

    uibc_calc_data_Ss = ((dw > dh) ? dh : dw) * uibc_calc_data_Ss;

    uibc_calc_data_Wbs = (mResolution_NativeW - this_w * uibc_calc_data_Ss) / 2;
    uibc_calc_data_Hbs = (mResolution_NativeH - this_h * uibc_calc_data_Ss) / 2;

    ALOGI("sendrolon: uibc_para: ss %f wbs %f hbs %f native: width %u height %u real:%f x %f",uibc_calc_data_Ss, uibc_calc_data_Wbs, uibc_calc_data_Hbs, mResolution_NativeW, mResolution_NativeH, this_w, this_h);

    return 0;
}

void WifiDisplaySource::calculateNormalXY(float x, float y, int *abs_x, int *abs_y)
{
    float dx, dy;
    dx = x - uibc_calc_data_Wbs;
    dx = dx > 0 ? dx : 0;
    dy = y - uibc_calc_data_Hbs;
    dy = dy > 0 ? dy : 0;

    if(uibc_calc_data_Ss >= -0.000001 && uibc_calc_data_Ss <= 0.000001) {
        *abs_x = (int)(x + 0.5);
        *abs_y = (int)(y + 0.5);
        ALOGW("calculateNormalXY: Ss is zero!!!");
        return;
    }

    ALOGD("calcNormalXY (%f,%f)",dx / uibc_calc_data_Ss, dy / uibc_calc_data_Ss);

    *abs_x = (int) (
             ((dx / uibc_calc_data_Ss) + 0.5));
    *abs_y = (int) (
             ((dy / uibc_calc_data_Ss) + 0.5));
}

int WifiDisplaySource::containsNonZeroByte(const uint8_t* array, uint32_t startIndex, uint32_t endIndex)
{
    const uint8_t* end = array + endIndex;
    array += startIndex;
    while (array != end) {
        if (*(array++) != 0) {
            return 1;
        }
    }
    return 0;
}
bool WifiDisplaySource::checkUIBCtimeStamp(const uint8_t *data) {
    if (data[0] & 0x08) {
        uint16_t timestamp = (int32_t)U16_AT(&data[4]);
        uint16_t tnow = (uint16_t) (((ALooper::GetNowUs() * 9) / 100ll) >> 16);
        ALOGI("UIBC latency is %d = %d - %d", tnow - timestamp, tnow, timestamp);
        uint16_t uibc_latency = tnow - timestamp;
        if (uibc_latency >= UIBC_LATENCY_THRESHOLD) {
            ALOGD("UIBC latency %d too long and drop it.", uibc_latency);
            return false;
        }
    }
    return true;

}
void WifiDisplaySource::recalculateUibcParamaterViaOrientation() {
    uint8_t ori = getOrientation();
    /* Orientation = 0 and 2 are the same and 1 and 3 are same*/
    if ((ori & 0x1) ^ (mOrientation & 0x1)) {
        ALOGW("sendrolon on UIBC swaped");
        mOrientation = ori;
        size_t tmp = mResolution_RealW;
        mResolution_RealW = mResolution_RealH;
        mResolution_RealH = tmp;
        calc_uibc_parameter(mVideoWidth, mVideoHeight);
    }


}
void WifiDisplaySource::parseUIBCtouchEvent(const uint8_t *data) {
    if (!checkUIBCtimeStamp(data))
        return;
    if (data[6] < 3) {
        for (int i = 0; i < data[7]; i++) {
            int32_t x = (int32_t)U16_AT(&data[9]);
            int32_t y = (int32_t)U16_AT(&data[11]);
            int32_t action = (int32_t)data[6];
            int32_t abs_x, abs_y;
            recalculateUibcParamaterViaOrientation();
            calculateNormalXY(x, y, &abs_x, &abs_y);

            ALOGI("WFD UIBC TOUCH x=%d y=%d action=%d", abs_x, abs_y,action);
            mClient->onUibcData(action, (float)abs_x, (float)abs_y, 0);
        }
    } else {
        ALOGE("parseUIBCtouchEvent wrong type!");
    }

}

void WifiDisplaySource::parseUIBCscrollEvent(const uint8_t *data) {
    if (data == NULL)
         return;
    if (!checkUIBCtimeStamp(data))
        return;
    if (data[3] != 9) {
         ALOGI("parseUIBCscrollEvent data len error!");
         return;
    }

    int16_t d = data[7];
    d = (d << 8) & 0xff00;
    d = d | data[8];
    int updown = 0;
    float x = 0;
    if (d & UIBC_SCROLL_MOUSE_UP) {
        updown = 1;
        x = 1.0f;
    } else {
        x = -1.0f;
        updown = 0;
    }
    ALOGI("WFD UIBC SCROLL updown(%d) action=%d", updown, data[6]);
    if (data[6] == UIBC_MOUSE_VSCROLL)
        mClient->onUibcData(ANDROID_ACTION_SCROLL, x, 0, updown);
    else
        mClient->onUibcData(ANDROID_ACTION_SCROLL, 0, x, updown);

}

void WifiDisplaySource::parseUIBCkeyEvent(const uint8_t *data) {

    if (!checkUIBCtimeStamp(data))
        return;
    if (data[3] != 10) {
        ALOGE("parseUIBCkeyEvent length err!!");
        return;
    }
    int action = data[6];
    int keycode = data[8];

    ALOGI("WFD UIBC KEY action=%d keycode=%d", action, keycode);

    mClient->onUibcData(action, 0, 0, keycode);



}

status_t WifiDisplaySource::parseUIBC(const uint8_t *data) {
    switch (data[6]) {
        case TOUCH_ACTION_DOWN:
        {
            parseUIBCtouchEvent(data);
            break;
        }
        case TOUCH_ACTION_UP:
        {
            parseUIBCtouchEvent(data);
            break;
        }
        case TOUCH_ACTION_MOVE:
        {
            parseUIBCtouchEvent(data);
            break;
        }
        case KEY_ACTION_DOWN:
        {
            parseUIBCkeyEvent(data);
            break;
        }
        case KEY_ACTION_UP:
        {
            parseUIBCkeyEvent(data);
            break;
        }
        case UIBC_MOUSE_HSCROLL:
        {
            parseUIBCscrollEvent(data);
            break;
        }
        case UIBC_MOUSE_VSCROLL:
        {
            parseUIBCscrollEvent(data);
            break;
        }
        default:
            ALOGW("On UIBC parse meet unknown style");

    }

    return OK;
}

status_t WifiDisplaySource::onUIBCData(const sp<ABuffer> &buffer) {
    const uint8_t *data = buffer->data();

    switch (data[1]) {
        case INPUT_CATEGORY_GENERIC: //Generic category
            {
                parseUIBC(data);
                break;
            }

        case INPUT_CATEGORY_HIDC:
            break;

        default:
            {
                ALOGW("Unknown RTCP packet type %u",(unsigned)data[1]);
                break;
            }
    }
    return OK;
}

void WifiDisplaySource::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatStart:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            AString iface;
            CHECK(msg->findString("iface", &iface));

            status_t err = OK;

            ssize_t colonPos = iface.find(":");

            unsigned long port;

            if (colonPos >= 0) {
                const char *s = iface.c_str() + colonPos + 1;

                char *end;
                port = strtoul(s, &end, 10);

                if (end == s || *end != '\0' || port > 65535) {
                    err = -EINVAL;
                } else {
                    iface.erase(colonPos, iface.size() - colonPos);
                }
            } else {
                port = kWifiDisplayDefaultPort;
            }

            if (err == OK) {
                if (inet_aton(iface.c_str(), &mInterfaceAddr) != 0) {
                    sp<AMessage> notify = new AMessage(kWhatRTSPNotify, this);

                    err = mNetSession->createRTSPServer(
                            mInterfaceAddr, port, notify, &mSessionID);
                } else {
                    err = -EINVAL;
                }
            }

            mState = AWAITING_CLIENT_CONNECTION;

            sp<AMessage> response = new AMessage;
            response->setInt32("err", err);
            response->postReply(replyID);
            break;
        }

       case kWhatUIBCNotify:
        {
            int32_t reason;
            CHECK(msg->findInt32("reason", &reason));
            ALOGI("get uibc notify reason=%d", reason);
            switch (reason) {
                case ANetworkSession::kWhatDatagram:
                {
                    int32_t sessionID;
                    CHECK(msg->findInt32("sessionID", &sessionID));

                    sp<ABuffer> data;
                    CHECK(msg->findBuffer("data", &data));
                    onUIBCData(data);
                    break;

                }

            }
            break;
        }
        case kWhatRTSPNotify:
        {
            int32_t reason;
            CHECK(msg->findInt32("reason", &reason));

            switch (reason) {
                case ANetworkSession::kWhatError:
                {
                    int32_t sessionID;
                    CHECK(msg->findInt32("sessionID", &sessionID));

                    int32_t err;
                    CHECK(msg->findInt32("err", &err));

                    AString detail;
                    CHECK(msg->findString("detail", &detail));

                    ALOGE("An error occurred in session %d (%d, '%s/%s').",
                          sessionID,
                          err,
                          detail.c_str(),
                          strerror(-err));

                    mNetSession->destroySession(sessionID);

                    if (sessionID == mClientSessionID) {
                        mClientSessionID = 0;

                        mClient->onDisplayError(
                                IRemoteDisplayClient::kDisplayErrorUnknown);
                    }
                    break;
                }

                case ANetworkSession::kWhatClientConnected:
                {
                    int32_t sessionID;
                    CHECK(msg->findInt32("sessionID", &sessionID));

                    if (mClientSessionID > 0) {
                        ALOGW("A client tried to connect, but we already "
                              "have one.");

                        mNetSession->destroySession(sessionID);
                        break;
                    }

                    CHECK_EQ(mState, AWAITING_CLIENT_CONNECTION);

                    CHECK(msg->findString("client-ip", &mClientInfo.mRemoteIP));
                    CHECK(msg->findString("server-ip", &mClientInfo.mLocalIP));

                    if (mClientInfo.mRemoteIP == mClientInfo.mLocalIP) {
                        // Disallow connections from the local interface
                        // for security reasons.
                        mNetSession->destroySession(sessionID);
                        break;
                    }

                    CHECK(msg->findInt32(
                                "server-port", &mClientInfo.mLocalPort));
                    mClientInfo.mPlaybackSessionID = -1;

                    mClientSessionID = sessionID;

                    ALOGI("We now have a client (%d) connected.", sessionID);

                    mState = AWAITING_CLIENT_SETUP;

                    status_t err = sendM1(sessionID);
                    CHECK_EQ(err, (status_t)OK);
                    break;
                }

                case ANetworkSession::kWhatData:
                {
                    status_t err = onReceiveClientData(msg);

                    if (err != OK) {
                        mClient->onDisplayError(
                                IRemoteDisplayClient::kDisplayErrorUnknown);
                    }

#if 0
                    // testing only.
                    char val[PROPERTY_VALUE_MAX];
                    if (property_get("media.wfd.trigger", val, NULL)) {
                        if (!strcasecmp(val, "pause") && mState == PLAYING) {
                            mState = PLAYING_TO_PAUSED;
                            sendTrigger(mClientSessionID, TRIGGER_PAUSE);
                        } else if (!strcasecmp(val, "play")
                                    && mState == PAUSED) {
                            mState = PAUSED_TO_PLAYING;
                            sendTrigger(mClientSessionID, TRIGGER_PLAY);
                        }
                    }
#endif
                    break;
                }

                case ANetworkSession::kWhatNetworkStall:
                {
                    break;
                }

                default:
                    TRESPASS();
            }
            break;
        }

        case kWhatStop:
        {
            CHECK(msg->senderAwaitsResponse(&mStopReplyID));

            CHECK_LT(mState, AWAITING_CLIENT_TEARDOWN);

            if (mState >= AWAITING_CLIENT_PLAY) {
                // We have a session, i.e. a previous SETUP succeeded.

                status_t err = sendTrigger(
                        mClientSessionID, TRIGGER_TEARDOWN);

                if (err == OK) {
                    mState = AWAITING_CLIENT_TEARDOWN;

                    (new AMessage(kWhatTeardownTriggerTimedOut, this))->post(
                            kTeardownTriggerTimeouSecs * 1000000ll);

                    break;
                }

                // fall through.
            }

            finishStop();
            break;
        }

        case kWhatPause:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            status_t err = OK;

            if (mState != PLAYING) {
                err = INVALID_OPERATION;
            } else {
                mState = PLAYING_TO_PAUSED;
                sendTrigger(mClientSessionID, TRIGGER_PAUSE);
            }

            sp<AMessage> response = new AMessage;
            response->setInt32("err", err);
            response->postReply(replyID);
            break;
        }

        case kWhatResume:
        {
            sp<AReplyToken> replyID;
            CHECK(msg->senderAwaitsResponse(&replyID));

            status_t err = OK;

            if (mState != PAUSED) {
                err = INVALID_OPERATION;
            } else {
                mState = PAUSED_TO_PLAYING;
                sendTrigger(mClientSessionID, TRIGGER_PLAY);
            }

            sp<AMessage> response = new AMessage;
            response->setInt32("err", err);
            response->postReply(replyID);
            break;
        }

        case kWhatReapDeadClients:
        {
            mReaperPending = false;

            if (mClientSessionID == 0
                    || mClientInfo.mPlaybackSession == NULL) {
                break;
            }

            if (mClientInfo.mPlaybackSession->getLastLifesignUs()
                    + kPlaybackSessionTimeoutUs < ALooper::GetNowUs()) {
                ALOGI("playback session timed out, reaping.");

                mNetSession->destroySession(mClientSessionID);
                mClientSessionID = 0;

                mClient->onDisplayError(
                        IRemoteDisplayClient::kDisplayErrorUnknown);
            } else {
                scheduleReaper();
            }
            break;
        }

        case kWhatPlaybackSessionNotify:
        {
            int32_t playbackSessionID;
            CHECK(msg->findInt32("playbackSessionID", &playbackSessionID));

            int32_t what;
            CHECK(msg->findInt32("what", &what));

            if (what == PlaybackSession::kWhatSessionDead) {
                ALOGI("playback session wants to quit.");

                mClient->onDisplayError(
                        IRemoteDisplayClient::kDisplayErrorUnknown);
            } else if (what == PlaybackSession::kWhatSessionEstablished) {
                mPlaybackSessionEstablished = true;

                if (mClient != NULL) {
                    if (!mSinkSupportsVideo) {
                        mClient->onDisplayConnected(
                                NULL,  // SurfaceTexture
                                0, // width,
                                0, // height,
                                mUsingHDCP
                                    ? IRemoteDisplayClient::kDisplayFlagSecure
                                    : 0,
                                0);
                    } else {
                        size_t width, height;

                        CHECK(VideoFormats::GetConfiguration(
                                    mChosenVideoResolutionType,
                                    mChosenVideoResolutionIndex,
                                    &width,
                                    &height,
                                    NULL /* framesPerSecond */,
                                    NULL /* interlaced */));
                        ALOGI("get configuration width=%d height=%d", width,height);
                        mClient->onDisplayConnected(
                                mClientInfo.mPlaybackSession
                                    ->getSurfaceTexture(),
                                width,
                                height,
                                mUsingHDCP
                                    ? IRemoteDisplayClient::kDisplayFlagSecure
                                    : 0,
                                playbackSessionID);
                    }
                }

                finishPlay();

                if (mState == ABOUT_TO_PLAY) {
                    mState = PLAYING;
                }
            } else if (what == PlaybackSession::kWhatSessionDestroyed) {
                disconnectClient2();
            } else {
                CHECK_EQ(what, PlaybackSession::kWhatBinaryData);

                int32_t channel;
                CHECK(msg->findInt32("channel", &channel));

                sp<ABuffer> data;
                CHECK(msg->findBuffer("data", &data));

                CHECK_LE(channel, 0xffu);
                CHECK_LE(data->size(), 0xffffu);

                int32_t sessionID;
                CHECK(msg->findInt32("sessionID", &sessionID));

                char header[4];
                header[0] = '$';
                header[1] = channel;
                header[2] = data->size() >> 8;
                header[3] = data->size() & 0xff;

                mNetSession->sendRequest(
                        sessionID, header, sizeof(header));

                mNetSession->sendRequest(
                        sessionID, data->data(), data->size());
            }
            break;
        }

        case kWhatKeepAlive:
        {
            int32_t sessionID;
            CHECK(msg->findInt32("sessionID", &sessionID));

            if (mClientSessionID != sessionID) {
                // Obsolete event, client is already gone.
                break;
            }

            sendM16(sessionID);
            break;
        }

        case kWhatTeardownTriggerTimedOut:
        {
            if (mState == AWAITING_CLIENT_TEARDOWN) {
                ALOGI("TEARDOWN trigger timed out, forcing disconnection.");

                CHECK(mStopReplyID != NULL);
                finishStop();
                break;
            }
            break;
        }

        case kWhatHDCPNotify:
        {
            int32_t msgCode, ext1, ext2;
            CHECK(msg->findInt32("msg", &msgCode));
            CHECK(msg->findInt32("ext1", &ext1));
            CHECK(msg->findInt32("ext2", &ext2));

            ALOGI("Saw HDCP notification code %d, ext1 %d, ext2 %d",
                    msgCode, ext1, ext2);

            switch (msgCode) {
                case HDCPModule::HDCP_INITIALIZATION_COMPLETE:
                {
                    mHDCPInitializationComplete = true;

                    if (mSetupTriggerDeferred) {
                        mSetupTriggerDeferred = false;

                        sendTrigger(mClientSessionID, TRIGGER_SETUP);
                    }
                    break;
                }

                case HDCPModule::HDCP_SHUTDOWN_COMPLETE:
                case HDCPModule::HDCP_SHUTDOWN_FAILED:
                {
                    // Ugly hack to make sure that the call to
                    // HDCPObserver::notify is completely handled before
                    // we clear the HDCP instance and unload the shared
                    // library :(
                    (new AMessage(kWhatFinishStop2, this))->post(300000ll);
                    break;
                }

                default:
                {
                    ALOGE("HDCP failure, shutting down.");

                    mClient->onDisplayError(
                            IRemoteDisplayClient::kDisplayErrorUnknown);
                    break;
                }
            }
            break;
        }

        case kWhatFinishStop2:
        {
            finishStop2();
            break;
        }

        default:
            TRESPASS();
    }
}

void WifiDisplaySource::registerResponseHandler(
        int32_t sessionID, int32_t cseq, HandleRTSPResponseFunc func) {
    ResponseID id;
    id.mSessionID = sessionID;
    id.mCSeq = cseq;
    mResponseHandlers.add(id, func);
}

status_t WifiDisplaySource::sendM1(int32_t sessionID) {
    AString request = "OPTIONS * RTSP/1.0\r\n";
    AppendCommonResponse(&request, mNextCSeq);

    request.append(
            "Require: org.wfa.wfd1.0\r\n"
            "\r\n");

    status_t err =
        mNetSession->sendRequest(sessionID, request.c_str(), request.size());

    if (err != OK) {
        return err;
    }

    registerResponseHandler(
            sessionID, mNextCSeq, &WifiDisplaySource::onReceiveM1Response);

    ++mNextCSeq;

    return OK;
}

status_t WifiDisplaySource::sendM3(int32_t sessionID) {
    AString body =
        "wfd_content_protection\r\n"
        "wfd_uibc_capability\r\n"
        "wfd_video_formats\r\n"
        "wfd_audio_codecs\r\n"
        "wfd_client_rtp_ports\r\n";

    AString request = "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n";
    AppendCommonResponse(&request, mNextCSeq);

    request.append("Content-Type: text/parameters\r\n");
    request.append(AStringPrintf("Content-Length: %d\r\n", body.size()));
    request.append("\r\n");
    request.append(body);

    status_t err =
        mNetSession->sendRequest(sessionID, request.c_str(), request.size());

    if (err != OK) {
        return err;
    }

    registerResponseHandler(
            sessionID, mNextCSeq, &WifiDisplaySource::onReceiveM3Response);

    ++mNextCSeq;

    return OK;
}

status_t WifiDisplaySource::sendM4(int32_t sessionID) {
    CHECK_EQ(sessionID, mClientSessionID);

    AString body;

    if (mSinkSupportsVideo) {
        body.append("wfd_video_formats: ");

        VideoFormats chosenVideoFormat;
        chosenVideoFormat.disableAll();
        chosenVideoFormat.setNativeResolution(
                mChosenVideoResolutionType, mChosenVideoResolutionIndex);
        chosenVideoFormat.setProfileLevel(
                mChosenVideoResolutionType, mChosenVideoResolutionIndex,
                mChosenVideoProfile, mChosenVideoLevel);

        body.append(chosenVideoFormat.getFormatSpec(true /* forM4Message */));
        body.append("\r\n");
    }

    if (mSinkSupportsAudio) {
        body.append(
                AStringPrintf("wfd_audio_codecs: %s\r\n",
                             (mUsingPCMAudio
                                ? "LPCM 00000002 00" // 2 ch PCM 48kHz
                                : "AAC 00000001 00")));  // 2 ch AAC 48kHz
    }

    body.append(
            AStringPrintf(
                "wfd_presentation_URL: rtsp://%s/wfd1.0/streamid=0 none\r\n",
                mClientInfo.mLocalIP.c_str()));

    body.append(
            AStringPrintf(
                "wfd_client_rtp_ports: %s\r\n", mWfdClientRtpPorts.c_str()));
    if (mSinkSupportsUIBC) {
        body.append(
                AStringPrintf(
                    "wfd_uibc_capability: input_category_list=GENERIC;"
                    "generic_cap_list=Mouse,SingleTouch;"
                    "hidc_cap_list=none;"
                    "port=%d\r\n", DEFAULT_UIBC_PORT)
                );
        body.append(
                AStringPrintf(
                    "wfd_uibc_setting: %s\r\n", "enable")
                );

    }
    AString request = "SET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n";
    AppendCommonResponse(&request, mNextCSeq);

    request.append("Content-Type: text/parameters\r\n");
    request.append(AStringPrintf("Content-Length: %d\r\n", body.size()));
    request.append("\r\n");
    request.append(body);

    status_t err =
        mNetSession->sendRequest(sessionID, request.c_str(), request.size());

    if (err != OK) {
        return err;
    }

    registerResponseHandler(
            sessionID, mNextCSeq, &WifiDisplaySource::onReceiveM4Response);

    ++mNextCSeq;

    return OK;
}

status_t WifiDisplaySource::sendTrigger(
        int32_t sessionID, TriggerType triggerType) {
    AString body = "wfd_trigger_method: ";
    switch (triggerType) {
        case TRIGGER_SETUP:
            body.append("SETUP");
            break;
        case TRIGGER_TEARDOWN:
            ALOGI("Sending TEARDOWN trigger.");
            body.append("TEARDOWN");
            break;
        case TRIGGER_PAUSE:
            body.append("PAUSE");
            break;
        case TRIGGER_PLAY:
            body.append("PLAY");
            break;
        default:
            TRESPASS();
    }

    body.append("\r\n");

    AString request = "SET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n";
    AppendCommonResponse(&request, mNextCSeq);

    request.append("Content-Type: text/parameters\r\n");
    request.append(AStringPrintf("Content-Length: %d\r\n", body.size()));
    request.append("\r\n");
    request.append(body);

    status_t err =
        mNetSession->sendRequest(sessionID, request.c_str(), request.size());

    if (err != OK) {
        return err;
    }

    registerResponseHandler(
            sessionID, mNextCSeq, &WifiDisplaySource::onReceiveM5Response);

    ++mNextCSeq;

    return OK;
}

status_t WifiDisplaySource::sendM16(int32_t sessionID) {
    AString request = "GET_PARAMETER rtsp://localhost/wfd1.0 RTSP/1.0\r\n";
    AppendCommonResponse(&request, mNextCSeq);

    CHECK_EQ(sessionID, mClientSessionID);
    request.append(
            AStringPrintf("Session: %d\r\n", mClientInfo.mPlaybackSessionID));
    request.append("\r\n");  // Empty body

    status_t err =
        mNetSession->sendRequest(sessionID, request.c_str(), request.size());

    if (err != OK) {
        return err;
    }

    registerResponseHandler(
            sessionID, mNextCSeq, &WifiDisplaySource::onReceiveM16Response);

    ++mNextCSeq;

    scheduleKeepAlive(sessionID);

    return OK;
}

status_t WifiDisplaySource::onReceiveM1Response(
        int32_t /* sessionID */, const sp<ParsedMessage> &msg) {
    int32_t statusCode;
    if (!msg->getStatusCode(&statusCode)) {
        return ERROR_MALFORMED;
    }

    if (statusCode != 200) {
        return ERROR_UNSUPPORTED;
    }

    return OK;
}

// sink_audio_list := ("LPCM"|"AAC"|"AC3" HEXDIGIT*8 HEXDIGIT*2)
//                       (", " sink_audio_list)*
static void GetAudioModes(const char *s, const char *prefix, uint32_t *modes) {
    *modes = 0;

    size_t prefixLen = strlen(prefix);

    while (*s != '0') {
        if (!strncmp(s, prefix, prefixLen) && s[prefixLen] == ' ') {
            unsigned latency;
            if (sscanf(&s[prefixLen + 1], "%08x %02x", modes, &latency) != 2) {
                *modes = 0;
            }

            return;
        }

        char *commaPos = strchr(s, ',');
        if (commaPos != NULL) {
            s = commaPos + 1;

            while (isspace(*s)) {
                ++s;
            }
        } else {
            break;
        }
    }
}

status_t WifiDisplaySource::onReceiveM3Response(
        int32_t sessionID, const sp<ParsedMessage> &msg) {
    int32_t statusCode;
    if (!msg->getStatusCode(&statusCode)) {
        return ERROR_MALFORMED;
    }

    if (statusCode != 200) {
        return ERROR_UNSUPPORTED;
    }

    sp<Parameters> params =
        Parameters::Parse(msg->getContent(), strlen(msg->getContent()));

    if (params == NULL) {
        return ERROR_MALFORMED;
    }

    AString value;
    if (!params->findParameter("wfd_client_rtp_ports", &value)) {
        ALOGE("Sink doesn't report its choice of wfd_client_rtp_ports.");
        return ERROR_MALFORMED;
    }

    unsigned port0 = 0, port1 = 0;
    if (sscanf(value.c_str(),
               "RTP/AVP/UDP;unicast %u %u mode=play",
               &port0,
               &port1) == 2
        || sscanf(value.c_str(),
               "RTP/AVP/TCP;unicast %u %u mode=play",
               &port0,
               &port1) == 2) {
            if (port0 == 0 || port0 > 65535 || port1 != 0) {
                ALOGE("Sink chose its wfd_client_rtp_ports poorly (%s)",
                      value.c_str());

                return ERROR_MALFORMED;
            }
    } else if (strcmp(value.c_str(), "RTP/AVP/TCP;interleaved mode=play")) {
        ALOGE("Unsupported value for wfd_client_rtp_ports (%s)",
              value.c_str());

        return ERROR_UNSUPPORTED;
    }

    mWfdClientRtpPorts = value;
    mChosenRTPPort = port0;

    if (!params->findParameter("wfd_video_formats", &value)) {
        ALOGE("Sink doesn't report its choice of wfd_video_formats.");
        return ERROR_MALFORMED;
    }

    mSinkSupportsVideo = false;

    if  (!(value == "none")) {
        mSinkSupportsVideo = true;
        if (!mSupportedSinkVideoFormats.parseFormatSpec(value.c_str())) {
            ALOGE("Failed to parse sink provided wfd_video_formats (%s)",
                  value.c_str());

            return ERROR_MALFORMED;
        }

        if (!VideoFormats::PickBestFormat(
                    mSupportedSinkVideoFormats,
                    mSupportedSourceVideoFormats,
                    &mChosenVideoResolutionType,
                    &mChosenVideoResolutionIndex,
                    &mChosenVideoProfile,
                    &mChosenVideoLevel)) {
            ALOGE("Sink and source share no commonly supported video "
                  "formats.");

            return ERROR_UNSUPPORTED;
        }

        size_t width, height, framesPerSecond;
        bool interlaced;
        CHECK(VideoFormats::GetConfiguration(
                    mChosenVideoResolutionType,
                    mChosenVideoResolutionIndex,
                    &width,
                    &height,
                    &framesPerSecond,
                    &interlaced));

        ALOGI("Picked video resolution %zu x %zu %c%zu Type: %u Index : %u ",
              width, height, interlaced ? 'i' : 'p', framesPerSecond, mChosenVideoResolutionType, mChosenVideoResolutionIndex);

        VideoFormats::ResolutionType mResolutionType;
        size_t mResolutionIndex;

        mSupportedSinkVideoFormats.getNativeResolution(&mResolutionType,&mResolutionIndex);

        VideoFormats::GetConfiguration(
                       mResolutionType,
                       mResolutionIndex,
                       &mResolution_NativeW,
                       &mResolution_NativeH,
                       NULL, NULL);

        mVideoHeight = height;
        mVideoWidth = width;
        calc_uibc_parameter(width,height);

        ALOGI("Picked AVC profile %d, level %d",
              mChosenVideoProfile, mChosenVideoLevel);
    } else {
        ALOGI("Sink doesn't support video at all.");
    }

    if (!params->findParameter("wfd_audio_codecs", &value)) {
        ALOGE("Sink doesn't report its choice of wfd_audio_codecs.");
        return ERROR_MALFORMED;
    }

    mSinkSupportsAudio = false;

    if  (!(value == "none")) {
        mSinkSupportsAudio = true;

        uint32_t modes;
        GetAudioModes(value.c_str(), "AAC", &modes);

        bool supportsAAC = (modes & 1) != 0;  // AAC 2ch 48kHz

        GetAudioModes(value.c_str(), "LPCM", &modes);

        bool supportsPCM = (modes & 2) != 0;  // LPCM 2ch 48kHz

        char val[PROPERTY_VALUE_MAX];
        if (supportsPCM
                && property_get("media.wfd.use-pcm-audio", val, NULL)
                && (!strcasecmp("true", val) || !strcmp("1", val))) {
            ALOGI("Using PCM audio.");
            mUsingPCMAudio = true;
        } else if (supportsAAC) {
            ALOGI("Using AAC audio.");
            mUsingPCMAudio = false;
        } else if (supportsPCM) {
            ALOGI("Using PCM audio.");
            mUsingPCMAudio = true;
        } else {
            ALOGI("Sink doesn't support an audio format we do.");
            return ERROR_UNSUPPORTED;
        }
    } else {
        ALOGI("Sink doesn't support audio at all.");
    }

    if (!mSinkSupportsVideo && !mSinkSupportsAudio) {
        ALOGE("Sink supports neither video nor audio...");
        return ERROR_UNSUPPORTED;
    }

    mSinkSupportsUIBC = false;
    if (!params->findParameter("wfd_uibc_capability", &value)) {
        ALOGI("Sink doesn't appear to support uibc.");
    } else if (value == "none") {
        ALOGI("Sink does not support uibc.");
    } else {
        mSinkSupportsUIBC = true;
        ALOGI("Sink support uibc.");
        status_t err = startUibc(DEFAULT_UIBC_PORT);
        if (err != OK) {
            ALOGI("start uibc channel failed");
        }
    }
    mUsingHDCP = false;
    if (!params->findParameter("wfd_content_protection", &value)) {
        ALOGI("Sink doesn't appear to support content protection.");
    } else if (value == "none") {
        ALOGI("Sink does not support content protection.");
    } else {
        mUsingHDCP = true;

        bool isHDCP2_0 = false;
        if (value.startsWith("HDCP2.0 ")) {
            isHDCP2_0 = true;
        } else if (!value.startsWith("HDCP2.1 ")) {
            ALOGE("malformed wfd_content_protection: '%s'", value.c_str());

            return ERROR_MALFORMED;
        }

        int32_t hdcpPort;
        if (!ParsedMessage::GetInt32Attribute(
                    value.c_str() + 8, "port", &hdcpPort)
                || hdcpPort < 1 || hdcpPort > 65535) {
            return ERROR_MALFORMED;
        }

        mIsHDCP2_0 = isHDCP2_0;
        mHDCPPort = hdcpPort;

        status_t err = makeHDCP();
        if (err != OK) {
            ALOGE("Unable to instantiate HDCP component. "
                  "Not using HDCP after all.");

            mUsingHDCP = false;
        }
    }
    return sendM4(sessionID);
}

status_t WifiDisplaySource::onReceiveM4Response(
        int32_t sessionID, const sp<ParsedMessage> &msg) {
    int32_t statusCode;
    if (!msg->getStatusCode(&statusCode)) {
        return ERROR_MALFORMED;
    }

    if (statusCode != 200) {
        return ERROR_UNSUPPORTED;
    }

    if (mUsingHDCP && !mHDCPInitializationComplete) {
        ALOGI("Deferring SETUP trigger until HDCP initialization completes.");

        mSetupTriggerDeferred = true;
        return OK;
    }

    return sendTrigger(sessionID, TRIGGER_SETUP);
}

status_t WifiDisplaySource::onReceiveM5Response(
        int32_t /* sessionID */, const sp<ParsedMessage> &msg) {
    int32_t statusCode;
    if (!msg->getStatusCode(&statusCode)) {
        return ERROR_MALFORMED;
    }

    if (statusCode != 200) {
        return ERROR_UNSUPPORTED;
    }

    return OK;
}

status_t WifiDisplaySource::onReceiveM16Response(
        int32_t sessionID, const sp<ParsedMessage> & /* msg */) {
    // If only the response was required to include a "Session:" header...

    CHECK_EQ(sessionID, mClientSessionID);

    if (mClientInfo.mPlaybackSession != NULL) {
        mClientInfo.mPlaybackSession->updateLiveness();
    }

    return OK;
}

void WifiDisplaySource::scheduleReaper() {
    if (mReaperPending) {
        return;
    }

    mReaperPending = true;
    (new AMessage(kWhatReapDeadClients, this))->post(kReaperIntervalUs);
}

void WifiDisplaySource::scheduleKeepAlive(int32_t sessionID) {
    // We need to send updates at least 5 secs before the timeout is set to
    // expire, make sure the timeout is greater than 5 secs to begin with.
    CHECK_GT(kPlaybackSessionTimeoutUs, 5000000ll);

    sp<AMessage> msg = new AMessage(kWhatKeepAlive, this);
    msg->setInt32("sessionID", sessionID);
    msg->post(kPlaybackSessionTimeoutUs - 5000000ll);
}

status_t WifiDisplaySource::onReceiveClientData(const sp<AMessage> &msg) {
    int32_t sessionID;
    CHECK(msg->findInt32("sessionID", &sessionID));

    sp<RefBase> obj;
    CHECK(msg->findObject("data", &obj));

    sp<ParsedMessage> data =
        static_cast<ParsedMessage *>(obj.get());

    ALOGV("session %d received '%s'",
          sessionID, data->debugString().c_str());

    AString method;
    AString uri;
    data->getRequestField(0, &method);

    int32_t cseq;
    if (!data->findInt32("cseq", &cseq)) {
        sendErrorResponse(sessionID, "400 Bad Request", -1 /* cseq */);
        return ERROR_MALFORMED;
    }

    if (method.startsWith("RTSP/")) {
        // This is a response.

        ResponseID id;
        id.mSessionID = sessionID;
        id.mCSeq = cseq;

        ssize_t index = mResponseHandlers.indexOfKey(id);

        if (index < 0) {
            ALOGW("Received unsolicited server response, cseq %d", cseq);
            return ERROR_MALFORMED;
        }

        HandleRTSPResponseFunc func = mResponseHandlers.valueAt(index);
        mResponseHandlers.removeItemsAt(index);

        status_t err = (this->*func)(sessionID, data);

        if (err != OK) {
            ALOGW("Response handler for session %d, cseq %d returned "
                  "err %d (%s)",
                  sessionID, cseq, err, strerror(-err));

            return err;
        }

        return OK;
    }

    AString version;
    data->getRequestField(2, &version);
    if (!(version == AString("RTSP/1.0"))) {
        sendErrorResponse(sessionID, "505 RTSP Version not supported", cseq);
        return ERROR_UNSUPPORTED;
    }

    status_t err;
    if (method == "OPTIONS") {
        err = onOptionsRequest(sessionID, cseq, data);
    } else if (method == "SETUP") {
        err = onSetupRequest(sessionID, cseq, data);
    } else if (method == "PLAY") {
        err = onPlayRequest(sessionID, cseq, data);
    } else if (method == "PAUSE") {
        err = onPauseRequest(sessionID, cseq, data);
    } else if (method == "TEARDOWN") {
        err = onTeardownRequest(sessionID, cseq, data);
    } else if (method == "GET_PARAMETER") {
        err = onGetParameterRequest(sessionID, cseq, data);
    } else if (method == "SET_PARAMETER") {
        err = onSetParameterRequest(sessionID, cseq, data);
    } else {
        sendErrorResponse(sessionID, "405 Method Not Allowed", cseq);

        err = ERROR_UNSUPPORTED;
    }

    return err;
}

status_t WifiDisplaySource::onOptionsRequest(
        int32_t sessionID,
        int32_t cseq,
        const sp<ParsedMessage> &data) {
    int32_t playbackSessionID;
    sp<PlaybackSession> playbackSession =
        findPlaybackSession(data, &playbackSessionID);

    if (playbackSession != NULL) {
        playbackSession->updateLiveness();
    }

    AString response = "RTSP/1.0 200 OK\r\n";
    AppendCommonResponse(&response, cseq);

    response.append(
            "Public: org.wfa.wfd1.0, SETUP, TEARDOWN, PLAY, PAUSE, "
            "GET_PARAMETER, SET_PARAMETER\r\n");

    response.append("\r\n");

    status_t err = mNetSession->sendRequest(sessionID, response.c_str());

    if (err == OK) {
        err = sendM3(sessionID);
    }

    return err;
}

status_t WifiDisplaySource::onSetupRequest(
        int32_t sessionID,
        int32_t cseq,
        const sp<ParsedMessage> &data) {
    CHECK_EQ(sessionID, mClientSessionID);
    if (mClientInfo.mPlaybackSessionID != -1) {
        // We only support a single playback session per client.
        // This is due to the reversed keep-alive design in the wfd specs...
        sendErrorResponse(sessionID, "400 Bad Request", cseq);
        return ERROR_MALFORMED;
    }

    AString transport;
    if (!data->findString("transport", &transport)) {
        sendErrorResponse(sessionID, "400 Bad Request", cseq);
        return ERROR_MALFORMED;
    }

    RTPSender::TransportMode rtpMode = RTPSender::TRANSPORT_UDP;

    int clientRtp, clientRtcp;
    if (transport.startsWith("RTP/AVP/TCP;")) {
        AString interleaved;
        if (ParsedMessage::GetAttribute(
                    transport.c_str(), "interleaved", &interleaved)
                && sscanf(interleaved.c_str(), "%d-%d",
                          &clientRtp, &clientRtcp) == 2) {
            rtpMode = RTPSender::TRANSPORT_TCP_INTERLEAVED;
        } else {
            bool badRequest = false;

            AString clientPort;
            if (!ParsedMessage::GetAttribute(
                        transport.c_str(), "client_port", &clientPort)) {
                badRequest = true;
            } else if (sscanf(clientPort.c_str(), "%d-%d",
                              &clientRtp, &clientRtcp) == 2) {
            } else if (sscanf(clientPort.c_str(), "%d", &clientRtp) == 1) {
                // No RTCP.
                clientRtcp = -1;
            } else {
                badRequest = true;
            }

            if (badRequest) {
                sendErrorResponse(sessionID, "400 Bad Request", cseq);
                return ERROR_MALFORMED;
            }

            rtpMode = RTPSender::TRANSPORT_TCP;
        }
    } else if (transport.startsWith("RTP/AVP;unicast;")
            || transport.startsWith("RTP/AVP/UDP;unicast;")) {
        bool badRequest = false;

        AString clientPort;
        if (!ParsedMessage::GetAttribute(
                    transport.c_str(), "client_port", &clientPort)) {
            badRequest = true;
        } else if (sscanf(clientPort.c_str(), "%d-%d",
                          &clientRtp, &clientRtcp) == 2) {
        } else if (sscanf(clientPort.c_str(), "%d", &clientRtp) == 1) {
            // No RTCP.
            clientRtcp = -1;
        } else {
            badRequest = true;
        }

        if (badRequest) {
            sendErrorResponse(sessionID, "400 Bad Request", cseq);
            return ERROR_MALFORMED;
        }
#if 1
    // The older LG dongles doesn't specify client_port=xxx apparently.
    } else if (transport == "RTP/AVP/UDP;unicast") {
        clientRtp = 19000;
        clientRtcp = -1;
#endif
    } else {
        sendErrorResponse(sessionID, "461 Unsupported Transport", cseq);
        return ERROR_UNSUPPORTED;
    }

    int32_t playbackSessionID = makeUniquePlaybackSessionID();

    sp<AMessage> notify = new AMessage(kWhatPlaybackSessionNotify, this);
    notify->setInt32("playbackSessionID", playbackSessionID);
    notify->setInt32("sessionID", sessionID);

    sp<PlaybackSession> playbackSession =
        new PlaybackSession(
                mOpPackageName, mNetSession, notify, mInterfaceAddr, mHDCP, mMediaPath.c_str());

    looper()->registerHandler(playbackSession);

    AString uri;
    data->getRequestField(1, &uri);

    if (strncasecmp("rtsp://", uri.c_str(), 7)) {
        sendErrorResponse(sessionID, "400 Bad Request", cseq);
        return ERROR_MALFORMED;
    }

    if (!(uri.startsWith("rtsp://") && uri.endsWith("/wfd1.0/streamid=0"))) {
        sendErrorResponse(sessionID, "404 Not found", cseq);
        return ERROR_MALFORMED;
    }

    RTPSender::TransportMode rtcpMode = RTPSender::TRANSPORT_UDP;
    if (clientRtcp < 0) {
        rtcpMode = RTPSender::TRANSPORT_NONE;
    }

    status_t err = playbackSession->init(
            mClientInfo.mRemoteIP.c_str(),
            clientRtp,
            rtpMode,
            clientRtcp,
            rtcpMode,
            mSinkSupportsAudio,
            mUsingPCMAudio,
            mSinkSupportsVideo,
            mChosenVideoResolutionType,
            mChosenVideoResolutionIndex,
            mChosenVideoProfile,
            mChosenVideoLevel);

    if (err != OK) {
        looper()->unregisterHandler(playbackSession->id());
        playbackSession.clear();
    }

    switch (err) {
        case OK:
            break;
        case -ENOENT:
            sendErrorResponse(sessionID, "404 Not Found", cseq);
            return err;
        default:
            sendErrorResponse(sessionID, "403 Forbidden", cseq);
            return err;
    }

    mClientInfo.mPlaybackSessionID = playbackSessionID;
    mClientInfo.mPlaybackSession = playbackSession;

    AString response = "RTSP/1.0 200 OK\r\n";
    AppendCommonResponse(&response, cseq, playbackSessionID);

    if (rtpMode == RTPSender::TRANSPORT_TCP_INTERLEAVED) {
        response.append(
                AStringPrintf(
                    "Transport: RTP/AVP/TCP;interleaved=%d-%d;",
                    clientRtp, clientRtcp));
    } else {
        int32_t serverRtp = playbackSession->getRTPPort();

        AString transportString = "UDP";
        if (rtpMode == RTPSender::TRANSPORT_TCP) {
            transportString = "TCP";
        }

        if (clientRtcp >= 0) {
            response.append(
                    AStringPrintf(
                        "Transport: RTP/AVP/%s;unicast;client_port=%d-%d;"
                        "server_port=%d-%d\r\n",
                        transportString.c_str(),
                        clientRtp, clientRtcp, serverRtp, serverRtp + 1));
        } else {
            response.append(
                    AStringPrintf(
                        "Transport: RTP/AVP/%s;unicast;client_port=%d;"
                        "server_port=%d\r\n",
                        transportString.c_str(),
                        clientRtp, serverRtp));
        }
    }

    response.append("\r\n");

    err = mNetSession->sendRequest(sessionID, response.c_str());

    if (err != OK) {
        return err;
    }

    mState = AWAITING_CLIENT_PLAY;

    scheduleReaper();
    scheduleKeepAlive(sessionID);

    return OK;
}

status_t WifiDisplaySource::onPlayRequest(
        int32_t sessionID,
        int32_t cseq,
        const sp<ParsedMessage> &data) {
    int32_t playbackSessionID;
    sp<PlaybackSession> playbackSession =
        findPlaybackSession(data, &playbackSessionID);

    if (playbackSession == NULL) {
        sendErrorResponse(sessionID, "454 Session Not Found", cseq);
        return ERROR_MALFORMED;
    }

    if (mState != AWAITING_CLIENT_PLAY
     && mState != PAUSED_TO_PLAYING
     && mState != PAUSED) {
        ALOGW("Received PLAY request but we're in state %d", mState);

        sendErrorResponse(
                sessionID, "455 Method Not Valid in This State", cseq);

        return INVALID_OPERATION;
    }

    ALOGI("Received PLAY request.");
    if (mPlaybackSessionEstablished) {
        finishPlay();
    } else {
        ALOGI("deferring PLAY request until session established.");
    }

    AString response = "RTSP/1.0 200 OK\r\n";
    AppendCommonResponse(&response, cseq, playbackSessionID);
    response.append("Range: npt=now-\r\n");
    response.append("\r\n");

    status_t err = mNetSession->sendRequest(sessionID, response.c_str());

    if (err != OK) {
        return err;
    }

    if (mState == PAUSED_TO_PLAYING || mPlaybackSessionEstablished) {
        mState = PLAYING;
        return OK;
    }

    CHECK_EQ(mState, AWAITING_CLIENT_PLAY);
    mState = ABOUT_TO_PLAY;

    return OK;
}

void WifiDisplaySource::finishPlay() {
    const sp<PlaybackSession> &playbackSession =
        mClientInfo.mPlaybackSession;

    status_t err = playbackSession->play();
    CHECK_EQ(err, (status_t)OK);
}

status_t WifiDisplaySource::onPauseRequest(
        int32_t sessionID,
        int32_t cseq,
        const sp<ParsedMessage> &data) {
    int32_t playbackSessionID;
    sp<PlaybackSession> playbackSession =
        findPlaybackSession(data, &playbackSessionID);

    if (playbackSession == NULL) {
        sendErrorResponse(sessionID, "454 Session Not Found", cseq);
        return ERROR_MALFORMED;
    }

    ALOGI("Received PAUSE request.");

    if (mState != PLAYING_TO_PAUSED && mState != PLAYING) {
        return INVALID_OPERATION;
    }

    status_t err = playbackSession->pause();
    CHECK_EQ(err, (status_t)OK);

    AString response = "RTSP/1.0 200 OK\r\n";
    AppendCommonResponse(&response, cseq, playbackSessionID);
    response.append("\r\n");

    err = mNetSession->sendRequest(sessionID, response.c_str());

    if (err != OK) {
        return err;
    }

    mState = PAUSED;

    return err;
}

status_t WifiDisplaySource::onTeardownRequest(
        int32_t sessionID,
        int32_t cseq,
        const sp<ParsedMessage> &data) {
    ALOGI("Received TEARDOWN request.");

    int32_t playbackSessionID;
    sp<PlaybackSession> playbackSession =
        findPlaybackSession(data, &playbackSessionID);

    if (playbackSession == NULL) {
        sendErrorResponse(sessionID, "454 Session Not Found", cseq);
        return ERROR_MALFORMED;
    }

    AString response = "RTSP/1.0 200 OK\r\n";
    AppendCommonResponse(&response, cseq, playbackSessionID);
    response.append("Connection: close\r\n");
    response.append("\r\n");

    mNetSession->sendRequest(sessionID, response.c_str());

    if (mState == AWAITING_CLIENT_TEARDOWN) {
        CHECK(mStopReplyID != NULL);
        finishStop();
    } else {
        mClient->onDisplayError(IRemoteDisplayClient::kDisplayErrorUnknown);
    }

    return OK;
}

void WifiDisplaySource::finishStop() {
    ALOGI("finishStop");

    mState = STOPPING;

    disconnectClientAsync();
}

void WifiDisplaySource::finishStopAfterDisconnectingClient() {
    ALOGV("finishStopAfterDisconnectingClient");

    if (mHDCP != NULL) {
        ALOGI("Initiating HDCP shutdown.");
        mHDCP->shutdownAsync();
        return;
    }

    finishStop2();
}

void WifiDisplaySource::finishStop2() {
    ALOGV("finishStop2");

    if (mHDCP != NULL) {
        mHDCP->setObserver(NULL);
        mHDCPObserver.clear();
        mHDCP.clear();
    }

    if (mSessionID != 0) {
        mNetSession->destroySession(mSessionID);
        mSessionID = 0;
    }

    if (mUibcSessionID != 0) {
        mNetSession->destroySession(mUibcSessionID);
        mUibcSessionID = 0;
   }
    ALOGI("We're stopped.");
    mState = STOPPED;

    status_t err = OK;

    sp<AMessage> response = new AMessage;
    response->setInt32("err", err);
    response->postReply(mStopReplyID);
}

status_t WifiDisplaySource::onGetParameterRequest(
        int32_t sessionID,
        int32_t cseq,
        const sp<ParsedMessage> &data) {
    int32_t playbackSessionID;
    sp<PlaybackSession> playbackSession =
        findPlaybackSession(data, &playbackSessionID);

    if (playbackSession == NULL) {
        sendErrorResponse(sessionID, "454 Session Not Found", cseq);
        return ERROR_MALFORMED;
    }

    playbackSession->updateLiveness();

    AString response = "RTSP/1.0 200 OK\r\n";
    AppendCommonResponse(&response, cseq, playbackSessionID);
    response.append("\r\n");

    status_t err = mNetSession->sendRequest(sessionID, response.c_str());
    return err;
}

status_t WifiDisplaySource::onSetParameterRequest(
        int32_t sessionID,
        int32_t cseq,
        const sp<ParsedMessage> &data) {
    int32_t playbackSessionID;
    sp<PlaybackSession> playbackSession =
        findPlaybackSession(data, &playbackSessionID);

    if (playbackSession == NULL) {
        sendErrorResponse(sessionID, "454 Session Not Found", cseq);
        return ERROR_MALFORMED;
    }

    if (strstr(data->getContent(), "wfd_idr_request\r\n")) {
        playbackSession->requestIDRFrame();
    }

    playbackSession->updateLiveness();

    AString response = "RTSP/1.0 200 OK\r\n";
    AppendCommonResponse(&response, cseq, playbackSessionID);
    response.append("\r\n");

    status_t err = mNetSession->sendRequest(sessionID, response.c_str());
    return err;
}

// static
void WifiDisplaySource::AppendCommonResponse(
        AString *response, int32_t cseq, int32_t playbackSessionID) {
    time_t now = time(NULL);
    struct tm *now2 = gmtime(&now);
    char buf[128];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %z", now2);

    response->append("Date: ");
    response->append(buf);
    response->append("\r\n");

    response->append(AStringPrintf("Server: %s\r\n", sUserAgent.c_str()));

    if (cseq >= 0) {
        response->append(AStringPrintf("CSeq: %d\r\n", cseq));
    }

    if (playbackSessionID >= 0ll) {
        response->append(
                AStringPrintf(
                    "Session: %d;timeout=%lld\r\n",
                    playbackSessionID, kPlaybackSessionTimeoutSecs));
    }
}

void WifiDisplaySource::sendErrorResponse(
        int32_t sessionID,
        const char *errorDetail,
        int32_t cseq) {
    AString response;
    response.append("RTSP/1.0 ");
    response.append(errorDetail);
    response.append("\r\n");

    AppendCommonResponse(&response, cseq);

    response.append("\r\n");

    mNetSession->sendRequest(sessionID, response.c_str());
}

int32_t WifiDisplaySource::makeUniquePlaybackSessionID() const {
    return rand();
}

sp<WifiDisplaySource::PlaybackSession> WifiDisplaySource::findPlaybackSession(
        const sp<ParsedMessage> &data, int32_t *playbackSessionID) const {
    if (!data->findInt32("session", playbackSessionID)) {
        // XXX the older dongles do not always include a "Session:" header.
        *playbackSessionID = mClientInfo.mPlaybackSessionID;
        return mClientInfo.mPlaybackSession;
    }

    if (*playbackSessionID != mClientInfo.mPlaybackSessionID) {
        return NULL;
    }

    return mClientInfo.mPlaybackSession;
}

void WifiDisplaySource::disconnectClientAsync() {
    ALOGI("disconnectClient");

    if (mClientInfo.mPlaybackSession == NULL) {
        disconnectClient2();
        return;
    }

    if (mClientInfo.mPlaybackSession != NULL) {
        ALOGI("Destroying PlaybackSession");
        mClientInfo.mPlaybackSession->destroyAsync();
    }
}

void WifiDisplaySource::disconnectClient2() {
    ALOGI("disconnectClient2");

    if (mClientInfo.mPlaybackSession != NULL) {
        looper()->unregisterHandler(mClientInfo.mPlaybackSession->id());
        mClientInfo.mPlaybackSession.clear();
    }

    if (mClientSessionID != 0) {
        mNetSession->destroySession(mClientSessionID);
        mClientSessionID = 0;
    }
    if (mUibcTouchFd >= 0) {
        close(mUibcTouchFd);
        mUibcTouchFd = -1;
    }

    mClient->onDisplayDisconnected();

    finishStopAfterDisconnectingClient();
}

struct WifiDisplaySource::HDCPObserver : public BnHDCPObserver {
    HDCPObserver(const sp<AMessage> &notify);

    virtual void notify(
            int msg, int ext1, int ext2, const Parcel *obj);

private:
    sp<AMessage> mNotify;

    DISALLOW_EVIL_CONSTRUCTORS(HDCPObserver);
};

WifiDisplaySource::HDCPObserver::HDCPObserver(
        const sp<AMessage> &notify)
    : mNotify(notify) {
}

void WifiDisplaySource::HDCPObserver::notify(
        int msg, int ext1, int ext2, const Parcel * /* obj */) {
    sp<AMessage> notify = mNotify->dup();
    notify->setInt32("msg", msg);
    notify->setInt32("ext1", ext1);
    notify->setInt32("ext2", ext2);
    notify->post();
}

status_t WifiDisplaySource::makeHDCP() {
    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = sm->getService(String16("media.player"));

    sp<IMediaPlayerService> service =
        interface_cast<IMediaPlayerService>(binder);

    CHECK(service != NULL);

    mHDCP = service->makeHDCP(true /* createEncryptionModule */);

    if (mHDCP == NULL) {
        return ERROR_UNSUPPORTED;
    }

    sp<AMessage> notify = new AMessage(kWhatHDCPNotify, this);
    mHDCPObserver = new HDCPObserver(notify);

    status_t err = mHDCP->setObserver(mHDCPObserver);

    if (err != OK) {
        ALOGE("Failed to set HDCP observer.");

        mHDCPObserver.clear();
        mHDCP.clear();

        return err;
    }

    ALOGI("Initiating HDCP negotiation w/ host %s:%d",
            mClientInfo.mRemoteIP.c_str(), mHDCPPort);

    err = mHDCP->initAsync(mClientInfo.mRemoteIP.c_str(), mHDCPPort);

    if (err != OK) {
        return err;
    }

    return OK;
}

}  // namespace android

