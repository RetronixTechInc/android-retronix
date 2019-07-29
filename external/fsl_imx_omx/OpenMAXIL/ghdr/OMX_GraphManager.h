/**
 *  Copyright (c) 2008-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef _omx_graphmanager_h_
#define _omx_graphmanager_h_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "OMX_Core.h"
#include "OMX_Component.h"
#include "fsl_osal.h"
#include "OMX_Common.h"

#define   OMX_AUDIO_CodingAC3     (OMX_AUDIO_CodingVendorStartUnused + 2)

extern OMX_BOOL bNewGMAvailable;

extern OMX_PTR  pLoadedLib;

typedef enum
{
    GM_AUDIO_STREAM_INDEX = 0,
    GM_VIDEO_STREAM_INDEX = 1,
    GM_SUBPIC_STREAM_INDEX = 2,
    GM_MAX_STREAM_INDEX =3
}GM_STREAM_INDEX;

typedef enum
{
    GM_VIDEO_MODE_NORMAL = 0,
    GM_VIDEO_MODE_FULLSCREEN = 1,
    GM_VIDEO_MODE_ZOOM = 2,
}GM_VIDEO_MODE;

typedef enum
{
    GM_STATUS_INDEX_VIDEO_DEVICE_SETTING = 1
}GM_STATUS_INDEX;

typedef enum
{
    GM_VIDEO_DEVICE_LCD = 0,
    GM_VIDEO_DEVICE_TV_NTSC = 1,
    GM_VIDEO_DEVICE_TV_PAL = 2,
    GM_VIDEO_DEVICE_TV_720P = 3,
}GM_VIDEO_DEVICE;




typedef enum
{
    GM_EVENT_NONE,
    GM_EVENT_EOS,
    GM_EVENT_FF_EOS,
    GM_EVENT_BW_BOS,
    GM_EVENT_BUFFERING_UPDATE,
    GM_EVENT_CORRUPTED_STREM,
    GM_EVENT_RENDERING_START,
    GM_EVENT_SUBTITLE,
    GM_EVENT_SET_SURFACE,
    GM_EVENT_SET_VIDEO_SIZE
}GM_EVENT;

typedef struct
{
	int streamCount;
}GM_FILEINFO;

typedef struct
{
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
    OMX_AUDIO_CODINGTYPE eEncoding;
}GM_AUDIO_INFO;

typedef struct
{
    OMX_U32 nFrameWidth;
    OMX_U32 nFrameHeight;
    //OMX_U32 nBitrate;
    float   xFramerate;
    OMX_VIDEO_CODINGTYPE eCompressionFormat;
}GM_VIDEO_INFO;

typedef struct
{
    OMX_U32 nFrameWidth;
    OMX_U32 nFrameHeight;
    //OMX_U32 nBitrate;
    float   xFramerate;
    OMX_IMAGE_CODINGTYPE eCompressionFormat;
}GM_IMAGE_INFO;

typedef struct
{
	int streamIndex;
	OMX_PORTDOMAINTYPE streamType;
    union {
        GM_AUDIO_INFO audio_info;
        GM_VIDEO_INFO video_info;
        GM_IMAGE_INFO image_info;
    } streamFormat;
}GM_STREAMINFO;

typedef enum {
    SUBTITLE_UNKNOWN,
    SUBTITLE_TEXT,
    SUBTITLE_BITMAP,
}SUBTITLE_FMT;

typedef struct {
    SUBTITLE_FMT fmt;
    OMX_U8 *pBuffer;
    OMX_S32 nLen;
    OMX_TICKS nTime;
}GM_SUBTITLE_SAMPLE;

typedef int (*GM_EVENTHANDLER)(void* context, GM_EVENT eventID, void* Eventpayload);

#ifndef MFW_AVI_SUPPORT_DIVX_DRM
	#define MFW_AVI_SUPPORT_DIVX_DRM
#endif
#ifdef MFW_AVI_SUPPORT_DIVX_DRM

/**
  *eFslUserInputType-the type of user input result under drm case
  */
typedef enum
{
	E_FSL_INPUT_NONE = 0,
	E_FSL_INPUT_CONFIRM,
	E_FSL_INPUT_CANCEL,
} eFslDrmUserInputType;

typedef struct
{
    union {
        OMX_BOOL bStatus;
    } status;
} GM_STATUS;
/**
  *sFslDrmInfo reserve the drm info which will be used by GUI.
  */
