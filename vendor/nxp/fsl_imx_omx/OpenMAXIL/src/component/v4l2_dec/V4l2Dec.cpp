/**
 *  Copyright 2018-2019 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "V4l2Dec.h"
#include "G2dProcess.h"
#include "OpenCL2dProcess.h"

#ifdef ENABLE_TS_MANAGER
#include "Tsm_wrapper.h"
#endif

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_LOG
#define LOG_LOG printf
#endif

#define VPU_LOG_LEVELFILE "/data/vpu_dec_level"
#define DUMP_INPUT_FILE "/data/temp_dec_in.bit"
#define DUMP_POSTPROCESS_FILE "/data/temp_dec_process.yuv"
#define DUMP_OUTPUT_FILE "/data/temp_dec_out.yuv"
#define MAX_DUMP_FRAME (200)
#define DUMP_LOG_FLAG_DUMP_INPUT     0x1
#define DUMP_LOG_FLAG_DUMP_OUTPUT    0x2
#define DUMP_LOG_FLAG_DUMP_POSTPROCESS   0x4


#define DEFAULT_BUF_IN_CNT  (3)
#define DEFAULT_BUF_IN_SIZE (1024*1024)


#define DEFAULT_BUF_OUT_CNT    (3)
#define DEFAULT_DMA_BUF_CNT    (9)

#define DEFAULT_FRM_WIDTH       (2048)
#define DEFAULT_FRM_HEIGHT      (1280)
#define DEFAULT_FRM_RATE        (30 * Q16_SHIFT)

#define FRAME_ALIGN     (512)

#define EXTRA_BUF_OUT_CNT  (3)

#define MIN_CAPTURE_BUFFER_CNT (3)

typedef struct{
OMX_U32 type;
const char* role;
const char* name;
}V4L2_DEC_ROLE;

static V4L2_DEC_ROLE role_table[]={
{OMX_VIDEO_CodingMPEG2,"video_decoder.mpeg2","OMX.Freescale.std.video_decoder.mpeg2.hw-based"},
{OMX_VIDEO_CodingH263,"video_decoder.h263","OMX.Freescale.std.video_decoder.h263.hw-based"},
{OMX_VIDEO_CodingSORENSON263,"video_decoder.sorenson","OMX.Freescale.std.video_decoder.sorenson.hw-based"},
{OMX_VIDEO_CodingMPEG4,"video_decoder.mpeg4","OMX.Freescale.std.video_decoder.mpeg4.hw-based"},
{OMX_VIDEO_CodingWMV9,"video_decoder.wmv9","OMX.Freescale.std.video_decoder.wmv9.hw-based"},
{OMX_VIDEO_CodingRV,"video_decoder.rv","OMX.Freescale.std.video_decoder.rv.hw-based"},
{OMX_VIDEO_CodingAVC, "video_decoder.avc","OMX.Freescale.std.video_decoder.avc.hw-based"},
{OMX_VIDEO_CodingDIVX,"video_decoder.divx","OMX.Freescale.std.video_decoder.divx.hw-based"},
{OMX_VIDEO_CodingDIV4, "video_decoder.div4","OMX.Freescale.std.video_decoder.div4.hw-based"},
{OMX_VIDEO_CodingXVID,"video_decoder.xvid","OMX.Freescale.std.video_decoder.xvid.hw-based"},
{OMX_VIDEO_CodingMJPEG,"video_decoder.mjpeg","OMX.Freescale.std.video_decoder.mjpeg.hw-based"},
{OMX_VIDEO_CodingVP8, "video_decoder.vp8","OMX.Freescale.std.video_decoder.vp8.hw-based"},
{OMX_VIDEO_CodingVP6, "video_decoder.vp6","OMX.Freescale.std.video_decoder.vp6.hw-based"},
{OMX_VIDEO_CodingHEVC,"video_decoder.hevc","OMX.Freescale.std.video_decoder.hevc.hw-based"},
};

static OMX_ERRORTYPE PostProcessCallBack(Process3 *pPostProcess, OMX_PTR pAppData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pOutputBufHdlr = NULL;

    if(pPostProcess == NULL || pAppData == NULL)
        return OMX_ErrorBadParameter;

    V4l2Dec * base = (V4l2Dec *)pAppData;
    LOG_LOG("PostProcessCallBack\n");

    if(OMX_ErrorNone == pPostProcess->GetOutputReturnBuffer(&pOutputBufHdlr)){
        base->ReturnBuffer(pOutputBufHdlr,OUT_PORT);
    }

    return OMX_ErrorNone;
}
void *filterThreadHandler(void *arg)
{
    V4l2Dec *base = (V4l2Dec*)arg;
    OMX_S32 ret = 0;
    OMX_S32 poll_ret = 0;

    LOG_LOG("[%p]filterThreadHandler BEGIN \n",base);
    poll_ret = base->pV4l2Dev->Poll(base->nFd);
    LOG_LOG("[%p]filterThreadHandler END ret=%x \n",base,poll_ret);

    if(poll_ret & V4L2_DEV_POLL_RET_EVENT_RC){
        LOG_LOG("V4L2_DEV_POLL_RET_EVENT_RC \n");

        if(base->bEnabledPostProcess && base->bUseDmaBuffer)
            ret = base->HandleFormatChangedForIon(OUT_PORT);
        else
            ret = base->HandleFormatChanged(OUT_PORT);

        if(ret == OMX_ErrorStreamCorrupt)
            base->SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
    }

    if(poll_ret & V4L2_DEV_POLL_RET_EVENT_EOS){
        LOG_LOG("V4L2_DEV_POLL_RET_EVENT_EOS \n");

        if(base->eState == V4L2_DEC_STATE_RES_CHANGED){
            LOG_LOG("HandleEOSEvent delayed");
            base->bRetryHandleEOS = OMX_TRUE;
        }
        else
        {
            ret = base->HandleEOSEvent();
            if(ret == OMX_ErrorNone)
                base->bReceiveOutputEOS = OMX_TRUE;
            else
                base->bRetryHandleEOS = OMX_TRUE;
        }
        LOG_LOG("V4L2_DEV_POLL_RET_EVENT_EOS ret=%d\n",ret);
    }
    if(poll_ret & V4L2_DEV_POLL_RET_EVENT_SKIP){
        base->HandleSkipEvent();
        LOG_LOG("V4L2_DEV_POLL_RET_EVENT_SKIP \n");
    }

    if(base->bOutputEOS)
        return NULL;

    if(poll_ret & V4L2_DEV_POLL_RET_CAPTURE){
        LOG_LOG("V4L2_DEV_POLL_RET_CAPTURE \n");
        base->outObj->ProcessBuffer(0);
    }

    if(base->outObj->HasOutputBuffer()){
        ret = base->HandleOutObjBuffer();
        if(ret != OMX_ErrorNone)
            LOG_ERROR("HandleOutObjBuffer err=%d",ret);
    }

    if(poll_ret & V4L2_DEV_POLL_RET_OUTPUT){
        LOG_LOG("V4L2_DEV_POLL_RET_OUTPUT \n");
        base->inObj->ProcessBuffer(0);
    }

    if(base->inObj->HasOutputBuffer()){
        ret = base->HandleInObjBuffer();
        if(ret != OMX_ErrorNone)
            LOG_ERROR("HandleInObjBuffer err=%d",ret);
    }

    if(!base->bOutputStarted){
        fsl_osal_sleep(1000);
        return NULL;
    }

    if(poll_ret & V4L2OBJECT_ERROR)
        base->HandleErrorEvent();

    return NULL;
}
V4l2Dec::V4l2Dec()
{
    eDevType = V4L2_DEV_TYPE_DECODER;
    ParseVpuLogLevel(VPU_LOG_LEVELFILE);
}
OMX_ERRORTYPE V4l2Dec::CreateObjects()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    inObj = FSL_NEW(V4l2Object,());

    outObj = FSL_NEW(V4l2Object,());

    if(inObj == NULL || outObj == NULL)
        return OMX_ErrorInsufficientResources;

    nInputPlane = 1;
    if(pV4l2Dev->isV4lBufferTypeSupported(nFd,eDevType,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE))
        ret = inObj->Create(nFd,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,1);
    else if(pV4l2Dev->isV4lBufferTypeSupported(nFd,eDevType,V4L2_BUF_TYPE_VIDEO_OUTPUT))
        ret = inObj->Create(nFd,V4L2_BUF_TYPE_VIDEO_OUTPUT,1);

    if(ret != OMX_ErrorNone)
        return ret;

    if(pV4l2Dev->isV4lBufferTypeSupported(nFd,eDevType,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)){
        nOutputPlane = 2;
        ret = outObj->Create(nFd,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,nOutputPlane);
    }
    else if(pV4l2Dev->isV4lBufferTypeSupported(nFd,eDevType,V4L2_BUF_TYPE_VIDEO_CAPTURE)){
        nOutputPlane = 1;
        ret = outObj->Create(nFd,V4L2_BUF_TYPE_VIDEO_CAPTURE,nOutputPlane);
    }
    return ret;
}
OMX_ERRORTYPE V4l2Dec::SetDefaultPortSetting()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_decoder.avc.hw-based");

    fsl_osal_memset(&sInFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    sInFmt.nFrameWidth = DEFAULT_FRM_WIDTH;
    sInFmt.nFrameHeight = DEFAULT_FRM_HEIGHT;
    sInFmt.xFramerate = DEFAULT_FRM_RATE;
    sInFmt.eColorFormat = OMX_COLOR_FormatUnused;
    sInFmt.eCompressionFormat = OMX_VIDEO_CodingAVC;

    fsl_osal_memset(&sOutFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    sOutFmt.nFrameWidth = DEFAULT_FRM_WIDTH;
    sOutFmt.nFrameHeight = DEFAULT_FRM_HEIGHT;
    sOutFmt.eColorFormat = OMX_COLOR_FormatYCbYCr;
    sOutFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

    nInBufferCnt = DEFAULT_BUF_IN_CNT;
    nInBufferSize = DEFAULT_BUF_IN_SIZE;
    nOutBufferCnt = DEFAULT_BUF_OUT_CNT;

    nWidthAlign = FRAME_ALIGN;
    nHeightAlign = FRAME_ALIGN;

    nOutBufferSize = Align(sOutFmt.nFrameWidth, nWidthAlign) * Align(sOutFmt.nFrameHeight, nHeightAlign) * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;

    OMX_INIT_STRUCT(&sOutputCrop, OMX_CONFIG_RECTTYPE);
    sOutputCrop.nWidth = DEFAULT_FRM_WIDTH;
    sOutputCrop.nHeight = DEFAULT_FRM_HEIGHT;

    nInPortFormatCnt = 0;

    #ifdef MALONE_VPU
    nOutPortFormatCnt = 2;
    eOutPortPormat[0] = OMX_COLOR_FormatYCbYCr;
    eOutPortPormat[1] = OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled;
    //disable it as opencl2d convert function has low performance
    //eOutPortPormat[2] = OMX_COLOR_FormatYUV420SemiPlanar;
    #endif


    nMaxDurationMsThr = -1;
    nMaxBufCntThr = -1;

    //this will start decoding without sending the OMX_EventPortSettingsChanged.
    //android decoders prefer not to send the event, set the default value to OMX_TRUE
    bSendPortSettingChanged = OMX_TRUE;

    pParser = NULL;
    bEnabledFrameParser = OMX_FALSE;

    pPostProcess = NULL;
    bEnabledPostProcess = OMX_FALSE;

    pDmaBuffer = NULL;
    bUseDmaBuffer = OMX_TRUE;

    nDmaBufferCnt = DEFAULT_DMA_BUF_CNT;
    eDmaBufferFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    nDmaBufferSize[0] = Align(sOutFmt.nFrameWidth, nWidthAlign) * Align(sOutFmt.nFrameHeight, nHeightAlign);
    nDmaBufferSize[1] = nDmaBufferSize[0] / 2;
    nDmaBufferSize[2] = 0;

    bUseDmaOutputBuffer = OMX_FALSE;
    pDmaOutputBuffer = NULL;

    pCodecData = NULL;
    nCodecDataLen = 0;
    bReceiveFrame = OMX_FALSE;
    bNeedCodecData = OMX_FALSE;

    eState = V4L2_DEC_STATE_IDLE;

    bAndroidNativeBuffer = OMX_FALSE;
    bAdaptivePlayback = OMX_FALSE;

    bInputEOS = OMX_FALSE;
    bOutputEOS = OMX_FALSE;
    bOutputStarted = OMX_FALSE;
    bNewSegment = OMX_FALSE;

    bDmaBufferAllocated = OMX_FALSE;
    bAllocateFailed = OMX_FALSE;
    nErrCnt = 0;
    bRetryHandleEOS = OMX_FALSE;
    bReceiveOutputEOS = OMX_FALSE;
    LOG_LOG("V4l2Dec::SetDefaultPortSetting success\n");
    nOutObjBufferCnt = 0;

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dec::QueryVideoProfile(OMX_PTR pComponentParameterStructure)
{
    struct CodecProfileLevel {
        OMX_U32 mProfile;
        OMX_U32 mLevel;
    };

    static const CodecProfileLevel kProfileLevels[] = {
        { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
        { OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel51 },
        { OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel51 },
    };
    
    static const CodecProfileLevel kM4VProfileLevels[] = {
        { OMX_VIDEO_MPEG4ProfileSimple, 0x100 /*OMX_VIDEO_MPEG4Level6*/ },
        { OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level5 },
    };
    
    static const CodecProfileLevel kMpeg2ProfileLevels[] = {
        { OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelHL},
        { OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelHL},
    };

    static const CodecProfileLevel kHevcProfileLevels[] = {
        { OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCMainTierLevel51 },
    };

    static const CodecProfileLevel kH263ProfileLevels[] = {
        { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level70 },
        { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level70 }
    };

    static const CodecProfileLevel kVp8ProfileLevels[] = {
        { OMX_VIDEO_VP8ProfileMain,     OMX_VIDEO_VP8Level_Version0 },
    };

    OMX_VIDEO_PARAM_PROFILELEVELTYPE  *pPara;
    OMX_S32 index;
    OMX_S32 nProfileLevels;
    
    pPara = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;

    LOG_LOG("QueryVideoProfile CodingType=%d",CodingType);

    switch((int)CodingType)
    {
        case OMX_VIDEO_CodingAVC:
            index = pPara->nProfileIndex;

            nProfileLevels =sizeof(kProfileLevels) / sizeof(kProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }

            pPara->eProfile = kProfileLevels[index].mProfile;
            pPara->eLevel = kProfileLevels[index].mLevel;
            LOG_LOG("QueryVideoProfile AVC pPara->eProfile=%x,level=%x\n",pPara->eProfile,pPara->eLevel);
            break;
        case OMX_VIDEO_CodingHEVC:
            index = pPara->nProfileIndex;

            nProfileLevels =sizeof(kHevcProfileLevels) / sizeof(kHevcProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }

            pPara->eProfile = kHevcProfileLevels[index].mProfile;
            pPara->eLevel = kHevcProfileLevels[index].mLevel;
            LOG_LOG("QueryVideoProfile HEVC profile=%d,level=%d",pPara->eProfile, pPara->eLevel);
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
        case OMX_VIDEO_CodingMPEG2:
            index = pPara->nProfileIndex;

            nProfileLevels =sizeof(kMpeg2ProfileLevels) / sizeof(kMpeg2ProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }

            pPara->eProfile = kMpeg2ProfileLevels[index].mProfile;
            pPara->eLevel = kMpeg2ProfileLevels[index].mLevel;
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
        case 9://google index OMX_VIDEO_CodingVP8:
            index = pPara->nProfileIndex;

            nProfileLevels =sizeof(kVp8ProfileLevels) / sizeof(kVp8ProfileLevels[0]);
            if (index >= nProfileLevels) {
                return OMX_ErrorNoMore;
            }

            pPara->eProfile = kVp8ProfileLevels[index].mProfile;
            pPara->eLevel = kVp8ProfileLevels[index].mLevel;
            break;
        default:
            return OMX_ErrorUnsupportedIndex;
    }

    return OMX_ErrorNone;

}

