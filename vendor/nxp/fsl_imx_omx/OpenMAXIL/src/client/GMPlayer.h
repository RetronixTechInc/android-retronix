/**
 *  Copyright (c) 2009-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file GraphManager.h
 *  @brief Class definition of playback graph manager
 *  @ingroup GraphManager
 */

#ifndef GraphManager_h
#define GraphManager_h

#include "GMComponent.h"
#include "OMX_ContentPipe.h"
#include "Queue.h"
#include "List.h"

#include "OMX_GraphManager.h"
#include "GMEgl.h"
#include "OMX_MetadataExtractor.h"
#include "ShareLibarayMgr.h"

#define MAX_COMPONENT_NUM 8
#define MAX_LANGUAGE_BYTES 8

typedef struct {
    GMComponent *pComponent;
    OMX_EVENTTYPE eEvent;
    OMX_U32 nData1;
    OMX_U32 nData2;
    OMX_PTR pEventData;
}GM_SYSEVENT;


typedef enum {
    SCREEN_NONE,
    SCREEN_STD,
    SCREEN_FUL,
    SCREEN_ZOOM
}SCREENTYPE;

class GMSubtitlePlayer;
class GMSubtitleSource;

class GMPlayer {
    public:
        OMX_ERRORTYPE Init();
        OMX_ERRORTYPE DeInit();
        OMX_ERRORTYPE Reset();
        OMX_ERRORTYPE PreLoad(OMX_STRING contentURI);
        OMX_ERRORTYPE Load(OMX_STRING contentURI);
        OMX_ERRORTYPE Start();
        OMX_ERRORTYPE Stop();
        OMX_ERRORTYPE Pause();
        OMX_ERRORTYPE Resume();
        OMX_ERRORTYPE Seek(OMX_TIME_SEEKMODETYPE mode, OMX_TICKS position);
        OMX_ERRORTYPE SetAVPlaySpeed(OMX_S32 Scale, OMX_PTR extraData, OMX_U32 extraDataSize);
        OMX_ERRORTYPE SetKeyFramePlaySpeed(OMX_S32 Scale, OMX_PTR extraData, OMX_U32 extraDataSize);
        OMX_ERRORTYPE GetPlaySpeed(OMX_S32 *Scale, OMX_PTR extraData, OMX_U32 *extraDataSize);
        OMX_ERRORTYPE GetMediaDuration(OMX_TICKS *pDur);
        OMX_ERRORTYPE GetState(GM_STATE *pState);
        OMX_ERRORTYPE GetMediaPosition(OMX_TICKS *pCur);
        OMX_ERRORTYPE GetMediaPositionNoLock(OMX_TICKS *pCur);
        OMX_ERRORTYPE GetStreamInfo(OMX_U32 StreamIndex, GM_STREAMINFO *pInfo);
        OMX_ERRORTYPE DisableStream(OMX_U32 StreamIndex);
        OMX_ERRORTYPE AddSysEvent(GM_SYSEVENT *pSysEvent);
        OMX_ERRORTYPE GetSysEvent(GM_SYSEVENT *pSysEvent);
        OMX_ERRORTYPE SysEventHandler(GM_SYSEVENT *pSysEvent);
        OMX_ERRORTYPE RegistAppCallback(OMX_PTR pAppPrivate, GM_EVENTHANDLER pCallback);
        OMX_BOOL Rotate(OMX_S32 nRotate);
		OMX_BOOL FullScreen(OMX_BOOL bzoomin, OMX_BOOL bkeepratio);
		OMX_BOOL AdjustAudioVolume(OMX_BOOL bVolumeUp);
		OMX_BOOL AddRemoveAudioPP(OMX_BOOL bAddComponent);
		OMX_U32 GetAudioTrackNum();
                OMX_STRING GetAudioTrackName(OMX_U32 nTrackIndex);
		OMX_U32 GetCurAudioTrack();
		OMX_BOOL SelectAudioTrack(OMX_U32 nSelectedTrack);
		OMX_U32 GetSubtitleTrackNum();
		OMX_S32 GetSubtitleCurTrack();
		OMX_STRING GetSubtitleTrackName(OMX_U32 nTrackIndex);
		OMX_BOOL SelectSubtitleTrack(OMX_S32 nSelectedTrack);
		OMX_ERRORTYPE GetSubtitleNextSample(OMX_U8**ppData, OMX_S32* pFilledLen, OMX_TICKS* pTime, OMX_TICKS *pDuration);
		OMX_U32 GetBandNumPEQ();
		OMX_BOOL SetAudioEffectPEQ(OMX_U32 nBindIndex, OMX_U32 nFC, OMX_S32 nGain);
		OMX_BOOL EnableDisablePEQ(OMX_BOOL bAudioEffect);
		OMX_BOOL SetVideoMode(GM_VIDEO_MODE vmode);
        OMX_BOOL SetVideoDevice(GM_VIDEO_DEVICE vdevice, OMX_BOOL bBlockMode);
        OMX_BOOL GetThumbNail(OMX_TICKS pos, OMX_U8 *pBuf);
        OMX_BOOL GetSnapShot(OMX_U8 *buf);
        OMX_BOOL GetScreenShotInfo(OMX_IMAGE_PORTDEFINITIONTYPE *pfmt, OMX_CONFIG_RECTTYPE *pCropRect);
        OMX_BOOL SysSleep(OMX_BOOL bsleep);
        OMX_ERRORTYPE SetAudioSink(OMX_PTR sink);
        OMX_ERRORTYPE SetDisplayRect(OMX_CONFIG_RECTTYPE *pRect);
        OMX_ERRORTYPE SetVideoRect(OMX_CONFIG_RECTTYPE *pRect);

