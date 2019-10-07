/**
 *  Copyright (c) 2009-2016, Freescale Semiconductor Inc.,
 *  Copyright 2017-2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file OMX_Implement_h
 *  @brief Contains the common definitions for implementing OpenMAX IL
 *  @ingroup OMX_Implement
 */


#ifndef OMX_Implement_h
#define OMX_Implement_h

#include "fsl_osal.h"
#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_Audio.h"
#include "OMX_Video.h"
#include "OMX_ContentPipe.h"


/*copied from DivxDrmExtn.h*/
#ifndef MFW_AVI_SUPPORT_DIVX_DRM
#define MFW_AVI_SUPPORT_DIVX_DRM
#endif

/**< fsl defined buffer flags */
#define OMX_BUFFERFLAG_STARTTRICK 0x10000000
#define OMX_BUFFERFLAG_MAX_FILESIZE 0x20000000
#define OMX_BUFFERFLAG_MAX_DURATION 0x40000000

/**< fsl defined error type */
#define FSL_ERRORTYPE(n) ((OMX_ERRORTYPE)(OMX_ErrorVendorStartUnused + n))
#define OMX_ErrorNotComplete FSL_ERRORTYPE(1)
#define OMX_ErrorNetworkFail FSL_ERRORTYPE(2)
#define OMX_ErrorServerUnseekable FSL_ERRORTYPE(3)
#define OMX_ErrorTerminated FSL_ERRORTYPE(4)
#define OMX_ErrorMalFormat FSL_ERRORTYPE(5)


/**< fsl defined Event type */
#define FSL_EVENTTYPE(n) ((OMX_EVENTTYPE)(OMX_EventVendorStartUnused + n))
#define OMX_EventBufferingData FSL_EVENTTYPE(1)
#define OMX_EventBufferingDone FSL_EVENTTYPE(2)
#define OMX_EventBufferingUpdate FSL_EVENTTYPE(3)
#define OMX_EventStreamSkipped FSL_EVENTTYPE(4)
#define OMX_EventRenderingStart FSL_EVENTTYPE(5)

#define FSL_ORIGINTYPE(n) ((CP_ORIGINTYPE)(CP_OriginVendorStartUnused + n))
#define CP_OriginSeekable FSL_ORIGINTYPE(1)

/**< fsl defined check bytes result type */
#define FSL_CHECKBYTESRESULTTYPE(n) ((CP_CHECKBYTESRESULTTYPE)(CP_CheckBytesVendorStartUnused + n))
#define CP_CheckBytesStreamIOFail FSL_CHECKBYTESRESULTTYPE(1)
#define CP_CheckBytesUnseekable FSL_CHECKBYTESRESULTTYPE(2)

/**< fsl defined media type */
#define   OMX_VIDEO_CodingDIVX    (OMX_VIDEO_CodingVendorStartUnused + 1)  /**< Divx */
#define   OMX_VIDEO_CodingXVID    (OMX_VIDEO_CodingVendorStartUnused + 2)  /**< Xvid */
#define   OMX_VIDEO_CodingDIV3    (OMX_VIDEO_CodingVendorStartUnused + 3)  /**< Divx3 */
#define   OMX_VIDEO_CodingDIV4    (OMX_VIDEO_CodingVendorStartUnused + 4)  /**< Divx4 */
#define   OMX_VIDEO_CodingVP8     (OMX_VIDEO_CodingVendorStartUnused + 5)  /**< VP8 */
#define   OMX_VIDEO_CodingVP9     (OMX_VIDEO_CodingVendorStartUnused + 6)  /**< VP9 */
#define   OMX_VIDEO_CodingHEVC    (OMX_VIDEO_CodingVendorStartUnused + 7)  /**< HEVC */
#define   OMX_VIDEO_CodingWMV9    (OMX_VIDEO_CodingVendorStartUnused + 8) /**< WVC1 */
#define   OMX_VIDEO_CodingVP6     (OMX_VIDEO_CodingVendorStartUnused + 9)  /**< VP6 */
#define   OMX_VIDEO_CodingSORENSON263   (OMX_VIDEO_CodingVendorStartUnused + 10) /**< SORENSON */

/**< Google externed the type to 9. */
#define   OMX_VIDEO_CodingVPX     (9)  /**< Google Android 4.x defines non-standard coding type for VP8 */

#define   OMX_VIDEO_WMVFormat9a   (OMX_VIDEO_WMFFormatVendorStartUnused + 1) /**< WVC1 */
#define   OMX_VIDEO_WMVFormatWVC1 (OMX_VIDEO_WMFFormatVendorStartUnused + 2) /**< WVC1 */


#define   OMX_AUDIO_CodingFLAC    (OMX_AUDIO_CodingVendorStartUnused + 1)
#define   OMX_AUDIO_CodingAC3     (OMX_AUDIO_CodingVendorStartUnused + 2)
#define   OMX_AUDIO_CodingIEC937  ((OMX_AUDIO_CODINGTYPE)(OMX_AUDIO_CodingVendorStartUnused + 3))
#define   OMX_AUDIO_CodingEC3     (OMX_AUDIO_CodingVendorStartUnused + 4)
#define   OMX_AUDIO_CodingOPUS    (OMX_AUDIO_CodingVendorStartUnused + 5)
#define   OMX_AUDIO_CodingAPE     (OMX_AUDIO_CodingVendorStartUnused + 6)
#define   OMX_AUDIO_CodingBSAC    (OMX_AUDIO_CodingVendorStartUnused + 7)


