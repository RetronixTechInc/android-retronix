/**
 *  Copyright (C) 2016 Freescale Semiconductor, Inc.
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**< fsl defined media type */
#define   OMX_VIDEO_CodingDIVX    (OMX_VIDEO_CodingVendorStartUnused + 1)  /**< Divx */
#define   OMX_VIDEO_CodingXVID    (OMX_VIDEO_CodingVendorStartUnused + 2)  /**< Xvid */
#define   OMX_VIDEO_CodingDIV3    (OMX_VIDEO_CodingVendorStartUnused + 3)  /**< Divx3 */
#define   OMX_VIDEO_CodingDIV4    (OMX_VIDEO_CodingVendorStartUnused + 4)  /**< Divx4 */

#define   OMX_VIDEO_WMVFormat9a   (OMX_VIDEO_WMFFormatVendorStartUnused + 1) /**< WVC1 */
/**< WVC1 */
#define   OMX_VIDEO_CodingWMV9    (OMX_VIDEO_WMFFormatVendorStartUnused + 21) 
/**< WVC1 */
#define   OMX_VIDEO_WMVFormatWVC1 (OMX_VIDEO_WMFFormatVendorStartUnused + 22) 
/**< WVC1 */
#define   OMX_VIDEO_SORENSON263   (OMX_VIDEO_WMFFormatVendorStartUnused + 23) /**< SORENSON */

#define   OMX_AUDIO_CodingAPE     (OMX_AUDIO_CodingVendorStartUnused + 6)

#define OMX_INIT_STRUCT(ptr, type) \
    do { \
        memset((ptr), 0x0, sizeof(type)); \
        (ptr)->nSize = sizeof(type); \
        (ptr)->nVersion.s.nVersionMajor = 0x1; \
        (ptr)->nVersion.s.nVersionMinor = 0x1; \
        (ptr)->nVersion.s.nRevision = 0x2; \
        (ptr)->nVersion.s.nStep = 0x0; \
    } while(0);

#define   FSL_INDEXTYPE(n) ((OMX_INDEXTYPE)(OMX_IndexVendorStartUnused + n))
#define   OMX_IndexParamAudioWmaExt FSL_INDEXTYPE(31)
#define   OMX_IndexParamAudioApe FSL_INDEXTYPE(62)

#define FSL_AUDIO_WMAFORMATTYPE(n) ((OMX_AUDIO_WMAFORMATTYPE)(OMX_AUDIO_WMAFormatVendorStartUnused + n))
#define   OMX_AUDIO_WMAFormatLL   FSL_AUDIO_WMAFORMATTYPE(1)

/** WMA params externtion */
typedef struct OMX_AUDIO_PARAM_WMATYPE_EXT {
    OMX_U32 nSize;            /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
    OMX_U32 nPortIndex;       /**< port that this structure applies to */
    /**< Extensions for WMA codec config */
    OMX_U32 nBitsPerSample;
} OMX_AUDIO_PARAM_WMATYPE_EXT;

#define   OMX_IndexParamAudioOutputConvert FSL_INDEXTYPE(69)

typedef struct
{
    OMX_U32 nSize;              /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< OMX specification version information */
    OMX_BOOL bEnable;
} OMX_PARAM_AUDIO_OUTPUT_CONVERT;

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

#define   OMX_IndexParamAudioSendFirstPortSettingChanged FSL_INDEXTYPE(70)

typedef struct
{
    OMX_U32 nSize;              /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< OMX specification version information */
    OMX_BOOL bEnable;
} OMX_PARAM_AUDIO_SEND_FIRST_PORT_SETTING_CHANGED;

#define OMX_IndexConfigVideoMediaTime FSL_INDEXTYPE(71)
typedef struct
{
    OMX_U32 nSize;              /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion;   /**< OMX specification version information */
    OMX_S64 nTime;
} OMX_CONFIG_VIDEO_MEDIA_TIME;