		OMX_ERRORTYPE loadMetadataExtractor(OMX_STRING contentURI);
		OMX_U32 getMetadataNum();
		OMX_U32 getMetadataSize(OMX_U32 index);
		OMX_ERRORTYPE getMetadata(OMX_U32 index, OMX_METADATA *pMetadata);
		OMX_ERRORTYPE getThumbnail(OMX_IMAGE_PORTDEFINITIONTYPE *pfmt, OMX_TICKS position, OMX_U8 **ppBuf);
        OMX_ERRORTYPE unloadMetadataExtractor();
        OMX_ERRORTYPE SetDisplayMode( OMX_CONFIG_RECTTYPE *pInRect, OMX_CONFIG_RECTTYPE *pOutRect, GM_VIDEO_MODE eVideoMode,  GM_VIDEO_DEVICE tv_dev, OMX_U32 nRotate);
		OMX_ERRORTYPE DistroyVideoPipeline();
		OMX_ERRORTYPE ReSetupVideoPipeline();
        OMX_ERRORTYPE SetupVideoPipelineDelay();
        OMX_ERRORTYPE SetVideoSurface(OMX_PTR surface, OMX_VIDEO_SURFACE_TYPE type);
        OMX_ERRORTYPE SetVideoScalingMode(OMX_S32 mode);
        OMX_ERRORTYPE SetVideoRenderType(OMX_VIDEO_RENDER_TYPE type);
        OMX_ERRORTYPE SetAudioPassThrough(OMX_BOOL bEnable);
        OMX_ERRORTYPE GetAudioTrackType(OMX_U32 nTrackIndex, OMX_AUDIO_CODINGTYPE *pType);

        OMX_ERRORTYPE AddOutBandSubtitleSource(OMX_STRING uri, OMX_STRING type);
        OMX_ERRORTYPE RenderSubtitle(GM_SUBTITLE_SAMPLE &sample);

        OMX_BOOL IsSeekable();
        OMX_ERRORTYPE GetSyncSetting(GM_AV_SYNC_SETTING *pSetting,OMX_S32 *pFps);
        OMX_ERRORTYPE SetSyncSetting(GM_AV_SYNC_SETTING *pSetting,OMX_S32 pFps);

        OMX_U32 GetVideoTrackNum();
        OMX_U32 GetCurVideoTrack();
        OMX_BOOL GetVideoTrackInfo(OMX_U32 nTrackIndex,GM_VIDEO_TRACK_INFO*pInfo);
        OMX_BOOL GetAudioTrackInfo(OMX_U32 nTrackIndex,GM_AUDIO_TRACK_INFO*pInfo);
        OMX_BOOL GetSubtitleTrackInfo(OMX_U32 nTrackIndex,GM_SUBTITLE_TRACK_INFO*pInfo);
        OMX_PTR EglContext;
        OMX_PTR PmContext;
        OMX_PTR GpumContext;

