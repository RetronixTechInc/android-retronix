/**
 *  Copyright (c) 2010-2016, Freescale Semiconductor Inc.,
 *  Copyright 2017-2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "VideoFilter.h"
#include "Tsm_wrapper.h"

#define MOSAIC_COUNT 3

//#undef LOG_DEBUG
//#define LOG_DEBUG printf
//#undef LOG_INFO
//#define LOG_INFO printf

VideoFilter::VideoFilter()
{
    bInContext = OMX_FALSE;
    nPorts = NUM_PORTS;
    bFilterSupportPartilInput = OMX_FALSE;
}

OMX_ERRORTYPE VideoFilter:: SetDefaultSetting()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
	OMX_VIDEO_PARAM_PORTFORMATTYPE sPortFormat;
    //OMX_BUFFERSUPPLIERTYPE SupplierType;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = IN_PORT;
    sPortDef.eDir = OMX_DirInput;
    sPortDef.eDomain = OMX_PortDomainVideo;
    fsl_osal_memcpy(&sPortDef.format.video, &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

    sPortDef.format.video.nStride = sPortDef.format.video.nFrameWidth;
    sPortDef.format.video.nSliceHeight= sPortDef.format.video.nFrameHeight;

    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_TRUE;
    sPortDef.nBufferCountMin = nInBufferCnt;
    sPortDef.nBufferCountActual = nInBufferCnt;
    sPortDef.nBufferSize = nInBufferSize;
    ret = ports[IN_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for in port failed.\n");
        return ret;
    }

	for (OMX_U32 i=0; i<nInPortFormatCnt; i++) {
    	OMX_INIT_STRUCT(&sPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    	sPortFormat.nPortIndex = IN_PORT;
		sPortFormat.nIndex = i;
		sPortFormat.eCompressionFormat = sPortDef.format.video.eCompressionFormat;
		sPortFormat.eColorFormat = eInPortPormat[i];
		sPortFormat.xFramerate = sPortDef.format.video.xFramerate;
		LOG_DEBUG("Set support color format: %d\n", eInPortPormat[i]);
		ret = ports[IN_PORT]->SetPortFormat(&sPortFormat);
		if(ret != OMX_ErrorNone) {
			LOG_ERROR("Set port format for in port failed.\n");
			return ret;
		}
	}

    sPortDef.nPortIndex = OUT_PORT;
    sPortDef.eDir = OMX_DirOutput;
    sPortDef.nBufferCountMin = nOutBufferCnt;
    sPortDef.nBufferCountActual = nOutBufferCnt;
    fsl_osal_memcpy(&sPortDef.format.video, &sOutFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

    sPortDef.format.video.nStride = sPortDef.format.video.nFrameWidth;
    sPortDef.format.video.nSliceHeight= sPortDef.format.video.nFrameHeight;

    sPortDef.nBufferSize=nOutBufferSize;
    ret = ports[OUT_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for out port failed.\n");
        return ret;
    }

	for (OMX_U32 i=0; i<nOutPortFormatCnt; i++) {
    	OMX_INIT_STRUCT(&sPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    	sPortFormat.nPortIndex = OUT_PORT;
		sPortFormat.nIndex = i;
		sPortFormat.eCompressionFormat = sPortDef.format.video.eCompressionFormat;
		sPortFormat.eColorFormat = eOutPortPormat[i];
		sPortFormat.xFramerate = sPortDef.format.video.xFramerate;
		LOG_DEBUG("Set support color format: %d\n", eOutPortPormat[i]);
		ret = ports[OUT_PORT]->SetPortFormat(&sPortFormat);
		if(ret != OMX_ErrorNone) {
			LOG_ERROR("Set port format for in port failed.\n");
			return ret;
		}
	}

    bInit = OMX_FALSE;
    bGetNewSegment = OMX_TRUE;
    bNewSegment = OMX_FALSE;
	nInvalidFrameCnt = 0;
    bLastInput = OMX_FALSE;
    bLastOutput = OMX_FALSE;
    pCodecData = NULL;
    nCodecDataLen = 0;
    bResourceChanged = OMX_FALSE;
    bFilterSupportFrmSizeRpt=OMX_FALSE;
    fsl_osal_memset(&PartialInputHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));
    bInReturnBufferState = OMX_FALSE;
    nDecodeOnly = 0;
    pInBufferHdr = NULL;
    bNeedOutBuffer = DefaultOutputBufferNeeded();
    bNeedInputBuffer = OMX_TRUE;
    pImageConvert = NULL;

	OMX_INIT_STRUCT(&Rotation, OMX_CONFIG_ROTATIONTYPE);

	Rotation.nPortIndex = OMX_ALL;
	Rotation.nRotation = 0;

    hTsHandle = NULL;

    bNeedMapDecAndOutput=OMX_FALSE;   //default: needn't map decoded frame and output frame
    fsl_osal_memset(&outputCrop, 0, sizeof(OMX_CONFIG_RECTTYPE));

    nMediaTime = 0;
    nAnchorTime = 0;
    nScale = 1*Q16_SHIFT;
    bThreadedPreProcess = OMX_FALSE;
    bEnableAndroidNativeHandleBuffer = OMX_FALSE;
    bUpdateColorAspects = OMX_FALSE;
    return ret;
}

OMX_ERRORTYPE VideoFilter::InitComponent()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = InitFilterComponent();
    if(ret != OMX_ErrorNone)
        return ret;

    ret=SetDefaultSetting();
    if(ret != OMX_ErrorNone)
        return ret;

    return ret;
}

OMX_ERRORTYPE VideoFilter::DeInitComponent()
{
    DeInitFilterComponent();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::DoAllocateBuffer(OMX_PTR *buffer, OMX_U32 nSize,OMX_U32 nPortIndex)
{
    if(nPortIndex == IN_PORT)
        *buffer = AllocateInputBuffer(nSize);
    if(nPortIndex == OUT_PORT)
        *buffer = AllocateOutputBuffer(nSize);

    if (*buffer == NULL)
        return OMX_ErrorInsufficientResources;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::DoFreeBuffer(OMX_PTR buffer,OMX_U32 nPortIndex)
{
    if(nPortIndex == IN_PORT)
        FreeInputBuffer(buffer);
    if(nPortIndex == OUT_PORT)
        FreeOutputBuffer(buffer);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::FlushComponent(OMX_U32 nPortIndex)
{
    LOG_DEBUG("Flush filter port: %d\n", (int)nPortIndex);

    if(nPortIndex == IN_PORT) {
        FlushInputBuffer();
        tsmFlush(hTsHandle);
        while(InBufferHdrList.GetNodeCnt()>0) {
            OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
            pBufferHdr = InBufferHdrList.GetNode(0);
            InBufferHdrList.Remove(pBufferHdr);
            ports[IN_PORT]->SendBuffer(pBufferHdr);
        }
        ReturnInputBuffer();
        bLastInput = OMX_FALSE;
        bLastOutput = OMX_FALSE;
        nDecodeOnly = 0;
        bGetNewSegment = OMX_TRUE;
    }

    if(nPortIndex == OUT_PORT) {
        FlushOutputBuffer();
        while(1) {
            OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
            pBufferHdr = GetFirstOutBufferHdrFromList();
            if(pBufferHdr == NULL)
                break;
            ports[OUT_PORT]->SendBuffer(pBufferHdr);
        }
        bNeedOutBuffer = DefaultOutputBufferNeeded();
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::ComponentReturnBuffer(OMX_U32 nPortIndex)
{
    if(nPortIndex == IN_PORT) {
        FlushInputBuffer();
        while(InBufferHdrList.GetNodeCnt()>0) {
            OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
            pBufferHdr = InBufferHdrList.GetNode(0);
            InBufferHdrList.Remove(pBufferHdr);
            ports[IN_PORT]->SendBuffer(pBufferHdr);
        }
        ReturnInputBuffer();
    }

    if(nPortIndex == OUT_PORT) {
        FlushOutputBuffer();
        while(1) {
            OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
            pBufferHdr = GetFirstOutBufferHdrFromList();
            if(pBufferHdr == NULL)
                break;
            ports[OUT_PORT]->SendBuffer(pBufferHdr);
        }
        bNeedOutBuffer = DefaultOutputBufferNeeded();
    }

    bInReturnBufferState = OMX_FALSE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::InputFmtChanged()
{
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = IN_PORT;
    ports[IN_PORT]->GetPortDefinition(&sPortDef);

    if(sInFmt.eCompressionFormat != OMX_VIDEO_CodingUnused) {
        //decoder
        fsl_osal_memcpy(&(sPortDef.format.video), &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
        ports[IN_PORT]->SetPortDefinition(&sPortDef);
    }
    else{
        //encoder
        return CheckPortResource(IN_PORT);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::OutputFmtChanged()
{
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = OUT_PORT;
    ports[OUT_PORT]->GetPortDefinition(&sPortDef);

    if(sOutFmt.eCompressionFormat != OMX_VIDEO_CodingUnused) {
        //encoder
        fsl_osal_memcpy(&(sPortDef.format.video), &sOutFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
        if (sPortDef.nBufferSize != nOutBufferSize)
            sPortDef.nBufferSize = nOutBufferSize;
        ports[OUT_PORT]->SetPortDefinition(&sPortDef);
    }
    else{
        //decoder
        sPortDef.nPortIndex = IN_PORT;
        ports[IN_PORT]->GetPortDefinition(&sPortDef);
        sPortDef.format.video.nFrameWidth = sInFmt.nFrameWidth;
        sPortDef.format.video.nFrameHeight = sInFmt.nFrameHeight;
        ports[IN_PORT]->SetPortDefinition(&sPortDef);
        return CheckPortResource(OUT_PORT);
    }

    return OMX_ErrorNone;
}

OMX_TICKS VideoFilter::QueryStreamTs()
{
    return (OMX_TICKS)tsmQueryCurrTs(hTsHandle);
}

OMX_ERRORTYPE VideoFilter::CheckPortResource(OMX_U32 nPortIndex)
{
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_VIDEO_PORTDEFINITIONTYPE *pFmt;
    OMX_U32 nBufferCnt = 0;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = nPortIndex;
    ports[nPortIndex]->GetPortDefinition(&sPortDef);

    if(nPortIndex == IN_PORT) {
        pFmt = &sInFmt;
        nBufferCnt = nInBufferCnt;
    }
    else {
        pFmt = &sOutFmt;
        nBufferCnt = nOutBufferCnt;
    }

    if(pFmt->nFrameWidth != sPortDef.format.video.nFrameWidth
            || pFmt->nFrameHeight != sPortDef.format.video.nFrameHeight
            || nBufferCnt != sPortDef.nBufferCountMin
            || (nPortIndex == OUT_PORT && sPortDef.nBufferSize != nOutBufferSize)) {
        LOG_INFO("Filter port #%d resource changed, need reconfigure.\n", nPortIndex);
        LOG_INFO("from %dx%dx%d to %dx%dx%d.\n",
                sPortDef.format.video.nFrameWidth,
                sPortDef.format.video.nFrameHeight,
                sPortDef.nBufferCountMin,
                pFmt->nFrameWidth, pFmt->nFrameHeight, nBufferCnt);
        bResourceChanged = OMX_TRUE;
        sPortDef.nBufferSize = pFmt->nFrameWidth * pFmt->nFrameHeight * pxlfmt2bpp(pFmt->eColorFormat) / 8;
        if(nPortIndex == OUT_PORT && sPortDef.nBufferSize < nOutBufferSize)
            sPortDef.nBufferSize = nOutBufferSize;
        sPortDef.nBufferCountActual = nBufferCnt;
        sPortDef.nBufferCountMin = nBufferCnt;
        LOG_DEBUG("Need buffer size: %d\n", sPortDef.nBufferSize);
    }

    fsl_osal_memcpy(&(sPortDef.format.video), pFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    ports[nPortIndex]->SetPortDefinition(&sPortDef);

    if(bResourceChanged) {
        tsmClearCachedFrameTs(hTsHandle);
        SendEvent(OMX_EventPortSettingsChanged, nPortIndex, 0, NULL);
        LOG_DEBUG("Send Port setting changed event.\n");
        bInReturnBufferState = OMX_TRUE;
        bResourceChanged = OMX_FALSE;
        return OMX_ErrorNotReady;
    }

    return OMX_ErrorNoMore;
}

OMX_ERRORTYPE VideoFilter::GetInputDataDepthThreshold(OMX_S32* pDurationThr, OMX_S32* pBufCntThr)
{
    /*
      for some application, such rtsp/http, we need to set some thresholds to avoid input data is consumed by decoder too fast.
      -1: no threshold
    */
    *pDurationThr=-1;
    *pBufCntThr=-1;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::GetReturnedInputDataPtr(OMX_PTR* ppInput)
{
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::InitFilterComponent()
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::DeInitFilterComponent()
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::PortFormatChanged(OMX_U32 nPortIndex)
{
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);

    if(nPortIndex == IN_PORT) {
        sPortDef.nPortIndex = IN_PORT;
        ports[IN_PORT]->GetPortDefinition(&sPortDef);
        //fsl_osal_memcpy(&sInFmt, &(sPortDef.format.video), sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
        sInFmt.nFrameWidth = sPortDef.format.video.nFrameWidth;
        sInFmt.nFrameHeight = sPortDef.format.video.nFrameHeight;
        sInFmt.eCompressionFormat = sPortDef.format.video.eCompressionFormat;
        sInFmt.eColorFormat = sPortDef.format.video.eColorFormat;
        sInFmt.xFramerate = sPortDef.format.video.xFramerate;

        // in case encoder's input buffer size is not passed in by SetParameter
        if(sInFmt.eColorFormat != OMX_COLOR_FormatUnused){
            OMX_U32 newBufferSize = MAX(sPortDef.format.video.nStride, (OMX_S32)sPortDef.format.video.nFrameWidth)
                               * MAX(sPortDef.format.video.nSliceHeight, sPortDef.format.video.nFrameHeight)
                               * pxlfmt2bpp(sInFmt.eColorFormat) / 8;
            // testReconfigureWithoutSurface, nBufferSize == 12 means StoreANWBufferInMetadata
            // don't change buffer size or Port::AllocateBuffer will return OMX_ErrorBadParameter
            if(sPortDef.nBufferSize < newBufferSize && sPortDef.nBufferSize != 12){
                printf("PortFormatChanged() adjust nBufferSize of input port: %d->%d",sPortDef.nBufferSize, newBufferSize);
                sPortDef.nBufferSize = newBufferSize;
                ports[IN_PORT]->SetPortDefinition(&sPortDef);
            }
        }

		OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
		sPortDef.nPortIndex = OUT_PORT;
		ports[OUT_PORT]->GetPortDefinition(&sPortDef);
		if (Rotation.nRotation == 90 || Rotation.nRotation == 270) {
			sPortDef.format.video.nFrameWidth = sInFmt.nFrameHeight;
			sPortDef.format.video.nFrameHeight = sInFmt.nFrameWidth;

		} else {
			sPortDef.format.video.nFrameWidth = sInFmt.nFrameWidth;
			sPortDef.format.video.nFrameHeight = sInFmt.nFrameHeight;

		}
		sPortDef.format.video.xFramerate = sInFmt.xFramerate;
                if(sInFmt.eCompressionFormat != OMX_VIDEO_CodingUnused)
                    sPortDef.nBufferSize = sPortDef.format.video.nFrameWidth * sPortDef.format.video.nFrameHeight
                                            * pxlfmt2bpp(sPortDef.format.video.eColorFormat) / 8;
		ports[OUT_PORT]->SetPortDefinition(&sPortDef);

        if(hTsHandle != NULL)
            tsmSetFrmRate(hTsHandle, sInFmt.xFramerate, Q16_SHIFT);
    }

    if(nPortIndex == OUT_PORT) {
        OMX_CONFIG_RECTTYPE sConf;
        sPortDef.nPortIndex = OUT_PORT;
        ports[OUT_PORT]->GetPortDefinition(&sPortDef);
        //fsl_osal_memcpy(&sOutFmt, &(sPortDef.format.video), sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
        sOutFmt.nFrameWidth = sPortDef.format.video.nFrameWidth;
        sOutFmt.nFrameHeight = sPortDef.format.video.nFrameHeight;
        sOutFmt.xFramerate=sPortDef.format.video.xFramerate;
        sOutFmt.nBitrate=sPortDef.format.video.nBitrate;
        sOutFmt.eCompressionFormat = sPortDef.format.video.eCompressionFormat;
        sOutFmt.eColorFormat = sPortDef.format.video.eColorFormat;
        nOutBufferCnt = sPortDef.nBufferCountActual;

        OMX_INIT_STRUCT(&sConf, OMX_CONFIG_RECTTYPE);
        sConf.nPortIndex = OUT_PORT;
        if(OMX_ErrorNone == GetCropInfo(&sConf)){
            if(sConf.nWidth == 0 || sConf.nWidth > sOutFmt.nFrameWidth
                || sConf.nHeight == 0 || sConf.nHeight > sOutFmt.nFrameHeight){
                OMX_INIT_STRUCT(&sConf, OMX_CONFIG_RECTTYPE);
                sConf.nPortIndex = OUT_PORT;
                sConf.nLeft = 0;
                sConf.nTop = 0;
                sConf.nWidth = sOutFmt.nFrameWidth;
                sConf.nHeight = sOutFmt.nFrameHeight;
                SetCropInfo(&sConf);
            }
        }

    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::ProcessDataBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 flags=0;

    if(bInReturnBufferState == OMX_TRUE)
        return OMX_ErrorNoMore;

    ret = ProcessInputBuffer();
    if(ret == OMX_ErrorNotReady)
        return OMX_ErrorNone;
    if(ret != OMX_ErrorNone)
        return ret;

    ret = ProcessOutputBuffer();
    if(ret != OMX_ErrorNone){
        if(InBufferHdrList.GetNodeCnt() + (pInBufferHdr?1:0) >= 2
            && bNeedOutBuffer == OMX_TRUE
            && bThreadedPreProcess == OMX_TRUE)
        {
            // android.media.cts.MediaCodecTest#testReleaseAfterFlush
            // keep checking until only 0 or 1 input buffer stays in vpu component
            // to make sure at least one input buffer can be fetched by upstream surface.
            LOG_DEBUG("Keep checking input buffer from vpu while not encoding, InBufferHdrList %d", InBufferHdrList.GetNodeCnt());
            fsl_osal_sleep(10000);
            ret = OMX_ErrorNone;
        }
        else
            return ret;
    }

    FilterBufRetCode DecRet = FILTER_OK;
    DecRet = FilterOneBuffer();
    if(DecRet & FILTER_INPUT_CONSUMED) {
        DecRet = (FilterBufRetCode)(DecRet & ~FILTER_INPUT_CONSUMED);
        ReturnInputBuffer();
    }
    if(DecRet & FILTER_INPUT_CONSUMED_EXT_READ) {
        DecRet = (FilterBufRetCode)(DecRet & ~FILTER_INPUT_CONSUMED_EXT_READ);
        if(pInBufferHdr){
            InBufferHdrList.Add(pInBufferHdr);
            pInBufferHdr = NULL;
        }
        else{
            //for eos buffer with size=0, pInBufferHdr may be retured aleady before (in ProcessInputBuffer())
        }
    }
    if(DecRet & FILTER_INPUT_CONSUMED_EXT_RETURN) {
        OMX_PTR ptr;
        DecRet = (FilterBufRetCode)(DecRet & ~FILTER_INPUT_CONSUMED_EXT_RETURN);
        GetReturnedInputDataPtr(&ptr);
        //since the list is FIFO, we needn't map ptr and pHdr
        if(InBufferHdrList.GetNodeCnt()>0){
            OMX_BUFFERHEADERTYPE* pHdr;
            pHdr=InBufferHdrList.GetNode(0);
            if(pHdr==NULL){
                LOG_ERROR("warning: get one null hdr from InBufferHdrList !\n");
            }else{
                if(pHdr->pBuffer!=ptr){
                    LOG_ERROR("warning: the address doesn't match between ptr and pHdr->pBuffer !\n");
                }
                InBufferHdrList.Remove(pHdr);
                ports[IN_PORT]->SendBuffer(pHdr);
            }
        }
        else{
            //this path is only for eos
            if((DecRet&FILTER_LAST_OUTPUT)==0){
                LOG_ERROR("warning: the numbers between insert and get doesn't matched !\n");
            }
        }
    }

    if(DecRet & FILTER_ONE_FRM_DECODED){
        OMX_S32 nStuffSize;
        OMX_S32 nFrmSize;
        OMX_PTR pFrm;
        ret=GetDecBuffer(&pFrm, &nStuffSize, &nFrmSize);
        if(ret == OMX_ErrorNone){
            LOG_DEBUG("%s: get one decoded frm: 0x%X(%d,%d) \n",__FUNCTION__,(long)pFrm,(int)nStuffSize,(int)nFrmSize);
            tsmSetFrmBoundary(hTsHandle, nStuffSize, nFrmSize, pFrm);
        }
        else{
            LOG_ERROR("%s: get decoded buffer failure !\n",__FUNCTION__);
        }
        DecRet = (FilterBufRetCode)(DecRet & ~FILTER_ONE_FRM_DECODED);
    }

    switch(DecRet & FILTER_FLAGS_MASK) {
        case FILTER_FLAG_CODEC_DATA:
            flags=OMX_BUFFERFLAG_CODECCONFIG;
            break;
        case FILTER_FLAG_NONKEY_FRAME:
            flags=OMX_BUFFERFLAG_ENDOFFRAME;
            break;
        case FILTER_FLAG_KEY_FRAME:
            flags=OMX_BUFFERFLAG_SYNCFRAME|OMX_BUFFERFLAG_ENDOFFRAME;
            break;
        default:
            flags=0;
            break;
    }
    DecRet = (FilterBufRetCode)(DecRet & ~FILTER_FLAGS_MASK);

    if(DecRet > 0) {
        LOG_DEBUG("DecRet: %d\n", DecRet);
    }

    switch(DecRet) {
        case FILTER_OK:
            break;
        case FILTER_NO_INPUT_BUFFER:
            if(pInBufferHdr != NULL) {
                if (pInBufferHdr->nOffset + pInBufferHdr->nFilledLen + 8 > pInBufferHdr->nAllocLen)
                    LOG_WARNING ("Please ensure allocate more 8 bytes for libav video decoder.");
                SetInputBuffer(pInBufferHdr->pBuffer + pInBufferHdr->nOffset, pInBufferHdr->nFilledLen, bLastInput);
            }
            else
                bNeedInputBuffer = OMX_TRUE;
            break;
        case FILTER_NO_OUTPUT_BUFFER:
            bNeedOutBuffer = OMX_TRUE;
            break;
        case FILTER_DO_INIT:
            ret = InitFilter();
            if(ret == OMX_ErrorNone)
                bInit = OMX_TRUE;
            break;
        case FILTER_LAST_OUTPUT:
            HandleLastOutput(flags);
            ret = OMX_ErrorNoMore;
            break;
        case FILTER_HAS_OUTPUT:
            {
                OMX_PTR pBuffer = NULL;
                OMX_S32 nOutSize=0;
                OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
                GetOutputBuffer(&pBuffer,&nOutSize);
				if (nOutSize == 0) {
					nInvalidFrameCnt ++;
					if (nInvalidFrameCnt <= MOSAIC_COUNT) {
						SetOutputBuffer(pBuffer);	//still need to return it to vpu to avoid the frame is isolated in the pipeline
						tsmGetFrmTs(hTsHandle, NULL);
						break;
					}
				} else {
					nInvalidFrameCnt = 0;
				}
                CheckOutputCropChanged();
                pBufferHdr = GetOutBufferHdrFromList(pBuffer);
                if(pBufferHdr != NULL) {
                    pBufferHdr->nFlags = flags;
                    pBufferHdr->nFilledLen = nOutSize;//pBufferHdr->nAllocLen;
                    ReturnOutputBuffer(pBufferHdr,flags);
                }
                else{
                    SetOutputBuffer(pBuffer);	//still need to return it to vpu to avoid the frame is isolated in the pipeline
                    LOG_ERROR("Can't find related bufferhdr with frame: %p\n", pBuffer);
                }
            }
            break;
        case FILTER_SKIP_OUTPUT:
            tsmGetFrmTs(hTsHandle, NULL);
            break;
        case FILTER_ERROR:
            SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
            ret=OMX_ErrorStreamCorrupt;
            break;
        default:
            break;
    }

    return ret;
}

OMX_ERRORTYPE VideoFilter::ProcessInputBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("Filter In port has #%d buffers.\n", ports[IN_PORT]->BufferNum());

    if(pInBufferHdr == NULL) {
        if(ports[IN_PORT]->BufferNum() > 0) {
            if(OMX_TRUE != tsmHasEnoughSlot(hTsHandle) && bNeedInputBuffer != OMX_TRUE) {
                LOG_DEBUG("No more space to handle input ts.\n");
                if(bInit == OMX_FALSE){
                    LOG_ERROR("init operation timeout, clip is corrupted ! \n");
                    SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
                    return OMX_ErrorStreamCorrupt;
                }
                return OMX_ErrorNone;
            }

            ports[IN_PORT]->GetBuffer(&pInBufferHdr);
            if(pInBufferHdr == NULL)
                return OMX_ErrorUnderflow;

            LOG_DEBUG("Get Inbuffer %p:%d:%lld:%x\n", pInBufferHdr->pBuffer, pInBufferHdr->nFilledLen, pInBufferHdr->nTimeStamp, pInBufferHdr->nFlags);

            ret = ProcessInBufferFlags();
            if(ret != OMX_ErrorNone)
                return ret;

            if (pInBufferHdr->nOffset + pInBufferHdr->nFilledLen + 8 > pInBufferHdr->nAllocLen)
                LOG_WARNING ("Please ensure allocate more 8 bytes for libav video decoder.");
            ret = SetInputBuffer(pInBufferHdr->pBuffer + pInBufferHdr->nOffset, pInBufferHdr->nFilledLen, bLastInput);
            if(ret != OMX_ErrorNone) {
                ReturnInputBuffer();
                return ret;
            }

            if(pInBufferHdr->nFilledLen == 0) {
                ReturnInputBuffer();
                return OMX_ErrorNone;
            }

            bNeedInputBuffer = OMX_FALSE;

            tsmSetBlkTs(hTsHandle, pInBufferHdr->nFilledLen, pInBufferHdr->nTimeStamp);
        }
        else {
            if(bNeedInputBuffer == OMX_TRUE)
                return OMX_ErrorNoMore;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::ProcessOutputBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(bLastOutput == OMX_TRUE)
        return OMX_ErrorNoMore;

    LOG_DEBUG("Filter Out port has #%d buffers.\n", ports[OUT_PORT]->BufferNum());

    if(bNeedOutBuffer != OMX_FALSE) {
        if(ports[OUT_PORT]->BufferNum() > 0) {
            OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
            ports[OUT_PORT]->GetBuffer(&pBufferHdr);
            if(pBufferHdr == NULL)
                return OMX_ErrorUnderflow;
/*
            if(bNewSegment == OMX_TRUE && bLastInput == OMX_TRUE) {
                bLastOutput = OMX_TRUE;
                pBufferHdr->nFlags = OMX_BUFFERFLAG_EOS | OMX_BUFFERFLAG_STARTTIME;
                pBufferHdr->nTimeStamp = tsmGetFrmTs(hTsHandle, NULL);
                ports[OUT_PORT]->SendBuffer(pBufferHdr);
                return OMX_ErrorNoMore;
            }
*/
            ret = SetOutputBuffer(pBufferHdr->pBuffer);
            if(ret != OMX_ErrorNone) {
                pBufferHdr->nFilledLen = 0;
                ports[OUT_PORT]->SendBuffer(pBufferHdr);
                bNeedOutBuffer = OMX_FALSE;
                return OMX_ErrorNone;
            }

            LOG_DEBUG("Get Outbuffer: %p\n", pBufferHdr->pBuffer);



            OutBufferHdrList.Add(pBufferHdr);
            bNeedOutBuffer = OMX_FALSE;
        }
        else {
            OMX_S32 nCnt = OutBufferHdrList.GetNodeCnt();
            LOG_DEBUG("No more OutBuffer, #%d buffers holded by filter.\n", nCnt);
            return OMX_ErrorNoMore;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::DoLoaded2Idle()
{
    hTsHandle = tsmCreate();
    if(hTsHandle == NULL) {
        LOG_ERROR("Create Ts manager failed.\n");
        return OMX_ErrorUndefined;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::DoIdle2Loaded()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(bInit == OMX_TRUE)
        DeInitFilter();

    if(pCodecData != NULL)
        FSL_FREE(pCodecData);

    tsmDestroy(hTsHandle);

    if(pImageConvert)
        pImageConvert->delete_it(pImageConvert);

    ret=SetDefaultSetting(); //restore default to support following switch from loaded to idle
    if(ret != OMX_ErrorNone)
        return ret;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::ProcessInBufferFlags()
{
    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTIME)
        bGetNewSegment = OMX_TRUE;

    if(bGetNewSegment == OMX_TRUE && (!(pInBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG))) {
        OMX_S32 nDurationThr,nBufCntThr;
        LOG_DEBUG("Get New sement buffer, ts %lld\n", pInBufferHdr->nTimeStamp);
        bGetNewSegment = OMX_FALSE;
        bNewSegment = OMX_TRUE;
		bLastInput = OMX_FALSE;
        bLastOutput = OMX_FALSE;
		nInvalidFrameCnt = 0;
        if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_STARTTRICK) {
            LOG_DEBUG("Set ts manager to FIFO mode.\n");
            tsmReSync(hTsHandle, pInBufferHdr->nTimeStamp, MODE_FIFO);
        }
        else {
            LOG_DEBUG("Set ts manager to AI mode.\n");
            tsmReSync(hTsHandle, pInBufferHdr->nTimeStamp, MODE_AI);
        }
        GetInputDataDepthThreshold(&nDurationThr, &nBufCntThr);
        LOG_INFO("nDurationThr: %d, nBufCntThr: %d\n", nDurationThr, nBufCntThr);
        tsmSetDataDepthThreshold(hTsHandle, nDurationThr, nBufCntThr);
    }

    if(bLastInput != OMX_FALSE) {
        LOG_DEBUG("Filter drop input buffers in EOS state.\n");
        ReturnInputBuffer();
        return OMX_ErrorNoMore;
    }

    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_EOS) {
        bLastInput = OMX_TRUE;
        bNeedInputBuffer = OMX_FALSE;
        return OMX_ErrorNone;
    }

    if(!(pInBufferHdr->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) && bFilterSupportPartilInput != OMX_TRUE) {
        ProcessPartialInput();
        return OMX_ErrorNotReady;
    }

    if(PartialInputHdr.pBuffer != NULL) {
        ProcessPartialInput();
        pInBufferHdr = &PartialInputHdr;
    }

    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_DECODEONLY)
        nDecodeOnly ++;

    if(pInBufferHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG && pInBufferHdr->nFilledLen > 0 && !bEnableAndroidNativeHandleBuffer) {
        if(pCodecData == NULL) {
            pCodecData = FSL_MALLOC(pInBufferHdr->nFilledLen);
            if(pCodecData == NULL) {
                SendEvent(OMX_EventError, OMX_ErrorInsufficientResources, 0, NULL);
                return OMX_ErrorInsufficientResources;
            }
        }
        else {
            pCodecData = FSL_REALLOC(pCodecData, nCodecDataLen + pInBufferHdr->nFilledLen);
            if(pCodecData == NULL) {
                SendEvent(OMX_EventError, OMX_ErrorInsufficientResources, 0, NULL);
                return OMX_ErrorInsufficientResources;
            }
        }
        fsl_osal_memcpy((char *)pCodecData + nCodecDataLen, pInBufferHdr->pBuffer, pInBufferHdr->nFilledLen);
        nCodecDataLen += pInBufferHdr->nFilledLen;
        LOG_INFO("Get Codec configure data, len: %d\n", nCodecDataLen);
        ReturnInputBuffer();
        return OMX_ErrorNotReady;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::ProcessPartialInput()
{
    if(PartialInputHdr.pBuffer == NULL) {
        PartialInputHdr.pBuffer = (OMX_U8*)AllocateInputBuffer(2*pInBufferHdr->nAllocLen);
        if(PartialInputHdr.pBuffer == NULL) {
            LOG_WARNING("Allocate buffer for partial input failed, size: %d.\n", 2*pInBufferHdr->nAllocLen);
            return OMX_ErrorInsufficientResources;
        }
        PartialInputHdr.nAllocLen = 2*pInBufferHdr->nAllocLen;
        PartialInputHdr.nFlags = pInBufferHdr->nFlags;
        PartialInputHdr.nTimeStamp = pInBufferHdr->nTimeStamp;
    }
    else {
        if(PartialInputHdr.nAllocLen - PartialInputHdr.nFilledLen < pInBufferHdr->nFilledLen) {
            OMX_PTR pBuffer = NULL;
            pBuffer = AllocateInputBuffer(2*PartialInputHdr.nAllocLen);
            if(pBuffer == NULL) {
                LOG_WARNING("Allocate buffer for partial input failed, size: %d.\n", PartialInputHdr.nAllocLen);
                return OMX_ErrorInsufficientResources;
            }
            fsl_osal_memcpy(pBuffer, PartialInputHdr.pBuffer, PartialInputHdr.nFilledLen);
            FreeInputBuffer(PartialInputHdr.pBuffer);
            PartialInputHdr.pBuffer = (OMX_U8 *)pBuffer;
            PartialInputHdr.nAllocLen *= 2;
        }
    }

    fsl_osal_memcpy(PartialInputHdr.pBuffer + PartialInputHdr.nFilledLen, pInBufferHdr->pBuffer, pInBufferHdr->nFilledLen);
    PartialInputHdr.nFilledLen += pInBufferHdr->nFilledLen;
    ports[IN_PORT]->SendBuffer(pInBufferHdr);
    pInBufferHdr = NULL;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::ReturnInputBuffer()
{
    if(PartialInputHdr.pBuffer != NULL) {
        FreeInputBuffer(PartialInputHdr.pBuffer);
        fsl_osal_memset(&PartialInputHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));
        pInBufferHdr = NULL;
    }
    else if(pInBufferHdr != NULL) {
        ports[IN_PORT]->SendBuffer(pInBufferHdr);
        pInBufferHdr = NULL;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::ReturnOutputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr,OMX_U32 flags)
{
    OMX_PTR pMappedOut=NULL;
    if(nDecodeOnly > 0) {
        LOG_INFO("VideoFilter drop frame as decode only: %d\n", nDecodeOnly);
        nDecodeOnly --;
        pBufferHdr->nFilledLen = 0;
        if(bNeedMapDecAndOutput){
            GetPostMappedDecBuffer((OMX_PTR)pBufferHdr->pBuffer,(OMX_PTR*)(&pMappedOut));
            tsmGetFrmTs(hTsHandle, pMappedOut);
        }
        else{
            tsmGetFrmTs(hTsHandle, pBufferHdr->pBuffer);
        }
        SetOutputBuffer(pBufferHdr->pBuffer);
        OutBufferHdrList.Add(pBufferHdr);
        return OMX_ErrorNone;
    }

    if(bNewSegment == OMX_TRUE) {
        pBufferHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        bNewSegment = OMX_FALSE;
    }

    pBufferHdr->nOffset = 0;
    if(flags!=OMX_BUFFERFLAG_CODECCONFIG){   //check for encoder
        if(bNeedMapDecAndOutput){
            GetPostMappedDecBuffer((OMX_PTR)pBufferHdr->pBuffer,(OMX_PTR*)(&pMappedOut));
            pBufferHdr->nTimeStamp = tsmGetFrmTs(hTsHandle, pMappedOut);
        }
        else{
            pBufferHdr->nTimeStamp = tsmGetFrmTs(hTsHandle, pBufferHdr->pBuffer);
        }
    }

    LOG_DEBUG("VideoDecoder send bufer: %p:%lld:%x\n", pBufferHdr->pBuffer, pBufferHdr->nTimeStamp, pBufferHdr->nFlags);

    if(0){
        static int iiCnt=0;
        static FILE *pfTest = NULL;
        iiCnt ++;
        if (iiCnt==1) {
            pfTest = fopen("/sdcard/DumpData.yuv", "wb");
            if(pfTest == NULL)
                printf("Unable to open test file! \n");
        }
        if(iiCnt > 0 && pfTest != NULL) {
            if(pBufferHdr->nFilledLen > 0)  {
                printf("dump data %d\n", pBufferHdr->nFilledLen);
                fwrite(pBufferHdr->pBuffer, sizeof(char), pBufferHdr->nFilledLen, pfTest);
                fflush(pfTest);
                fclose(pfTest);
                pfTest = NULL;
           }
         }
    }

    ports[OUT_PORT]->SendBuffer(pBufferHdr);

    return OMX_ErrorNone;
}

OMX_BUFFERHEADERTYPE* VideoFilter::GetOutBufferHdrFromList(OMX_PTR pBuffer)
{
    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
    OMX_U32 nBufferHdr = 0;

    if(pBuffer == NULL)
        return NULL;

    nBufferHdr = OutBufferHdrList.GetNodeCnt();

    OMX_S32 i;
    OMX_BOOL bFound = OMX_FALSE;
    for(i=0; i<(int)nBufferHdr; i++) {
        pBufferHdr = OutBufferHdrList.GetNode(i);
        if(pBufferHdr == NULL) {
            LOG_ERROR("VideoFilter outbuffer list has one NULL buffer.\n");
            continue;
        }
        if(pBufferHdr->pBuffer == pBuffer) {
            bFound = OMX_TRUE;
            break;
        }
    }

    if(bFound != OMX_TRUE)
        return NULL;

    LOG_DEBUG("Remove buffer %p from list.\n", pBufferHdr->pBuffer);
    OutBufferHdrList.Remove(pBufferHdr);

    return pBufferHdr;
}

OMX_BUFFERHEADERTYPE * VideoFilter::GetFirstOutBufferHdrFromList()
{
    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
    OMX_U32 nBufferHdr = 0;

    nBufferHdr = OutBufferHdrList.GetNodeCnt();
    if(nBufferHdr == 0)
        return NULL;

    pBufferHdr = OutBufferHdrList.GetNode(0);
    if(pBufferHdr == NULL){
        LOG_ERROR("VideoFilter outbuffer list has one NULL buffer.\n");
        return NULL;
    }
    LOG_DEBUG("Remove buffer %p from list.\n", pBufferHdr->pBuffer);
    OutBufferHdrList.Remove(pBufferHdr);

    return pBufferHdr;
}

OMX_ERRORTYPE VideoFilter::HandleLastOutput(OMX_U32 flags)
{
    OMX_PTR pBuffer = NULL;
    OMX_S32 nOutSize=0;
    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;

    bLastOutput = OMX_TRUE;
    GetOutputBuffer(&pBuffer,&nOutSize);
    if(pBuffer != NULL)
        pBufferHdr = GetOutBufferHdrFromList(pBuffer);

    if(pBuffer == NULL || pBufferHdr == NULL) {
        pBufferHdr = GetFirstOutBufferHdrFromList();
        if(pBufferHdr == NULL) {
            LOG_ERROR("No buffer holded by VideoFilter.\n");
            return OMX_ErrorUndefined;
        }
    }

    pBufferHdr->nFilledLen = nOutSize;//0;
    pBufferHdr->nFlags = OMX_BUFFERFLAG_EOS|flags;
    ReturnOutputBuffer(pBufferHdr,flags);
    LOG_INFO("VideoFilter send last output frame.\n");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::GetDecBuffer(OMX_PTR *ppBuffer,OMX_S32 * pOutStuffSize,OMX_S32* pOutFrmSize)
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE VideoFilter::GetPostMappedDecBuffer(OMX_PTR pPostBuf,OMX_PTR *ppDecBuffer)
{
    return OMX_ErrorNone;
}

OMX_PTR VideoFilter::AllocateInputBuffer(OMX_U32 nSize)
{
    return FSL_MALLOC(nSize);
}

OMX_ERRORTYPE VideoFilter::FreeInputBuffer(OMX_PTR pBuffer)
{
    FSL_FREE(pBuffer);
    return OMX_ErrorNone;
}

OMX_PTR VideoFilter::AllocateOutputBuffer(OMX_U32 nSize)
{
    return FSL_MALLOC(nSize);
}

OMX_ERRORTYPE VideoFilter::FreeOutputBuffer(OMX_PTR pBuffer)
{
    FSL_FREE(pBuffer);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE VideoFilter::CheckOutputCropChanged()
{
    OMX_CONFIG_RECTTYPE recConf;
    recConf.nPortIndex = OUT_PORT;

    if(OMX_ErrorNone == GetCropInfo(&recConf)){
        if(outputCrop.nLeft == 0 && outputCrop.nTop== 0 && outputCrop.nWidth == 0
            && outputCrop.nHeight == 0){
            fsl_osal_memcpy(&outputCrop,&recConf,sizeof(OMX_CONFIG_RECTTYPE));
        }
        else if(outputCrop.nLeft != recConf.nLeft || outputCrop.nTop != recConf.nTop ||
            outputCrop.nWidth != recConf.nWidth || outputCrop.nHeight!= recConf.nHeight){
            fsl_osal_memcpy(&outputCrop,&recConf,sizeof(OMX_CONFIG_RECTTYPE));
            SendEvent(OMX_EventPortSettingsChanged, OUT_PORT, OMX_IndexConfigCommonOutputCrop, NULL);
        }else if(bUpdateColorAspects){
            SendEvent(OMX_EventPortSettingsChanged, OUT_PORT, OMX_IndexConfigDescribeColorInfo, NULL);
            bUpdateColorAspects = OMX_FALSE;
        }
    }
    return OMX_ErrorNone;
}
OMX_ERRORTYPE VideoFilter::GetCropInfo(OMX_CONFIG_RECTTYPE *sCrop)
{
    return OMX_ErrorNotImplemented;
}
OMX_ERRORTYPE VideoFilter::SetCropInfo(OMX_CONFIG_RECTTYPE *sCrop)
{
    return OMX_ErrorNotImplemented;
}
OMX_ERRORTYPE VideoFilter::SetMediaTime(OMX_S64 ts, OMX_S32 scale)
{
    fsl_osal_timeval time;

    if(E_FSL_OSAL_SUCCESS != fsl_osal_systime(&time))
        return OMX_ErrorUndefined;

    nAnchorTime = (OMX_S64)time.sec * 1000000 + time.usec;
    nMediaTime = ts;
    nScale = scale;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE VideoFilter::GetMediaTime(OMX_S64 *ts)
{
    fsl_osal_timeval time;
    OMX_S64 now;
    OMX_S64 position;
    if(ts == NULL){
        return OMX_ErrorBadParameter;
    }
    if(nMediaTime <= 0){
        return OMX_ErrorNotReady;
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_systime(&time))
        return OMX_ErrorUndefined;

    now = (OMX_S64)time.sec * 1000000 + time.usec;
    if(now < nAnchorTime){
        return OMX_ErrorUndefined;
    }
    //nu player will call SetMediaTime() function every second, in playing state, current media time will be in the range.
    //if not, it means nuplayer is not in playing state.
    if(now < nAnchorTime + 2000000 * (OMX_S64)nScale / Q16_SHIFT){
        position = (now - nAnchorTime) * nScale / Q16_SHIFT + nMediaTime;
        *ts = position;
        return OMX_ErrorNone;
    }else{
        return OMX_ErrorNotReady;
    }
}
OMX_BOOL VideoFilter::DefaultOutputBufferNeeded()
{
    return OMX_TRUE;
}
/* File EOF */