typedef struct
{
	OMX_S32			drmCode;
	OMX_BOOL isDrmFile;
	eFslDrmUserInputType userInput;
	OMX_S32 use_limit;
    OMX_S32 use_count;
    OMX_U8 cgmsaSignal;
    OMX_U8 acptbSignal;
    OMX_U8 digitalProtectionSignal;
    OMX_BOOL bOutputProtection;
	OMX_BOOL bDivxDrmPresent;
} sFslDrmInfo;
typedef OMX_S32 (*divxdrmcallback)(void* context, sFslDrmInfo* pFslDrmInfo);
#endif
typedef struct OMX_GraphManager OMX_GraphManager;

typedef enum {
    GM_STATE_NULL,
    GM_STATE_LOADING,
    GM_STATE_LOADED,
    GM_STATE_PAUSE,
    GM_STATE_PLAYING,
    GM_STATE_EOS,
    GM_STATE_STOP
}GM_STATE;


typedef enum {
    OMX_VIDEO_SURFACE_NONE,
    OMX_VIDEO_SURFACE_ISURFACE,
    OMX_VIDEO_SURFACE_SURFACE,
    OMX_VIDEO_SURFACE_TEXTURE,
}OMX_VIDEO_SURFACE_TYPE;




typedef enum
{
    GM_AV_SOURCE_DEFAULT = 0,
    GM_AV_SOURCE_SYSTEM_CLOCK,
    GM_AV_SOURCE_AUDIO,
    GM_AV_SOURCE_VSYNC,
}GM_AV_SOURCE;

typedef enum
{
    GM_AV_AUDIO_ADJUST_MODE_DEFAULT = 0,
    GM_AV_AUDIO_ADJUST_MODE_STRETCH,
    GM_AV_AUDIO_ADJUST_MODE_RESAMPLE,
}GM_AV_AUDIO_ADJUST_MODE;

typedef struct{
    GM_AV_SOURCE source;
    GM_AV_AUDIO_ADJUST_MODE audioAdjustMode;
    OMX_U32 tolerance;
}GM_AV_SYNC_SETTING;

typedef struct {
    OMX_U32 nTrackNum;
    OMX_VIDEO_CODINGTYPE eCodingType;
    OMX_U8 lanuage[16];
    OMX_U8 mime[20];
}GM_VIDEO_TRACK_INFO;

typedef struct {
    OMX_U32 nTrackNum;
    OMX_AUDIO_CODINGTYPE eCodingType;
    OMX_U8 lanuage[16];
    OMX_U8 mime[20];
}GM_AUDIO_TRACK_INFO;

typedef struct {
    OMX_U32 nTrackNum;
   OMX_U8 type[8];
   OMX_U8 lanuage[8];
   OMX_BOOL bInBand;
   OMX_U8 mime[24];
}GM_SUBTITLE_TRACK_INFO;


struct OMX_GraphManager
{
	OMX_BOOL (*preload)(OMX_GraphManager *h, const char *filename, int length);
	OMX_BOOL (*load)(OMX_GraphManager *h, const char *filename, int length);
	OMX_BOOL (*unLoad)(OMX_GraphManager *h);

	OMX_BOOL (*start)(OMX_GraphManager* h);
	OMX_BOOL (*stop)(OMX_GraphManager* h);
	OMX_BOOL (*pause)(OMX_GraphManager* h);
	OMX_BOOL (*resume)(OMX_GraphManager* h);
	OMX_BOOL (*seek)(OMX_GraphManager* h, OMX_TIME_SEEKMODETYPE mode, OMX_TICKS position);
    OMX_BOOL (*setVolume)(OMX_GraphManager* h, OMX_BOOL up);

    OMX_BOOL (*disableStream)(OMX_GraphManager* h, GM_STREAM_INDEX stream_index);
	OMX_BOOL (*getFileInfo)(OMX_GraphManager* h, GM_FILEINFO* fileInfo);
	OMX_BOOL (*getStreamInfo)(OMX_GraphManager* h, int streamIndex, GM_STREAMINFO* streamInfo);
	OMX_BOOL (*getPosition)(OMX_GraphManager* h, OMX_TICKS* pUsec);
	OMX_BOOL (*getDuration)(OMX_GraphManager* h, OMX_TICKS* pUsec);
	OMX_BOOL (*getState)(OMX_GraphManager* h, GM_STATE *state);

    OMX_BOOL (*getScreenShotInfo)(OMX_GraphManager* h, OMX_IMAGE_PORTDEFINITIONTYPE *pfmt, OMX_CONFIG_RECTTYPE *pCropRect);
    OMX_BOOL (*getThumbnail)(OMX_GraphManager* h, OMX_TICKS pos, OMX_U8 *buf);
    OMX_BOOL (*getSnapshot)(OMX_GraphManager* h, OMX_U8 *buf);