    private:
        OMX_PARAM_CONTENTURITYPE *Content;
        List<GMComponent> *Pipeline;
        List<GM_AUDIO_TRACK_INFO> *AudioTrackInfo;
        List<GM_SUBTITLE_TRACK_INFO> *SubtitleTrackInfo;
        Queue *SysEventQueue;
        fsl_osal_ptr SysEventThread;
        GM_EVENTHANDLER pAppCallback;
        OMX_PTR pAppData;
        fsl_osal_mutex Lock;
        GM_STATE State;
        OMX_BOOL bEnableAudio;
        OMX_BOOL bEnableVideo;
        OMX_BOOL bEnableSubtitle;
        GMComponent *Parser;
        GMComponent *AudioDecoder;
        GMComponent *VideoDecoder;
        GMComponent *AudioPostProcess;
        GMComponent *VideoPostProcess;
        GMComponent *AudioRender;
        GMComponent *VideoRender;
        GMComponent *Clock;
        OMX_BOOL bHasAudio;
        OMX_BOOL bHasVideo;
        OMX_BOOL bHasSubtitle;
        OMX_U32 nAudioTrackCnt;
        OMX_U32 nVideoTrackCnt;
        OMX_U32 nInbandSubtitleTrackCnt;
        OMX_U32 nOutbandSubtitleTrackCnt;
	OMX_S32 nCurSubtitleTrack;
        OMX_AUDIO_CODINGTYPE AudioFmt;
        OMX_VIDEO_CODINGTYPE VideoFmt;
        OMX_BOOL bEnableAudioPP;
        OMX_BOOL bEnableVideoPP;
        OMX_BOOL bAddRemoveComponentPostponed;
        OMX_BOOL bAddRemoveComponentReceived;
        OMX_BOOL bAudioEos;
        OMX_BOOL bVideoEos;
        OMX_BOOL bMediaEos;
        OMX_BOOL bAudioMarkDone;
        OMX_BOOL bVideoMarkDone;
		OMX_BOOL bGetMetadata;
        OMX_TICKS MediaDuration;
        OMX_TICKS MediaStartTime;
        OMX_TIME_CONFIG_PLAYBACKTYPE DelayedRate;
        OMX_BOOL bError;
        OMX_BOOL bBuffering;
        OMX_BOOL bStreamingSource;
        OMX_BOOL bLiveSource;
        OMX_BOOL bLiveMode;
        OMX_BOOL bSuspend;
        OMX_BOOL bInVideoSurfaceSetting;
        ShareLibarayMgr *libMgr;
        OMX_PTR hLibMp3;
        OMX_BOOL bGetMp3Metadata;
        OMX_PTR ParserMetadataHandle;
		OMX_STRING parserRole;

        OMX_BOOL bAbort;
        OMX_BOOL bLowLatency;//for rtp/udp streaming
        OMX_BOOL bWaitForVideoSurface;

        //copy from video_render_test,new implement of setting screen mode and tv out.
        SCREENTYPE eScreen;
        OMX_CONFIG_RECTTYPE sRectIn;
        OMX_CONFIG_RECTTYPE sScaledRectIn;
        OMX_CONFIG_RECTTYPE sRectOut;
        OMX_CONFIG_RECTTYPE sDispWindow;
        OMX_CONFIG_RECTTYPE sVideoRect;
        OMX_CONFIG_OUTPUTMODE sOutputMode;
        OMX_CONFIG_ROTATIONTYPE sRotate;
        OMX_BOOL bRotate;
		OMX_CONFIG_SCALEFACTORTYPE sDispRatio;

        OMX_PTR mAudioSink;
		OMX_BOOL bUseFakeAudioSink;

        CP_PIPETYPE *hPipe;
        CPhandle hPipeHandle;
        OMX_PTR mSurface;
        OMX_S32 mScalingMode;
        OMX_S32 mRotate;
        OMX_VIDEO_RENDER_TYPE mVideoRenderType;
        OMX_BOOL bVideoDirectRendering;
        static const VIDEO_RENDER_MAP_ENTRY VideoRenderRoleTable[OMX_VIDEO_RENDER_NUM];
        OMX_VIDEO_SURFACE_TYPE mSurfaceType;

        OMX_STRING ParserRole;

        OMX_BOOL bAudioPassthrough;

        GMSubtitlePlayer *pSubtitlePlayer;
        GMSubtitleSource *InbandSubtitleSource;
        List<GMSubtitleSource> *OutbandSubtitleSource;

        OMX_BOOL bPreLoaded;

        fsl_osal_ptr Cond;

        OMX_PTR DelayedSurface;
        OMX_VIDEO_SURFACE_TYPE DelayedSurfaceType;
        OMX_BOOL bSetDelayedSurface;
        OMX_BOOL bSeeking;
        GM_AV_SYNC_SETTING sSyncSetting;
        OMX_S32 nFps;
        OMX_PLAYBACK_MODE playbackMode;