#define FSL_AUDIO_WMAFORMATTYPE(n) ((OMX_AUDIO_WMAFORMATTYPE)(OMX_AUDIO_WMAFormatVendorStartUnused + n))
#define   OMX_AUDIO_WMAFormatLL   FSL_AUDIO_WMAFORMATTYPE(1)

/**<Reserved android opaque colorformat. Tells the encoder that
 * the actual colorformat will be  relayed by the
 * Gralloc Buffers.
 * FIXME: In the process of reserving some enum values for
 * Android-specific OMX IL colorformats. Change this enum to
 * an acceptable range once that is done.
 * */
#define   OMX_COLOR_FormatAndroidOpaque (0x7F000789)
#define   FSL_INDEXCOLOR(n) ((OMX_COLOR_FORMATTYPE)(OMX_COLOR_FormatVendorStartUnused + n))
#define   OMX_COLOR_Format32bitRGBA8888 FSL_INDEXCOLOR(1)
#define   OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled FSL_INDEXCOLOR(2)
#define   OMX_COLOR_FormatYUV420SemiPlanar8x4Tiled FSL_INDEXCOLOR(3)
#define   OMX_COLOR_FormatYUV420SemiPlanar4x4Tiled FSL_INDEXCOLOR(4)
#define   OMX_COLOR_FormatYUV420SemiPlanar4x4TiledCompressed FSL_INDEXCOLOR(5)
#define   OMX_COLOR_FormatYUV420SemiPlanarHDR10 FSL_INDEXCOLOR(6)
#define   OMX_COLOR_FormatYUV420SemiPlanarHDR10Tiled FSL_INDEXCOLOR(7)
#define   OMX_COLOR_FormatYUV420SemiPlanarHDR10TiledCompressed FSL_INDEXCOLOR(8)

/**< fsl defined index */
#define   FSL_INDEXTYPE(n) ((OMX_INDEXTYPE)(OMX_IndexVendorStartUnused + n))
#define   OMX_IndexParamMediaSeekable FSL_INDEXTYPE(1)
#define   OMX_IndexParamMediaDuration FSL_INDEXTYPE(2)
#define   OMX_IndexParamTrackDuration FSL_INDEXTYPE(3)          /* OMX_TRACK_DURATION */
#define   OMX_IndexConfigParserSendAudioFirst FSL_INDEXTYPE(4)  /* OMX_CONFIG_SENDAUDIOFIRST */

#define   OMX_IndexConfigCaptureFrame FSL_INDEXTYPE(5)          /* OMX_CONFIG_CAPTUREFRAME */
#define   OMX_IndexOutputMode FSL_INDEXTYPE(6)                  /* OMX_CONFIG_OUTPUTMODE */
#define   OMX_IndexSysSleep FSL_INDEXTYPE(7)                   /* OMX_CONFIG_SYSSLEEP */