	OMX_BOOL (*registerEventHandler)(OMX_GraphManager* h, void* context, GM_EVENTHANDLER handler);

	OMX_BOOL (*dumpPipeLine)(OMX_GraphManager* h);
	OMX_BOOL (*deleteIt)(OMX_GraphManager* h);
    OMX_BOOL (*setAVPlaySpeed)(OMX_GraphManager* h, int speed, OMX_PTR extraData, OMX_U32 extraDataSize);
    OMX_BOOL (*setKeyFramePlaySpeed)(OMX_GraphManager* h, int speed, OMX_PTR extraData, OMX_U32 extraDataSize);
    OMX_BOOL (*getPlaySpeed)(OMX_GraphManager* h, int *speed, OMX_PTR extraData, OMX_U32 *extraDataSize);
//settvout fullscreen and zoomin is obsoleted, replace with setvideodevice and setvideomode
	OMX_BOOL (*settvout)(OMX_GraphManager* h, OMX_BOOL bTvOut, OMX_S32 tv_mode, OMX_BOOL bLayer2);
//settvout fullscreen and zoomin is obsoleted, replace with setvideodevice and setvideomode

	OMX_BOOL (*syssleep)(OMX_GraphManager* h, OMX_BOOL bsleep);
#ifdef MFW_AVI_SUPPORT_DIVX_DRM
	OMX_BOOL (*setdivxdrmcallback)(OMX_GraphManager* h, void* context, divxdrmcallback handler, void *pfunc);
#endif
	OMX_BOOL (*rotate)(OMX_GraphManager* h, OMX_S32 rotate);

//settvout fullscreen and zoomin is obsoleted, replace with setvideodevice and setvideomode
	OMX_BOOL (*fullscreen)(OMX_GraphManager* h, OMX_BOOL bzoomin, OMX_BOOL bkeepratio);
	OMX_BOOL (*AdjustAudioVolume)(OMX_GraphManager* h, OMX_BOOL bVolumeUp);
	OMX_BOOL (*AddRemoveAudioPP)(OMX_GraphManager* h, OMX_BOOL bAddComponent);
	OMX_U32 (*GetAudioTrackNum)(OMX_GraphManager* h);
	OMX_STRING (*GetAudioTrackName)(OMX_GraphManager* h, OMX_U32 nTrackIndex);
	OMX_U32 (*GetCurAudioTrack)(OMX_GraphManager* h);
	OMX_BOOL (*SelectAudioTrack)(OMX_GraphManager* h, OMX_U32 nSelectedTrack);
	OMX_U32 (*GetSubtitleTrackNum)(OMX_GraphManager* h);
	OMX_U32 (*GetSubtitleCurTrack)(OMX_GraphManager* h);
	OMX_STRING (*GetSubtitleTrackName)(OMX_GraphManager* h, OMX_U32 nTrackIndex);
	OMX_BOOL (*SelectSubtitleTrack)(OMX_GraphManager* h, OMX_U32 nSelectedTrack);
	OMX_U32 (*GetSubtitleNextSample)(OMX_GraphManager* h, OMX_U8**ppData, OMX_S32* pFilledLen, OMX_TICKS* pTime);
	OMX_U32 (*GetBandNumPEQ)(OMX_GraphManager* h);
	OMX_BOOL (*SetAudioEffectPEQ)(OMX_GraphManager* h, OMX_U32 nBindIndex, OMX_U32 nFC, OMX_S32 nGain);
	OMX_BOOL (*EnableDisablePEQ)(OMX_GraphManager* h, OMX_BOOL bAudioEffect);
	OMX_BOOL (*zoomin)(OMX_GraphManager* h);

//new gm function to control video output device and mode
    OMX_BOOL (*setvideodevice)(OMX_GraphManager * h, GM_VIDEO_DEVICE vDevice, OMX_BOOL bBlockMode);
    OMX_BOOL (*setvideomode)(OMX_GraphManager * h, GM_VIDEO_MODE vMode);
    OMX_BOOL (*querystatus)(OMX_GraphManager * h, GM_STATUS_INDEX statusIndex, GM_STATUS * pStatus);
    OMX_BOOL (*force2lcd)(OMX_GraphManager * h);