OMX_ERRORTYPE V4l2Dec::SetRoleFormat(OMX_STRING role)
{
    OMX_BOOL bGot = OMX_FALSE;
    OMX_U32 i = 0;
    for(i = 0; i < sizeof(role_table)/sizeof(V4L2_DEC_ROLE); i++){
        if(fsl_osal_strcmp(role, role_table[i].role) == 0){
            CodingType = (OMX_VIDEO_CODINGTYPE)role_table[i].type;
            bGot = OMX_TRUE;
            fsl_osal_strcpy((fsl_osal_char*)name, role_table[i].name);
            break;
        }
    }

    if(bGot)
        return OMX_ErrorNone;
    else 
        return OMX_ErrorUndefined;
}

OMX_ERRORTYPE V4l2Dec::SetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((OMX_U32)nParamIndex) {
        case OMX_IndexParamStandardComponentRole:
        {
            fsl_osal_strcpy( (fsl_osal_char *)cRole,(fsl_osal_char *)((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole);
            ret = SetRoleFormat((OMX_STRING)cRole);
            if(ret == OMX_ErrorNone){
                if(sInFmt.eCompressionFormat!=CodingType){
                    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

                    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                    sPortDef.nPortIndex = IN_PORT;
                    ports[IN_PORT]->GetPortDefinition(&sPortDef);
                    sInFmt.eCompressionFormat=CodingType;
                    //decoder
                    fsl_osal_memcpy(&(sPortDef.format.video), &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
                    ports[IN_PORT]->SetPortDefinition(&sPortDef);
                    HandleFormatChanged(IN_PORT);
                }
            }
            LOG_DEBUG("SetParameter OMX_IndexParamStandardComponentRole.type=%d\n",CodingType);
        }
        break;
        case OMX_IndexParamVideoPortFormat:
        {
            OMX_VIDEO_PARAM_PORTFORMATTYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex==IN_PORT);
            //set port
            if((sInFmt.eCompressionFormat!=pPara->eCompressionFormat)
                ||(sInFmt.eColorFormat!=pPara->eColorFormat))
            {
                OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

                OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                sPortDef.nPortIndex = IN_PORT;
                ports[IN_PORT]->GetPortDefinition(&sPortDef);
                sInFmt.eCompressionFormat=pPara->eCompressionFormat;
                sInFmt.eColorFormat=pPara->eColorFormat;
                //sInputParam.nFrameRate=pPara->xFramerate/Q16_SHIFT;
                fsl_osal_memcpy(&(sPortDef.format.video), &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
                ports[IN_PORT]->SetPortDefinition(&sPortDef);
                LOG_DEBUG("SetParameter OMX_IndexParamVideoPortFormat.format=%x,f2=%x\n",
                    sInFmt.eCompressionFormat,sInFmt.eColorFormat);

            }
        }
        break;
        case OMX_IndexParamVideoRegisterFrameExt:
        {
            OMX_VIDEO_REG_FRM_EXT_INFO* pExtInfo=(OMX_VIDEO_REG_FRM_EXT_INFO*)pComponentParameterStructure;
            if(pExtInfo->nPortIndex==OUT_PORT){
                LOG_DEBUG("set OMX_IndexParamVideoRegisterFrameExt cnt=%d,w=%d,h=%d",pExtInfo->nMaxBufferCnt,pExtInfo->nWidthStride,pExtInfo->nHeightStride);
                nDmaBufferCnt = pExtInfo->nMaxBufferCnt;
                sOutFmt.nFrameWidth = pExtInfo->nWidthStride;
                sOutFmt.nFrameHeight = pExtInfo->nHeightStride;

                OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
                
                OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                sPortDef.nPortIndex = OUT_PORT;
                ports[OUT_PORT]->GetPortDefinition(&sPortDef);
                sPortDef.format.video.nFrameWidth = sOutFmt.nFrameWidth;
                sPortDef.format.video.nFrameHeight = sOutFmt.nFrameHeight;
                sPortDef.format.video.nStride = sOutFmt.nStride;

                ports[OUT_PORT]->SetPortDefinition(&sPortDef);
                //bAdaptivePlayback = OMX_TRUE;
            }
        }
        break;
        case OMX_IndexParamDecoderCachedThreshold:
        {
            OMX_DECODER_CACHED_THR* pDecCachedInfo=(OMX_DECODER_CACHED_THR*)pComponentParameterStructure;
            if(pDecCachedInfo->nPortIndex==IN_PORT)
            {
                nMaxDurationMsThr=pDecCachedInfo->nMaxDurationMsThreshold;
                nMaxBufCntThr=pDecCachedInfo->nMaxBufCntThreshold;
            }
        }
        break;
        case OMX_IndexParamVideoWmv:
        {
            OMX_VIDEO_PARAM_WMVTYPE  *pPara;
            pPara = (OMX_VIDEO_PARAM_WMVTYPE *)pComponentParameterStructure;
            eWmvFormat = pPara->eFormat;
        }
        break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}

OMX_ERRORTYPE V4l2Dec::GetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((OMX_U32)nParamIndex) {
        case OMX_IndexParamStandardComponentRole:
        {
            fsl_osal_strcpy((OMX_STRING)((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole,(OMX_STRING)cRole);
        }
        break;
        case OMX_IndexParamVideoProfileLevelQuerySupported:
            ret = QueryVideoProfile(pComponentParameterStructure);
            break;
        case OMX_IndexParamVideoWmv:
        {
            OMX_VIDEO_PARAM_WMVTYPE  *pPara;
            pPara = (OMX_VIDEO_PARAM_WMVTYPE *)pComponentParameterStructure;
            pPara->eFormat = eWmvFormat;
        }
        break;
        case OMX_IndexParamVideoPortFormat:
        {
            OMX_VIDEO_PARAM_PORTFORMATTYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;

            if(pPara->nPortIndex >= 2)
                return OMX_ErrorBadPortIndex;

            ret = ports[pPara->nPortIndex]->GetPortFormat(pPara);
            LOG_DEBUG("GET OMX_IndexParamVideoPortFormat index=%d, index=%d, format=%d,color=%x",
                pPara->nPortIndex,pPara->nIndex, pPara->eCompressionFormat,pPara->eColorFormat);
            break;
        }
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;

}

OMX_ERRORTYPE V4l2Dec::GetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch (nParamIndex) {
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *pCropDef;
            pCropDef = (OMX_CONFIG_RECTTYPE*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pCropDef, OMX_CONFIG_RECTTYPE, ret);
            if(ret != OMX_ErrorNone)
                break;

            if(pCropDef->nPortIndex != OUT_PORT){
                ret = OMX_ErrorBadParameter;
                break;
            }

            OMX_CONFIG_RECTTYPE rect;
            ret = outObj->GetCrop(&rect);

            if(rect.nTop >= 0 && rect.nLeft >= 0 && rect.nWidth > 0 && rect.nHeight > 0){
                pCropDef->nTop = rect.nTop;
                pCropDef->nLeft = rect.nLeft;
                pCropDef->nWidth = rect.nWidth;
                pCropDef->nHeight = rect.nHeight;
            }
            else{
                // check correctness of sOutputCrop
                OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
                OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                sPortDef.nPortIndex = OUT_PORT;
                if(ports[OUT_PORT]->GetPortDefinition(&sPortDef) == OMX_ErrorNone){
                    if(sOutputCrop.nLeft + sOutputCrop.nWidth > sPortDef.format.video.nFrameWidth)
                        sOutputCrop.nWidth = sPortDef.format.video.nFrameWidth - sOutputCrop.nLeft ;
                    if(sOutputCrop.nTop + sOutputCrop.nHeight > sPortDef.format.video.nFrameHeight)
                        sOutputCrop.nHeight = sPortDef.format.video.nFrameHeight - sOutputCrop.nTop;
                }
                pCropDef->nTop = sOutputCrop.nTop;
                pCropDef->nLeft = sOutputCrop.nLeft;
                pCropDef->nWidth = sOutputCrop.nWidth;
                pCropDef->nHeight = sOutputCrop.nHeight;
            }
            LOG_DEBUG("GetConfig OMX_IndexConfigCommonOutputCrop OUT_PORT top=%d,left=%d,w=%d,h=%d\n",
                pCropDef->nTop,pCropDef->nLeft,pCropDef->nWidth,pCropDef->nHeight);
        }
        break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE V4l2Dec::SetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((OMX_U32)nParamIndex) {
        case OMX_IndexConfigCommonOutputCrop:
        {
            OMX_CONFIG_RECTTYPE *pCropDef;
            pCropDef = (OMX_CONFIG_RECTTYPE*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pCropDef, OMX_CONFIG_RECTTYPE, ret);
            if(ret != OMX_ErrorNone)
                break;

            if(pCropDef->nPortIndex != OUT_PORT){
                ret = OMX_ErrorBadParameter;
                break;
            }
            ret = outObj->SetCrop(pCropDef);

            fsl_osal_memcpy(&sOutputCrop, pCropDef, sizeof(OMX_CONFIG_RECTTYPE));
            LOG_DEBUG("SetConfig OMX_IndexConfigCommonOutputCrop OUT_PORT crop w=%d\n",pCropDef->nWidth);
        }
        break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE V4l2Dec::ProcessInit()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    CheckIfNeedFrameParser();
    if(bEnabledFrameParser){
        pParser = FSL_NEW(FrameParser,());
        if(pParser ==NULL)
            return OMX_ErrorInsufficientResources;
        ret = pParser->Create(sInFmt.eCompressionFormat);
        if(ret != OMX_ErrorNone)
            return ret;
    }

    bEnabledPostProcess = CheckIfNeedPostProcess();
    if(bEnabledPostProcess && pPostProcess == NULL){
        if(sOutFmt.eColorFormat == OMX_COLOR_Format16bitRGB565 || sOutFmt.eColorFormat == OMX_COLOR_FormatYCbYCr)
            pPostProcess = FSL_NEW( G2dProcess,());
        else if(sOutFmt.eColorFormat == OMX_COLOR_FormatYUV420SemiPlanar)
            pPostProcess = FSL_NEW( OpenCL2dProcess,());

        if(pPostProcess == NULL)
            return OMX_ErrorInsufficientResources;

        ret = pPostProcess->Create(PROCESS3_BUF_DMA_IN_OMX_OUT);
        if(ret != OMX_ErrorNone)
            return ret;
    }

    if(bUseDmaBuffer && pDmaBuffer == NULL){
        pDmaBuffer = FSL_NEW( DmaBuffer,());
        if(pDmaBuffer == NULL)
            return OMX_ErrorInsufficientResources;

        ret = pDmaBuffer->Create(nOutputPlane);
        if(ret != OMX_ErrorNone)
            return ret;

        LOG_LOG("V4l2Dec::ProcessInit pDmaBuffer->Create SUCCESS \n");
    }

    return ret;
}
OMX_ERRORTYPE V4l2Dec::CheckIfNeedFrameParser()
{
    switch((int)sInFmt.eCompressionFormat){
        case OMX_VIDEO_CodingAVC:
        case OMX_VIDEO_CodingHEVC:
            bEnabledFrameParser = OMX_TRUE;
            LOG_LOG("CheckIfNeedFrameParser bEnabledFrameParser\n");
            break;
        default:
            bEnabledFrameParser = OMX_FALSE;
            break;
    }
    return OMX_ErrorNone;
}

OMX_BOOL V4l2Dec::CheckIfNeedPostProcess()
{
    if(OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled == sOutFmt.eColorFormat)
        return OMX_FALSE;
    LOG_LOG("CheckIfNeedPostProcess TRUE sOutFmt.eColorFormat=%x",sOutFmt.eColorFormat);
    return OMX_TRUE;
}
OMX_ERRORTYPE V4l2Dec::DoAllocateBuffer(OMX_PTR *buffer, OMX_U32 nSize,OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_LOG("DoAllocateBuffer port=%d,size=%d\n",nPortIndex,nSize);
    if(nPortIndex == IN_PORT){
        if(!bSetInputBufferCount){
            LOG_LOG("DoAllocateBuffer set inport buffer count=%d\n",nInBufferCnt);
            ret = inObj->SetBufferCount(nInBufferCnt,V4L2_MEMORY_MMAP,nInputPlane);

            if(ret != OMX_ErrorNone){
                LOG_ERROR("inObj->SetBufferCount FAILD");
                return ret;
            }

            bSetInputBufferCount = OMX_TRUE;
        }

        if(nInBufferNum > (OMX_S32)nInBufferCnt){
            LOG_ERROR("nInBufferNum=%d,nInBufferCnt=%d",nInBufferNum, nInBufferCnt);
            return OMX_ErrorInsufficientResources;
        }

        *buffer = inObj->AllocateBuffer(nSize);

        if(*buffer != NULL)
            nInBufferNum++;

    }else if(nPortIndex == OUT_PORT){

        if(bUseDmaBuffer){

            if(!bSetOutputBufferCount){
                LOG_LOG("DoAllocateBuffer set dma buffer count=%d\n",nDmaBufferCnt);
                ret = outObj->SetBufferCount(nDmaBufferCnt,V4L2_MEMORY_DMABUF,nOutputPlane);

                if(ret != OMX_ErrorNone)
                    return ret;

                bSetOutputBufferCount = OMX_TRUE;
            }
            bUseDmaOutputBuffer = OMX_TRUE;

            if(bUseDmaOutputBuffer && pDmaOutputBuffer == NULL){
                pDmaOutputBuffer = FSL_NEW( DmaBuffer,());
                if(pDmaOutputBuffer == NULL)
                    return OMX_ErrorInsufficientResources;
                //do not call create because only call AllocateForOutput() and Free
                //ret = pDmaOutputBuffer->Create(nInputPlane);
                //if(ret != OMX_ErrorNone)
                //    return ret;
            }

            LOG_LOG("pDmaBuffer->AllocateForOutput ");
            ret = pDmaOutputBuffer->AllocateForOutput(nSize, buffer);
            if(ret == OMX_ErrorNone){
                nOutBufferNum++;
                return OMX_ErrorNone;
            }else
                return OMX_ErrorInsufficientResources;
        }

        if(!bSetOutputBufferCount){
            LOG_LOG("DoAllocateBuffer set outport buffer count=%d\n",nOutBufferCnt);
            ret = outObj->SetBufferCount(nOutBufferCnt,V4L2_MEMORY_MMAP,nOutputPlane);

            if(ret != OMX_ErrorNone)
                return ret;

            bSetOutputBufferCount = OMX_TRUE;
        }

        if(nOutBufferNum > (OMX_S32)nOutBufferCnt)
            return OMX_ErrorInsufficientResources;

        *buffer = outObj->AllocateBuffer(nSize);
        if(*buffer != NULL)
            nOutBufferNum++;
    }

    if (*buffer == NULL){
        LOG_ERROR("DoAllocateBuffer OMX_ErrorInsufficientResources");
        return OMX_ErrorInsufficientResources;
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dec::DoFreeBuffer(OMX_PTR buffer,OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(nPortIndex == IN_PORT){

        ret = inObj->FreeBuffer(buffer);

        if(ret != OMX_ErrorNone)
            return ret;
        if(nInBufferNum > 0)
            nInBufferNum --;

    }else if(nPortIndex == OUT_PORT){

        if(bUseDmaOutputBuffer){
            LOG_LOG("call pDmaBuffer->Free");
            ret = pDmaOutputBuffer->Free(buffer);
            if(ret == OMX_ErrorNone)
                nOutBufferNum --;
            return ret;
        }

        if((OMX_U64)buffer == 0x00000001){
            LOG_LOG("DoFreeBuffer output 01 %d\n",nOutBufferNum);
            return OMX_ErrorNone;
        }

        LOG_LOG("DoFreeBuffer output %d\n",nOutBufferNum);
        ret = outObj->FreeBuffer(buffer);
        if(ret != OMX_ErrorNone)
            return ret;
        nOutBufferNum --;
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dec::DoUseBuffer(OMX_PTR buffer,OMX_U32 nSize,OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_LOG("DoUseBuffer nPortIndex=%d",nPortIndex);
    if(nPortIndex == IN_PORT){
        if(nInBufferNum != 0)
            return OMX_ErrorInsufficientResources;
        if(!bSetInputBufferCount){
            LOG_DEBUG("DoUseBuffer set inport buffer count=%d",nInBufferCnt);

            if(eDevType == V4L2_DEV_TYPE_DECODER){
                ret = inObj->SetBufferCount(nInBufferCnt,V4L2_MEMORY_USERPTR,nInputPlane);
            }
            bSetInputBufferCount = OMX_TRUE;
        }

    }else if(nPortIndex == OUT_PORT){
        if(nOutBufferNum != 0)
            return OMX_ErrorInsufficientResources;
        if(!bSetOutputBufferCount){
            // MA-13950: check if need post process in IDLE state 
            bEnabledPostProcess = CheckIfNeedPostProcess();
            LOG_LOG("V4l2Dec::DoUseBuffer bUseDmaBuffer=%d \n",bUseDmaBuffer);
            if(bUseDmaBuffer){
                LOG_LOG("DoUseBuffer set outport buffer nDmaBufferCnt=%d nOutBufferCnt=%d,plane num=%d",nDmaBufferCnt,nOutBufferCnt,nOutputPlane);
                if(bEnabledPostProcess)
                    ret = outObj->SetBufferCount(nDmaBufferCnt, V4L2_MEMORY_DMABUF, nOutputPlane);
                else
                    ret = outObj->SetBufferCount(nOutBufferCnt, V4L2_MEMORY_DMABUF, nOutputPlane);
            }else if(bEnabledPostProcess){
                if(nOutBufferCnt <= PROCESS3_BUF_CNT)
                    return OMX_ErrorInsufficientResources;
                ret = outObj->SetBufferCount(nOutBufferCnt - PROCESS3_BUF_CNT,V4L2_MEMORY_USERPTR,nOutputPlane);
            }else
                ret = outObj->SetBufferCount(nOutBufferCnt,V4L2_MEMORY_USERPTR,nOutputPlane);

            bSetOutputBufferCount = OMX_TRUE;
            bAndroidNativeBuffer = OMX_TRUE;
            LOG_LOG("bAndroidNativeBuffer");
        }
    }

    if(ret != OMX_ErrorNone)
        LOG_ERROR("DoUseBuffer failed,ret=%d",ret);

    return ret;

}
OMX_ERRORTYPE V4l2Dec::PortFormatChanged(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    V4l2ObjectFormat sFormat;
    OMX_COLOR_FORMATTYPE color_format = OMX_COLOR_FormatUnused;
    OMX_VIDEO_CODINGTYPE codec_format = OMX_VIDEO_CodingUnused;
    LOG_DEBUG("PortFormatChanged index=%d\n",nPortIndex);
    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    sPortDef.nPortIndex = nPortIndex;
    ret = ports[nPortIndex]->GetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone){
        LOG_ERROR("GetPortDefinition FAILED\n");
        return ret;
    }
    sFormat.format = 0;
    sFormat.bufferSize[0] = sFormat.bufferSize[1] = sFormat.bufferSize[2] = 0;

    sFormat.width = sPortDef.format.video.nFrameWidth;
    sFormat.height = sPortDef.format.video.nFrameHeight;
    sFormat.stride = Align(sFormat.width, nWidthAlign);

    LOG_DEBUG("PortFormatChanged w=%d,h=%d,s=%d,format=%x\n",sFormat.width,sFormat.height,sFormat.stride,sFormat.format);

    if(nPortIndex == IN_PORT){

        sFormat.bufferSize[0] = nInBufferSize = sPortDef.nBufferSize;
        sFormat.plane_num = nInputPlane;

        if(sPortDef.format.video.eCompressionFormat != OMX_VIDEO_CodingUnused){
            sFormat.format = ConvertOmxCodecFormatToV4l2Format(sPortDef.format.video.eCompressionFormat);
        }

        LOG_LOG("inObj->SetFormat buffer size=%d",sFormat.bufferSize[0]);
        ret = inObj->SetFormat(&sFormat);
        if(ret != OMX_ErrorNone)
            return ret;

        ret = inObj->GetFormat(&sFormat);
        if(ret != OMX_ErrorNone)
            return ret;

        LOG_LOG("inObj->GetFormat buffer size=%d\n",sFormat.bufferSize[0]);

        sInFmt.nFrameWidth = sFormat.width;
        sInFmt.nFrameHeight = sFormat.height;
        sInFmt.nStride = sFormat.stride;
        sInFmt.nSliceHeight = sFormat.height;
        sInFmt.xFramerate = sPortDef.format.video.xFramerate;

        if(0 == sInFmt.nStride)
            sInFmt.nStride = sFormat.width;

        if(ConvertV4l2FormatToOmxCodecFormat(sFormat.format,&codec_format))
            sInFmt.eCompressionFormat = codec_format;

        nInBufferCnt = sPortDef.nBufferCountActual;
        nInBufferSize = sFormat.bufferSize[0] + sFormat.bufferSize[1] + sFormat.bufferSize[2];

        sPortDef.nBufferCountActual = nInBufferCnt;
        sPortDef.nBufferSize = nInBufferSize;
        fsl_osal_memcpy(&sPortDef.format.video, &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

        LOG_DEBUG("PortFormatChanged sInFmt.eColorFormat=%x, sInFmt.eCompressionFormat=%x\n", sInFmt.eColorFormat, sInFmt.eCompressionFormat);
        LOG_DEBUG("PortFormatChanged IN_PORT nInBufferCnt=%d nInBufferSize=%d,\n",nInBufferCnt, nInBufferSize);

    }else if(nPortIndex == OUT_PORT){

        OMX_U32 pad_width = Align(sFormat.width, nWidthAlign);
        OMX_U32 pad_height = Align(sFormat.height, nHeightAlign);

        nOutBufferSize = pad_width * pad_height * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;
        if(nOutBufferSize < sPortDef.nBufferSize)
            nOutBufferSize = sPortDef.nBufferSize;

        nOutBufferCnt = sPortDef.nBufferCountActual;

        sFormat.bufferSize[0] = nOutBufferSize;

        //use one plane_num for decoder output port. size of each plane is calculated by outObj
        //do not use nOutputPlane
        sFormat.plane_num = 1;

        #ifdef MALONE_VPU
        //TODO: handle 10 bit video
        sFormat.format = V4L2_PIX_FMT_NV12;
        #endif

        LOG_LOG("outObj->SetFormat buffer size=%d",sFormat.bufferSize[0]);
        ret = outObj->SetFormat(&sFormat);

        if(ret != OMX_ErrorNone)
            return ret;

        ret = outObj->GetFormat(&sFormat);
        if(ret != OMX_ErrorNone)
            return ret;

        LOG_DEBUG("PortFormatChanged modify buffer size w=%d,h=%d, format=%x,w=%d,h=%d,stride=%d\n",
            sFormat.width,sFormat.height,sFormat.format,sFormat.width,sFormat.height,sFormat.stride);

        sOutFmt.nFrameWidth = pad_width;
        sOutFmt.nFrameHeight = pad_height;
        sOutFmt.nStride = sFormat.stride;
        if(0 == sOutFmt.nStride)
            sOutFmt.nStride = sOutFmt.nFrameWidth;
        sOutFmt.nSliceHeight = sOutFmt.nFrameHeight;

        #ifdef MALONE_VPU
        //set default port format
        if(OMX_COLOR_FormatYCbYCr == sPortDef.format.video.eColorFormat || OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled == sPortDef.format.video.eColorFormat)
            sOutFmt.eColorFormat = sPortDef.format.video.eColorFormat;
        else
            sOutFmt.eColorFormat = OMX_COLOR_FormatYCbYCr;
        #endif

        fsl_osal_memcpy(&sPortDef.format.video, &sOutFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
        sPortDef.nBufferCountActual = nOutBufferCnt;

        if(nOutBufferSize < sFormat.bufferSize[0] + sFormat.bufferSize[1] + sFormat.bufferSize[2])
            nOutBufferSize = sFormat.bufferSize[0] + sFormat.bufferSize[1] + sFormat.bufferSize[2];

        sPortDef.nBufferSize = nOutBufferSize;

        LOG_DEBUG("PortFormatChanged sOutFmt.eColorFormat=%x, sOutFmt.eCompressionFormat=%x\n", sOutFmt.eColorFormat, sOutFmt.eCompressionFormat);
        LOG_DEBUG("PortFormatChanged OUT_PORT nOutBufferCnt=%d nOutBufferSize=%d,\n",nOutBufferCnt, nOutBufferSize);

    }

    sPortDef.eDomain = OMX_PortDomainVideo;
    ret = ports[nPortIndex]->SetPortDefinition(&sPortDef);

    if(ret != OMX_ErrorNone)
        return ret;

    ret = updateCropInfo(nPortIndex);
    if(ret != OMX_ErrorNone)
        return ret;

    if(nPortIndex == IN_PORT && bEnabledFrameParser && pParser != NULL)
        pParser->Reset(sInFmt.eCompressionFormat);

    return OMX_ErrorNone;

}

OMX_ERRORTYPE V4l2Dec::FlushComponent(OMX_U32 nPortIndex)
{
    OMX_BUFFERHEADERTYPE * bufHdlr;
    LOG_LOG("FlushComponent index=%d,in num=%d,out num=%d\n",nPortIndex,ports[IN_PORT]->BufferNum(),ports[OUT_PORT]->BufferNum());
    fsl_osal_mutex_lock(sMutex);
    if(nPortIndex == IN_PORT){
        bInputEOS = OMX_FALSE;

        #ifdef ENABLE_TS_MANAGER
        tsmFlush(hTsHandle);
        #endif

        inObj->Flush();
        nInputCnt = 0;
        while(inObj->HasBuffer()){
            OMX_BUFFERHEADERTYPE *pBufHdr = NULL;
            if(OMX_ErrorNone == inObj->GetBuffer(&pBufHdr)){
                ReturnBuffer(pBufHdr,IN_PORT);
            }
        }

        while(ports[IN_PORT]->BufferNum() > 0) {
            ports[IN_PORT]->GetBuffer(&bufHdlr);
            if(bufHdlr != NULL)
                ports[IN_PORT]->SendBuffer(bufHdlr);
        }
    }

    if(nPortIndex == OUT_PORT){

        //inThread->pause();
        bOutputEOS = OMX_FALSE;
        bReceiveOutputEOS = OMX_FALSE;

        outObj->Flush();
        bOutputStarted = OMX_FALSE;
        nOutputCnt = 0;
        nOutObjBufferCnt = 0;

        if(bEnabledPostProcess){

            if (pPostProcess != NULL)
                pPostProcess->Flush();
            
            if(bUseDmaBuffer){
                DmaBufferHdr *pBufHdr = NULL;
                while(outObj->HasBuffer()){
                    if(OMX_ErrorNone == outObj->GetBuffer(&pBufHdr)){
                        pBufHdr->bReadyForProcess = OMX_FALSE;
                        pBufHdr->flag = 0;
                        if(bDmaBufferAllocated)
                            pDmaBuffer->Add(pBufHdr);
                    }
                }

            }

            if (pPostProcess != NULL) {
                while(bUseDmaBuffer){
                    DmaBufferHdr *pBufHdr = NULL;
                    if(OMX_ErrorNone == pPostProcess->GetInputReturnBuffer(&pBufHdr)){
                        pBufHdr->bReadyForProcess = OMX_FALSE;
                        pBufHdr->flag = 0;
                        if(bDmaBufferAllocated)
                            pDmaBuffer->Add(pBufHdr);
                    }else
                        break;
                }

                while(1){
                    if(OMX_ErrorNone == pPostProcess->GetOutputReturnBuffer(&bufHdlr)){
                        ReturnBuffer(bufHdlr,OUT_PORT);
                    }else
                        break;
                }
            }

        }else{

            while(outObj->HasBuffer()){
                if(OMX_ErrorNone == outObj->GetBuffer(&bufHdlr)){
                    ReturnBuffer(bufHdlr,OUT_PORT);
                }
            }

        }

        LOG_LOG("FlushComponent index=1 port num=%d\n",ports[OUT_PORT]->BufferNum());
        while(ports[OUT_PORT]->BufferNum() > 0) {
            ports[OUT_PORT]->GetBuffer(&bufHdlr);
            if(bufHdlr != NULL)
                ports[OUT_PORT]->SendBuffer(bufHdlr);
        }

        eState = V4L2_DEC_STATE_FLUSHED;

    }
    nErrCnt = 0;
    LOG_DEBUG("FlushComponent index=%d END\n",nPortIndex);
    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Dec::DoIdle2Loaded()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_LOG("DoIdle2Loaded BEGIN");

    if(pCodecData != NULL) {
        FSL_FREE(pCodecData);
        nCodecDataLen = 0;
    }

    if(inThread){
        inThread->destroy();
        FSL_DELETE(inThread);
    }

    LOG_LOG("in thread destroy success\n");
    bOutputStarted = OMX_FALSE;
    bReceiveOutputEOS = OMX_FALSE;
    bReceiveFrame = OMX_FALSE;
    bNeedCodecData = OMX_FALSE;

    bInputEOS = OMX_FALSE;
    bOutputEOS = OMX_FALSE;

    FSL_DELETE(pParser);

    if(pPostProcess != NULL){
        pPostProcess->Destroy();
        FSL_DELETE(pPostProcess);
    }

    if(pDmaBuffer != NULL){
        pDmaBuffer->FreeAll();
        FSL_DELETE(pDmaBuffer);
        bDmaBufferAllocated = OMX_FALSE;
    }

    if(pDmaOutputBuffer != NULL){
        FSL_DELETE(pDmaOutputBuffer);
        bUseDmaOutputBuffer = OMX_FALSE;
    }

    #ifdef ENABLE_TS_MANAGER
    tsmDestroy(hTsHandle);
    hTsHandle = NULL;
    #endif

    //free driver buffers
    if(bSetInputBufferCount)
        inObj->SetBufferCount(0, V4L2_MEMORY_MMAP, 1);
    if(bSetOutputBufferCount)
        outObj->SetBufferCount(0, V4L2_MEMORY_MMAP, nOutputPlane);

    ret=SetDefaultSetting();

    LOG_LOG("DoIdle2Loaded ret=%x",ret);

    return ret;
}
OMX_ERRORTYPE V4l2Dec::DoLoaded2Idle()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    inThread = FSL_NEW(VThread,());

    if(inThread == NULL)
        return OMX_ErrorInsufficientResources;
        
    ret = inThread->create(this,OMX_FALSE,filterThreadHandler);
    if(ret != OMX_ErrorNone)
        return ret;

    #ifdef ENABLE_TS_MANAGER
    hTsHandle = tsmCreate();
    if(hTsHandle == NULL) {
        LOG_ERROR("Create Ts manager failed.\n");
        return OMX_ErrorUndefined;
    }
    #endif
    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Dec::ProcessDataBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch(eState){
        case V4L2_DEC_STATE_IDLE:
            LOG_LOG("V4l2Dec eState V4L2_DEC_STATE_IDLE \n");
            eState = V4L2_DEC_STATE_INIT;
            break;
        case V4L2_DEC_STATE_INIT:
            LOG_LOG("V4l2Dec eState V4L2_DEC_STATE_INIT \n");
            ret = ProcessInit();
            if(ret != OMX_ErrorNone)
                break;

            if(bEnabledPostProcess){
                if(pPostProcess != NULL){
                    PROCESS3_CALLBACKTYPE * callback = &PostProcessCallBack;
                    pPostProcess->SetCallbackFunc(callback,this);
                }else{
                    LOG_ERROR("bEnabledPostProcess BUT pPostProcess is NULL");
                    return OMX_ErrorResourcesLost;
                }
            }

            eState = V4L2_DEC_STATE_QUEUE_INPUT;

            break;

        case V4L2_DEC_STATE_QUEUE_INPUT:
            LOG_LOG("V4l2Dec eState V4L2_DEC_STATE_QUEUE_INPUT \n");
            ret = ProcessInputBuffer();

            if(ret == OMX_ErrorNoMore && bInputEOS == OMX_TRUE)
                ret = OMX_ErrorNone;

            if(ret != OMX_ErrorNone)
                break;
            if(nInputCnt == nInBufferCnt || bInputEOS == OMX_TRUE)
                eState = V4L2_DEC_STATE_START_INPUT;

            break;

        case V4L2_DEC_STATE_START_INPUT:
            LOG_LOG("V4l2Dec eState V4L2_DEC_STATE_START_INPUT \n");

            ret = inThread->start();
            if(ret == OMX_ErrorNone){
                eState = V4L2_DEC_STATE_WAIT_RES;
            }
            break;

        case V4L2_DEC_STATE_WAIT_RES:
            fsl_osal_sleep(1000);
            ProcessInputBuffer();
            LOG_LOG("V4l2Dec eState V4L2_DEC_STATE_WAIT_RES \n");
            break;

        case V4L2_DEC_STATE_RES_CHANGED:
            LOG_LOG("V4l2Dec eState V4L2_DEC_STATE_RES_CHANGED \n");
            fsl_osal_sleep(1000);
            break;

        case V4L2_DEC_STATE_FLUSHED:
            LOG_LOG("V4l2Dec eState V4L2_DEC_STATE_FLUSHED \n");
            if(bSetOutputBufferCount){
                eState = V4L2_DEC_STATE_RUN;
            }else
                fsl_osal_sleep(1000);
            break;

        case V4L2_DEC_STATE_RUN:
        {
            OMX_ERRORTYPE ret_in = OMX_ErrorNone;
            OMX_ERRORTYPE ret_other = OMX_ErrorNone;

            if(bOutputEOS){
                LOG_LOG("bOutputEOS OMX_ErrorNoMore");
                ret = OMX_ErrorNoMore;
                break;
            }

            if(bUseDmaBuffer && !bDmaBufferAllocated)
                AllocateDmaBuffer();

            if(bAllocateFailed){
                LOG_LOG("bAllocateFailed return OMX_ErrorNoMore");
                ret = OMX_ErrorNoMore;
                break;
            }

            if(bRetryHandleEOS){
                ret = HandleEOSEvent();
                if(ret == OMX_ErrorNone){
                    bReceiveOutputEOS = OMX_TRUE;
                    bRetryHandleEOS = OMX_FALSE;
                }
            }

            ret = ProcessOutputBuffer();

            if(bEnabledPostProcess && pPostProcess->OutputBufferAdded()){
                ret_other = ProcessPostBuffer();
                if(ret_other == OMX_ErrorNone){
                    LOG_LOG("ProcessOutputBuffer OMX_ErrorNone");
                    return OMX_ErrorNone;
                }
            }

            if(ret != OMX_ErrorNone){
                ret_in = ProcessInputBuffer();
                LOG_LOG("ProcessInputBuffer ret=%x",ret);
            }

            if(ret == OMX_ErrorNoMore && ret_in == OMX_ErrorNoMore){
                ret = OMX_ErrorNoMore;
            }else{
                ret = OMX_ErrorNone;
            }

            // Fix MA-13859: start ignore OMX_ErrorNoMore because there maybe output in vpu
            if((ret == OMX_ErrorNone || ret == OMX_ErrorNoMore) && !bOutputStarted){
                bOutputStarted = OMX_TRUE;
                inThread->start();
            }

            break;
        }
        default:
            break;
    }

    return ret;
}



OMX_ERRORTYPE V4l2Dec::ProcessInputBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    OMX_U32 flags = 0;
    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;

    fsl_osal_mutex_lock(sMutex);
    if(ports[IN_PORT]->BufferNum() == 0){
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }

#ifdef ENABLE_TS_MANAGER
        if(OMX_TRUE != tsmHasEnoughSlot(hTsHandle)) {
            fsl_osal_mutex_unlock(sMutex);
            return OMX_ErrorNoMore;
        }
#endif

    ports[IN_PORT]->GetBuffer(&pBufferHdr);

    if(pBufferHdr == NULL){
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }
    LOG_LOG("Get Inbuffer %p,len=%d,ts=%lld,flag=%x,offset=%d,nInputCnt=%d\n", pBufferHdr->pBuffer, pBufferHdr->nFilledLen, pBufferHdr->nTimeStamp, pBufferHdr->nFlags,pBufferHdr->nOffset,nInputCnt);

    ret = ProcessInBufferFlags(pBufferHdr);
    if(ret != OMX_ErrorNone){
        fsl_osal_mutex_unlock(sMutex);
        return ret;
    }

    if(bEnabledFrameParser && pParser != NULL)
        pParser->Parse(pBufferHdr->pBuffer,&pBufferHdr->nFilledLen);

    if(pBufferHdr->nFilledLen > 0){
        LOG_LOG("set Input buffer BEGIN ts=%lld\n",pBufferHdr->nTimeStamp);
        dumpInputBuffer(pBufferHdr);
        #ifdef ENABLE_TS_MANAGER
        tsmSetBlkTs(hTsHandle, pBufferHdr->nFilledLen, pBufferHdr->nTimeStamp);
        #endif

        ret = inObj->SetBuffer(pBufferHdr, flags);

        nInputCnt ++;
        LOG_LOG("ProcessInputBuffer ret=%x nInputCnt=%d\n",ret,nInputCnt);
        if(ret != OMX_ErrorNone) {
            ports[IN_PORT]->SendBuffer(pBufferHdr);
            LOG_ERROR("set buffer err=%x\n",ret);
            fsl_osal_mutex_unlock(sMutex);
            return ret;
        }

    }else{
        ports[IN_PORT]->SendBuffer(pBufferHdr);
        pBufferHdr = NULL;
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNone;
    }

    //handle last buffer here when it has data and eos flag
    if(pBufferHdr->nFilledLen > 0 && (pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)){
        LOG_LOG("OMX_BUFFERFLAG_EOS nFilledLen > 0");

        pV4l2Dev->StopDecoder(nFd);

        ports[IN_PORT]->SendBuffer(pBufferHdr);
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }

    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Dec::ProcessInBufferFlags(OMX_BUFFERHEADERTYPE *pInBufferHdr)
{
    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTIME){
        bNewSegment = OMX_TRUE;

        if (bNeedCodecData && pCodecData && nCodecDataLen > 0 && pInBufferHdr->nFilledLen > 0) {
            // RTSP streaming suspend/resume will return eos buffer before first IDR frame,
            // need send codecdata again before IDR frame, otherwise v4l2 firmware can't start.
            fsl_osal_memmove(pInBufferHdr->pBuffer + nCodecDataLen, pInBufferHdr->pBuffer, pInBufferHdr->nFilledLen);
            fsl_osal_memcpy(pInBufferHdr->pBuffer, pCodecData, nCodecDataLen);
            pInBufferHdr->nFilledLen += nCodecDataLen;
            bNeedCodecData = OMX_FALSE;
        }

        if (!bReceiveFrame && pInBufferHdr->nFilledLen > 0)
            bReceiveFrame = OMX_TRUE;

    }

    #ifdef ENABLE_TS_MANAGER
    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTIME) {

        //could not get consume length now, use FIFO mode
        if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTRICK) {
            LOG_LOG("Set ts manager to FIFO mode.\n");
            tsmReSync(hTsHandle, pInBufferHdr->nTimeStamp, MODE_FIFO);
        }
        else {
            LOG_LOG("Set ts manager to AI mode. ts=%lld\n",pInBufferHdr->nTimeStamp);
            tsmReSync(hTsHandle, pInBufferHdr->nTimeStamp, MODE_AI);
        }

        LOG_LOG("nDurationThr: %d, nBufCntThr: %d\n", nMaxDurationMsThr, nMaxBufCntThr);
        tsmSetDataDepthThreshold(hTsHandle, nMaxDurationMsThr, nMaxBufCntThr);
    }
    #endif

    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_EOS){
        LOG_LOG("get OMX_BUFFERFLAG_EOS for input buffer");
        bInputEOS = OMX_TRUE;

        //when the eos buffer has data, need to call inObj->SetBuffer before stop command
        if(0 == pInBufferHdr->nFilledLen){

            pV4l2Dev->StopDecoder(nFd);

            ports[IN_PORT]->SendBuffer(pInBufferHdr);

            if (!bReceiveFrame)
                bNeedCodecData = OMX_TRUE;

            return OMX_ErrorNoMore;
        }
    }

    // MA-10250: backup codecdata to send again if rtsp return eos after suspend/resume.
    if((pInBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG) && pInBufferHdr->nFilledLen > 0) {
        if(pCodecData == NULL) {
            pCodecData = FSL_MALLOC(pInBufferHdr->nFilledLen);
            if(pCodecData == NULL) {
                SendEvent(OMX_EventError, OMX_ErrorInsufficientResources, 0, NULL);
                return OMX_ErrorInsufficientResources;
            }
        } else {
            pCodecData = FSL_REALLOC(pCodecData, nCodecDataLen + pInBufferHdr->nFilledLen);
            if(pCodecData == NULL) {
                SendEvent(OMX_EventError, OMX_ErrorInsufficientResources, 0, NULL);
                return OMX_ErrorInsufficientResources;
            }
        }
        fsl_osal_memcpy((char *)pCodecData + nCodecDataLen, pInBufferHdr->pBuffer, pInBufferHdr->nFilledLen);
        nCodecDataLen += pInBufferHdr->nFilledLen;
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dec::ProcessOutputBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    fsl_osal_mutex_lock(sMutex);
    
    if(ports[OUT_PORT]->BufferNum() == 0){
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }

    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;

    ports[OUT_PORT]->GetBuffer(&pBufferHdr);
    if(pBufferHdr == NULL){
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }
    LOG_LOG("Get output buffer %p,len=%d,alloLen=%d,flag=%x,ts=%lld,bufferCnt=%d\n",
        pBufferHdr->pBuffer,pBufferHdr->nFilledLen,pBufferHdr->nAllocLen,pBufferHdr->nFlags,pBufferHdr->nTimeStamp,ports[OUT_PORT]->BufferNum());

    if(pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
        bOutputEOS = OMX_TRUE;

    //v4l2 buffer has alignment requirement
    //replace the allocated buffer size if it is larger than required buffer size
    if(nOutBufferSize > 0 && pBufferHdr->nAllocLen > nOutBufferSize && !bEnabledPostProcess){
        pBufferHdr->nAllocLen = nOutBufferSize;
        LOG_LOG("reset nAllocLen to %d\n",pBufferHdr->nAllocLen);
    }

    #ifdef ENABLE_TS_MANAGER
    pBufferHdr->nTimeStamp = -1;
    #endif

    LOG_LOG("Set output buffer BEGIN,pBufferHdr=%p \n",pBufferHdr);
    if(bEnabledPostProcess)
        ret = pPostProcess->AddOutputFrame(pBufferHdr);
    else{
        OMX_U32 flag = V4L2_OBJECT_FLAG_NO_AUTO_START;
        if(bAndroidNativeBuffer)
            flag = V4L2_OBJECT_FLAG_NATIVE_BUFFER;

        ret = outObj->SetBuffer(pBufferHdr,flag);

        if(OMX_ErrorNone == ret){
            nOutObjBufferCnt++;
            if(MIN_CAPTURE_BUFFER_CNT == nOutObjBufferCnt)
                ret = outObj->Start();
        }
    }
    LOG_LOG("ProcessOutputBuffer ret=%x\n",ret);

    if(ret != OMX_ErrorNone) {
        pBufferHdr->nFilledLen = 0;
        ports[OUT_PORT]->SendBuffer(pBufferHdr);
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }

    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dec::ReturnBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr,OMX_U32 nPortIndex)
{
    if(pBufferHdr == NULL){
        LOG_LOG("ReturnBuffer failed\n");
        return OMX_ErrorBadParameter;
    }

    if(nPortIndex == IN_PORT){
        if(pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
            pV4l2Dev->StopDecoder(nFd);

        ports[IN_PORT]->SendBuffer(pBufferHdr);
        LOG_LOG("ReturnBuffer input =%p,ts=%lld,len=%d,offset=%d flag=%x nInputCnt=%d\n",
            pBufferHdr->pBuffer, pBufferHdr->nTimeStamp,pBufferHdr->nFilledLen,pBufferHdr->nOffset,pBufferHdr->nFlags, nInputCnt);

    }else if(nPortIndex == OUT_PORT) {

        if(bNewSegment){
            pBufferHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME;
            bNewSegment = OMX_FALSE;
            LOG_LOG("send buffer OMX_BUFFERFLAG_STARTTIME\n");
        }

        if((pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)){
            LOG_LOG("send buffer OMX_BUFFERFLAG_EOS\n");
            bOutputEOS = OMX_TRUE;
        }

        dumpOutputBuffer(pBufferHdr);

        #ifdef ENABLE_TS_MANAGER
        if(!(pBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG))
            pBufferHdr->nTimeStamp = tsmGetFrmTs(hTsHandle, NULL);
        #endif
        nOutputCnt++;

        LOG_LOG("ReturnBuffer output buffer,ts=%lld,ptr=%p, offset=%d, len=%d,flags=%x,alloc len=%d nOutputCnt=%d\n",
            pBufferHdr->nTimeStamp, pBufferHdr->pBuffer, pBufferHdr->nOffset, pBufferHdr->nFilledLen,pBufferHdr->nFlags,pBufferHdr->nAllocLen,nOutputCnt);
        ports[OUT_PORT]->SendBuffer(pBufferHdr);

        if(bOutputEOS){
            LOG_LOG("call inThread->pause\n");
            inThread->pause();
        }
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dec::HandleFormatChanged(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;


    if(nPortIndex == IN_PORT){
        PortFormatChanged(IN_PORT);
        return ret;
    }

    OMX_BOOL bResourceChanged = OMX_FALSE;
    OMX_BOOL bSuppress = OMX_FALSE;
    OMX_BOOL bSendEvent = OMX_FALSE;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_CONFIG_RECTTYPE sCropDef;
    V4l2ObjectFormat sFormat;
    OMX_U32 numCnt = 0;
    OMX_U32 buffer_size = 0;
    OMX_U32 pad_width = 0;
    OMX_U32 pad_height = 0;
    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    sPortDef.eDomain = OMX_PortDomainVideo;
    sPortDef.nPortIndex = OUT_PORT;
    ports[nPortIndex]->GetPortDefinition(&sPortDef);

    ret = outObj->GetFormat(&sFormat);
    if(ret != OMX_ErrorNone)
        return ret;

    ret = outObj->GetMinimumBufferCount(&numCnt);

    if(ret != OMX_ErrorNone)
        return ret;

    //add 3 buffers to avoid hang issue
    numCnt = numCnt+EXTRA_BUF_OUT_CNT;

    if(bEnabledPostProcess)
        numCnt += PROCESS3_BUF_CNT;

    LOG_LOG("HandleFormatChanged OUT_PORT numCnt=%d\n",numCnt);
    
    pad_width = Align(sFormat.width, nWidthAlign);
    pad_height = Align(sFormat.height, nHeightAlign);

    if(pad_width > sOutFmt.nFrameWidth ||
        pad_height > sOutFmt.nFrameHeight){
        bResourceChanged = OMX_TRUE;
    }else if(pad_width < sOutFmt.nFrameWidth ||
        pad_height < sOutFmt.nFrameHeight){
        bResourceChanged = OMX_TRUE;
        if(bAdaptivePlayback){
            bSuppress = OMX_TRUE;
        }
    }

    if(sFormat.format == v4l2_fourcc('N', 'T', '1', '2')){
        LOG_LOG("10 bit video");
        nErrCnt = 10;
        HandleErrorEvent();
        return OMX_ErrorStreamCorrupt;
    }
    
    buffer_size = sFormat.bufferSize[0] + sFormat.bufferSize[1] + sFormat.bufferSize[2];
    //no need to reallocate the output buffer when current buffer count is larger than the required buffer count.
    if(nOutBufferCnt != numCnt || buffer_size != nOutBufferSize){
        bResourceChanged = OMX_TRUE;
        if(numCnt+4 <= nOutBufferCnt && buffer_size <= nOutBufferSize){
            bSuppress = OMX_TRUE;
        }
    }

    if(!bSendPortSettingChanged || (bResourceChanged && !bSuppress)){
        OMX_U32 calcBufferSize = 0;
        sPortDef.format.video.nFrameWidth = pad_width;
        sPortDef.format.video.nFrameHeight = pad_height;
        //stride use frame width
        sPortDef.format.video.nStride= sPortDef.format.video.nFrameWidth;
        sPortDef.nBufferCountActual = nOutBufferCnt = numCnt;
        sPortDef.nBufferCountMin = numCnt;
        calcBufferSize = pad_width * pad_height * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;

        if(buffer_size <= nOutBufferSize)
            sPortDef.nBufferSize = nOutBufferSize;
        else
            sPortDef.nBufferSize = buffer_size;

        if(sPortDef.nBufferSize < calcBufferSize)
            sPortDef.nBufferSize = calcBufferSize;

        ports[nPortIndex]->SetPortDefinition(&sPortDef);

        LOG_LOG("send OMX_EventPortSettingsChanged output port cnt=%d \n", sPortDef.nBufferCountActual);
        eState = V4L2_DEC_STATE_RES_CHANGED;
        fsl_osal_memcpy(&sOutFmt, &sPortDef.format.video, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

        bSendEvent = OMX_TRUE;
        bSetOutputBufferCount = OMX_FALSE;
        bSendPortSettingChanged = OMX_TRUE;
        SendEvent(OMX_EventPortSettingsChanged, OUT_PORT, 0, NULL);
        LOG_LOG("send OMX_EventPortSettingsChanged \n");
    }else{
        LOG_LOG("HandleFormatChanged do not send OMX_EventPortSettingsChanged\n");
        eState = V4L2_DEC_STATE_RUN;
        outObj->Start();
    }

    (void)outObj->GetCrop(&sCropDef);

    if(sCropDef.nWidth != sOutputCrop.nWidth || sCropDef.nHeight != sOutputCrop.nHeight){
        fsl_osal_memcpy(&sOutputCrop, &sCropDef, sizeof(OMX_CONFIG_RECTTYPE));
        if(!bSendEvent){
            SendEvent(OMX_EventPortSettingsChanged, OUT_PORT, OMX_IndexConfigCommonOutputCrop, NULL);
            LOG_LOG("send OMX_EventPortSettingsChanged OMX_IndexConfigCommonOutputCrop\n");
        }
    }

    if(bEnabledPostProcess){ 

        PROCESS3_FORMAT fmt;
        fmt.bufferSize = nOutBufferSize;
        fmt.format = OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled;
        fmt.width = sOutFmt.nFrameWidth;
        fmt.height = sOutFmt.nFrameHeight;
        fmt.stride = sOutFmt.nStride;
        ret = pPostProcess->ConfigInput(&fmt);
        if(ret != OMX_ErrorNone)
            return ret;
    
        fmt.format = OMX_COLOR_Format16bitRGB565;
        ret = pPostProcess->ConfigOutput(&fmt);
        if(ret != OMX_ErrorNone)
            return ret;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Dec::HandleFormatChangedForIon(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(nPortIndex != OUT_PORT)
        return OMX_ErrorBadParameter;

    OMX_BOOL bResourceChanged = OMX_FALSE;
    OMX_BOOL bSuppress = OMX_FALSE;
    OMX_BOOL bSendEvent = OMX_FALSE;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_CONFIG_RECTTYPE sCropDef;
    V4l2ObjectFormat sFormat;
    OMX_U32 numCnt = 0;
    OMX_U32 buffer_size = 0;
    OMX_U32 pad_width = 0;
    OMX_U32 pad_height = 0;
    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    sPortDef.eDomain = OMX_PortDomainVideo;
    sPortDef.nPortIndex = OUT_PORT;
    ports[nPortIndex]->GetPortDefinition(&sPortDef);

    ret = outObj->GetFormat(&sFormat);
    if(ret != OMX_ErrorNone)
        return ret;

    ret = outObj->GetMinimumBufferCount(&numCnt);

    if(ret != OMX_ErrorNone)
        return ret;

    //add extra 3 buffers so the buffer count will be equal to that in gstreamer.
    numCnt = numCnt+EXTRA_BUF_OUT_CNT;

    LOG_LOG("HandleFormatChangedForIon OUT_PORT numCnt=%d\n",numCnt);
    pad_width = Align(sFormat.width, nWidthAlign);
    pad_height = Align(sFormat.height, nHeightAlign);

    if(pad_width > sOutFmt.nFrameWidth ||
        pad_height > sOutFmt.nFrameHeight){
        bResourceChanged = OMX_TRUE;
    }else if(pad_width < sOutFmt.nFrameWidth ||
        pad_height < sOutFmt.nFrameHeight){
        bResourceChanged = OMX_TRUE;
        if(bAdaptivePlayback){
            bSuppress = OMX_TRUE;
        }
    }

    if(sFormat.format == v4l2_fourcc('N', 'T', '1', '2')){
        LOG_LOG("10 bit video");
        nErrCnt = 10;
        HandleErrorEvent();
        return OMX_ErrorStreamCorrupt;
    }

    buffer_size = sFormat.bufferSize[0] + sFormat.bufferSize[1] + sFormat.bufferSize[2];
    //no need to reallocate the output buffer when current buffer count is larger than the required buffer count.
    if(nDmaBufferCnt != numCnt || sFormat.bufferSize[0] != nDmaBufferSize[0]
        || sFormat.bufferSize[1] != nDmaBufferSize[1] || sFormat.bufferSize[2] != nDmaBufferSize[2]){
        bResourceChanged = OMX_TRUE;

        if(numCnt <= nDmaBufferCnt && 
            (sFormat.bufferSize[0] <= nDmaBufferSize[0]) && 
            (sFormat.bufferSize[1] <= nDmaBufferSize[1]) &&
            (sFormat.bufferSize[2] <= nDmaBufferSize[2])){
            bSuppress = OMX_TRUE;
            LOG_LOG("sFormat Buffer Size=%d,%d,%d, dma buffer size=%d,%d,%d",
                sFormat.bufferSize[0],sFormat.bufferSize[1],sFormat.bufferSize[2],
                nDmaBufferSize[0],nDmaBufferSize[1],nDmaBufferSize[2]);

            if(sPortDef.format.video.nStride != (OMX_S32)sFormat.stride)
                bSuppress = OMX_FALSE;
        }
    }

    OMX_U32 calcBufferSize = 0;
    sPortDef.format.video.nFrameWidth = pad_width;
    sPortDef.format.video.nFrameHeight = pad_height;
 
    sPortDef.format.video.nStride = sFormat.stride;
    LOG_DEBUG("HandleFormatChangedForIon w=%d,h=%d,stride=%d",sPortDef.format.video.nFrameWidth,sPortDef.format.video.nFrameHeight,sPortDef.format.video.nStride);
    sPortDef.nBufferCountActual = nOutBufferCnt;
    sPortDef.nBufferCountMin = DEFAULT_BUF_OUT_CNT;
    calcBufferSize = pad_width * pad_height * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;

    if(buffer_size < calcBufferSize)
        buffer_size = calcBufferSize;
    sPortDef.nBufferSize = nOutBufferSize = buffer_size;

    LOG_DEBUG("HandleFormatChangedForIon nDmaBufferSize=%d,%d,%d nDmaBufferCnt=%d,nOutBufferCnt=%d\n",
        nDmaBufferSize[0],nDmaBufferSize[1],nDmaBufferSize[2],nDmaBufferCnt,nOutBufferCnt);

    if(!bSendPortSettingChanged || (bResourceChanged && !bSuppress)){

        ports[nPortIndex]->SetPortDefinition(&sPortDef);

        nDmaBufferSize[0] = sFormat.bufferSize[0];
        nDmaBufferSize[1] = sFormat.bufferSize[1];
        nDmaBufferSize[2] = sFormat.bufferSize[2];
        nDmaBufferCnt = numCnt;

        LOG_DEBUG("send OMX_EventPortSettingsChanged output port cnt=%d \n", sPortDef.nBufferCountActual);
        eState = V4L2_DEC_STATE_RES_CHANGED;
        fsl_osal_memcpy(&sOutFmt, &sPortDef.format.video, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

        if(bDmaBufferAllocated){
            pDmaBuffer->FreeAll();
            bDmaBufferAllocated = OMX_FALSE;
        }

        bSendEvent = OMX_TRUE;
        bSetOutputBufferCount = OMX_FALSE;
        bSendPortSettingChanged = OMX_TRUE;

        SendEvent(OMX_EventPortSettingsChanged, OUT_PORT, 0, NULL);

    }else{

        ports[nPortIndex]->SetPortDefinition(&sPortDef);
        fsl_osal_memcpy(&sOutFmt, &sPortDef.format.video, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

        LOG_LOG("HandleFormatChanged do not send OMX_EventPortSettingsChanged\n");
        eState = V4L2_DEC_STATE_RUN;
        outObj->Stop();
        outObj->Start();
    }

    (void)outObj->GetCrop(&sCropDef);

    if(sCropDef.nWidth != sOutputCrop.nWidth || sCropDef.nHeight != sOutputCrop.nHeight){
        fsl_osal_memcpy(&sOutputCrop, &sCropDef, sizeof(OMX_CONFIG_RECTTYPE));
        if(!bSendEvent){
            SendEvent(OMX_EventPortSettingsChanged, OUT_PORT, OMX_IndexConfigCommonOutputCrop, NULL);
            LOG_DEBUG("send OMX_EventPortSettingsChanged OMX_IndexConfigCommonOutputCrop\n");
        }
    }

    if(bEnabledPostProcess){ 
        PROCESS3_FORMAT fmt;
        fmt.bufferSize = nOutBufferSize;
        fmt.format = OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled;
        fmt.width = sOutFmt.nFrameWidth;
        fmt.height = sOutFmt.nFrameHeight;
        fmt.stride = sOutFmt.nStride;
        ret = pPostProcess->ConfigInput(&fmt);
        if(ret != OMX_ErrorNone)
            return ret;

        fmt.format = sOutFmt.eColorFormat;
        ret = pPostProcess->ConfigOutput(&fmt);
        if(ret != OMX_ErrorNone)
            return ret;
    }
    return ret;
}
OMX_ERRORTYPE V4l2Dec::HandleErrorEvent()
{
    nErrCnt ++;

    if(nErrCnt > 10){
        SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
        LOG_ERROR("HandleErrorEvent send event\n");
        nErrCnt = 0;
        pV4l2Dev->StopDecoder(nFd);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Dec::HandleEOSEvent()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    fsl_osal_mutex_lock(sMutex);
    
    //get one buffer from post process to send eos event
    if( bEnabledPostProcess){
        DmaBufferHdr * pDmaBufHdlr = NULL;
        OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
        LOG_LOG("HandleEOSEvent 1 BEGIN");
        OMX_S32 i = 20;
        while(i > 0){
            pDmaBufHdlr = NULL;

            ret = outObj->GetBuffer(&pDmaBufHdlr);

            if(ret != OMX_ErrorNone){
                ret = pPostProcess->GetInputReturnBuffer(&pDmaBufHdlr);
            }

            if(ret == OMX_ErrorNone && pDmaBufHdlr != NULL){
                pDmaBufHdlr->bReadyForProcess = OMX_FALSE;
                pDmaBufHdlr->flag |= DMA_BUF_EOS;
                LOG_LOG("AddInputFrame OMX_BUFFERFLAG_EOS 1");
                ret = pPostProcess->AddInputFrame(pDmaBufHdlr);
                fsl_osal_mutex_unlock(sMutex);
                return ret;
            }
            fsl_osal_sleep(1000);
            i--;
        }

        i = 20;
        while(i > 0){
            pBufferHdr = NULL;
            if(ports[OUT_PORT]->BufferNum() > 0)
                ret = ports[OUT_PORT]->GetBuffer(&pBufferHdr);
            else
                ret = pPostProcess->GetOutputBuffer(&pBufferHdr);

            if(ret == OMX_ErrorNone && pBufferHdr != NULL){
                pBufferHdr->nFilledLen = 0;
                pBufferHdr->nFlags = OMX_BUFFERFLAG_EOS;
                pBufferHdr->nTimeStamp = -1;
                LOG_LOG("HandleEOSEvent use outport buffer to send eos flag");
                ret = ReturnBuffer(pBufferHdr,OUT_PORT);
                fsl_osal_mutex_unlock(sMutex);
                return ret;
            }
            fsl_osal_sleep(1000);
            i--;
            LOG_LOG("wait to get one output buffer");
        }
        ret = OMX_ErrorOverflow;
    }else{
        OMX_S32 i = 20;
        LOG_LOG("HandleEOSEvent 2 BEGIN");
        OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;

        while(i > 0){
            pBufferHdr = NULL;
            if(ports[OUT_PORT]->BufferNum() > 0){
                ret = ports[OUT_PORT]->GetBuffer(&pBufferHdr);
            }else{
                ret = outObj->GetBuffer(&pBufferHdr);
            }
            if(ret == OMX_ErrorNone && pBufferHdr != NULL){
                pBufferHdr->nFilledLen = 0;
                pBufferHdr->nFlags = OMX_BUFFERFLAG_EOS;
                pBufferHdr->nTimeStamp = -1;
                LOG_LOG("HandleEOSEvent use outport buffer to send eos flag");
                ret = ReturnBuffer(pBufferHdr,OUT_PORT);
                fsl_osal_mutex_unlock(sMutex);
                return ret;
            }
            fsl_osal_sleep(1000);
            i--;
            LOG_LOG("wait to get one output buffer");
        }
        LOG_LOG("HandleEOSEvent 2 END");
    }

    fsl_osal_mutex_unlock(sMutex);
    return ret;
}
OMX_ERRORTYPE V4l2Dec::HandleInObjBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    fsl_osal_mutex_lock(sMutex);

    OMX_BUFFERHEADERTYPE * pBufHdlr;
    LOG_LOG("send input buffer BEGIN \n");

    if(OMX_ErrorNone == inObj->GetOutputBuffer(&pBufHdlr)){
        ret = ReturnBuffer(pBufHdlr,IN_PORT);
        LOG_LOG("send input buffer END ret=%x\n",ret);
    }
    fsl_osal_mutex_unlock(sMutex);

    return ret;
}
OMX_ERRORTYPE V4l2Dec::HandleOutObjBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    fsl_osal_mutex_lock(sMutex);

    LOG_LOG("send output buffer BEGIN \n");
    if(bEnabledPostProcess && bUseDmaBuffer){
        DmaBufferHdr * pBufHdlr;
        if(OMX_ErrorNone == outObj->GetOutputBuffer(&pBufHdlr)){
            dumpPostProcessBuffer(pBufHdlr);
            ret = pPostProcess->AddInputFrame(pBufHdlr);
        }
        LOG_LOG("send dma output buffer END ret=%x \n", ret);
    }else{
        OMX_BUFFERHEADERTYPE * pBufHdlr;
        if(OMX_ErrorNone == outObj->GetOutputBuffer(&pBufHdlr))
            ret = ReturnBuffer(pBufHdlr,OUT_PORT);
        LOG_LOG("send output buffer END ret=%x \n", ret);
    }
    fsl_osal_mutex_unlock(sMutex);
    return ret;
}

OMX_ERRORTYPE V4l2Dec::ProcessPostBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    DmaBufferHdr *buf = NULL;

    fsl_osal_mutex_lock(sMutex);

    if(OMX_ErrorNone == pPostProcess->GetInputReturnBuffer(&buf)){
        buf->bReadyForProcess = OMX_FALSE;
        buf->flag = 0;
        ret = pDmaBuffer->Add(buf);
        if(OMX_ErrorNone != ret){
            fsl_osal_mutex_unlock(sMutex);
            return ret;
        }
        LOG_LOG("ProcessPostBuffer add dma buffer success\n");
    }
    if(bReceiveOutputEOS){
        fsl_osal_mutex_unlock(sMutex);
        return ret;
    }

    buf = NULL;

    ret = pDmaBuffer->Get(&buf);
    if(OMX_ErrorNone != ret){
        fsl_osal_mutex_unlock(sMutex);
        return ret;
    }
    LOG_LOG("ProcessPostBuffer vaddr1=%lx,vaddr2=%lx",buf->plane[0].vaddr,buf->plane[1].vaddr);
    buf->bReadyForProcess = OMX_FALSE;
    ret = outObj->SetBuffer(buf, V4L2_OBJECT_FLAG_NO_AUTO_START);
    LOG_LOG("ProcessPostBuffer ret=%d\n",ret);

    if(OMX_ErrorNone == ret){
        nOutObjBufferCnt++;
        if(MIN_CAPTURE_BUFFER_CNT == nOutObjBufferCnt)
            ret = outObj->Start();
    }

    fsl_osal_mutex_unlock(sMutex);
    return ret;
}

OMX_ERRORTYPE V4l2Dec::AllocateDmaBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_LOG("V4l2Filter::AllocateDmaBuffer cnt=%d,size0=%d,size1=%d,size2=%d\n",nDmaBufferCnt,nDmaBufferSize[0],nDmaBufferSize[1],nDmaBufferSize[2]);
    ret = pDmaBuffer->Allocate(nDmaBufferCnt, &nDmaBufferSize[0], eDmaBufferFormat);
    //set it to true even if not all buffers are allocated. some buffer will be freed when change to loaded state
    bDmaBufferAllocated = OMX_TRUE;
    if(ret != OMX_ErrorNone){
        LOG_ERROR("ProcessDmaBuffer Allocate failed");
        bAllocateFailed = OMX_TRUE;
        nErrCnt = 10;
        pDmaBuffer->FreeAll();
        HandleErrorEvent();
        SendEvent(OMX_EventError, OMX_ErrorInsufficientResources, 0, NULL);
        return ret;
    }

    return ret;
}

OMX_ERRORTYPE V4l2Dec::HandleSkipEvent()
{
    tsmGetFrmTs(hTsHandle, NULL);
    return OMX_ErrorNone;
}

void V4l2Dec::dumpInputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    FILE * pfile = NULL;
    if(pBufferHdr == NULL)
        return;

    if(!(nDebugFlag & DUMP_LOG_FLAG_DUMP_INPUT))
        return;

    if(nInputCnt < MAX_DUMP_FRAME)
        pfile = fopen(DUMP_INPUT_FILE,"ab");

    if(pfile){
        fwrite(pBufferHdr->pBuffer,1,pBufferHdr->nFilledLen,pfile);
        fclose(pfile);
    }
    return;
}
void V4l2Dec::dumpOutputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    FILE * pfile = NULL;
    if(pBufferHdr == NULL)
        return;

    if(!(nDebugFlag & DUMP_LOG_FLAG_DUMP_OUTPUT))
        return;

    if(nOutputCnt < MAX_DUMP_FRAME)
        pfile = fopen(DUMP_OUTPUT_FILE,"wb");

    if(pfile){
        fwrite(pBufferHdr->pBuffer,1,pBufferHdr->nFilledLen,pfile);
        fclose(pfile);
    }
    return;
}
void V4l2Dec::dumpPostProcessBuffer(DmaBufferHdr *pBufferHdr)
{
    FILE * pfile = NULL;

    if(pBufferHdr == NULL)
        return;

    if(!(nDebugFlag & DUMP_LOG_FLAG_DUMP_POSTPROCESS))
        return;

    if(nOutputCnt < MAX_DUMP_FRAME)
        pfile = fopen(DUMP_POSTPROCESS_FILE,"wb");

    if(pfile){
        fwrite((OMX_U8*)pBufferHdr->plane[0].vaddr,1,pBufferHdr->plane[0].size,pfile);
        fwrite((OMX_U8*)pBufferHdr->plane[1].vaddr,1,pBufferHdr->plane[1].size,pfile);
        fclose(pfile);
    }

    return;
}

/**< C style functions to expose entry point for the shared library */
extern "C"
{
    OMX_ERRORTYPE VpuDecoderInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        V4l2Dec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(V4l2Dec, ());
        if(obj == NULL)
        {
            return OMX_ErrorInsufficientResources;
        }
        base = (ComponentBase*)obj;
        ret = base->ConstructComponent(pHandle);
        if(ret != OMX_ErrorNone)
        {
            return ret;
        }
        return ret;
    }
}