#define   INDEX_CONFIG_DIVX_DRM_CALLBACK FSL_INDEXTYPE(8)
#define   INDEX_VC1_EXTRA_DATA 	FSL_INDEXTYPE(9)
#define   OMX_IndexParamAudioAc3 FSL_INDEXTYPE(10)
#define   OMX_IndexConfigAudioPostProcess FSL_INDEXTYPE(11)
#define   OMX_IndexParamAudioSink FSL_INDEXTYPE(12)
#define   OMX_IndexConfigClock FSL_INDEXTYPE(13)                 /* OMX_CONFIG_CLOCK */
#define   OMX_IndexConfigVideoOutBufPhyAddr FSL_INDEXTYPE(14)
#define   OMX_IndexParamAudioFlac FSL_INDEXTYPE(15)
#define   OMX_IndexParamMemOperator FSL_INDEXTYPE(16)
#define   OMX_IndexConfigAbortBuffering FSL_INDEXTYPE(17)  //OMX_CONFIG_ABORTBUFFERING
#define   OMX_IndexParamVideoCamera FSL_INDEXTYPE(18)
#define   OMX_IndexParamVideoCameraId FSL_INDEXTYPE(19)
#define   OMX_IndexParamVideoSurface FSL_INDEXTYPE(20)
#define   OMX_IndexParamMaxFileDuration FSL_INDEXTYPE(21)
#define   OMX_IndexParamMaxFileSize FSL_INDEXTYPE(22)
#define   OMX_IndexParamInterleaveUs FSL_INDEXTYPE(23)
#define   OMX_IndexParamIsGetMetadata FSL_INDEXTYPE(24)
#define   OMX_IndexParamSurface FSL_INDEXTYPE(25)
#define   OMX_IndexConfigEOS FSL_INDEXTYPE(26)
#define   OMX_IndexParamAudioSource FSL_INDEXTYPE(27)
#define   OMX_IndexConfigMaxAmplitude FSL_INDEXTYPE(28)
#define   OMX_IndexParamDecoderPlayMode FSL_INDEXTYPE(29)
#define   OMX_IndexParamTimeLapseUs FSL_INDEXTYPE(30)
#define   OMX_IndexParamAudioWmaExt FSL_INDEXTYPE(31)
#define   OMX_IndexParamVideoDecChromaAlign FSL_INDEXTYPE(32)
#define   OMX_IndexParamVideoCameraProxy FSL_INDEXTYPE(33)
#define   OMX_IndexParamLongitudex FSL_INDEXTYPE(34)
#define   OMX_IndexParamLatitudex FSL_INDEXTYPE(35)
#define   OMX_IndexParamBufferUsage FSL_INDEXTYPE(36)  //OMX_BUFFER_USAGE
#define   OMX_IndexParamDecoderCachedThreshold FSL_INDEXTYPE(37)  //control the speed of input consumed by decoder
#define   OMX_IndexParamVideoRegisterFrameExt FSL_INDEXTYPE(38)  //control the stride for frame width/height
#define   OMX_IndexParamAudioRenderMode FSL_INDEXTYPE(40)
#define   OMX_IndexConfigScalingMode FSL_INDEXTYPE(41)
#define   OMX_IndexParamEnableAndroidNativeBuffers FSL_INDEXTYPE(42)
#define   OMX_IndexParamNativeBufferUsage FSL_INDEXTYPE(43)
#define   OMX_IndexParamStoreMetaDataInBuffers FSL_INDEXTYPE(44)
#define   OMX_IndexParamUseAndroidNativeBuffer FSL_INDEXTYPE(45)
#define   OMX_IndexParamSubtitleSelect FSL_INDEXTYPE(46)
#define   OMX_IndexParamSubtitleNumAvailableStreams FSL_INDEXTYPE(47)
#define   OMX_IndexParamSubtitleNextSample FSL_INDEXTYPE(48)
#define   OMX_IndexParamSubtitleConfigTimePosition FSL_INDEXTYPE(49)
#define   OMX_IndexConfigInsertVideoCodecData FSL_INDEXTYPE(50)
#define   OMX_IndexConfigGrallocBufferParameter FSL_INDEXTYPE(51)
#define   OMX_IndexParamUseAndroidPrependSPSPPStoIDRFrames FSL_INDEXTYPE(52)
#define   OMX_IndexParamFileOffset64Bits FSL_INDEXTYPE(53)
#define   OMX_IndexParamClientName FSL_INDEXTYPE(54)
#define   OMX_IndexParamClientUID FSL_INDEXTYPE(55)
#define   OMX_IndexParamAudioEc3 FSL_INDEXTYPE(56)
#define   OMX_IndexParamVideoDecReorderDisable FSL_INDEXTYPE(57)
#define   OMX_IndexParamCustomContentPipeHandle FSL_INDEXTYPE(58)
#define   OMX_IndexParamAudioOpus FSL_INDEXTYPE(59)
#define   OMX_IndexParamParserLowLatency FSL_INDEXTYPE(60)//for rtp/udp streaming
#define   OMX_IndexParamAndroidAdaptivePlayback FSL_INDEXTYPE(61)
#define   OMX_IndexParamAudioApe FSL_INDEXTYPE(62)
#define   OMX_IndexParamPackageName FSL_INDEXTYPE(63)
#define   OMX_IndexConfigPlaybackRate FSL_INDEXTYPE(64)
#define   OMX_IndexParamBufferProducer FSL_INDEXTYPE(65)
#define   OMX_IndexParamBufferConsumer FSL_INDEXTYPE(66)
#define   OMX_IndexParamCaptureFps FSL_INDEXTYPE(67)
#define   OMX_IndexParamAndroidVersion FSL_INDEXTYPE(68)
#define   OMX_IndexParamAudioOutputConvert FSL_INDEXTYPE(69)
#define   OMX_IndexParamAudioSendFirstPortSettingChanged FSL_INDEXTYPE(70)
#define   OMX_IndexConfigVideoMediaTime FSL_INDEXTYPE(71)
#define   OMX_IndexParamStoreANWBufferInMetadata FSL_INDEXTYPE(72)
#define   OMX_IndexParamConfigureVideoTunnelMode FSL_INDEXTYPE(73)
#define   OMX_IndexConfigVideoTunnelMode FSL_INDEXTYPE(74)
#define   OMX_IndexParamAllocateNativeHandle FSL_INDEXTYPE(75)
#define   OMX_IndexParamAudioBsac FSL_INDEXTYPE(76)
#define   OMX_IndexConfigDescribeColorInfo FSL_INDEXTYPE(77)
#define   OMX_IndexConfigDescribeHDRColorInfo FSL_INDEXTYPE(78)
#define   OMX_IndexConfigVideoTileCompressedOffset FSL_INDEXTYPE(79)

//the value is got from OMX_IndexConfigOperatingRate in OMX_IndexExt.h from stagefright
#define   OMX_IndexConfigAndroidOperatingRate ((OMX_INDEXTYPE)(OMX_IndexKhronosExtensions + 0x00800003))

//the value is got from OMX_IndexConfigAndroidIntraRefresh in OMX_IndexExt.h from stagefright
#define   OMX_IndexConfigIntraRefresh  ((OMX_INDEXTYPE)(OMX_IndexKhronosExtensions + 0x0060000A))

//the value is got from OMX_IndexParamVideoAndroidVp8Encoder in OMX_IndexExt.h from stagefright
#define OMX_IndexParamVideoVp8Encoder ((OMX_INDEXTYPE)(OMX_IndexKhronosExtensions + 0x00600007))

//the value is got from OMX_IndexParamVideoVp8 in OMX_IndexExt.h from stagefright
#define OMX_IndexParamVideoAndroidVp8 ((OMX_INDEXTYPE)(OMX_IndexKhronosExtensions + 0x00600004))