        OMX_ERRORTYPE GetPipeHandle();
        OMX_STRING MediaTypeInspect();
        OMX_STRING MediaTypeInspectBySubfix();
        OMX_STRING MediaTypeInspectByContent();
        OMX_ERRORTYPE LoadComponent(OMX_STRING role, GMComponent **ppComponent);
        OMX_ERRORTYPE LoadParser(OMX_STRING role);
        OMX_ERRORTYPE GetContentPipe(CP_PIPETYPE **Pipe, OMX_STRING url);
        OMX_ERRORTYPE LoadClock();
        OMX_ERRORTYPE RemoveClock();
        OMX_ERRORTYPE AttachClock(OMX_U32 nClockPortIndex, GMComponent *pComponent, OMX_U32 nPortIndex);
        OMX_ERRORTYPE DeAttachClock(OMX_U32 nClockPortIndex, GMComponent *pComponent, OMX_U32 nPortIndex);
        OMX_ERRORTYPE SetupPipeline();
		OMX_ERRORTYPE SetupAudioPipeline();
                OMX_ERRORTYPE SetupAudioPipeline(OMX_STRING role);
		OMX_ERRORTYPE SetupVideoPipeline();
		OMX_ERRORTYPE StartPipeline();
		OMX_ERRORTYPE StartAudioPipeline();

		OMX_ERRORTYPE SeekTrackTo(OMX_U32 nPortIndex, OMX_TICKS position);
		OMX_BOOL AddRemoveComponent(OMX_STRING role, GMComponent **ppComponent, GMComponent *pUpComponent, OMX_BOOL bAddComponent);
		OMX_ERRORTYPE RemoveComponent(GMComponent *pComponent);
		OMX_ERRORTYPE ConnectAndStartComponent(GMComponent *pOutComp, OMX_U32 nOutPortIndex, GMComponent *pInComp, OMX_U32 nInPortIndex);
		OMX_ERRORTYPE DoRemoveComponent(GMComponent *pUpComponent, GMComponent *pComponent, GMComponent *pDownComponent);
		OMX_ERRORTYPE AddComponent(OMX_STRING role, GMComponent **ppComponent, GMComponent *pUpComponent);
		OMX_ERRORTYPE StartVideoPipeline();
		OMX_ERRORTYPE ConnectComponent(GMComponent *pOutComp, OMX_U32 nOutPortIndex, GMComponent *pInComp, OMX_U32 nInPortIndex);
		OMX_ERRORTYPE DisConnectComponent(GMComponent *pOutComp, OMX_U32 nOutPortIndex, GMComponent *pInComp, OMX_U32 nInPortIndex);
		OMX_ERRORTYPE GetStreamDuration(OMX_TICKS *pDur, OMX_U32 nPortIndex);
		OMX_ERRORTYPE DoSeek(OMX_TIME_SEEKMODETYPE mode, OMX_TICKS position);
        OMX_ERRORTYPE Normal2Trick(OMX_TIME_CONFIG_PLAYBACKTYPE &sPlaybackRate);
        OMX_ERRORTYPE Trick2Normal(OMX_TIME_CONFIG_PLAYBACKTYPE &sPlaybackRate);

        OMX_ERRORTYPE SetDefaultScreenMode();
        OMX_ERRORTYPE ScreenMode(SCREENTYPE eScreenParam, OMX_BOOL bTvOut, OMX_U32 nTvMode, OMX_U32 nRotate);
        OMX_ERRORTYPE SetCrop(SCREENTYPE eScreenParam,OMX_CONFIG_RECTTYPE *pCropRect, OMX_CONFIG_OUTPUTMODE *pOutputMode);
        OMX_ERRORTYPE ModifyOutputRectWhenRotate(OMX_CONFIG_OUTPUTMODE *pOutputMode);
        OMX_BOOL IsSameScreenMode(SCREENTYPE eScreenParam, OMX_BOOL bTvOut, OMX_U32 nTvMode, OMX_U32 nRotate);
        OMX_BOOL CheckIsScreenModeChangeFinished();
        OMX_ERRORTYPE ApplyDispScaleFactor();
        OMX_ERRORTYPE ParserMp3Metadata();
        OMX_ERRORTYPE DeStructAudioPipeline();
        OMX_ERRORTYPE ReSetupAudioPipeline(OMX_STRING NewAudioRole, OMX_S32 nAudioTrackIdx);

        OMX_ERRORTYPE SetupSubtitlePlayer();
        void DestroySubtitlePlayer();
        OMX_ERRORTYPE SelectInbandSubtitle(OMX_S32 nSelectedTrack);
        OMX_ERRORTYPE SelectOutbandSubtitle(OMX_S32 nSelectedTrack);

        OMX_ERRORTYPE DisableParserAudioPort();
        OMX_ERRORTYPE DisableParserVideoPort();
        OMX_STRING GetVideoMime(OMX_U32 type);
        OMX_STRING GetAudioMime(OMX_U32 type);
};

#endif
