/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "V4l2Enc.h"
#include "IsiColorConvert.h"

#ifdef ENABLE_TS_MANAGER
#include "Tsm_wrapper.h"
#endif

#include <sys/mman.h>

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_LOG
#define LOG_LOG printf
#endif

#define VPU_LOG_LEVELFILE "/data/vpu_enc_level"
#define DUMP_INPUT_FILE "/data/temp_enc_in.yuv"
#define DUMP_PREPROCESS_FILE "/data/temp_enc_process.yuv"
#define DUMP_OUTPUT_FILE "/data/temp_enc_out.bit"
#define MAX_DUMP_FRAME (200)
#define DUMP_LOG_FLAG_DUMP_INPUT     0x1
#define DUMP_LOG_FLAG_DUMP_OUTPUT    0x2
#define DUMP_LOG_FLAG_DUMP_PREPROCESS   0x4


#define DEFAULT_FRM_WIDTH       (1280)
#define DEFAULT_FRM_HEIGHT      (720)
#define DEFAULT_FRM_RATE        (30 * Q16_SHIFT)

#define DEFAULT_BUF_IN_CNT  (6)
#define DEFAULT_BUF_IN_SIZE (DEFAULT_FRM_WIDTH*DEFAULT_FRM_HEIGHT*3/2)

#define DEFAULT_BUF_OUT_CNT    (4)//at least 2 buffers, one for codec data
#define DEFAULT_BUF_OUT_SIZE    (1024*1024*3/2)

#define DEFAULT_DMA_BUF_CNT    (6)

#define EXTRA_BUF_OUT_CNT  (3)

typedef struct{
    OMX_U32 type;
    const char* role;
    const char* name;
}V4L2_ENC_ROLE;

static V4L2_ENC_ROLE role_table[]={
{OMX_VIDEO_CodingAVC, "video_encoder.avc", "OMX.Freescale.std.video_encoder.avc.hw-based" },
};
static OMX_ERRORTYPE PreProcessCallBack(IsiColorConvert*pPreProcess, OMX_PTR pAppData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE* pInputBufHdlr = NULL;

    if(pPreProcess == NULL || pAppData == NULL)
        return OMX_ErrorBadParameter;

    V4l2Enc * base = (V4l2Enc *)pAppData;
    LOG_LOG("PreProcessCallBack\n");

    DmaBufferHdr *buf = NULL;

    if(OMX_ErrorNone == pPreProcess->GetOutputReturnBuffer(&buf)){
        ret = base->HandlePreProcessBuffer(buf);
    }

    if(OMX_ErrorNone == pPreProcess->GetInputReturnBuffer(&pInputBufHdlr)){
        base->ReturnBuffer(pInputBufHdlr,IN_PORT);
    }

    if(pPreProcess->GetEos())
        base->HandleEOSEvent(IN_PORT);

    return OMX_ErrorNone;
}
void *filterThreadHandler(void *arg)
{
    V4l2Enc *base = (V4l2Enc*)arg;
    OMX_S32 ret = 0;
    OMX_S32 poll_ret = 0;

    LOG_LOG("[%p]filterThreadHandler BEGIN \n",base);
    poll_ret = base->pV4l2Dev->Poll(base->nFd);
    LOG_LOG("[%p]filterThreadHandler END ret=%x \n",base,poll_ret);

    if(poll_ret & V4L2_DEV_POLL_RET_EVENT_RC){
        LOG_LOG("V4L2_DEV_POLL_RET_EVENT_RC \n");

        ret = base->HandleFormatChanged(OUT_PORT);
        if(ret == OMX_ErrorStreamCorrupt)
            base->SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
    }

    if(poll_ret & V4L2_DEV_POLL_RET_EVENT_EOS){
        ret = base->HandleEOSEvent(OUT_PORT);
        LOG_LOG("V4L2_DEV_POLL_RET_EVENT_EOS ret=%d\n",ret);
    }

    if(base->bOutputEOS)
        return NULL;

    if(poll_ret & V4L2_DEV_POLL_RET_OUTPUT){
        OMX_U32 flag = base->bStoreMetaData ? V4L2_OBJECT_FLAG_METADATA_BUFFER:0;
        LOG_LOG("V4L2_DEV_POLL_RET_OUTPUT \n");
        base->inObj->ProcessBuffer(flag);
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

    if(poll_ret & V4L2_DEV_POLL_RET_CAPTURE){
        LOG_LOG("V4L2_DEV_POLL_RET_CAPTURE \n");
        base->outObj->ProcessBuffer(0);
    }

    if(base->outObj->HasOutputBuffer()){
        ret = base->HandleOutObjBuffer();
        if(ret != OMX_ErrorNone)
            LOG_ERROR("HandleOutObjBuffer err=%d",ret);
    }

    if(poll_ret & V4L2OBJECT_ERROR)
        base->HandleErrorEvent();

    return NULL;
}

V4l2Enc::V4l2Enc()
{
    eDevType = V4L2_DEV_TYPE_ENCODER;
    ParseVpuLogLevel(VPU_LOG_LEVELFILE);
    bStoreMetaData = OMX_FALSE;
}
OMX_ERRORTYPE V4l2Enc::CreateObjects()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    inObj = FSL_NEW(V4l2Object,());

    outObj = FSL_NEW(V4l2Object,());

    if(inObj == NULL || outObj == NULL)
        return OMX_ErrorInsufficientResources;

    if(pV4l2Dev->isV4lBufferTypeSupported(nFd,eDevType,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)){
        nInputPlane = 2;
        ret = inObj->Create(nFd,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,nInputPlane);
    }
    if(ret != OMX_ErrorNone)
        return ret;

    if(pV4l2Dev->isV4lBufferTypeSupported(nFd,eDevType,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)){
        nOutputPlane = 1;
        ret = outObj->Create(nFd,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,nOutputPlane);
    }

    return ret;
}
OMX_ERRORTYPE V4l2Enc::SetDefaultPortSetting()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_encoder.avc.hw-based");

    fsl_osal_memset(&sInFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    sInFmt.nFrameWidth = DEFAULT_FRM_WIDTH;
    sInFmt.nFrameHeight = DEFAULT_FRM_HEIGHT;
    sInFmt.xFramerate = DEFAULT_FRM_RATE;
    sInFmt.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    sInFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

    fsl_osal_memset(&sOutFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    sOutFmt.nFrameWidth = DEFAULT_FRM_WIDTH;
    sOutFmt.nFrameHeight = DEFAULT_FRM_HEIGHT;
    sOutFmt.eColorFormat = OMX_COLOR_FormatUnused;
    sOutFmt.eCompressionFormat = OMX_VIDEO_CodingAVC;

    nInBufferCnt = DEFAULT_BUF_IN_CNT;
    nInBufferSize = DEFAULT_BUF_IN_SIZE;
    nOutBufferCnt = DEFAULT_BUF_OUT_CNT;
    nOutBufferSize = DEFAULT_BUF_OUT_SIZE;

    nWidthAlign = 1;
    nHeightAlign = 1;
    //get alignment from h264 format
    pV4l2Dev->GetFrameAlignment(nFd, V4L2_PIX_FMT_H264, &nWidthAlign, &nHeightAlign);

    nInPortFormatCnt = 3;
    eInPortPormat[0] = OMX_COLOR_FormatYUV420SemiPlanar;
    eInPortPormat[1] = OMX_COLOR_Format16bitRGB565;
    eInPortPormat[2] = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque;

    nOutPortFormatCnt = 1;
    eOutPortPormat[0] = OMX_COLOR_FormatUnused;


    OMX_INIT_STRUCT(&sOutputCrop, OMX_CONFIG_RECTTYPE);
    sOutputCrop.nWidth = DEFAULT_FRM_WIDTH;
    sOutputCrop.nHeight = DEFAULT_FRM_HEIGHT;

    fsl_osal_memset(&sInputParam,0,sizeof(V4l2EncInputParam));
    nProfile = OMX_VIDEO_AVCProfileBaseline;
    nLevel = OMX_VIDEO_AVCLevel1;

    nMaxDurationMsThr = -1;
    nMaxBufCntThr = -1;
    bSendPortSettingChanged = OMX_FALSE;

    pPreProcess = NULL;
    bEnabledPreProcess = OMX_FALSE;
    bCheckPreProcess = OMX_FALSE;

    pCodecDataConverter = NULL;
    bEnabledAVCCConverter = OMX_FALSE;

    bRefreshIntra = OMX_FALSE;

    //android cts will enable the bStoreMetaData only once, so do not reset the bStoreMetaData
    if(bStoreMetaData)
        nInBufferSize = 12;

    bInsertSpsPps2IDR = OMX_FALSE;

    pCodecData = NULL;
    nCodecDataLen = 0;
    bSendCodecData = OMX_FALSE;
    pCodecDataBufferHdr = NULL;

    bInputEOS = OMX_FALSE;
    bOutputEOS = OMX_FALSE;
    bOutputStarted = OMX_FALSE;
    bNewSegment = OMX_FALSE;

    pDmaInputBuffer = NULL;
    bUseDmaInputBuffer = OMX_FALSE;

    pDmaBuffer = NULL;
    bUseDmaBuffer = OMX_FALSE;
    bDmaBufferAllocated = OMX_FALSE;
    bAllocateFailed = OMX_FALSE;

    nDmaBufferCnt = DEFAULT_DMA_BUF_CNT;
    eDmaBufferFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    nDmaBufferSize[0] = Align(sOutFmt.nFrameWidth, nWidthAlign) * Align(sOutFmt.nFrameHeight, nHeightAlign);
    nDmaBufferSize[1] = nDmaBufferSize[0] / 2;
    nDmaBufferSize[2] = 0;
    nDmaPlanes = 2;

    eState = V4L2_ENC_STATE_IDLE;

    fsl_osal_memset(&Rotation,0,sizeof(OMX_CONFIG_ROTATIONTYPE));

    nErrCnt = 0;

        OMX_U32 width = 0;
        OMX_U32 height = 0;


    LOG_DEBUG("V4l2Enc::SetDefaultPortSetting SUCCESS\n");

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Enc::QueryVideoProfile(OMX_PTR pComponentParameterStructure)
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
            LOG_DEBUG("V4l2Enc::QueryVideoProfile pPara->eProfile=%x,level=%x\n",pPara->eProfile,pPara->eLevel);
            break;
        default:
            return OMX_ErrorUnsupportedIndex;
            break;
    }

    return OMX_ErrorNone;

}