/** Maximum number of VP8 temporal layers */
#define OMX_VIDEO_MAXVP8TEMPORALLAYERS 3

/** VP8 temporal layer patterns */
typedef enum OMX_VIDEO_VPXTEMPORALLAYERPATTERNTYPE {
    OMX_VIDEO_VPXTemporalLayerPatternNone = 0,
    OMX_VIDEO_VPXTemporalLayerPatternWebRTC = 1,
    OMX_VIDEO_VPXTemporalLayerPatternMax = 0x7FFFFFFF
} OMX_VIDEO_VPXTEMPORALLAYERPATTERNTYPE;

/**
 * Android specific VP8/VP9 encoder params
 *
 * STRUCT MEMBERS:
 *  nSize                      : Size of the structure in bytes
 *  nVersion                   : OMX specification version information
 *  nPortIndex                 : Port that this structure applies to
 *  nKeyFrameInterval          : Key frame interval in frames
 *  eTemporalPattern           : Type of temporal layer pattern
 *  nTemporalLayerCount        : Number of temporal coding layers
 *  nTemporalLayerBitrateRatio : Bitrate ratio allocation between temporal
 *                               streams in percentage
 *  nMinQuantizer              : Minimum (best quality) quantizer
 *  nMaxQuantizer              : Maximum (worst quality) quantizer
 */
typedef struct OMX_VIDEO_PARAM_VP8ENCODERTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nKeyFrameInterval;        // distance between consecutive key_frames (including one
                                      // of the key_frames). 0 means interval is unspecified and
                                      // can be freely chosen by the codec. 1 means a stream of
                                      // only key_frames.

    OMX_VIDEO_VPXTEMPORALLAYERPATTERNTYPE eTemporalPattern;
    OMX_U32 nTemporalLayerCount;
    OMX_U32 nTemporalLayerBitrateRatio[OMX_VIDEO_MAXVP8TEMPORALLAYERS];
    OMX_U32 nMinQuantizer;
    OMX_U32 nMaxQuantizer;
} OMX_VIDEO_PARAM_VP8ENCODERTYPE;

/**
 * Structure for configuring video compression intra refresh period
 *
 * STRUCT MEMBERS:
 *  nSize               : Size of the structure in bytes
 *  nVersion            : OMX specification version information
 *  nPortIndex          : Port that this structure applies to
 *  nRefreshPeriod      : Intra refreh period in frames. Value 0 means disable intra refresh
 */
typedef struct OMX_VIDEO_CONFIG_INTRAREFRESHTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nRefreshPeriod;
} OMX_VIDEO_CONFIG_INTRAREFRESHTYPE;

//get vp8,vp9,hevc profile enum from android/frameworks/native/include/media/openmax
/** VP8 profiles */
typedef enum OMX_VIDEO_VP8PROFILETYPE {
    OMX_VIDEO_VP8ProfileMain = 0x01,
    OMX_VIDEO_VP8ProfileUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP8ProfileMax = 0x7FFFFFFF
} OMX_VIDEO_VP8PROFILETYPE;

/** VP8 levels */
typedef enum OMX_VIDEO_VP8LEVELTYPE {
    OMX_VIDEO_VP8Level_Version0 = 0x01,
    OMX_VIDEO_VP8Level_Version1 = 0x02,
    OMX_VIDEO_VP8Level_Version2 = 0x04,
    OMX_VIDEO_VP8Level_Version3 = 0x08,
    OMX_VIDEO_VP8LevelUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP8LevelMax = 0x7FFFFFFF
} OMX_VIDEO_VP8LEVELTYPE;

/** VP9 profiles */
typedef enum OMX_VIDEO_VP9PROFILETYPE {
    OMX_VIDEO_VP9Profile0       = 0x1,
    OMX_VIDEO_VP9Profile1       = 0x2,
    OMX_VIDEO_VP9Profile2       = 0x4,
    OMX_VIDEO_VP9Profile3       = 0x8,
    // HDR profiles also support passing HDR metadata
    OMX_VIDEO_VP9Profile2HDR    = 0x1000,
    OMX_VIDEO_VP9Profile3HDR    = 0x2000,
    OMX_VIDEO_VP9ProfileUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP9ProfileMax     = 0x7FFFFFFF
} OMX_VIDEO_VP9PROFILETYPE;

/** VP9 levels */
typedef enum OMX_VIDEO_VP9LEVELTYPE {
    OMX_VIDEO_VP9Level1       = 0x1,
    OMX_VIDEO_VP9Level11      = 0x2,
    OMX_VIDEO_VP9Level2       = 0x4,
    OMX_VIDEO_VP9Level21      = 0x8,
    OMX_VIDEO_VP9Level3       = 0x10,
    OMX_VIDEO_VP9Level31      = 0x20,
    OMX_VIDEO_VP9Level4       = 0x40,
    OMX_VIDEO_VP9Level41      = 0x80,
    OMX_VIDEO_VP9Level5       = 0x100,
    OMX_VIDEO_VP9Level51      = 0x200,
    OMX_VIDEO_VP9Level52      = 0x400,
    OMX_VIDEO_VP9Level6       = 0x800,
    OMX_VIDEO_VP9Level61      = 0x1000,
    OMX_VIDEO_VP9Level62      = 0x2000,
    OMX_VIDEO_VP9LevelUnknown = 0x6EFFFFFF,
    OMX_VIDEO_VP9LevelMax     = 0x7FFFFFFF
} OMX_VIDEO_VP9LEVELTYPE;