    OMX_BOOL (*setAudioSink)(OMX_GraphManager *h, OMX_PTR sink);
    OMX_BOOL (*setDisplayRect)(OMX_GraphManager *h, OMX_CONFIG_RECTTYPE *pRect);
    OMX_BOOL (*setVideoRect)(OMX_GraphManager *h, OMX_CONFIG_RECTTYPE *pRect);
    OMX_BOOL (*setDisplayMode)(OMX_GraphManager *h, OMX_CONFIG_RECTTYPE *pInRect, OMX_CONFIG_RECTTYPE *pOutRect, GM_VIDEO_MODE eVideoMode,  GM_VIDEO_DEVICE tv_dev, OMX_U32 nRotate);
    OMX_BOOL (*setVideoSurface)(OMX_GraphManager *h, OMX_PTR surface, OMX_VIDEO_SURFACE_TYPE type);
    OMX_BOOL (*setVideoScalingMode)(OMX_GraphManager *h, OMX_S32 mode);
    OMX_BOOL (*setVideoRenderType)(OMX_GraphManager *h, OMX_VIDEO_RENDER_TYPE type);
    OMX_BOOL (*setAudioPassThrough)(OMX_GraphManager *h, OMX_BOOL bEnable);
    OMX_BOOL (*getAudioTrackType)(OMX_GraphManager *h, OMX_U32 nTrackIndex, OMX_AUDIO_CODINGTYPE *pType);
    OMX_BOOL (*addOutBandSubtitleSource)(OMX_GraphManager *h, OMX_STRING url, OMX_STRING type);
    OMX_BOOL (*isSeekable)(OMX_GraphManager *h);
    OMX_BOOL (*setSyncSetting)(OMX_GraphManager *h,GM_AV_SYNC_SETTING *pSetting,OMX_S32 fps);
    OMX_BOOL (*getSyncSetting)(OMX_GraphManager *h,GM_AV_SYNC_SETTING *pSetting,OMX_S32* fps);

    OMX_U32 (*GetVideoTrackNum)(OMX_GraphManager* h);
    OMX_U32 (*GetCurVideoTrack)(OMX_GraphManager* h);

    OMX_BOOL (*GetVideoTrackInfo)(OMX_GraphManager* h,OMX_U32 nTrackIndex,GM_VIDEO_TRACK_INFO * pInfo);
    OMX_BOOL (*GetAudioTrackInfo)(OMX_GraphManager* h,OMX_U32 nTrackIndex,GM_AUDIO_TRACK_INFO * pInfo);
    OMX_BOOL (*GetSubtitleTrackInfo)(OMX_GraphManager* h,OMX_U32 nTrackIndex,GM_SUBTITLE_TRACK_INFO * pInfo);


	void* pData;
};

OMX_GraphManager* OMX_GraphManagerCreate();




void gm_setheader(void* header, OMX_U32 size);
#define NTSC    0
#define PAL     1
#define NV_MODE 2
#define MAX_RATE_VIDEOONLY         (16)
#ifndef Q16_SHIFT
#define Q16_SHIFT           0x10000
#endif
#define OMX_CAT_LOG_ERROR_GRAPHMANAGER(fm, ...) OMX_CAT_LEVEL_LOG(OMX_DEBUG_GRAPH_MANAGER,OMX_LEVEL_ERROR,fm,##__VA_ARGS__)
#define OMX_CAT_LOG_WARNING_GRAPHMANAGER(fm, ...) OMX_CAT_LEVEL_LOG(OMX_DEBUG_GRAPH_MANAGER,OMX_LEVEL_WARNING,fm,##__VA_ARGS__)
#define OMX_CAT_LOG_INFO_GRAPHMANAGER(fm, ...) OMX_CAT_LEVEL_LOG(OMX_DEBUG_GRAPH_MANAGER,OMX_LEVEL_INFO,fm,##__VA_ARGS__)
#define OMX_CAT_LOG_DEBUG_GRAPHMANAGER(fm, ...) OMX_CAT_LEVEL_LOG(OMX_DEBUG_GRAPH_MANAGER,OMX_LEVEL_DEBUG,fm,##__VA_ARGS__)
#define OMX_CAT_LOG_DEBUG_BUFFER_GRAPHMANAGER(fm, ...) OMX_CAT_LEVEL_LOG(OMX_DEBUG_GRAPH_MANAGER,OMX_LEVEL_DEBUG_BUFFER,fm,##__VA_ARGS__)
#define OMX_CAT_LOG_LOG_GRAPHMANAGER(fm, ...) OMX_CAT_LEVEL_LOG(OMX_DEBUG_GRAPH_MANAGER,OMX_LEVEL_LOG,fm,##__VA_ARGS__)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