OMX_ERRORTYPE V4l2Enc::SetRoleFormat(OMX_STRING role)
{
    OMX_BOOL bGot = OMX_FALSE;
    OMX_U32 i = 0;
    for(i = 0; i < sizeof(role_table)/sizeof(V4L2_ENC_ROLE); i++){
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

OMX_ERRORTYPE V4l2Enc::SetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_DEBUG("GetParameter index=%x",nParamIndex);

    switch ((OMX_U32)nParamIndex) {
        case OMX_IndexParamStandardComponentRole:
        {
            fsl_osal_strcpy( (fsl_osal_char *)cRole,(fsl_osal_char *)((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole);
            ret = SetRoleFormat((OMX_STRING)cRole);
            if(ret == OMX_ErrorNone){
                if(sOutFmt.eCompressionFormat != CodingType){
                    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

                    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                    sPortDef.nPortIndex = OUT_PORT;
                    ports[OUT_PORT]->GetPortDefinition(&sPortDef);
                    sOutFmt.eCompressionFormat=CodingType;
                    fsl_osal_memcpy(&(sPortDef.format.video), &sOutFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
                    ports[OUT_PORT]->SetPortDefinition(&sPortDef);
                }
            }
            LOG_LOG("SetParameter OMX_IndexParamStandardComponentRole.type=%d\n",CodingType);
        }
        break;
        case OMX_IndexParamStoreMetaDataInBuffers:
        {
            OMX_CONFIG_BOOLEANTYPE *pParams = (OMX_CONFIG_BOOLEANTYPE*)pComponentParameterStructure;
            bStoreMetaData = pParams->bEnabled;
            LOG_DEBUG("SetParameter OMX_IndexParamStoreMetaDataInBuffers.bStoreMetaData=%d\n",bStoreMetaData);
        }
        break;
        case OMX_IndexParamVideoPortFormat:
        {
            OMX_VIDEO_PARAM_PORTFORMATTYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
            if(pPara->nPortIndex == IN_PORT){
                if(pPara->eCompressionFormat != OMX_VIDEO_CodingUnused ||
                    (pPara->eColorFormat != eInPortPormat[0] && pPara->eColorFormat != eInPortPormat[1]
                    && pPara->eColorFormat != eInPortPormat[2]))
                    return OMX_ErrorUnsupportedSetting;

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
                    sInFmt.xFramerate=pPara->xFramerate;
                    fsl_osal_memcpy(&(sPortDef.format.video), &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
                    ports[IN_PORT]->SetPortDefinition(&sPortDef);
                    LOG_DEBUG("SetParameter OMX_IndexParamVideoPortFormat IN_PORT eCompressionFormat=%x,eColorFormat=%x\n",
                        sInFmt.eCompressionFormat,sInFmt.eColorFormat);
                }
            }else if(pPara->nPortIndex == OUT_PORT){
                if(pPara->eCompressionFormat != OMX_VIDEO_CodingAVC || pPara->eColorFormat != OMX_COLOR_FormatUnused)
                    return OMX_ErrorUnsupportedSetting;

                //set port
                if((sOutFmt.eCompressionFormat!=pPara->eCompressionFormat)
                    ||(sOutFmt.eColorFormat!=pPara->eColorFormat))
                {
                    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

                    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                    sPortDef.nPortIndex = OUT_PORT;
                    ports[OUT_PORT]->GetPortDefinition(&sPortDef);
                    sOutFmt.eCompressionFormat=pPara->eCompressionFormat;
                    sOutFmt.eColorFormat=pPara->eColorFormat;
                    //sInputParam.nFrameRate=pPara->xFramerate/Q16_SHIFT;
                    fsl_osal_memcpy(&(sPortDef.format.video), &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
                    ports[OUT_PORT]->SetPortDefinition(&sPortDef);
                    LOG_DEBUG("SetParameter OMX_IndexParamVideoPortFormat OUT_PORT eCompressionFormat=%x,eColorFormat=%x\n",
                        sOutFmt.eCompressionFormat,sOutFmt.eColorFormat);
                }

            }
        }
        break;
        case OMX_IndexParamVideoAvc:
        {
            OMX_VIDEO_PARAM_AVCTYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex==OUT_PORT);
            //set AVC GOP size
            sInputParam.nGOPSize=pPara->nPFrames+pPara->nBFrames+1;
            nProfile = pPara->eProfile;
            nLevel = pPara->eLevel;
            LOG_DEBUG("SetParameter OMX_IndexParamVideoAvc.nGOPSize=%d,profile=%d,level=%d\n",sInputParam.nGOPSize,nProfile,nLevel);
        }
        break;
        case OMX_IndexParamVideoH263:
        {
            OMX_VIDEO_PARAM_H263TYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex==IN_PORT);
            //set H263 GOP size
            sInputParam.nGOPSize=pPara->nPFrames+pPara->nBFrames+1;
        }
        break;
        case OMX_IndexParamVideoMpeg4:
        {
            OMX_VIDEO_PARAM_MPEG4TYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex==IN_PORT);
            //set MPEG4 GOP size
            sInputParam.nGOPSize=pPara->nPFrames+pPara->nBFrames+1;
        }
        break;
        case OMX_IndexParamVideoIntraRefresh:
        {
            OMX_VIDEO_PARAM_INTRAREFRESHTYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex==OUT_PORT);
        
            switch(pPara->eRefreshMode){
                case OMX_VIDEO_IntraRefreshCyclic:
                    sInputParam.nIntraFreshNum=pPara->nCirMBs;
                    break;
                case OMX_VIDEO_IntraRefreshAdaptive:
                    sInputParam.nIntraFreshNum=pPara->nAirMBs;
                    break;
                case OMX_VIDEO_IntraRefreshBoth:
                case OMX_VIDEO_IntraRefreshMax:
                default:
                    break;
                }
        }
        LOG_DEBUG("SetParameter OMX_IndexParamVideoIntraRefresh.nIntraFreshNum=%d\n",sInputParam.nIntraFreshNum);
        break;
        case OMX_IndexParamVideoBitrate:
        {
            OMX_VIDEO_PARAM_BITRATETYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex==OUT_PORT);
            //set bit rate
            switch (pPara->eControlRate)
            {
                case OMX_Video_ControlRateDisable:
                    //in this mode the encoder will ignore nTargetBitrate setting
                    //and use the appropriate Qp (nQpI, nQpP, nQpB) values for encoding
                    break;
                case OMX_Video_ControlRateVariable:
                    sInputParam.nBitRate=pPara->nTargetBitrate;
                    sInputParam.nBitRateMode = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
                    //Variable bit rate
                    break;
                case OMX_Video_ControlRateConstant:
                    //the encoder can modify the Qp values to meet the nTargetBitrate target
                    sInputParam.nBitRate=pPara->nTargetBitrate;
                    sInputParam.nBitRateMode = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
                    break;
                case OMX_Video_ControlRateVariableSkipFrames:
                    //Variable bit rate with frame skipping
                    //sInputParam.nEnableAutoSkip=1;
                    break;
                case OMX_Video_ControlRateConstantSkipFrames:
                    //Constant bit rate with frame skipping
                    //the encoder cannot modify the Qp values to meet the nTargetBitrate target.
                    //Instead, the encoder can drop frames to achieve nTargetBitrate
                    //sInputParam.nEnableAutoSkip=1;
                    break;
                case OMX_Video_ControlRateMax:
                    //Maximum value
                    if(sInputParam.nBitRate>(OMX_S32)pPara->nTargetBitrate)
                    {
                        sInputParam.nBitRate=pPara->nTargetBitrate;
                    }
                    sInputParam.nBitRateMode = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
                    break;
                default:
                    //unknown
                    ret = OMX_ErrorUnsupportedIndex;
                    break;
            }
            LOG_DEBUG("SetParameter OMX_IndexParamVideoBitrate bitrate=%d eControlRate=%d\n",sInputParam.nBitRate, pPara->eControlRate);

        }
        break;
        case OMX_IndexParamUseAndroidPrependSPSPPStoIDRFrames:
        {
            OMX_PARAM_PREPEND_SPSPPS_TO_IDR * pPara;
            pPara=(OMX_PARAM_PREPEND_SPSPPS_TO_IDR*)pComponentParameterStructure;
            bInsertSpsPps2IDR=pPara->bEnableSPSToIDR;
            LOG_DEBUG("SetParameter OMX_IndexParamUseAndroidPrependSPSPPStoIDRFrames bEnabledSPSIDR=%d\n",pPara->bEnableSPSToIDR);
        }
        break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE V4l2Enc::GetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_DEBUG("GetParameter index=%x",nParamIndex);
    switch ((OMX_U32)nParamIndex) {
        case OMX_IndexParamStandardComponentRole:
        {
            fsl_osal_strcpy((OMX_STRING)((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole,(OMX_STRING)cRole);
        }
        break;
        case OMX_IndexParamVideoProfileLevelQuerySupported:
            ret = QueryVideoProfile(pComponentParameterStructure);
        break;
        case OMX_IndexParamVideoBitrate:
        {
            OMX_VIDEO_PARAM_BITRATETYPE * pPara;
            pPara=(OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
            LOG_DEBUG("GetParameter OMX_VIDEO_PARAM_BITRATETYPE index=%d\n",pPara->nPortIndex);
            ASSERT(pPara->nPortIndex==OUT_PORT);
            //get bit rate
            if(0==sInputParam.nBitRate)
            {
                //in this mode the encoder will ignore nTargetBitrate setting
                //and use the appropriate Qp (nQpI, nQpP, nQpB) values for encoding
                pPara->eControlRate=OMX_Video_ControlRateDisable;
                pPara->nTargetBitrate=0;
            }
            else
            {
                pPara->eControlRate=OMX_Video_ControlRateConstant;
                pPara->nTargetBitrate=sInputParam.nBitRate;
            }
        }
        break;
        case OMX_IndexParamVideoMpeg4:
        {
            OMX_VIDEO_PARAM_MPEG4TYPE* pPara;
            pPara=(OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
            pPara->eProfile=OMX_VIDEO_MPEG4ProfileSimple;
            pPara->eLevel=OMX_VIDEO_MPEG4Level0;
            pPara->nPFrames=sInputParam.nGOPSize-1;
            pPara->nBFrames=0;
        }
        break;
        case OMX_IndexParamVideoAvc:
        {
            OMX_VIDEO_PARAM_AVCTYPE* pPara;
            pPara=(OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
            pPara->eProfile=(OMX_VIDEO_AVCPROFILETYPE)nProfile;
            pPara->eLevel=(OMX_VIDEO_AVCLEVELTYPE)nLevel;
            pPara->nPFrames=sInputParam.nGOPSize-1;
            pPara->nBFrames=0;
        }
        break;
        case OMX_IndexParamVideoH263:
        {
            OMX_VIDEO_PARAM_H263TYPE* pPara;
            pPara=(OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
            pPara->eProfile=OMX_VIDEO_H263ProfileBaseline;
            pPara->eLevel=OMX_VIDEO_H263Level10;
            pPara->nPFrames=sInputParam.nGOPSize-1;
            pPara->nBFrames=0;
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
OMX_ERRORTYPE V4l2Enc::SetConfig(
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
        case OMX_IndexConfigVideoBitrate:
            {
            OMX_VIDEO_CONFIG_BITRATETYPE * pPara;
            pPara=(OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentParameterStructure;
            //set bit rate
            ASSERT(pPara->nPortIndex==IN_PORT);
            sInputParam.nBitRate=pPara->nEncodeBitrate;
            LOG_DEBUG("SetConfig OMX_IndexConfigVideoBitrate\n");
            }
        break;
        case OMX_IndexConfigVideoIntraVOPRefresh://force i frame
            {
            OMX_CONFIG_INTRAREFRESHVOPTYPE * pPara;
            pPara=(OMX_CONFIG_INTRAREFRESHVOPTYPE *)pComponentParameterStructure;
            //set IDR refresh manually
            ASSERT(pPara->nPortIndex==IN_PORT);
            bRefreshIntra = pPara->IntraRefreshVOP;
            LOG_DEBUG("SetConfig OMX_IndexConfigVideoIntraVOPRefresh\n");
            }
        break;
        case OMX_IndexConfigCommonRotate:
            {
            OMX_CONFIG_ROTATIONTYPE * pPara;
            pPara=(OMX_CONFIG_ROTATIONTYPE *)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex==IN_PORT);
            fsl_osal_memcpy(&Rotation, pPara, sizeof(OMX_CONFIG_ROTATIONTYPE));
            ret = pV4l2Dev->SetEncoderRotMode(nFd,Rotation.nRotation);
            LOG_DEBUG("SetConfig OMX_IndexConfigCommonRotate rotate=%d\n",pPara->nRotation);
            }
        break;
        case OMX_IndexConfigGrallocBufferParameter:
            {
            GRALLOC_BUFFER_PARAMETER * pPara;
            pPara=(GRALLOC_BUFFER_PARAMETER *)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex==IN_PORT);
            if(sInFmt.eColorFormat!=pPara->eColorFormat){
                OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
                OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                sPortDef.nPortIndex = IN_PORT;
                ports[IN_PORT]->GetPortDefinition(&sPortDef);
                sInFmt.eColorFormat=pPara->eColorFormat;
                fsl_osal_memcpy(&(sPortDef.format.video), &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
                ports[IN_PORT]->SetPortDefinition(&sPortDef);
            }
            LOG_DEBUG("SetConfig OMX_IndexConfigGrallocBufferParameter %x\n",sInFmt.eColorFormat);
            }
        break;
        case OMX_IndexConfigIntraRefresh:
            {
            OMX_VIDEO_CONFIG_INTRAREFRESHTYPE *pPara;
            pPara = (OMX_VIDEO_CONFIG_INTRAREFRESHTYPE*)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex == OUT_PORT);
            sInputParam.nIntraFreshNum = pPara->nRefreshPeriod;
            }
        break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE V4l2Enc::GetConfig(
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

            ret = outObj->GetCrop(&sOutputCrop);

            pCropDef->nTop = sOutputCrop.nTop;
            pCropDef->nLeft = sOutputCrop.nLeft;
            pCropDef->nWidth = sOutputCrop.nWidth;
            pCropDef->nHeight = sOutputCrop.nHeight;
        }
        break;
        case OMX_IndexConfigIntraRefresh:
        {
            OMX_VIDEO_CONFIG_INTRAREFRESHTYPE *pPara;
            pPara = (OMX_VIDEO_CONFIG_INTRAREFRESHTYPE*)pComponentParameterStructure;
            ASSERT(pPara->nPortIndex == OUT_PORT);
            pPara->nRefreshPeriod = sInputParam.nIntraFreshNum;
        }
        break;
        default:
            ret = OMX_ErrorNotImplemented;
            break;
    }
    return ret;
}
OMX_ERRORTYPE V4l2Enc::ProcessInit() 
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_DEBUG("ProcessDataBuffer bInit\n");

    ret = pV4l2Dev->SetEncoderParam(nFd,&sInputParam);

    if(ret != OMX_ErrorNone){
        LOG_WARNING("V4l2Enc::ProcessInit SetEncoderPrama has error.\n");
        ret = OMX_ErrorNone;
    }

    OMX_U32 nTargetFps = sInFmt.xFramerate/Q16_SHIFT;
    ret = pV4l2Dev->SetEncoderFps(nFd,nTargetFps);
    if(ret != OMX_ErrorNone){
        LOG_WARNING("V4l2Enc::ProcessInit SetEncoderFps has error.\n");
        ret = OMX_ErrorNone;
    }

    if(CodingType == OMX_VIDEO_CodingAVC){
        ret = pV4l2Dev->SetH264EncoderProfileAndLevel(nFd, nProfile, nLevel);
        if(ret != OMX_ErrorNone){
            LOG_WARNING("V4l2Enc::ProcessInit SetH264EncoderProfileAndLevel has error.\n");
            ret = OMX_ErrorNone;
        }

        pCodecDataConverter = FSL_NEW(FrameConverter,());
        if(OMX_ErrorNone == pCodecDataConverter->Create(CodingType))
            bEnabledAVCCConverter = OMX_TRUE;
    }

    return ret;
}

OMX_ERRORTYPE V4l2Enc::CheckIfNeedPreProcess()
{
    switch((int)sInFmt.eColorFormat){
        case OMX_COLOR_Format16bitRGB565:
        case OMX_COLOR_Format32bitRGBA8888:
            bEnabledPreProcess = OMX_TRUE;
            LOG_DEBUG("CheckIfNeedPreProcess bEnabledPreProcess\n");
            break;
        default:
            bEnabledPreProcess = OMX_FALSE;
            break;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Enc::DoAllocateBuffer(OMX_PTR *buffer, OMX_U32 nSize,OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_LOG("V4l2Enc DoAllocateBuffer port=%d,size=%d\n",nPortIndex,nSize);
    if(nPortIndex == IN_PORT){

        if(bStoreMetaData){
            if(!bSetInputBufferCount){
                LOG_LOG("V4l2Enc DoAllocateBuffer bStoreMetaData set inObj buffer count=%d, nInputPlane=%d\n",nInBufferCnt, nInputPlane);

                ret = inObj->SetBufferCount(nInBufferCnt,V4L2_MEMORY_DMABUF,nInputPlane);

                if(ret != OMX_ErrorNone)
                    return ret;

                bSetInputBufferCount = OMX_TRUE;
            }

            *buffer = FSL_MALLOC(nSize);
        }else{

            if(!bSetInputBufferCount){
                LOG_LOG("V4l2Enc DoAllocateBuffer set inObj buffer count=%d\n",nInBufferCnt);
                ret = inObj->SetBufferCount(nInBufferCnt,V4L2_MEMORY_USERPTR,nInputPlane);

                if(ret != OMX_ErrorNone)
                    return ret;

                bSetInputBufferCount = OMX_TRUE;
            }
            bUseDmaInputBuffer = OMX_TRUE;

            if(bUseDmaInputBuffer && pDmaInputBuffer == NULL){
                LOG_DEBUG("V4l2Enc::pDmaBuffer->Create BEGIN \n");
                pDmaInputBuffer = FSL_NEW( DmaBuffer,());
                if(pDmaInputBuffer == NULL)
                    return OMX_ErrorInsufficientResources;
                //do not call create because only call AllocateForOutput() and Free
                //ret = pDmaInputBuffer->Create(nInputPlane);
                //if(ret != OMX_ErrorNone)
                //    return ret;
                LOG_DEBUG("V4l2Enc::ProcessInit pDmaBuffer->Create SUCCESS \n");
            }

            LOG_LOG("V4l2Enc V4L2_DEV_TYPE_ENCODER pDmaBuffer->AllocateForOutput ");
            ret = pDmaInputBuffer->AllocateForOutput(nSize, buffer);
            if(ret != OMX_ErrorNone)
                return ret;
        }

        if(*buffer != NULL)
            nInBufferNum++;

    }else if(nPortIndex == OUT_PORT){

        if(!bSetOutputBufferCount){
            LOG_LOG("V4l2Enc DoAllocateBuffer set outport buffer count=%d\n",nOutBufferCnt);
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
        LOG_ERROR("V4l2Enc DoAllocateBuffer OMX_ErrorInsufficientResources");
        return OMX_ErrorInsufficientResources;
    }
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Enc::DoFreeBuffer(OMX_PTR buffer,OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(nPortIndex == IN_PORT){

        if(bStoreMetaData){
            FSL_FREE(buffer);
        }else{
             if(bUseDmaInputBuffer){
                LOG_LOG("V4l2Enc call pDmaBuffer->Free");
                ret = pDmaInputBuffer->Free(buffer);
            }
        }

        if(ret != OMX_ErrorNone)
            return ret;

        if(nInBufferNum > 0)
            nInBufferNum --;

    }else if(nPortIndex == OUT_PORT){

        LOG_LOG("V4l2Enc DoFreeBuffer output %d\n",nOutBufferNum);
        ret = outObj->FreeBuffer(buffer);
        if(ret != OMX_ErrorNone)
            return ret;

        if(nInBufferNum > 0)
            nOutBufferNum --;

    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Enc::DoUseBuffer(OMX_PTR buffer,OMX_U32 nSize,OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_LOG("V4l2Enc DoUseBuffer nPortIndex=%d",nPortIndex);
    if(nPortIndex == IN_PORT){

        if(nInBufferNum != 0)
            return OMX_ErrorInsufficientResources;

        if(!bSetInputBufferCount){
            LOG_DEBUG("V4l2Enc DoUseBuffer set inport buffer count=%d",nInBufferCnt);
            ret = inObj->SetBufferCount(nInBufferCnt,V4L2_MEMORY_USERPTR,nInputPlane);
            bSetInputBufferCount = OMX_TRUE;
        }

    }else if(nPortIndex == OUT_PORT){

        if(nOutBufferNum != 0)
            return OMX_ErrorInsufficientResources;

        if(!bSetOutputBufferCount){
            ret = outObj->SetBufferCount(nOutBufferCnt,V4L2_MEMORY_USERPTR,nOutputPlane);
            bSetOutputBufferCount = OMX_TRUE;
        }
    }

    if(ret != OMX_ErrorNone)
        LOG_ERROR("V4l2Enc DoUseBuffer failed,ret=%d",ret);
    return ret;
}
OMX_ERRORTYPE V4l2Enc::PortFormatChanged(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    V4l2ObjectFormat sFormat;
    OMX_COLOR_FORMATTYPE color_format = OMX_COLOR_FormatUnused;
    OMX_VIDEO_CODINGTYPE codec_format = OMX_VIDEO_CodingUnused;
    LOG_LOG("PortFormatChanged index=%d\n",nPortIndex);
    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    sPortDef.nPortIndex = nPortIndex;
    ret = ports[nPortIndex]->GetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone){
        LOG_ERROR("GetPortDefinition FAILED\n");
        return ret;
    }
    sFormat.format = 0;
    sFormat.bufferSize[0] = sFormat.bufferSize[1] = sFormat.bufferSize[2] = 0;

    sFormat.width = Align(sPortDef.format.video.nFrameWidth, nWidthAlign);
    sFormat.height = Align(sPortDef.format.video.nFrameHeight, nHeightAlign);
    sFormat.stride = sFormat.width;

    LOG_LOG("PortFormatChanged w=%d,h=%d,s=%d,format=%x\n",sFormat.width,sFormat.height,sFormat.stride,sFormat.format);

    if(nPortIndex == IN_PORT){
        OMX_COLOR_FORMATTYPE old_format = sPortDef.format.video.eColorFormat;

        //when enable bStoreMetaData, port buffer size is not actual frame buffer size.
        if(!bStoreMetaData){
            sFormat.bufferSize[0] = nInBufferSize = sPortDef.nBufferSize;
            sFormat.plane_num = nInputPlane;
        }else{
            sFormat.bufferSize[0] = nInBufferSize;
            sFormat.plane_num = 1;
        }

        #ifdef MALONE_VPU
        sFormat.format = V4L2_PIX_FMT_NV12;
        #endif
 
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

        #ifdef MALONE_VPU
        switch((int)old_format){
            case OMX_COLOR_FormatYUV420SemiPlanar:
                sInFmt.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
                break;
            case OMX_COLOR_Format16bitBGR565:
                sInFmt.eColorFormat = OMX_COLOR_Format16bitBGR565;
                break;
            case OMX_COLOR_FormatAndroidOpaque:
                sInFmt.eColorFormat = (enum OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque;
                break;
            default:
                //default in format
                sInFmt.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
                break;
        }
        #endif

        nInBufferCnt = sPortDef.nBufferCountActual;
        nInBufferSize = sFormat.bufferSize[0] + sFormat.bufferSize[1] + sFormat.bufferSize[2];

        fsl_osal_memcpy(&sPortDef.format.video, &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

        sPortDef.nBufferCountActual = nInBufferCnt;
        if(!bStoreMetaData)
            sPortDef.nBufferSize = nInBufferSize;

        LOG_DEBUG("PortFormatChanged sInFmt.eColorFormat=%x, sInFmt.eCompressionFormat=%x\n", sInFmt.eColorFormat, sInFmt.eCompressionFormat);
        LOG_DEBUG("PortFormatChanged IN_PORT nInBufferCnt=%d nInBufferSize=%d,\n",nInBufferCnt, nInBufferSize);

    }else if(nPortIndex == OUT_PORT){

        OMX_U32 pad_width = sFormat.width;
        OMX_U32 pad_height = sFormat.height;
        OMX_COLOR_FORMATTYPE old_format = sPortDef.format.video.eColorFormat;

        nOutBufferSize = pad_width * pad_height * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;
        if(nOutBufferSize < sPortDef.nBufferSize)
            nOutBufferSize = sPortDef.nBufferSize;

        sFormat.bufferSize[0] = nOutBufferSize;

        sFormat.plane_num = nOutputPlane;

        if(sPortDef.format.video.eCompressionFormat != OMX_VIDEO_CodingUnused){
            sFormat.format = ConvertOmxCodecFormatToV4l2Format(sPortDef.format.video.eCompressionFormat);
        }

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

        if(ConvertV4l2FormatToOmxCodecFormat(sFormat.format,&codec_format)){
            sOutFmt.eCompressionFormat = codec_format;
        }
        LOG_LOG("PortFormatChanged sOutFmt.eColorFormat=%x,sOutFmt.eCompressionFormat=%x\n",sOutFmt.eColorFormat,sOutFmt.eCompressionFormat);

        LOG_DEBUG("PortFormatChanged buffer count=%d",sPortDef.nBufferCountActual);
        nOutBufferCnt = sPortDef.nBufferCountActual;

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

    return OMX_ErrorNone;

}

OMX_ERRORTYPE V4l2Enc::FlushComponent(OMX_U32 nPortIndex)
{
    OMX_BUFFERHEADERTYPE * bufHdlr;
    LOG_LOG("FlushComponent index=%d,in num=%d,out num=%d\n",nPortIndex,ports[IN_PORT]->BufferNum(),ports[OUT_PORT]->BufferNum());

    if(nPortIndex == IN_PORT){
        bInputEOS = OMX_FALSE;

        if(bEnabledPreProcess){
            OMX_BUFFERHEADERTYPE *pBufHdr = NULL;
            DmaBufferHdr *pDmaBufHdr = NULL;
            pPreProcess->Flush();
            while(OMX_ErrorNone == pPreProcess->GetInputBuffer(&pBufHdr) && pBufHdr != NULL)
                ReturnBuffer(pBufHdr,IN_PORT);
            while(OMX_ErrorNone == pPreProcess->GetInputReturnBuffer(&pBufHdr) && pBufHdr != NULL)
                ReturnBuffer(pBufHdr,IN_PORT);
            while(OMX_ErrorNone == pPreProcess->GetOutputReturnBuffer(&pDmaBufHdr) && pDmaBufHdr != NULL){
                pDmaBufHdr->bReadyForProcess = OMX_FALSE;
                pDmaBufHdr->flag = 0;
                pDmaBuffer->Add(pDmaBufHdr);
            }
        }

        #ifdef ENABLE_TS_MANAGER
        tsmFlush(hTsHandle);
        #endif

        inObj->Flush();
        nInputCnt = 0;
        while(inObj->HasBuffer()){
            if(bEnabledPreProcess){
                DmaBufferHdr *pDmaBufHdr = NULL;
                if(OMX_ErrorNone == inObj->GetBuffer(&pDmaBufHdr)){
                    pDmaBufHdr->bReadyForProcess = OMX_FALSE;
                    pDmaBufHdr->flag = 0;
                    pDmaBuffer->Add(pDmaBufHdr);
                }
            }else{
                OMX_BUFFERHEADERTYPE *pBufHdr = NULL;
                if(OMX_ErrorNone == inObj->GetBuffer(&pBufHdr)){
                    ReturnBuffer(pBufHdr,IN_PORT);
                }
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
        outObj->Flush();
        bOutputStarted = OMX_FALSE;
        nOutputCnt = 0;

        if(!bSendCodecData && pCodecDataBufferHdr != NULL){
            ports[OUT_PORT]->SendBuffer(pCodecDataBufferHdr);
            pCodecDataBufferHdr = NULL;
            LOG_DEBUG("FlushComponent send codec data buffer hdr\n");
        }
        bSendCodecData = OMX_FALSE;

        while(outObj->HasBuffer()){
            if(OMX_ErrorNone == outObj->GetBuffer(&bufHdlr)){
                ReturnBuffer(bufHdlr,OUT_PORT);
            }
        }

        LOG_DEBUG("FlushComponent index=1 port num=%d\n",ports[OUT_PORT]->BufferNum());
        while(ports[OUT_PORT]->BufferNum() > 0) {
            ports[OUT_PORT]->GetBuffer(&bufHdlr);
            if(bufHdlr != NULL)
                ports[OUT_PORT]->SendBuffer(bufHdlr);
        }

        eState = V4L2_ENC_STATE_FLUSHED;

    }
    nErrCnt = 0;
    LOG_DEBUG("FlushComponent index=%d END\n",nPortIndex);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Enc::DoIdle2Loaded()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_LOG("DoIdle2Loaded BEGIN");

    if(inThread){
        inThread->destroy();
        FSL_DELETE(inThread);
    }

    bOutputStarted = OMX_FALSE;
    bInputEOS = OMX_FALSE;
    bOutputEOS = OMX_FALSE;

    FSL_FREE(pCodecData);

    if(pPreProcess != NULL){
        //pPreProcess->Destroy();
        FSL_DELETE(pPreProcess);
        bEnabledPreProcess = OMX_FALSE;
    }

    if(pCodecDataConverter!= NULL){
        pCodecDataConverter->Destroy();
        FSL_DELETE(pCodecDataConverter);
    }

    if(pDmaBuffer != NULL){
        pDmaBuffer->FreeAll();
        FSL_DELETE(pDmaBuffer);
        bDmaBufferAllocated = OMX_FALSE;
    }

    if(pDmaInputBuffer != NULL){
        FSL_DELETE(pDmaInputBuffer);
        bUseDmaInputBuffer = OMX_FALSE;
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
OMX_ERRORTYPE V4l2Enc::DoLoaded2Idle()
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

OMX_ERRORTYPE V4l2Enc::ProcessDataBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch(eState){
        case V4L2_ENC_STATE_IDLE:
            LOG_LOG("V4l2Enc eState V4L2_FILTER_STATE_IDLE \n");
            eState = V4L2_ENC_STATE_INIT;
            break;
        case V4L2_ENC_STATE_INIT:
            LOG_LOG("V4l2Enc eState V4L2_FILTER_STATE_INIT \n");
            ret = ProcessInit();
            if(ret != OMX_ErrorNone)
                break;

            eState = V4L2_ENC_STATE_START_ENCODE;
            break;

        case V4L2_ENC_STATE_FLUSHED:
            LOG_LOG("V4l2Enc eState V4L2_FILTER_STATE_FLUSHED \n");
            if(bSetOutputBufferCount){
                eState = V4L2_ENC_STATE_RUN;
            }else
                fsl_osal_sleep(1000);
            break;
        case V4L2_ENC_STATE_RES_CHANGED:
            LOG_LOG("V4l2Enc eState V4L2_FILTER_STATE_RES_CHANGED \n");
            fsl_osal_sleep(1000);
            break;
        case V4L2_ENC_STATE_RUN:
        {
            OMX_ERRORTYPE ret_in = OMX_ErrorNone;
            OMX_ERRORTYPE ret_other = OMX_ErrorNone;

            if(bOutputEOS){
                LOG_LOG("bOutputEOS OMX_ErrorNoMore");
                ret = OMX_ErrorNoMore;
                break;
            }

            //check pre process here because OMX_IndexConfigGrallocBufferParameter is set in empty buffer
            if(!bCheckPreProcess && ports[IN_PORT]->BufferNum() > 0){
                CheckIfNeedPreProcess();
                ret = PrepareForPreprocess();
                if(ret != OMX_ErrorNone){
                    LOG_ERROR("PrepareForPreprocess failed");
                    return ret;
                }
                bCheckPreProcess = OMX_TRUE;
            }

            fsl_osal_mutex_lock(sMutex);
            ret = ProcessOutputBuffer();
            fsl_osal_mutex_unlock(sMutex);

            if(ret != OMX_ErrorNone){
                fsl_osal_mutex_lock(sMutex);
                ret_in = ProcessInputBuffer();
                fsl_osal_mutex_unlock(sMutex);
                //LOG_LOG("ProcessInputBuffer ret=%x",ret);
            }


            if(bEnabledPreProcess){
                ret_other = ProcessPreBuffer();
                if(ret_other == OMX_ErrorNone)
                    return OMX_ErrorNone;
            }

            if(ret == OMX_ErrorNoMore && ret_in == OMX_ErrorNoMore){
                ret = OMX_ErrorNoMore;
            }else{
                ret = OMX_ErrorNone;
            }

            if(bInputEOS && !bOutputEOS && 0 == nInputCnt){
                eState = V4L2_ENC_STATE_STOP_ENCODE;
                LOG_LOG("V4l2Enc eState goto V4L2_FILTER_STATE_STOP_ENCODE \n");
                break;
            }

            if(ret == OMX_ErrorNone && !bOutputStarted){
                bOutputStarted = OMX_TRUE;
                inThread->start();
                outObj->Start();
            }

            break;
        }
        case V4L2_ENC_STATE_START_ENCODE:
            bNewSegment = OMX_TRUE;
            eState = V4L2_ENC_STATE_RUN;
            break;
        case V4L2_ENC_STATE_STOP_ENCODE:
            fsl_osal_mutex_lock(sMutex);
            OMX_BUFFERHEADERTYPE * pBufHdlr;
            if(ports[OUT_PORT]->BufferNum() == 0){
                fsl_osal_mutex_unlock(sMutex);
                return OMX_ErrorNoMore;
            }

            ports[OUT_PORT]->GetBuffer(&pBufHdlr);
            if(pBufHdlr == NULL){
                fsl_osal_mutex_unlock(sMutex);
                return OMX_ErrorNoMore;
            }

            pBufHdlr->nFlags |= OMX_BUFFERFLAG_EOS;
            ret = ReturnBuffer(pBufHdlr,OUT_PORT);
            eState = V4L2_ENC_STATE_END;
            LOG_LOG("V4l2Enc eState V4L2_FILTER_STATE_STOP_ENCODE \n");
            fsl_osal_mutex_unlock(sMutex);
            break;
        default:
            break;
    }

    return ret;
}

OMX_ERRORTYPE V4l2Enc::ProcessInputBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    OMX_U32 flags = 0;
    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
    if(ports[IN_PORT]->BufferNum() == 0)
        return OMX_ErrorNoMore;

#ifdef ENABLE_TS_MANAGER
        if(OMX_TRUE != tsmHasEnoughSlot(hTsHandle)) {
            LOG_LOG("tsmHasEnoughSlot FALSE");
            return OMX_ErrorNone;
        }
#endif

    ports[IN_PORT]->GetBuffer(&pBufferHdr);

    if(pBufferHdr == NULL){
        return OMX_ErrorNoMore;
    }
    LOG_LOG("Get Inbuffer %p,len=%d,ts=%lld,flag=%x,offset=%d\n", pBufferHdr->pBuffer, pBufferHdr->nFilledLen, pBufferHdr->nTimeStamp, pBufferHdr->nFlags,pBufferHdr->nOffset);

    ret = ProcessInBufferFlags(pBufferHdr);
    if(ret != OMX_ErrorNone)
        return ret;

    if(bRefreshIntra){
        flags |= V4L2_OBJECT_FLAG_KEYFRAME;
        bRefreshIntra = OMX_FALSE;
    }
    if(bStoreMetaData)
        flags |= V4L2_OBJECT_FLAG_METADATA_BUFFER;

    if(pBufferHdr->nFilledLen > 0){
        LOG_LOG("set Input buffer BEGIN ts=%lld\n",pBufferHdr->nTimeStamp);

        dumpInputBuffer(pBufferHdr);

        #ifdef ENABLE_TS_MANAGER
        tsmSetBlkTs(hTsHandle, pBufferHdr->nFilledLen, pBufferHdr->nTimeStamp);
        #endif

        if(bEnabledPreProcess){
            if(flags & V4L2_OBJECT_FLAG_KEYFRAME)
                pBufferHdr->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;
            ret = pPreProcess->AddInputFrame(pBufferHdr);
        }else
            ret = inObj->SetBuffer(pBufferHdr, flags);

        nInputCnt ++;
        LOG_LOG("ProcessInputBuffer ret=%x nInputCnt=%d\n",ret,nInputCnt);
        if(ret != OMX_ErrorNone) {
            ports[IN_PORT]->SendBuffer(pBufferHdr);
            LOG_ERROR("set buffer err=%x\n",ret);
            return ret;
        }

    }else{
        ports[IN_PORT]->SendBuffer(pBufferHdr);
        pBufferHdr = NULL;
        return ret;
    }

    //handle last buffer here when it has data and eos flag
    if(pBufferHdr->nFilledLen > 0 && (pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)){
        LOG_LOG("OMX_BUFFERFLAG_EOS nFilledLen > 0");
        if(bEnabledPreProcess){
            pPreProcess->Flush();
            return OMX_ErrorNoMore;
        }else{
            HandleEOSEvent(IN_PORT);
            ports[IN_PORT]->SendBuffer(pBufferHdr);
            return OMX_ErrorNoMore;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Enc::ProcessInBufferFlags(OMX_BUFFERHEADERTYPE *pInBufferHdr)
{
    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTIME){
        bNewSegment = OMX_TRUE;
        bRefreshIntra = OMX_TRUE;
    }

    #ifdef ENABLE_TS_MANAGER
    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTIME) {

        //could not get consume length now, use FIFO mode
        if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTRICK) {
            LOG_DEBUG("Set ts manager to FIFO mode.\n");
            tsmReSync(hTsHandle, pInBufferHdr->nTimeStamp, MODE_FIFO);
        }
        else {
            LOG_DEBUG("Set ts manager to AI mode. ts=%lld\n",pInBufferHdr->nTimeStamp);
            tsmReSync(hTsHandle, pInBufferHdr->nTimeStamp, MODE_AI);
        }
        LOG_INFO("nDurationThr: %d, nBufCntThr: %d\n", nMaxDurationMsThr, nMaxBufCntThr);
        tsmSetDataDepthThreshold(hTsHandle, nMaxDurationMsThr, nMaxBufCntThr);
    }
    #endif

    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_EOS){
        LOG_LOG("get OMX_BUFFERFLAG_EOS for input buffer");
        bInputEOS = OMX_TRUE;

        //when the eos buffer has data, need to call inObj->SetBuffer before stop command
        if(0 == pInBufferHdr->nFilledLen){
            if(bEnabledPreProcess){
                pPreProcess->AddInputFrame(pInBufferHdr);
                if(pPreProcess->GetEos()){
                    HandleEOSEvent(IN_PORT);
                }
                ports[IN_PORT]->SendBuffer(pInBufferHdr);
                return OMX_ErrorNoMore;
            }else{
                HandleEOSEvent(IN_PORT);
                ports[IN_PORT]->SendBuffer(pInBufferHdr);
                return OMX_ErrorNoMore;
            }
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Enc::ProcessOutputBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    
    if(ports[OUT_PORT]->BufferNum() == 0)
        return OMX_ErrorNoMore;

    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;

    ports[OUT_PORT]->GetBuffer(&pBufferHdr);
    if(pBufferHdr == NULL){
        return OMX_ErrorNoMore;
    }
    LOG_LOG("Get output buffer %p,len=%d,alloLen=%d,flag=%x,ts=%lld,bufferCnt=%d\n",
        pBufferHdr->pBuffer,pBufferHdr->nFilledLen,pBufferHdr->nAllocLen,pBufferHdr->nFlags,pBufferHdr->nTimeStamp,ports[OUT_PORT]->BufferNum());

    if(!bSendCodecData && pCodecDataBufferHdr == NULL){
        pCodecDataBufferHdr = pBufferHdr;
        LOG_DEBUG("store codec data bufer for output ptr=%p\n",pCodecDataBufferHdr->pBuffer);
        return OMX_ErrorNone;
    }

    if(pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
        bOutputEOS = OMX_TRUE;

    //v4l2 buffer has alignment requirement
    //replace the allocated buffer size if it is larger than required buffer size
    if(nOutBufferSize > 0 && pBufferHdr->nAllocLen > nOutBufferSize){
        pBufferHdr->nAllocLen = nOutBufferSize;
        LOG_DEBUG("reset nAllocLen to %d\n",pBufferHdr->nAllocLen);
    }

    LOG_LOG("Set output buffer BEGIN,pBufferHdr=%p \n",pBufferHdr);

    ret = outObj->SetBuffer(pBufferHdr,0);

    LOG_LOG("ProcessOutputBuffer ret=%x\n",ret);

    if(ret != OMX_ErrorNone) {
        pBufferHdr->nFilledLen = 0;
        ports[OUT_PORT]->SendBuffer(pBufferHdr);
        return OMX_ErrorNoMore;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Enc::ReturnBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr,OMX_U32 nPortIndex)
{
    if(pBufferHdr == NULL){
        LOG_LOG("ReturnBuffer failed\n");
        return OMX_ErrorBadParameter;
    }

    if(nPortIndex == IN_PORT){
        if(pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)
            HandleEOSEvent(IN_PORT);
        ports[IN_PORT]->SendBuffer(pBufferHdr);
        LOG_LOG("ReturnBuffer input =%p,ts=%lld,len=%d,offset=%d flag=%x nInputCnt=%d\n",
            pBufferHdr->pBuffer, pBufferHdr->nTimeStamp,pBufferHdr->nFilledLen,pBufferHdr->nOffset,pBufferHdr->nFlags, nInputCnt);

    }else if(nPortIndex == OUT_PORT) {

        if(!bSendCodecData && pCodecDataBufferHdr != NULL && bEnabledAVCCConverter){
            //send codec data for encoder
            OMX_U32 offset = 0;
            if(OMX_ErrorNone == pCodecDataConverter->CheckSpsPps(pBufferHdr->pBuffer, pBufferHdr->nFilledLen, &offset)){
                OMX_U8* pData = NULL;
                if(OMX_ErrorNone != pCodecDataConverter->GetSpsPpsPtr(&pData, &offset)){
                    LOG_ERROR("failed to get codec data");
                    return OMX_ErrorUndefined;
                }

                fsl_osal_memcpy(pCodecDataBufferHdr->pBuffer, pData, offset);
                pCodecDataBufferHdr->nFilledLen = offset;
                pCodecDataBufferHdr->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
                pCodecDataBufferHdr->nOffset = 0;
                pCodecDataBufferHdr->nTimeStamp = 0;

                dumpOutputBuffer(pCodecDataBufferHdr);
 
                ports[OUT_PORT]->SendBuffer(pCodecDataBufferHdr);
                bSendCodecData = OMX_TRUE;
                pBufferHdr->nOffset = offset;
                LOG_DEBUG("send codec data buffer len=%d,consume=%d.ptr=%p\n",
                    pCodecDataBufferHdr->nFilledLen,pBufferHdr->nOffset,pCodecDataBufferHdr->pBuffer);
                pBufferHdr->nFilledLen -= pBufferHdr->nOffset;
                pCodecDataBufferHdr = NULL;
                nOutputCnt++;
            }
        }

        if(bInsertSpsPps2IDR && (pBufferHdr->nFlags & OMX_BUFFERFLAG_SYNCFRAME)){
            OMX_U32 nLen = 0;
            OMX_U8* pData = NULL;
            if(OMX_ErrorNone == pCodecDataConverter->GetSpsPpsPtr(&pData,&nLen)){
                fsl_osal_memmove(pBufferHdr->pBuffer+nLen,pBufferHdr->pBuffer,pBufferHdr->nFilledLen);
                fsl_osal_memcpy(pBufferHdr->pBuffer+pBufferHdr->nOffset,pData,nLen);
                pBufferHdr->nFilledLen += nLen;
            }
        }

        if(bNewSegment){
            pBufferHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME;
            bNewSegment = OMX_FALSE;
            LOG_DEBUG("send buffer OMX_BUFFERFLAG_STARTTIME\n");

        }
        if((pBufferHdr->nFlags & OMX_BUFFERFLAG_EOS)){
            LOG_DEBUG("send buffer OMX_BUFFERFLAG_EOS\n");
            bOutputEOS = OMX_TRUE;
            //vpu encoder will add an eos nal for last frame, ignore the buffer.
            if(4 == pBufferHdr->nFilledLen)
                pBufferHdr->nFilledLen = 0;

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
OMX_ERRORTYPE V4l2Enc::HandleFormatChanged(OMX_U32 nPortIndex)
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

    LOG_LOG("HandleFormatChanged OUT_PORT numCnt=%d\n",numCnt);
    
    pad_width = Align(sFormat.width, nWidthAlign);
    pad_height = Align(sFormat.height, nHeightAlign);

    if(pad_width > sOutFmt.nFrameWidth ||
        pad_height > sOutFmt.nFrameHeight){
        bResourceChanged = OMX_TRUE;
    }else if(pad_width < sOutFmt.nFrameWidth ||
        pad_height < sOutFmt.nFrameHeight){
        bResourceChanged = OMX_TRUE;
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
        eState = V4L2_ENC_STATE_RES_CHANGED;
        fsl_osal_memcpy(&sOutFmt, &sPortDef.format.video, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

        bSendEvent = OMX_TRUE;
        bSetOutputBufferCount = OMX_FALSE;
        bSendPortSettingChanged = OMX_TRUE;
        SendEvent(OMX_EventPortSettingsChanged, OUT_PORT, 0, NULL);
        LOG_LOG("send OMX_EventPortSettingsChanged \n");
    }else{
        LOG_LOG("HandleFormatChanged do not send OMX_EventPortSettingsChanged\n");
        eState = V4L2_ENC_STATE_RUN;
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

    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Enc::HandleErrorEvent()
{
    nErrCnt ++;

    if(nErrCnt > 10){
        SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
        LOG_ERROR("HandleErrorEvent send event\n");
        nErrCnt = 0;
        pV4l2Dev->StopEncoder(nFd);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Enc::HandleEOSEvent(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(nPortIndex == IN_PORT){
        pV4l2Dev->StopEncoder(nFd);
        LOG_LOG("send stop command");
    }else{
        LOG_LOG("get eos event, wait for frame with V4L2_BUF_FLAG_LAST");
    }

    return ret;
}

OMX_ERRORTYPE V4l2Enc::HandleInObjBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    fsl_osal_mutex_lock(sMutex);

    OMX_BUFFERHEADERTYPE * pBufHdlr;
    DmaBufferHdr * pDmaBufHdlr;
    LOG_LOG("send input buffer BEGIN \n");

    if(bEnabledPreProcess && bUseDmaBuffer){
        if(OMX_ErrorNone == inObj->GetOutputBuffer(&pDmaBufHdlr)){
            ret = pDmaBuffer->Add(pDmaBufHdlr);
        }
    }else if(OMX_ErrorNone == inObj->GetOutputBuffer(&pBufHdlr)){
        ret = ReturnBuffer(pBufHdlr,IN_PORT);
        LOG_LOG("send input buffer END ret=%x\n",ret);
    }
    fsl_osal_mutex_unlock(sMutex);

    if(bEnabledPreProcess && bUseDmaBuffer)
        ProcessPreBuffer();

    return ret;
}
OMX_ERRORTYPE V4l2Enc::HandleOutObjBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    fsl_osal_mutex_lock(sMutex);

    LOG_LOG("send output buffer BEGIN \n");

    OMX_BUFFERHEADERTYPE * pBufHdlr;
    if(OMX_ErrorNone == outObj->GetOutputBuffer(&pBufHdlr))
        ret = ReturnBuffer(pBufHdlr,OUT_PORT);
    LOG_LOG("send output buffer END ret=%x \n", ret);

    fsl_osal_mutex_unlock(sMutex);
    return ret;
}
void V4l2Enc::dumpInputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    FILE * pfile = NULL;
    OMX_U8* pVaddr = NULL;
    int fd = -1;
    
    if(pBufferHdr == NULL)
        return;

    if(!(nDebugFlag & DUMP_LOG_FLAG_DUMP_INPUT))
        return;

    if(nInputCnt < MAX_DUMP_FRAME)
        pfile = fopen(DUMP_INPUT_FILE,"ab");

    if(bStoreMetaData){
        OMX_U8* temp = NULL;
        pVaddr = (OMX_U8*)((OMX_U64 *)(pBufferHdr->pBuffer))[0];
        fd = (unsigned long)pVaddr;
        pVaddr = (OMX_U8*)mmap(0, nInBufferSize, PROT_READ, MAP_SHARED, fd, 0);
    }else
        pVaddr = pBufferHdr->pBuffer;

    if(pfile && NULL != pVaddr){
        fwrite(pVaddr,1,nInBufferSize,pfile);
        if(bStoreMetaData)
            munmap((void*)pVaddr, nInBufferSize);
        fclose(pfile);
    }else if(bStoreMetaData && NULL != pVaddr)
        munmap((void*)pVaddr, nInBufferSize);

    return;
}
void V4l2Enc::dumpOutputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    FILE * pfile = NULL;
    if(pBufferHdr == NULL)
        return;

    if(!(nDebugFlag & DUMP_LOG_FLAG_DUMP_OUTPUT))
        return;

    if(nOutputCnt < MAX_DUMP_FRAME)
        pfile = fopen(DUMP_OUTPUT_FILE,"ab");

    if(pfile){
        fwrite(pBufferHdr->pBuffer,1,pBufferHdr->nFilledLen,pfile);
        fclose(pfile);
    }
    return;
}
void V4l2Enc::dumpPreProcessBuffer(DmaBufferHdr *pBufferHdr)
{
    FILE * pfile = NULL;
    if(pBufferHdr == NULL)
        return;

    if(!(nDebugFlag & DUMP_LOG_FLAG_DUMP_PREPROCESS))
        return;

    if(nOutputCnt < MAX_DUMP_FRAME)
        pfile = fopen(DUMP_PREPROCESS_FILE,"wb");

    if(pfile){
        fwrite((OMX_U8*)pBufferHdr->plane[0].vaddr,1,pBufferHdr->plane[0].size,pfile);
        fwrite((OMX_U8*)pBufferHdr->plane[1].vaddr,1,pBufferHdr->plane[1].size,pfile);
        fclose(pfile);
    }
    return;
}
OMX_ERRORTYPE V4l2Enc::PrepareForPreprocess()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(!bEnabledPreProcess)
        return ret;

    if(pPreProcess == NULL){
        pPreProcess = FSL_NEW( IsiColorConvert,());

        if(pPreProcess == NULL)
            return OMX_ErrorInsufficientResources;
            
        ret = pPreProcess->Create();
        if(ret != OMX_ErrorNone)
            return ret;

        pDmaBuffer = NULL;
        bUseDmaBuffer = OMX_TRUE;
        bDmaBufferAllocated = OMX_FALSE;
        bAllocateFailed = OMX_FALSE;

        nDmaBufferCnt = DEFAULT_DMA_BUF_CNT;
        eDmaBufferFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        nDmaBufferSize[0] = Align(sOutFmt.nFrameWidth, nWidthAlign) * Align(sOutFmt.nFrameHeight, nHeightAlign);
        nDmaBufferSize[1] = nDmaBufferSize[0] / 2;
        nDmaBufferSize[2] = 0;
        nDmaPlanes = 2;

        ISI_CALLBACKTYPE * callback = &PreProcessCallBack;
        ret = pPreProcess->SetCallbackFunc(callback,this);
        if(ret != OMX_ErrorNone)
            return ret;

        //ret = pPreProcess->Start();
        //if(ret != OMX_ErrorNone)
        //    return ret;
    }

    if(bUseDmaBuffer && pDmaBuffer == NULL){
        pDmaBuffer = FSL_NEW( DmaBuffer,());
        if(pDmaBuffer == NULL)
            return OMX_ErrorInsufficientResources;
        ret = pDmaBuffer->Create(nDmaPlanes);
        if(ret != OMX_ErrorNone)
            return ret;
        LOG_DEBUG("V4l2Enc::PrepareForPreprocess SUCCESS \n");
    }

    if(bUseDmaBuffer && !bDmaBufferAllocated && pDmaBuffer){
        LOG_LOG("pDmaBuffer->Allocate cnt=%d,size[0]=%d,size[1]=%d,size[2]=%d",nDmaBufferCnt, nDmaBufferSize[0], nDmaBufferSize[1], nDmaBufferSize[2]);
        ret = pDmaBuffer->Allocate(nDmaBufferCnt, &nDmaBufferSize[0], eDmaBufferFormat);
        bDmaBufferAllocated = OMX_TRUE;
        if(ret != OMX_ErrorNone){
            LOG_ERROR("ProcessDmaBuffer Allocate failed");
            bAllocateFailed = OMX_TRUE;
            nErrCnt = 10;
            pDmaBuffer->FreeAll();
            pV4l2Dev->StopEncoder(nFd);
            ret = OMX_ErrorInsufficientResources;
            SendEvent(OMX_EventError, OMX_ErrorInsufficientResources, 0, NULL);
            return ret;
        }
    }

    ISI_FORMAT fmt;
    fmt.format = sInFmt.eColorFormat;
    fmt.width = sOutFmt.nFrameWidth;
    fmt.height = sOutFmt.nFrameHeight;
    fmt.stride = sOutFmt.nStride;
    fmt.bufferSize = Align(sInFmt.nFrameWidth, nWidthAlign) * Align(sInFmt.nFrameHeight, nHeightAlign) * pxlfmt2bpp(sInFmt.eColorFormat) / 8;

    ret = pPreProcess->ConfigInput(&fmt);
    if(ret != OMX_ErrorNone)
        return ret;

    fmt.format = eDmaBufferFormat;
    fmt.bufferSize = Align(sInFmt.nFrameWidth, nWidthAlign) * Align(sInFmt.nFrameHeight, nHeightAlign) * pxlfmt2bpp(eDmaBufferFormat) / 8;

    ret = pPreProcess->ConfigOutput(&fmt);
    if(ret != OMX_ErrorNone)
        return ret;

    ret = pPreProcess->SetBufferCount(nInBufferCnt, nDmaBufferCnt);
    if(ret != OMX_ErrorNone)
        return ret;
    return ret;
}
OMX_ERRORTYPE V4l2Enc::ProcessPreBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    DmaBufferHdr *buf = NULL;
    LOG_LOG("ProcessPreBuffer BEGIN\n");
    fsl_osal_mutex_lock(sMutex);

    ret = pDmaBuffer->Get(&buf);
    if(OMX_ErrorNone != ret){
        LOG_LOG("ProcessPreBuffer get dma buffer failed\n");
        fsl_osal_mutex_unlock(sMutex);
        return ret;
    }

    buf->bReadyForProcess = OMX_FALSE;
    buf->flag = 0;
    ret = pPreProcess->AddOutputFrame(buf);
    LOG_LOG("ProcessPreBuffer ret=%d\n",ret);

    fsl_osal_mutex_unlock(sMutex);
    return ret;
}
OMX_ERRORTYPE V4l2Enc::HandlePreProcessBuffer(DmaBufferHdr *buf)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    fsl_osal_mutex_lock(sMutex);

    if(buf->flag & DMA_BUF_EOS){
        HandleEOSEvent(IN_PORT);
        fsl_osal_mutex_unlock(sMutex);
        return ret;
    }
    dumpPreProcessBuffer(buf);
    OMX_U32 flag = 0;
    if(buf->flag & DMA_BUF_SYNC)
        flag |= V4L2_OBJECT_FLAG_KEYFRAME;
    ret = inObj->SetBuffer(buf, flag);

    fsl_osal_mutex_unlock(sMutex);
    LOG_LOG("HandlePreProcessBuffer ret=%d\n", ret);

    return ret;
}
    
/**< C style functions to expose entry point for the shared library */
extern "C"
{
    OMX_ERRORTYPE VpuEncoderInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        V4l2Enc *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(V4l2Enc, ());
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