/** HEVC Profile enum type */
typedef enum OMX_VIDEO_HEVCPROFILETYPE {
    OMX_VIDEO_HEVCProfileUnknown      = 0x0,
    OMX_VIDEO_HEVCProfileMain         = 0x1,
    OMX_VIDEO_HEVCProfileMain10       = 0x2,
    // Main10 profile with HDR SEI support.
    OMX_VIDEO_HEVCProfileMain10HDR10  = 0x1000,
    OMX_VIDEO_HEVCProfileMax          = 0x7FFFFFFF
} OMX_VIDEO_HEVCPROFILETYPE;

/** HEVC Level enum type */
typedef enum OMX_VIDEO_HEVCLEVELTYPE {
    OMX_VIDEO_HEVCLevelUnknown    = 0x0,
    OMX_VIDEO_HEVCMainTierLevel1  = 0x1,
    OMX_VIDEO_HEVCHighTierLevel1  = 0x2,
    OMX_VIDEO_HEVCMainTierLevel2  = 0x4,
    OMX_VIDEO_HEVCHighTierLevel2  = 0x8,
    OMX_VIDEO_HEVCMainTierLevel21 = 0x10,
    OMX_VIDEO_HEVCHighTierLevel21 = 0x20,
    OMX_VIDEO_HEVCMainTierLevel3  = 0x40,
    OMX_VIDEO_HEVCHighTierLevel3  = 0x80,
    OMX_VIDEO_HEVCMainTierLevel31 = 0x100,
    OMX_VIDEO_HEVCHighTierLevel31 = 0x200,
    OMX_VIDEO_HEVCMainTierLevel4  = 0x400,
    OMX_VIDEO_HEVCHighTierLevel4  = 0x800,
    OMX_VIDEO_HEVCMainTierLevel41 = 0x1000,
    OMX_VIDEO_HEVCHighTierLevel41 = 0x2000,
    OMX_VIDEO_HEVCMainTierLevel5  = 0x4000,
    OMX_VIDEO_HEVCHighTierLevel5  = 0x8000,
    OMX_VIDEO_HEVCMainTierLevel51 = 0x10000,
    OMX_VIDEO_HEVCHighTierLevel51 = 0x20000,
    OMX_VIDEO_HEVCMainTierLevel52 = 0x40000,
    OMX_VIDEO_HEVCHighTierLevel52 = 0x80000,
    OMX_VIDEO_HEVCMainTierLevel6  = 0x100000,
    OMX_VIDEO_HEVCHighTierLevel6  = 0x200000,
    OMX_VIDEO_HEVCMainTierLevel61 = 0x400000,
    OMX_VIDEO_HEVCHighTierLevel61 = 0x800000,
    OMX_VIDEO_HEVCMainTierLevel62 = 0x1000000,
    OMX_VIDEO_HEVCHighTierLevel62 = 0x2000000,
    OMX_VIDEO_HEVCHighTiermax     = 0x7FFFFFFF
} OMX_VIDEO_HEVCLEVELTYPE;

/** VP8 Param */
typedef struct OMX_VIDEO_PARAM_ANDROID_VP8TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_VIDEO_VP8PROFILETYPE eProfile;
    OMX_VIDEO_VP8LEVELTYPE eLevel;
    OMX_U32 nDCTPartitions;
    OMX_BOOL bErrorResilientMode;
} OMX_VIDEO_PARAM_ANDROID_VP8TYPE;

#define OMX_VIDEO_AVCLevel52    0x10000 /**< Level 5.2 */


/**< fsl defined macro utils */
#define MAX_NAME_LEN 128
#define MAX_PORT_NUM 8
#define MAX_PORT_BUFFER 32
#define MAX_PORT_FORMAT_NUM 32

#define CONTENTPIPE_NAME_LEN 128
#define COMPONENT_NAME_LEN 128
#define FUNCTION_NAME_LEN 128
#define ROLE_NAME_LEN 128
#define MAX_COMPONENTNUM_WITH_SAME_ROLE 8
#define CORE_NAME_LEN 128
#define MAX_VALUE_S64 (OMX_S64)((1ULL<<63)-1)
#define MAX_VALUE_S32 (OMX_S32)((1ULL<<31)-1)

/* extension of CPresult types in OMX_ContentPipe.h */
#define FSL_KD_ETYPE(n) ((CPresult)(100 + n))
#define KD_EUNSEEKABLE FSL_KD_ETYPE(1)


#define MAX(a,b) ((a)>=(b)?(a):(b))
#define MIN(a,b) ((a)<=(b)?(a):(b))
#define ABS(a) (((a)>0)?(a):-(a))
#ifndef Q16_SHIFT
#define Q16_SHIFT (0x10000)
#endif
#define INVALID_TS (-1)


#define OMX_INIT_STRUCT(ptr, type) \
    do { \
        fsl_osal_memset((ptr), 0x0, sizeof(type)); \
        (ptr)->nSize = sizeof(type); \
        (ptr)->nVersion.s.nVersionMajor = 0x1; \
        (ptr)->nVersion.s.nVersionMinor = 0x1; \
        (ptr)->nVersion.s.nRevision = 0x2; \
        (ptr)->nVersion.s.nStep = 0x0; \
    } while(0);


