/**
 *  Copyright (c) 2014-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "LibavVideoDec.h"

#if 0
#undef LOG_INFO
#undef LOG_DEBUG
#define LOG_INFO printf
#define LOG_DEBUG printf
#endif

#define DROP_B_THRESHOLD	30000
#define MAX_FRAMES_TO_CHECK_DISCARDING 3 // including current decoding frame

#if (ANDROID_VERSION >= ICS) && defined(MX5X)
#define ALIGN_STRIDE(x)  (((x)+63)&(~63))
#define ALIGN_CHROMA(x) (((x) + 4095)&(~4095))
#else
#define ALIGN_STRIDE(x)  (((x)+31)&(~31))
#define ALIGN_CHROMA(x) (x)
#endif
#define G_N_ELEMENTS(arr)		(sizeof (arr) / sizeof ((arr)[0]))

#define LIBAV_COMP_NAME_AVCDEC "OMX.Freescale.std.video_decoder.avc.sw-based"
#define LIBAV_COMP_NAME_MPEG4DEC "OMX.Freescale.std.video_decoder.mpeg4.sw-based"
#define LIBAV_COMP_NAME_H263DEC	"OMX.Freescale.std.video_decoder.h263.sw-based"
#define LIBAV_COMP_NAME_MPEG2DEC "OMX.Freescale.std.video_decoder.mpeg2.sw-based"
#define LIBAV_COMP_NAME_VP8	"OMX.Freescale.std.video_decoder.vp8.sw-based"
#define LIBAV_COMP_NAME_VP9	"OMX.Freescale.std.video_decoder.vp9.sw-based"
#define LIBAV_COMP_NAME_HEVCDEC "OMX.Freescale.std.video_decoder.hevc.sw-based"


LibavVideoDec::LibavVideoDec()
{
    fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_AVCDEC);
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"video_decoder.avc";
    bNeedInput = OMX_TRUE;
    ILibavDec = NULL;
    libavDecMgr = NULL;

    SetDefaultSetting();

}

OMX_ERRORTYPE LibavVideoDec::SetDefaultSetting()
{
    fsl_osal_memset(&sInFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    sInFmt.nFrameWidth = 320;
    sInFmt.nFrameHeight = 240;
    sInFmt.xFramerate = 30 * Q16_SHIFT;
    sInFmt.eColorFormat = OMX_COLOR_FormatUnused;
    sInFmt.eCompressionFormat = OMX_VIDEO_CodingAVC;

    nInPortFormatCnt = 0;
    nOutPortFormatCnt = 1;
    eOutPortPormat[0] = OMX_COLOR_FormatYUV420Planar;

    sOutFmt.nFrameWidth = 320;
    sOutFmt.nFrameHeight = 240;
    sOutFmt.nStride = 320;
    sOutFmt.nSliceHeight = ALIGN_STRIDE(240);
    sOutFmt.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    sOutFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

    bFilterSupportPartilInput = OMX_FALSE;
    nInBufferCnt = 1;
    nInBufferSize = 512*1024;
    nOutBufferCnt = 3;
    nOutBufferSize = sOutFmt.nFrameWidth * sOutFmt.nFrameHeight \
                     * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;

    pInBuffer = pOutBuffer = NULL;
    nInputSize = nInputOffset = 0;
    bInEos = OMX_FALSE;
    nFrameSize = 0;
    pFrameBuffer = NULL;

    OMX_INIT_STRUCT(&sOutCrop, OMX_CONFIG_RECTTYPE);
    sOutCrop.nPortIndex = OUT_PORT;
    sOutCrop.nLeft = sOutCrop.nTop = 0;
    sOutCrop.nWidth = sInFmt.nFrameWidth;
    sOutCrop.nHeight = sInFmt.nFrameHeight;

    codecID = AV_CODEC_ID_NONE;
    codecContext = NULL;
    picture = NULL;
    pClock = NULL;
    CodingType = OMX_VIDEO_CodingUnused;
    bNeedReportOutputFormat = OMX_TRUE;
    bLibavHoldOutputBuffer = OMX_FALSE;
    bOutEos = OMX_FALSE;
    nInvisibleFrame = 0;
    nFramesToCheckDiscarding = 0;

    nFrameWidthStride=-1;//default: invalid
    nFrameHeightStride=-1;
    nFrameMaxCnt=-1;

    nPadWidth = 0;
    nPadHeight = 0;
    nChromaAddrAlign = 0;

    eDecState = LIBAV_DEC_INIT;

    return OMX_ErrorNone;

}

OMX_ERRORTYPE  LibavVideoDec::SetRoleFormat(OMX_STRING role)
{
    if(fsl_osal_strcmp(role, "video_decoder.avc") == 0) {
        CodingType = OMX_VIDEO_CodingAVC;
        codecID = AV_CODEC_ID_H264;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_AVCDEC);
    } else if(fsl_osal_strcmp(role, "video_decoder.mpeg4") == 0) {
        CodingType = OMX_VIDEO_CodingMPEG4;
        codecID = AV_CODEC_ID_MPEG4;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_MPEG4DEC);
    } else if(fsl_osal_strcmp(role, "video_decoder.h263") == 0) {
        CodingType = OMX_VIDEO_CodingH263;
        codecID = AV_CODEC_ID_H263;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_H263DEC);
    } else if(fsl_osal_strcmp(role, "video_decoder.hevc") == 0) {
        CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC;
        codecID = AV_CODEC_ID_HEVC;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_HEVCDEC);
    } else if(fsl_osal_strcmp(role, "video_decoder.mpeg2") == 0) {
        CodingType = OMX_VIDEO_CodingMPEG2;
        codecID = AV_CODEC_ID_MPEG2VIDEO;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_MPEG2DEC);
    } else if(fsl_osal_strcmp(role, "video_decoder.vp8") == 0) {
        CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP8;
        codecID = AV_CODEC_ID_VP8;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_VP8);
    } else if(fsl_osal_strcmp(role, "video_decoder.vp9") == 0) {
        CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP9;
        codecID = AV_CODEC_ID_VP9;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_VP9);
    } else {
        CodingType = OMX_VIDEO_CodingUnused;
        codecID = AV_CODEC_ID_NONE;
        LOG_ERROR("%s: failure: unknow role: %s \r\n",__FUNCTION__,role);
        return OMX_ErrorUndefined;
    }

    //check input change
    if(sInFmt.eCompressionFormat != CodingType) {
        sInFmt.eCompressionFormat = CodingType;
        InputFmtChanged();
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::GetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch((int)nParamIndex){
        case OMX_IndexParamStandardComponentRole:
            fsl_osal_strcpy((OMX_STRING)((OMX_PARAM_COMPONENTROLETYPE*) \
                        pComponentParameterStructure)->cRole,(OMX_STRING)cRole);
            break;
        case OMX_IndexParamVideoProfileLevelQuerySupported:
            struct CodecProfileLevel {
                OMX_U32 mProfile;
                OMX_U32 mLevel;
            };

            static const CodecProfileLevel kH263ProfileLevels[] = {
                { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level10 },
                { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level20 },
                { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level30 },
                { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level45 },
                { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level50 },
                { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level60 },
                { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level70 },
                { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level10 },
                { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level20 },
                { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level30 },
                { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level45 },
                { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level50 },
                { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level60 },
                { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level70 },
            };

            static const CodecProfileLevel kProfileLevels[] = {
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1  },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1b },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel11 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel12 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel13 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel2  },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel21 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel22 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel3  },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel31 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel32 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel4  },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel41 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel42 },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel5  },
                { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel1  },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel1b },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel11 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel12 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel13 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel2  },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel21 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel22 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel3  },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel31 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel32 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel4  },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel41 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel42 },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel5  },
                { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel51 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel1  },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel1b },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel11 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel12 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel13 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel2  },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel21 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel22 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel3  },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel31 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel32 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel4  },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel41 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel42 },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel5  },
                { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel51 },
            };

            static const CodecProfileLevel kM4VProfileLevels[] = {
                { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0 },
                { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0b },
                { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level1 },
                { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level2 },
                { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level3 },
                { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level0 },
                { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level0b },
                { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level1 },
                { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level2 },
                { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level3 },
                { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level4 },
                { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level4a },
                { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level5 },
            };

            static const CodecProfileLevel kMpeg2ProfileLevels[] = {
                { OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelLL },
                { OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelML },
                { OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelH14},
                { OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelHL},
                { OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelLL },
                { OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelML },
                { OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelH14},
                { OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelHL},
            };

            OMX_VIDEO_PARAM_PROFILELEVELTYPE  *pPara;
            OMX_S32 index;
            OMX_S32 nProfileLevels;

            pPara = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
            switch(CodingType)
            {
                case OMX_VIDEO_CodingAVC:
                    index = pPara->nProfileIndex;

                    nProfileLevels =sizeof(kProfileLevels) / sizeof(kProfileLevels[0]);
                    if (index >= nProfileLevels) {
                        return OMX_ErrorNoMore;
                    }

                    pPara->eProfile = kProfileLevels[index].mProfile;
                    pPara->eLevel = kProfileLevels[index].mLevel;
                    break;
                case OMX_VIDEO_CodingMPEG4:
                    index = pPara->nProfileIndex;

                    nProfileLevels =sizeof(kM4VProfileLevels) / sizeof(kM4VProfileLevels[0]);
                    if (index >= nProfileLevels) {
                        return OMX_ErrorNoMore;
                    }

                    pPara->eProfile = kM4VProfileLevels[index].mProfile;
                    pPara->eLevel = kM4VProfileLevels[index].mLevel;
                    break;
                case OMX_VIDEO_CodingH263:
                    index = pPara->nProfileIndex;

                    nProfileLevels =sizeof(kH263ProfileLevels) / sizeof(kH263ProfileLevels[0]);
                    if (index >= nProfileLevels) {
                        return OMX_ErrorNoMore;
                    }

                    pPara->eProfile = kH263ProfileLevels[index].mProfile;
                    pPara->eLevel = kH263ProfileLevels[index].mLevel;
                    break;
                case OMX_VIDEO_CodingMPEG2:
                    index = pPara->nProfileIndex;

                    nProfileLevels =sizeof(kMpeg2ProfileLevels) / sizeof(kMpeg2ProfileLevels[0]);
                    if (index >= nProfileLevels) {
                        return OMX_ErrorNoMore;
                    }

                    pPara->eProfile = kMpeg2ProfileLevels[index].mProfile;
                    pPara->eLevel = kMpeg2ProfileLevels[index].mLevel;
                    break;
                default:
                    ret = OMX_ErrorUnsupportedIndex;
                    break;
            }
            break;
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}

OMX_ERRORTYPE LibavVideoDec::SetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch((int)nParamIndex){
        case OMX_IndexParamStandardComponentRole:
            fsl_osal_strcpy( (fsl_osal_char *)cRole,(fsl_osal_char *) \
                    ((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole);
            if(OMX_ErrorNone != SetRoleFormat((OMX_STRING)cRole))
            {
                ret = OMX_ErrorBadParameter;
            }
            break;
        case OMX_IndexParamVideoDecChromaAlign:
            {
                OMX_U32* pAlignVal=(OMX_U32*)pComponentParameterStructure;
                nChromaAddrAlign=*pAlignVal;
                LOG_DEBUG("set OMX_IndexParamVideoDecChromaAlign: %d \r\n",nChromaAddrAlign);
                if(nChromaAddrAlign==0) nChromaAddrAlign=1;
                break;
            }
        case OMX_IndexParamVideoRegisterFrameExt:
            {
                OMX_VIDEO_REG_FRM_EXT_INFO* pExtInfo=(OMX_VIDEO_REG_FRM_EXT_INFO*)pComponentParameterStructure;
                if(pExtInfo->nPortIndex==OUT_PORT){
                    nFrameWidthStride=pExtInfo->nWidthStride;
                    nFrameHeightStride=pExtInfo->nHeightStride;
                    nFrameMaxCnt = pExtInfo->nMaxBufferCnt;
                    LOG_DEBUG("set OMX_IndexParamVideoRegisterFrameExt width=%d,height=%d",
                        nFrameWidthStride,nFrameHeightStride);
                }
                break;
            }
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}

OMX_ERRORTYPE LibavVideoDec::GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
    if(nParamIndex == OMX_IndexConfigCommonOutputCrop) {
        OMX_CONFIG_RECTTYPE *pRecConf = (OMX_CONFIG_RECTTYPE*)pComponentParameterStructure;
        if(pRecConf->nPortIndex == OUT_PORT) {
            pRecConf->nTop = sOutCrop.nTop;
            pRecConf->nLeft = sOutCrop.nLeft;
            pRecConf->nWidth = sOutCrop.nWidth;
            pRecConf->nHeight = sOutCrop.nHeight;
            LOG_DEBUG("GetConfig OMX_IndexConfigCommonOutputCrop w=%d,h=%d",sOutCrop.nWidth,sOutCrop.nHeight);
        }
        return OMX_ErrorNone;
    }
    else
        return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE LibavVideoDec::SetConfig(OMX_INDEXTYPE nIndex, OMX_PTR pComponentConfigStructure)
{
    OMX_CONFIG_CLOCK *pC;

    switch((int)nIndex)
    {
        case OMX_IndexConfigClock:
            pC = (OMX_CONFIG_CLOCK*) pComponentConfigStructure;
            pClock = pC->hClock;
            break;
        default:
            break;
    }
    return OMX_ErrorNone;
}

static void libav_log_callback (void *ptr, int level, const char *fmt, va_list vl)
{
    LogLevel log_level;

    switch (level) {
        case AV_LOG_QUIET:
            log_level = LOG_LEVEL_NONE;
            break;
        case AV_LOG_ERROR:
            log_level = LOG_LEVEL_ERROR;
            break;
        case AV_LOG_INFO:
            log_level = LOG_LEVEL_INFO;
            break;
        case AV_LOG_DEBUG:
            log_level = LOG_LEVEL_DEBUG;
            break;
        default:
            log_level = LOG_LEVEL_INFO;
            break;
    }

    LOG2(log_level, fmt, vl);
}

OMX_ERRORTYPE LibavVideoDec::InitFilterComponent()
{
    OMX_S32 ret = 0;

    libavDecMgr = FSL_NEW(LibavDecInterfaceMgr, ());
    if(!libavDecMgr)
        return OMX_ErrorInsufficientResources;

    ILibavDec = (LibavDecInterface*)FSL_MALLOC(sizeof(LibavDecInterface));
    if (!ILibavDec)
        return OMX_ErrorInsufficientResources;

    fsl_osal_memset(ILibavDec, 0, sizeof(LibavDecInterface));

    ret = libavDecMgr->CreateLibavDecInterfaces(ILibavDec);
    if (ret)
        return OMX_ErrorUndefined;

    ILibavDec->AvLogSetLevel(AV_LOG_DEBUG);
    ILibavDec->AvLogSetCallback(libav_log_callback);

    /* register all the codecs */
    ILibavDec->AvcodecRegisterAll();

    LOG_DEBUG("libav load done.");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::DeInitFilterComponent()
{
    if (ILibavDec) {
        libavDecMgr->DeleteLibavDecInterfaces(ILibavDec);
        FSL_DELETE(ILibavDec);
    }

    if(libavDecMgr)
        FSL_FREE(libavDecMgr);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::InitFilter()
{

    int n;

    AVCodec *codec =  ILibavDec->AvcodecFindDecoder(codecID);
    if(!codec){
        LOG_ERROR("find decoder fail, codecID %d" , codecID);
        return OMX_ErrorUndefined;
    }

    codecContext = ILibavDec->AvcodecAllocContext3(codec);
    if(!codecContext){
        LOG_ERROR("alloc context fail");
        return OMX_ErrorUndefined;
    }

#if defined(_SC_NPROCESSORS_ONLN)
        n  = sysconf(_SC_NPROCESSORS_ONLN);
#else
        // _SC_NPROC_ONLN must be defined...
        n  = sysconf(_SC_NPROC_ONLN);
#endif

    if (n < 1)
        n = 1;
    LOG_INFO("configure processor count: %d\n", n);
    codecContext->thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME;
    codecContext->thread_count = n;

    codecContext->width = sInFmt.nFrameWidth;
    codecContext->height = sInFmt.nFrameHeight;
    codecContext->coded_width = 0;
    codecContext->coded_height = 0;
    codecContext->extradata = (uint8_t *)pCodecData;
    codecContext->extradata_size = nCodecDataLen;

    if(ILibavDec->AvcodecOpen2(codecContext, codec, NULL) < 0){
        LOG_ERROR("codec open fail");
        return OMX_ErrorUndefined;
    }

    picture = ILibavDec->AvFrameAlloc();
    if(picture == NULL){
        LOG_ERROR("alloc frame fail");
        return OMX_ErrorInsufficientResources;
    }

    LOG_DEBUG("libav init done.");
    eDecState = LIBAV_DEC_RUN;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::DeInitFilter()
{
    if(codecContext) {
        ILibavDec->AvcodecClose(codecContext);
        ILibavDec->AvFree(codecContext);
        codecContext = NULL;
    }

    if(picture) {
        ILibavDec->AvFrameFree(&picture);
        picture = NULL;
    }

    eDecState = LIBAV_DEC_INIT;
    SetDefaultSetting();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::SetInputBuffer(
        OMX_PTR pBuffer,
        OMX_S32 nSize,
        OMX_BOOL bLast)
{
    if(pBuffer == NULL)
        return OMX_ErrorBadParameter;

    pInBuffer = pBuffer;
    nInputSize = nSize;
    nInputOffset = 0;
    bInEos = bLast;
    if(nSize == 0 && !bLast){
        nInputSize = 0;
        pInBuffer = NULL;
    }

    if(eDecState == LIBAV_DEC_STOP &&  nSize > 0)
        eDecState = LIBAV_DEC_RUN;

    LOG_DEBUG("libav dec set input buffer: %p:%d:%d\n", pBuffer, nSize, bInEos);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::SetOutputBuffer(OMX_PTR pBuffer)
{
    if(pBuffer == NULL)
        return OMX_ErrorBadParameter;

    pOutBuffer = pBuffer;

    LOG_DEBUG("libav dec set output buffer: %p\n", pBuffer);

    return OMX_ErrorNone;
}

FilterBufRetCode LibavVideoDec::FilterOneBuffer()
{
    FilterBufRetCode ret = FILTER_OK;

    switch(eDecState) {
        case LIBAV_DEC_INIT:
            ret = FILTER_DO_INIT;
            break;
        case LIBAV_DEC_RUN:
            ret = DecodeOneFrame();
            break;
        case LIBAV_DEC_STOP:
            break;
        default:
            break;
    }

    LOG_DEBUG("libav FilterOneBuffer ret: %d\n", ret);

    return ret;
}

typedef struct
{
    OMX_COLOR_FORMATTYPE format;
    enum AVPixelFormat pixfmt;
} PixToFmt;

static const PixToFmt pixtofmttable[] = {
    {OMX_COLOR_FormatYUV420Planar, AV_PIX_FMT_YUV420P},
    {OMX_COLOR_FormatYUV420SemiPlanar, AV_PIX_FMT_NV12},
    {OMX_COLOR_FormatYUV420Planar, AV_PIX_FMT_YUVJ420P},
};

static OMX_COLOR_FORMATTYPE LibavFmtToOMXFmt(enum AVPixelFormat pixfmt)
{
    OMX_U32 i;

    for (i = 0; i < G_N_ELEMENTS (pixtofmttable); i++)
        if (pixtofmttable[i].pixfmt == pixfmt)
            return pixtofmttable[i].format;

    LOG_DEBUG ("Unknown pixel format %d", pixfmt);
    return OMX_COLOR_FormatUnused;
}

OMX_ERRORTYPE LibavVideoDec::DetectOutputFmt()
{
    nPadWidth = (codecContext->coded_width +15)&(~15);
    nPadHeight = (codecContext->coded_height +15)&(~15);

    nOutBufferCnt = 5;
    if(sOutFmt.eColorFormat != OMX_COLOR_Format16bitRGB565)
        sOutFmt.eColorFormat = LibavFmtToOMXFmt(codecContext->pix_fmt);

    LOG_DEBUG("libav output format %d", codecContext->pix_fmt);

    sOutFmt.nFrameWidth = ALIGN_STRIDE(nPadWidth);
    sOutFmt.nFrameHeight = ALIGN_STRIDE(nPadHeight);

    if((OMX_S32)sOutFmt.nFrameWidth < nFrameWidthStride || (OMX_S32)sOutFmt.nFrameHeight < nFrameHeightStride){
        sOutFmt.nFrameWidth = ALIGN_STRIDE(nFrameWidthStride);
        sOutFmt.nFrameHeight = ALIGN_STRIDE(nFrameHeightStride);
    }

    sOutFmt.nStride = sOutFmt.nFrameWidth;
    sOutFmt.nSliceHeight = sOutFmt.nFrameHeight;
    nOutBufferSize = sOutFmt.nFrameWidth * sOutFmt.nFrameHeight * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;
    LOG_DEBUG("output color format: %d, nOutBufferSize %d\n", sOutFmt.eColorFormat, nOutBufferSize);

    sOutCrop.nLeft = 0;
    sOutCrop.nTop = 0;
    sOutCrop.nWidth = codecContext->width;
    sOutCrop.nHeight = codecContext->height;

    LOG_DEBUG("Width: %d, Height: %d\n", codecContext->width, codecContext->height);
    LOG_DEBUG("nPadWidth: %d, nPadHeight: %d\n", nPadWidth, nPadHeight);
    LOG_DEBUG("coded Width: %d, coded Height: %d\n", codecContext->coded_width, codecContext->coded_height);
    LOG_DEBUG("input width=%d,height=%d",sInFmt.nFrameWidth,sInFmt.nFrameHeight);
    VideoFilter::OutputFmtChanged();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::ProcessQOS()
{
    OMX_TIME_CONFIG_TIMESTAMPTYPE sCur;
    OMX_TIME_CONFIG_SCALETYPE sScale;
    OMX_TICKS nTimeStamp;
    OMX_S32 param;

    nTimeStamp=QueryStreamTs();
    if(nTimeStamp >= 0 && pClock!=NULL)
    {
        OMX_TIME_CONFIG_PLAYBACKTYPE sPlayback;
        OMX_INIT_STRUCT(&sPlayback, OMX_TIME_CONFIG_PLAYBACKTYPE);
        OMX_GetConfig(pClock, OMX_IndexConfigPlaybackRate, &sPlayback);
        if(sPlayback.ePlayMode != NORMAL_MODE){
            return OMX_ErrorNone;
        }
        OMX_INIT_STRUCT(&sCur, OMX_TIME_CONFIG_TIMESTAMPTYPE);
        OMX_GetConfig(pClock, OMX_IndexConfigTimeCurrentMediaTime, &sCur);
        if(sCur.nTimestamp > (nTimeStamp - DROP_B_THRESHOLD))
        {
            LOG_DEBUG("drop B frame \r\n");
            codecContext->skip_frame = AVDISCARD_NONREF;
            nFramesToCheckDiscarding = MAX_FRAMES_TO_CHECK_DISCARDING;
        }
        else
        {
            LOG_DEBUG("needn't drop B frame \r\n");
            codecContext->skip_frame = AVDISCARD_NONE;
        }
    }

    return OMX_ErrorNone;
}

FilterBufRetCode LibavVideoDec::DecodeOneFrame()
{
    FilterBufRetCode ret = FILTER_OK;
    AVPacket pkt;
    int got_picture = 0;
    int len = 0;
    int err;

    if(bLibavHoldOutputBuffer == OMX_TRUE && pOutBuffer != NULL)
        return FILTER_HAS_OUTPUT;

    if(bOutEos == OMX_TRUE && pOutBuffer != NULL)
        return FILTER_LAST_OUTPUT;

    if((bLibavHoldOutputBuffer == OMX_TRUE || bOutEos == OMX_TRUE) \
            && pOutBuffer == NULL)
        return FILTER_NO_OUTPUT_BUFFER;

    if(pInBuffer == NULL && bInEos != OMX_TRUE)
        return FILTER_NO_INPUT_BUFFER;

    if(bNeedReportOutputFormat) {
        bInit = OMX_FALSE;
    }

    if (bNeedInput) {
        ILibavDec->AvInitPacket(&pkt);
        /* set end of buffer to 0 (this ensures that no overreading happens for
         * damaged mpeg streams) */
        /* ensure allocate more FF_INPUT_BUFFER_PADDING_SIZE for input buffer. */
        if(pInBuffer) {
            fsl_osal_memset((fsl_osal_ptr)((unsigned long)pInBuffer + nInputSize), 0, \
                    FF_INPUT_BUFFER_PADDING_SIZE);
        }
        pkt.data = (uint8_t *)pInBuffer;
        pkt.size = nInputSize;

        ProcessQOS();

        if(codecID == AV_CODEC_ID_VP8) {
            if(pInBuffer && !(((uint8_t *)pInBuffer)[0] & 0x10)) {
                LOG_DEBUG("invisible: %d", !(((uint8_t *)pInBuffer)[0] & 0x10));
                nInvisibleFrame ++;
            }
        }

        LOG_DEBUG("input buffer size: %d codec data size: %d\n", nInputSize, nCodecDataLen);
        // an AVPacket with data set to NULL and size set to 0 is considered a flush packet, which signals the end of the stream.
        if (bInEos == OMX_TRUE && nInputSize == 0)
            pkt.data = NULL;

        err = ILibavDec->AvcodecSendPacket(codecContext, &pkt);
        if (err < 0 && err != AVERROR_EOF)
            LOG_WARNING("avcodec_send_packet failed with err %d\n", err);
    }

    //  Unlike with older APIs, the packet is always fully consumed, and if it contains multiple frames,
    //  it will require you to call avcodec_receive_frame() multiple times afterwards before you can send a new packet.
    err = ILibavDec->AvcodecReceiveFrame(codecContext, picture);
    if (err >= 0) {
        got_picture = 1;
        bNeedInput = OMX_FALSE;
    } else {
        pInBuffer = NULL;
        nInputSize = 0;
        ret = FILTER_INPUT_CONSUMED;
        bNeedInput = OMX_TRUE;
    }

    if(bInEos == OMX_TRUE && got_picture == 0) {
        if(pOutBuffer == NULL) {
            ret = (FilterBufRetCode) (ret | FILTER_NO_OUTPUT_BUFFER);
            bOutEos = OMX_TRUE;
            return ret;
        } else {
            // after the last avpacket is sent, keep calling avcodec_receive_frame to get the frames.
            if (ILibavDec->AvcodecReceiveFrame(codecContext, picture) >= 0) {
                got_picture = 1;
            }
            else {
                ret = (FilterBufRetCode) (ret | FILTER_LAST_OUTPUT);
                eDecState = LIBAV_DEC_STOP;
                return ret;
            }
        }
    }

    LOG_DEBUG("buffer len %d, got_picture %d", nInputSize, got_picture);
    LOG_DEBUG("nInvisibleFrame %d", nInvisibleFrame);

    if(got_picture) {
        bLibavHoldOutputBuffer = OMX_TRUE;

        if(bNeedReportOutputFormat) {
            DetectOutputFmt();
            bNeedReportOutputFormat = OMX_FALSE;
            bInit = OMX_TRUE;
            return ret;
        }

        //check resolution changed for non-adaptive playback
        if((nFrameWidthStride < 0 && nFrameHeightStride < 0) &&
            ((OMX_U32)codecContext->width < sOutCrop.nWidth || (OMX_U32)codecContext->height < sOutCrop.nHeight)){
            LOG_DEBUG("LibavVideoDec::DecodeOneFrame 2 DetectOutputFmt %d,%d,%d,%d\n",
                codecContext->width,sOutCrop.nWidth,codecContext->height,sOutCrop.nHeight);
            DetectOutputFmt();
        }else if((OMX_U32)codecContext->width > sOutFmt.nFrameWidth || (OMX_U32)codecContext->height >
            sOutFmt.nFrameHeight){
             //for adaptive playback, send port setting changed event only when sOutCrop.nWidth > nFrameWidthStride
            LOG_DEBUG("LibavVideoDec::DecodeOneFrame 1 DetectOutputFmt for adaptive playback\n");
            DetectOutputFmt();
        }

        sOutCrop.nWidth = codecContext->width;
        sOutCrop.nHeight = codecContext->height;

        if(pOutBuffer == NULL) {
            ret = (FilterBufRetCode) (ret | FILTER_NO_OUTPUT_BUFFER);
        } else {
            ret = (FilterBufRetCode) (ret | FILTER_HAS_OUTPUT);
        }
    } else {
        if(nFramesToCheckDiscarding > 0)
            ret = (FilterBufRetCode) (ret | FILTER_SKIP_OUTPUT);
        if (nInvisibleFrame) {
            nInvisibleFrame --;
            ret = (FilterBufRetCode) (ret | FILTER_SKIP_OUTPUT);
        }
    }

    if(nFramesToCheckDiscarding > 0)
        nFramesToCheckDiscarding--;

    return ret;
}

OMX_ERRORTYPE LibavVideoDec::GetOutputBuffer(OMX_PTR *ppBuffer,OMX_S32* pOutSize)
{
    OMX_S32 Ysize = sOutFmt.nFrameWidth * sOutFmt.nFrameHeight;
    OMX_S32 Usize = Ysize/4;
    OMX_S32 i;
    OMX_U8* y = (OMX_U8*)pOutBuffer;
    OMX_U8* u = (OMX_U8*)ALIGN_CHROMA((unsigned long)pOutBuffer+Ysize);
    OMX_U8* v = (OMX_U8*)ALIGN_CHROMA((unsigned long)u+Usize);
    OMX_U8* ysrc;
    OMX_U8* usrc;
    OMX_U8* vsrc;

    //LOG_DEBUG ("Frame w:h %d:%d, codecContext w:h %d:%d, linesize %d:%d:%d\n",
    //        sOutFmt.nFrameWidth, sOutFmt.nFrameHeight,
    //        codecContext->width, codecContext->height,
    //        picture->linesize[0], picture->linesize[1], picture->linesize[2]);

    if(!bLibavHoldOutputBuffer){
        *ppBuffer = pOutBuffer;
        *pOutSize = 0;
        pOutBuffer = NULL;
        return OMX_ErrorNone;
    }


    //printf("GetOutputBuffer: sOutFmt color %d", sOutFmt.eColorFormat);


    if(sOutFmt.eColorFormat != OMX_COLOR_Format16bitRGB565){
        ysrc = picture->data[0];
        usrc = picture->data[1];
        vsrc = picture->data[2];

        fsl_osal_memset(pOutBuffer, 0, Ysize*3/2);

        LOG_DEBUG("libav dec get output buffer: %p\n", pOutBuffer);
        for(i=0;i<codecContext->height;i++)
        {
            fsl_osal_memcpy((OMX_PTR)y, (OMX_PTR)ysrc, codecContext->width);
            y+=sOutFmt.nFrameWidth;
            ysrc+=picture->linesize[0];
        }
        for(i=0;i<codecContext->height/2;i++)
        {
            fsl_osal_memcpy((OMX_PTR)u, (OMX_PTR)usrc, codecContext->width/2);
            fsl_osal_memcpy((OMX_PTR)v, (OMX_PTR)vsrc, codecContext->width/2);
            u+=sOutFmt.nFrameWidth/2;
            v+=sOutFmt.nFrameWidth/2;
            usrc+=picture->linesize[1];
            vsrc+=picture->linesize[2];
        }
    }
    else{
        if(!pImageConvert)
            pImageConvert = OMX_ImageConvertCreate();

        if(!pImageConvert){
            LOG_ERROR("Create image converter fail!");
            return OMX_ErrorUndefined;
        }

        OMX_IMAGE_PORTDEFINITIONTYPE in_fmt, out_fmt;
        in_fmt.nFrameWidth = codecContext->width;
        in_fmt.nFrameHeight = codecContext->height;
        in_fmt.eColorFormat = LibavFmtToOMXFmt(codecContext->pix_fmt);
        in_fmt.nStride = picture->linesize[0];
        out_fmt.nFrameWidth = codecContext->width;
        out_fmt.nFrameHeight = codecContext->height;
        out_fmt.nStride = sOutFmt.nFrameWidth;
        out_fmt.eColorFormat = sOutFmt.eColorFormat;

        OMX_CONFIG_RECTTYPE crop;
        crop.nLeft = crop.nTop = 0;
        crop.nWidth = codecContext->width;
        crop.nHeight = codecContext->height;

        LOG_DEBUG("codecContext w/h %d/%d, linesize %d/%d/%d, outFmt w/h %d/%d",
            codecContext->width, codecContext->height, picture->linesize[0], picture->linesize[1],
            picture->linesize[2], sOutFmt.nFrameWidth, sOutFmt.nFrameHeight);

        OMX_S32 ret = pImageConvert->resize(pImageConvert, &in_fmt, &crop,
                                picture->data[0], picture->data[1], picture->data[2],
                                &out_fmt, (OMX_U8*)pOutBuffer);

        if(!ret){
            printf("image convert fail!");
            return OMX_ErrorUndefined;
        }

    }

    *ppBuffer = pOutBuffer;
    *pOutSize = sOutFmt.nFrameWidth * sOutFmt.nFrameHeight * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;
    pOutBuffer = NULL;

    bLibavHoldOutputBuffer = OMX_FALSE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::FlushInputBuffer()
{
    pInBuffer = NULL;
    nInputSize = 0;
    nInputOffset = 0;
    bInEos = OMX_FALSE;
    bLibavHoldOutputBuffer = OMX_FALSE;
    bOutEos = OMX_FALSE;
    bNeedInput = OMX_TRUE;

    nInvisibleFrame = 0;

    if(codecContext)
        ILibavDec->AvcodecFlushBuffers(codecContext);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavVideoDec::FlushOutputBuffer()
{
    pOutBuffer = NULL;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE LibavVideoDec::SetCropInfo(OMX_CONFIG_RECTTYPE *sCrop)
{
    if(sCrop == NULL || sCrop->nPortIndex != OUT_PORT)
        return OMX_ErrorBadParameter;

    sOutCrop.nTop = sCrop->nTop;
    sOutCrop.nLeft = sCrop->nLeft;
    sOutCrop.nWidth = sCrop->nWidth;
    sOutCrop.nHeight = sCrop->nHeight;
    LOG_DEBUG("SetCropInfo w=%d,h=%d",sCrop->nWidth,sCrop->nHeight);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE LibavVideoDec::GetCropInfo(OMX_CONFIG_RECTTYPE *sCrop)
{
    if(sCrop == NULL)
        return OMX_ErrorBadParameter;

    if(sCrop->nPortIndex != OUT_PORT)
        return OMX_ErrorUnsupportedIndex;

    sCrop->nLeft = sOutCrop.nLeft;
    sCrop->nTop = sOutCrop.nTop;
    sCrop->nWidth = sOutCrop.nWidth;
    sCrop->nHeight = sOutCrop.nHeight;
    LOG_LOG("GetCropInfo w=%d,h=%d",sCrop->nWidth,sCrop->nHeight);
    return OMX_ErrorNone;
}

/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE LibavVideoDecoderInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        LibavVideoDec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(LibavVideoDec, ());
        if(obj == NULL)
            return OMX_ErrorInsufficientResources;

        base = (ComponentBase*)obj;
        ret = base->ConstructComponent(pHandle);
        if(ret != OMX_ErrorNone)
            return ret;

        return ret;
    }
}

/* File EOF */
