/*
 * Copyright 2012, The Android Open Source Project
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
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

#ifndef WIFI_DISPLAY_SOURCE_H_

#define WIFI_DISPLAY_SOURCE_H_

#include "VideoFormats.h"

#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ANetworkSession.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#include <utils/String16.h>

namespace android {

struct AReplyToken;
struct IHDCP;
class IRemoteDisplayClient;
struct ParsedMessage;

// Represents the RTSP server acting as a wifi display source.
// Manages incoming connections, sets up Playback sessions as necessary.
struct WifiDisplaySource : public AHandler {
    static const unsigned kWifiDisplayDefaultPort = 7236;

    WifiDisplaySource(
            const String16 &opPackageName,
            const sp<ANetworkSession> &netSession,
            const sp<IRemoteDisplayClient> &client,
            const char *path = NULL);

    status_t start(const char *iface);
    status_t parseUIBC(const uint8_t *d);
    status_t onUIBCData(const sp<ABuffer> &buffer);
    status_t startUibc(const int32_t port);
    status_t stop();
    status_t sendtouchevent(int32_t action, int32_t x, int32_t y);
    status_t sendkeyevent(int16_t action, int16_t keycode);
    status_t pause();
    status_t resume();

protected:
    virtual ~WifiDisplaySource();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    struct PlaybackSession;
    struct HDCPObserver;

    enum State {
        INITIALIZED,
        AWAITING_CLIENT_CONNECTION,
        AWAITING_CLIENT_SETUP,
        AWAITING_CLIENT_PLAY,
        ABOUT_TO_PLAY,
        PLAYING,
        PLAYING_TO_PAUSED,
        PAUSED,
        PAUSED_TO_PLAYING,
        AWAITING_CLIENT_TEARDOWN,
        STOPPING,
        STOPPED,
    };

    enum {
        kWhatStart,
        kWhatRTSPNotify,
        kWhatUIBCNotify,
        kWhatStop,
        kWhatPause,
        kWhatResume,
        kWhatReapDeadClients,
        kWhatPlaybackSessionNotify,
        kWhatKeepAlive,
        kWhatHDCPNotify,
        kWhatFinishStop2,
        kWhatTeardownTriggerTimedOut,
    };

    struct ResponseID {
        int32_t mSessionID;
        int32_t mCSeq;

        bool operator<(const ResponseID &other) const {
            return mSessionID < other.mSessionID
                || (mSessionID == other.mSessionID
                        && mCSeq < other.mCSeq);
        }
    };

    typedef status_t (WifiDisplaySource::*HandleRTSPResponseFunc)(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    static const int64_t kReaperIntervalUs = 1000000ll;

    // We request that the dongle send us a "TEARDOWN" in order to
    // perform an orderly shutdown. We're willing to wait up to 2 secs
    // for this message to arrive, after that we'll force a disconnect
    // instead.
    static const int64_t kTeardownTriggerTimeouSecs = 2;

    static const int64_t kPlaybackSessionTimeoutSecs = 30;

    static const int64_t kPlaybackSessionTimeoutUs =
        kPlaybackSessionTimeoutSecs * 1000000ll;

    static const AString sUserAgent;

    String16 mOpPackageName;

    State mState;
    VideoFormats mSupportedSourceVideoFormats;
    sp<ANetworkSession> mNetSession;
    sp<IRemoteDisplayClient> mClient;
    AString mMediaPath;
    struct in_addr mInterfaceAddr;
    int32_t mSessionID;
    int32_t mUibcSessionID;

    int32_t mUibcTouchFd;
    int32_t mUibcKeyFd;
    int32_t mAbs_x_min;
    int32_t mAbs_x_max;
    int32_t mAbs_y_min;
    int32_t mAbs_y_max;
    sp<AReplyToken> mStopReplyID;

    AString mWfdClientRtpPorts;
    int32_t mChosenRTPPort;  // extracted from "wfd_client_rtp_ports"

    bool mSinkSupportsVideo;
    VideoFormats mSupportedSinkVideoFormats;

    VideoFormats::ResolutionType mChosenVideoResolutionType;
    size_t mChosenVideoResolutionIndex;
    VideoFormats::ProfileType mChosenVideoProfile;
    VideoFormats::LevelType mChosenVideoLevel;

    bool mSinkSupportsAudio;

    bool mSinkSupportsUIBC;
    bool mUsingPCMAudio;
    int32_t mClientSessionID;

    struct ClientInfo {
        AString mRemoteIP;
        AString mLocalIP;
        int32_t mLocalPort;
        int32_t mPlaybackSessionID;
        sp<PlaybackSession> mPlaybackSession;
    };
    ClientInfo mClientInfo;

    bool mReaperPending;

    int32_t mNextCSeq;

    KeyedVector<ResponseID, HandleRTSPResponseFunc> mResponseHandlers;

    //VideoResolution
    size_t resolutionWidth;
    size_t resolutionHeigh;

    //UIBC Native and real resolution data
    size_t mResolution_RealW;
    size_t mResolution_RealH;
    size_t mResolution_NativeW;
    size_t mResolution_NativeH;

    //UIBC calc data
    float uibc_calc_data_Ss;
    float uibc_calc_data_Wbs;
    float uibc_calc_data_Hbs;
    int32_t mUibcMouseID;

    // HDCP specific section >>>>
    bool mUsingHDCP;
    bool mIsHDCP2_0;
    int32_t mHDCPPort;
    sp<IHDCP> mHDCP;
    sp<HDCPObserver> mHDCPObserver;

    bool mHDCPInitializationComplete;
    bool mSetupTriggerDeferred;

    bool mPlaybackSessionEstablished;

    //Parse UIBC data
    void parseUIBCtouchEvent(const uint8_t *data);
    void parseUIBCscrollEvent(const uint8_t *data);
    void parseUIBCkeyEvent(const uint8_t *data);
    void recalculateUibcParamaterViaOrientation();

    uint8_t getOrientation();
    uint8_t mOrientation;
    size_t mVideoWidth;
    size_t mVideoHeight;

    bool checkUIBCtimeStamp(const uint8_t *data);
    status_t makeHDCP();
    // <<<< HDCP specific section

    status_t sendM1(int32_t sessionID);
    status_t sendM3(int32_t sessionID);
    status_t sendM4(int32_t sessionID);

    enum TriggerType {
        TRIGGER_SETUP,
        TRIGGER_TEARDOWN,
        TRIGGER_PAUSE,
        TRIGGER_PLAY,
    };

    // M5
    status_t sendTrigger(int32_t sessionID, TriggerType triggerType);

    status_t sendM16(int32_t sessionID);

    status_t onReceiveM1Response(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t onReceiveM3Response(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t onReceiveM4Response(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t onReceiveM5Response(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    status_t onReceiveM16Response(
            int32_t sessionID, const sp<ParsedMessage> &msg);

    void registerResponseHandler(
            int32_t sessionID, int32_t cseq, HandleRTSPResponseFunc func);

    status_t onReceiveClientData(const sp<AMessage> &msg);

    status_t onOptionsRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    status_t onSetupRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    status_t onPlayRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    status_t onPauseRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    status_t onTeardownRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    status_t onGetParameterRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    status_t onSetParameterRequest(
            int32_t sessionID,
            int32_t cseq,
            const sp<ParsedMessage> &data);

    void sendErrorResponse(
            int32_t sessionID,
            const char *errorDetail,
            int32_t cseq);

    static void AppendCommonResponse(
            AString *response, int32_t cseq, int32_t playbackSessionID = -1ll);

    void scheduleReaper();
    void scheduleKeepAlive(int32_t sessionID);

    //calc UIBC coordinate para
    int calc_uibc_parameter(size_t witdh, size_t height);

    int32_t makeUniquePlaybackSessionID() const;

    sp<PlaybackSession> findPlaybackSession(
            const sp<ParsedMessage> &data, int32_t *playbackSessionID) const;

    void finishStop();
    void disconnectClientAsync();
    void disconnectClient2();
    void finishStopAfterDisconnectingClient();
    void finishStop2();

    void finishPlay();
    int scan_dir(const char *dirname);
    int open_dev(const char *deviceName);
    int write_event(int fd, int type, int code, int value);
    void calculateXY(float x, float y, int *abs_x, int *abs_y);
    void calculateNormalXY(float x, float y, int *abs_x, int *abs_y);
    int containsNonZeroByte(const uint8_t* array, uint32_t startIndex, uint32_t endIndex);
    DISALLOW_EVIL_CONSTRUCTORS(WifiDisplaySource);
};

    //UIBC magic numbers
    const int32_t DEFAULT_UIBC_PORT = 7239;
    const int16_t INPUT_CATEGORY_GENERIC = 0x00;
    const int16_t INPUT_CATEGORY_HIDC = 0x01;
    const int16_t TOUCH_ACTION_DOWN = 0;
    const int16_t TOUCH_ACTION_UP = 1;
    const int16_t TOUCH_ACTION_MOVE = 2;
    const int16_t TOUCH_ACTION_CANCEL = 3;
    const int16_t TOUCH_ACTION_POINTER_DOWN = 5;
    const int16_t TOUCH_ACTION_POINTER_UP = 6;
    const int16_t TOUCH_RANDOM_PRESSURE = 1234;
    const int16_t KEY_ACTION_DOWN = 3;
    const int16_t KEY_ACTION_UP = 4;
    const int16_t MOUSE_WHEEL_SCROLL = 8;
    const int16_t UIBC_MOUSE_VSCROLL = 6;
    const int16_t UIBC_MOUSE_HSCROLL = 7;

    const int16_t ANDROID_ACTION_SCROLL = 0x8;
}  // namespace android

#endif  // WIFI_DISPLAY_SOURCE_H_