/**< fsl defined types */
/* Display rotation type */
typedef enum {
    ROTATE_NONE = 0,
    ROTATE_VERT_FLIP,
    ROTATE_HORIZ_FLIP,
    ROTATE_180,
    ROTATE_90_RIGHT,
    ROTATE_90_RIGHT_VFLIP,
    ROTATE_90_RIGHT_HFLIP,
    ROTATE_90_LEFT
}ROTATION;

typedef enum  {
    ROT_0   = 0x00,
    FLIP_H  = 0x01,
    FLIP_V  = 0x02,
    ROT_90  = 0x04,
    ROT_180 = FLIP_H|FLIP_V,
    ROT_270 = ROT_180|ROT_90,
    ROT_INVALID = 0x80
}DEVICE_ORIENTATION;


typedef enum
{
    DIVX_DRM_ERROR_NONE = 0,     /*!< No drm error happens */
    DIVX_DRM_OUTPUT_PROTECTION_COMMIT,
    DIVX_DRM_ERR_NEED_RENTAL_CONFIRMATION, /*!< No drm error happens but this is a rental file.
                                            Need user's confirmation to play it or not.
                                            GUI shall display how many views left for this file,
                                            and let user decide whether to play the file. */
    DIVX_DRM_ERROR_NOT_AUTH_USER,        /*!< Not an authorized user.
                                        The user shall activate the player if not,
                                        or this encrypted file is for another user.*/
    DIVX_DRM_ERROR_RENTAL_EXPIRED,       /*!< This is a rental file and is expired. So the user can no longer play it. */
    DIVX_DRM_ERROR_OTHERS,       /*!< Other error, reserved.*/
} DIVX_DRM_ERR_CODE;


typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_TICKS sTrackDuration;
}OMX_TRACK_DURATION;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bSendAudioFrameFirst;
}OMX_CONFIG_SENDAUDIOFIRST;

/** FLAC params */
typedef struct OMX_AUDIO_PARAM_FLACTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
    OMX_U32 nBitPerSample;
    OMX_U32 nTotalSample;
    OMX_U32 nBlockSize;
} OMX_AUDIO_PARAM_FLACTYPE;

/** AC3 params */
typedef struct OMX_AUDIO_PARAM_AC3TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
    OMX_U32 nAudioBandWidth;
    OMX_AUDIO_CHANNELMODETYPE eChannelMode;
} OMX_AUDIO_PARAM_AC3TYPE;

/** EC3 params */
typedef struct OMX_AUDIO_PARAM_EC3TYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
    OMX_U32 nAudioBandWidth;
    OMX_AUDIO_CHANNELMODETYPE eChannelMode;
} OMX_AUDIO_PARAM_EC3TYPE;

 /** OPUS params */
typedef struct OMX_AUDIO_PARAM_OPUSTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
    OMX_U32 nAudioBandWidth;
    OMX_AUDIO_CHANNELMODETYPE eChannelMode;
} OMX_AUDIO_PARAM_OPUSTYPE;

 /** APE params */
typedef struct OMX_AUDIO_PARAM_APETYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
    OMX_U32 nBitPerSample;
} OMX_AUDIO_PARAM_APETYPE;

  /** BSAC params */
typedef struct OMX_AUDIO_PARAM_BSACTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nBitRate;
    OMX_U32 nSampleRate;
    OMX_U32 nBitPerSample;
} OMX_AUDIO_PARAM_BSACTYPE;

/** ADPCM params */
typedef enum {
    ADPCM_NONE = 0,
    ADPCM_IMA_WAV,
    ADPCM_MS
}OMX_AUDIO_ADPCM_SUBDECODER;

typedef struct OMX_AUDIO_PARAM_ADPCMMODETYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nChannels;
    OMX_U32 nSampleRate;
    OMX_U32 nBitPerSample;
    OMX_U32 nBlockAlign;
    OMX_AUDIO_ADPCM_SUBDECODER CodecID;
} OMX_AUDIO_PARAM_ADPCMMODETYPE;

typedef struct OMX_AUDIO_CONFIG_POSTPROCESSTYPE {
    OMX_U32 nSize;             /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;  /**< OMX specification version information */
    OMX_U32 nPortIndex;        /**< port that this structure applies to */
    OMX_BOOL bEnable;          /**< Enable/disable for post process */
    OMX_BU32 sDelay;           /**< average delay in milliseconds */
} OMX_AUDIO_CONFIG_POSTPROCESSTYPE;

typedef struct
{
    OMX_STRING  sInFilePath;
    OMX_S32     nEncoder;
    OMX_S32     nWidth;
    OMX_S32     nHeight;
    OMX_BOOL    bMPEG4FrameParsing;   //Needs to be enabled for frame decoding
    OMX_BOOL    bH264FrameParsing;    //Needs to be enabled for frame decoding
    OMX_BOOL    bWmvDecParsing;     //Enabled if source parses for frame size
    OMX_S32     RealVideo;
    OMX_BOOL    bAVCEnc;
}VIDSRC_CONFIG;

typedef struct {
    OMX_U32 nPortIdx;
    OMX_U32 size;
    OMX_U8  data[128];
}EXTRA_DATA;

