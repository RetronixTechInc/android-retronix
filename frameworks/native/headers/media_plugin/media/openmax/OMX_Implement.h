/**
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
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

/**< fsl defined media type */
#define   OMX_VIDEO_CodingDIVX    (OMX_VIDEO_CodingVendorStartUnused + 1)  /**< Divx */
#define   OMX_VIDEO_CodingXVID    (OMX_VIDEO_CodingVendorStartUnused + 2)  /**< Xvid */
#define   OMX_VIDEO_CodingDIV3    (OMX_VIDEO_CodingVendorStartUnused + 3)  /**< Divx3 */
#define   OMX_VIDEO_CodingDIV4    (OMX_VIDEO_CodingVendorStartUnused + 4)  /**< Divx4 */

#define   OMX_VIDEO_CodingWMV9    (OMX_VIDEO_CodingVendorStartUnused + 8) /**< WVC1 */
#define   OMX_VIDEO_CodingVP6     (OMX_VIDEO_CodingVendorStartUnused + 9)  /**< VP6 */
#define   OMX_VIDEO_CodingSORENSON263   (OMX_VIDEO_CodingVendorStartUnused + 10) /**< SORENSON */

#define   OMX_VIDEO_WMVFormat9a   (OMX_VIDEO_WMFFormatVendorStartUnused + 1)
#define   OMX_VIDEO_WMVFormatWVC1 (OMX_VIDEO_WMFFormatVendorStartUnused + 2)

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
    OMX_S32 nScale;
} OMX_CONFIG_VIDEO_MEDIA_TIME;

#define OMX_IndexParamDecoderCachedThreshold FSL_INDEXTYPE(37)

typedef struct {
    OMX_U32 nSize;            /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
    OMX_U32 nPortIndex;       /**< port that this structure applies to */
    OMX_S32 nMaxDurationMsThreshold;
    OMX_S32 nMaxBufCntThreshold;
}OMX_DECODER_CACHED_THR;

#define   OMX_IndexParamDecoderPlayMode FSL_INDEXTYPE(29)

typedef enum {
    DEC_STREAM_MODE,
    DEC_FILE_MODE,
}OMX_DECODE_MODE;

// A pointer to this struct is passed to OMX_GetParameter when the extension
// index for the 'OMX.google.android.index.disableAVCReorder'
// extension is given.  The usage bits bDisable indicates that AVC reorder
// should be enable or disable.
typedef struct DisableAVCReorderParams {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bDisable;
}OMX_PARAM_VIDEO_DISABLE_AVC_REORDER;

#define   OMX_AUDIO_CodingBSAC    (OMX_AUDIO_CodingVendorStartUnused + 7)
#define   OMX_IndexParamAudioBsac FSL_INDEXTYPE(76)
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

#define   FSL_INDEXCOLOR(n) ((OMX_COLOR_FORMATTYPE)(OMX_COLOR_FormatVendorStartUnused + n))
#define   OMX_COLOR_Format32bitRGBA8888 FSL_INDEXCOLOR(1)
#define   OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled FSL_INDEXCOLOR(2)
#define   OMX_COLOR_FormatYUV420SemiPlanar8x4Tiled FSL_INDEXCOLOR(3)
#define   OMX_COLOR_FormatYUV420SemiPlanar4x4Tiled FSL_INDEXCOLOR(4)
#define   OMX_COLOR_FormatYUV420SemiPlanar4x4TiledCompressed FSL_INDEXCOLOR(5)