typedef enum {
    CAP_NONE,
    CAP_SNAPSHOT,
    CAP_THUMBNAL
}CAPTURETYPE;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    CAPTURETYPE eType;
    OMX_BOOL bDone;
    OMX_U8* pBuffer;
    OMX_U32 nFilledLen;
}OMX_CONFIG_CAPTUREFRAME;

typedef enum {
    MODE_NONE,
    MODE_PAL,
    MODE_NTSC,
    MODE_720P
}TV_MODE;

typedef enum {
    LAYER_NONE,
    LAYER0,
    LAYER1,
    LAYER2,
}FB_LAYER;

typedef enum {
    DEC_STREAM_MODE,
    DEC_FILE_MODE,
}OMX_DECODE_MODE;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bTv;
    FB_LAYER eFbLayer;
    TV_MODE eTvMode;
    OMX_CONFIG_RECTTYPE sRectIn;
    OMX_CONFIG_RECTTYPE sRectOut;
    ROTATION eRotation;
    OMX_BOOL bSetupDone;
}OMX_CONFIG_OUTPUTMODE;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bSleep;
}OMX_CONFIG_SYSSLEEP;


typedef struct _DivxDrmInfo
{
    int drm_code;   /* divx drm error code */
    int  use_limit;     /* Maximum views of a rental file. 0 means this is NOT a rental file. */
    int  use_count;     /* Views already played by the user for a rental file. Only valid for a rental file.
                        No greater than the "use_limit".If its equal to the "use_limit", this rental file is expired. */
    int rental_confirmed;   /* Only valid for a rental file,
                            whether user confirms to play the rental file. */
    OMX_U8 cgmsaSignal;
    OMX_U8 acptbSignal;
    OMX_U8 digitalProtectionSignal;
	OMX_BOOL bDivxDrmPresent;
}DivxDrmInfo;

typedef void (*pf_cb_drm)(void* context, OMX_PTR drm_info);


typedef struct OMX_CONFIG_CAPABILITY {
    OMX_U32 nSize;            /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
    OMX_U32  nPortIndex;      /**< Port index*/
    OMX_BOOL bCapability;     /**< Flag to indicate if this media syncable */
} OMX_PARAM_CAPABILITY;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_PTR hClock;
} OMX_CONFIG_CLOCK;

 /**
 * Video out port buffer physical address
 *
 * STRUCT MEMBERS:
 *  nSize         : Size of the structure in bytes
 *  nVersion      : OMX specification version information
 *  nBufferIndex  : Which buffer to query
 *  nPhysicalAddr : Physical address to return
 */
typedef struct OMX_CONFIG_VIDEO_OUTBUFTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BUFFERHEADERTYPE  * pBufferHdr;
    OMX_PTR nPhysicalAddr;
} OMX_CONFIG_VIDEO_OUTBUFTYPE;

/* memory operator functions */
typedef struct
{
    int nSize;                        /*!requested memory size */
    unsigned long nPhyAddr; /*!physical memory address allocated */
    unsigned long nCpuAddr; /*!cpu addr for system free usage */
    unsigned long nVirtAddr; /*!virtual user space address */
}OMX_MEM_DESC;
typedef OMX_BOOL (*pf_mem_malloc)(OMX_MEM_DESC* pOutMemDesc);
typedef OMX_BOOL (*pf_mem_free)(OMX_MEM_DESC* pOutMemDesc);
typedef struct {
    pf_mem_malloc pfMalloc;
    pf_mem_free pfFree;
} OMX_PARAM_MEM_OPERATOR;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bAbort;
} OMX_CONFIG_ABORTBUFFERING;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bGetMetadata;
} OMX_PARAM_IS_GET_METADATA;

/** WMA params externtion */
typedef struct OMX_AUDIO_PARAM_WMATYPE_EXT {
    OMX_U32 nSize;            /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
    OMX_U32 nPortIndex;       /**< port that this structure applies to */
	/**< Extensions for WMA codec config */
    OMX_U32 nBitsPerSample;
} OMX_AUDIO_PARAM_WMATYPE_EXT;

typedef enum {
    BUFFER_SW_READ_NEVER  = 0x1,
    BUFFER_SW_READ_OFTEN  = 0x2,
    BUFFER_SW_WRITE_NEVER = 0x4,
    BUFFER_SW_WRITE_OFTEN = 0x8,
    BUFFER_PHY_CONTINIOUS = 0x10
} OMX_BUFFER_USAGE;

typedef struct {
    OMX_U32 nSize;            /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
    OMX_U32 nPortIndex;       /**< port that this structure applies to */
    OMX_S32 nMaxDurationMsThreshold;
    OMX_S32 nMaxBufCntThreshold;
}OMX_DECODER_CACHED_THR;

typedef struct {
    OMX_U32 nSize;            /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
    OMX_U32 nPortIndex;       /**< port that this structure applies to */
    OMX_S32 nWidthStride;    /*-1: user don't care the stride, it is decoder's resposibility to decide the stride*/
    OMX_S32 nHeightStride;   /*-1: user don't care the stride, it is decoder's resposibility to decide the stride*/
    OMX_S32 nMaxBufferCnt;  /*-1: user don't care the count, it is decoder's responsibility to decide the count value*/
}OMX_VIDEO_REG_FRM_EXT_INFO;

// Used to transfer buffer between camera source and VPU encoder.
typedef struct {
    OMX_PTR pPhysicAddress;
}METADATA_BUFFER;

// Used to set parameters got from Gralloc buffer to VPU encoder.
typedef struct {
    OMX_U32 nSize;            /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
    OMX_U32 nPortIndex;       /**< port that this structure applies to */
    OMX_COLOR_FORMATTYPE eColorFormat;
}GRALLOC_BUFFER_PARAMETER;

/*subtitle sample structure*/
typedef struct {
    OMX_U8* pBuffer;
    OMX_S32 nFilledLen;
    OMX_TICKS nTimeStamp;
    OMX_TICKS nDuration;
    OMX_U32 nFlags;
}OMX_SUBTITLE_SAMPLE;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bEnableSPSToIDR; /*Add SPS/PPS into every IDR frame*/
} OMX_PARAM_PREPEND_SPSPPS_TO_IDR;

typedef struct {
    OMX_U32 nSize;                    /**< Size of this structure, in Bytes */
    OMX_VERSIONTYPE nVersion;         /**< OMX specification version information */
    OMX_BOOL bDisable;                     /**< BOOL value */
} OMX_DECODER_REORDER;

typedef struct OMX_PARAM_CONTENTPIPEHANDLETYPE
{
    OMX_U32 nSize;              /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< OMX specification version information */
    CPhandle hPipeHandle;       /**< The pipe handle*/
} OMX_PARAM_CONTENTPIPEHANDLETYPE;

typedef struct
{
    OMX_U32 nSize;              /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< OMX specification version information */
    OMX_BOOL bEnable;
} OMX_PARAM_PARSERLOWLATENCY;

typedef struct
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnable;
    OMX_U32 nMaxFrameWidth;
    OMX_U32 nMaxFrameHeight;
}OMX_PARAM_PREPARE_ANDROID_ADAPTIVE_PLAYBACK;

typedef enum {
    NORMAL_MODE = 0, //normal play mode, AV is sync
    FAST_FORWARD_TRICK_MODE,
    FAST_BACKWARD_TRICK_MODE
}OMX_PLAYBACK_MODE;

typedef struct OMX_TIME_CONFIG_PLAYBACKTYPE {
    OMX_U32 nSize;                  /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;       /**< OMX specification version information */
    OMX_S32 xScale;
    OMX_PLAYBACK_MODE ePlayMode;
    OMX_PTR pPrivateData;
    OMX_U32 nPrivateDataSize;
} OMX_TIME_CONFIG_PLAYBACKTYPE;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_PTR pBufferProducer;
}OMX_PARAM_BUFFER_PRODUCER;

typedef struct {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U8* pString;
    OMX_U32 nLength;
}OMX_PARAM_ANDROID_VERSION;

typedef struct
{
    OMX_U32 nSize;              /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< OMX specification version information */
    OMX_BOOL bEnable;
} OMX_PARAM_AUDIO_OUTPUT_CONVERT;

typedef struct
{
    OMX_U32 nSize;              /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< OMX specification version information */
    OMX_BOOL bEnable;
} OMX_PARAM_AUDIO_SEND_FIRST_PORT_SETTING_CHANGED;

typedef struct
{
    OMX_U32 nSize;              /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< OMX specification version information */
    OMX_S64 nTime;
    OMX_S32 nScale;
} OMX_CONFIG_VIDEO_MEDIA_TIME;

typedef struct {//ConfigureVideoTunnelModeParams
    OMX_U32 nSize;              // IN
    OMX_VERSIONTYPE nVersion;   // IN
    OMX_U32 nPortIndex;         // IN
    OMX_BOOL bTunneled;         // IN/OUT
    OMX_U32 nAudioHwSync;       // IN
    OMX_PTR pSidebandWindow;    // OUT
}OMX_PARAM_VIDEO_TUNNELED_MODE;

// A pointer to this struct is passed to OMX_GetParameter when the extension
// index for the 'OMX.google.android.index.disableAVCReorder'
// extension is given.  The usage bits bDisable indicates that AVC reorder
// should be enable or disable.
typedef struct DisableAVCReorderParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bDisable;
}OMX_PARAM_VIDEO_DISABLE_AVC_REORDER;

typedef struct EnableAndroidNativeBuffersParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL enable;
}OMX_PARAM_ENABLE_ANDROID_NATIVE_BUFFER;

typedef struct VpuColorDesc {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 primaries;
    OMX_U32 transfer;
    OMX_U32 matrixCoeffs;
    OMX_U32 fullRange;
}OMX_CONFIG_VPU_COLOR_DESC;

typedef struct VpuStaticHDRInfo {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U8 nID;
    OMX_U16 mR[2]; // display primary 0
    OMX_U16 mG[2]; // display primary 1
    OMX_U16 mB[2]; // display primary 2
    OMX_U16 mW[2]; // white point
    OMX_U16 mMaxDisplayLuminance; // in cd/m^2
    OMX_U16 mMinDisplayLuminance; // in 0.0001 cd/m^2
    OMX_U16 mMaxContentLightLevel; // in cd/m^2
    OMX_U16 mMaxFrameAverageLightLevel; // in cd/m^2

}OMX_CONFIG_VPU_STATIC_HDR_INFO;

typedef struct VideoTileCompressedOffset{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nYOffset;
    OMX_U32 nUVOffset;
}OMX_CONFIG_VIDEO_TILE_COMPRESSED_OFFSET;
#endif

/* File EOF */
