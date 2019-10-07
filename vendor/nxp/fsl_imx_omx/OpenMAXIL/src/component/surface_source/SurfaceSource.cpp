/**
 *  Copyright (c) 2015-2016, Freescale Semiconductor, Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "cutils/log.h"
#include "gralloc_priv.h"
#include "binder/IPCThreadState.h"
#include "media/hardware/MetadataBufferType.h"
#include "SurfaceSource.h"
#include "gui/IGraphicBufferProducer.h"
#include "utils/String16.h"
#include "ui/Fence.h"
#include <android/log.h>


#define NO_ERROR 0

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_WARNING
#define LOG_WARNING printf
#endif

namespace android {

SurfaceSource::SurfaceSource()
{
	OMX_U32 i;

    LOG_DEBUG("%s", __FUNCTION__);

	fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_source.surface.sw-based");
	ComponentVersion.s.nVersionMajor = 0x0;
	ComponentVersion.s.nVersionMinor = 0x1;
	ComponentVersion.s.nRevision = 0x0;
	ComponentVersion.s.nStep = 0x0;
	role_cnt = 1;
	role[0] = (OMX_STRING)"video_source.surface";

	nFrameBufferMin = 1;
	nFrameBufferActual = 3;
	nFrameBuffer = nFrameBufferActual;
	PreviewPortSupplierType = OMX_BufferSupplyInput;
	CapturePortSupplierType = OMX_BufferSupplyOutput;
	fsl_osal_memset(&sVideoFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
	sVideoFmt.nFrameWidth = 176;
	sVideoFmt.nFrameHeight = 144;
	sVideoFmt.eColorFormat = OMX_COLOR_Format32bitRGBA8888;
	sVideoFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

	lock = NULL;
    mListener = NULL;
    nFramesAvailable = 0;
    bStoreMetaData = OMX_TRUE;
    bFormatSent = OMX_FALSE;
    //mBufferConsumer = NULL;
    mIsPersistent = OMX_FALSE;
    nCurTS = 0L;

}

SurfaceSource::~SurfaceSource()
{
    LOG_DEBUG("%s", __FUNCTION__);

    //LOG_DEBUG("proxy %p strong count %d", mListener->proxy.get(),
    //    (static_cast<RefBase*>(mListener->proxy.get()))->getStrongCount());

}

OMX_ERRORTYPE SurfaceSource::InitSourceComponent()
{
    LOG_DEBUG("%s", __FUNCTION__);

	if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
		LOG_ERROR("Create mutex for camera device failed.\n");
		return OMX_ErrorInsufficientResources;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceSource::DeInitSourceComponent()
{
	if(lock)
		fsl_osal_mutex_destroy(lock);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceSource::PortFormatChanged(OMX_U32 nPortIndex)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_BOOL bFmtChanged = OMX_FALSE;

    LOG_DEBUG("%s port %d", __FUNCTION__, nPortIndex);

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = nPortIndex;
    ports[nPortIndex]->GetPortDefinition(&sPortDef);

    if (bStoreMetaData == OMX_FALSE) {
        LOG_ERROR("SurfaceSource shall set bStoreMetaData true!!!");
        return OMX_ErrorUndefined;
    }

	if(nPortIndex == CAPTURED_FRAME_PORT)
	{
		if(sVideoFmt.nFrameWidth != sPortDef.format.video.nFrameWidth) {
			sVideoFmt.nFrameWidth = sPortDef.format.video.nFrameWidth;
			bFmtChanged = OMX_TRUE;
		}
		if(sVideoFmt.nFrameHeight != sPortDef.format.video.nFrameHeight) {
			sVideoFmt.nFrameHeight = sPortDef.format.video.nFrameHeight;
			bFmtChanged = OMX_TRUE;
		}
		if(sVideoFmt.eColorFormat != sPortDef.format.video.eColorFormat) {
			sVideoFmt.eColorFormat = sPortDef.format.video.eColorFormat;
			bFmtChanged = OMX_TRUE;
		}
		if(nFrameBuffer != sPortDef.nBufferCountActual) {
			nFrameBuffer = sPortDef.nBufferCountActual;
			bFmtChanged = OMX_TRUE;
		}
        sVideoFmt.xFramerate = sPortDef.format.video.xFramerate;

		if(bFmtChanged) {
            LOG_DEBUG("format changed : width: %d height: %d, buffer count %d, color 0x%x\n",
                    sPortDef.format.video.nFrameWidth,
                    sPortDef.format.video.nFrameHeight,
                    sPortDef.nBufferCountActual,
                    sPortDef.format.video.eColorFormat);
			CloseDevice();
			OpenDevice();
		}

	}

    return ret;
}

OMX_ERRORTYPE SurfaceSource::InitBuffers()
{
    String8 name("GraphicBufferSource");
    status_t mInitCheck;

    LOG_DEBUG("%s mConsumer is %s, mProducer is %s", __FUNCTION__,
        mConsumer == NULL ? "NULL" : "not NULL",
        mProducer == NULL ? "NULL" : "not NULL");

    if(mProducer != NULL){
        return OMX_ErrorNone;
    }

    if(mConsumer == NULL){
        LOG_DEBUG("createBufferQueue() create producer and consumer");
        BufferQueue::createBufferQueue(&mProducer, &mConsumer);

        mConsumer->setConsumerName(name);
        mConsumer->setConsumerUsageBits(GRALLOC_USAGE_HW_VIDEO_ENCODER);

        mInitCheck = mConsumer->setMaxAcquiredBufferCount(nFrameBuffer);
        if (mInitCheck != NO_ERROR) {
            LOG_ERROR("Unable to set BQ max acquired buffer count to %u: %d", nFrameBuffer);
            return OMX_ErrorUndefined;
        }
    }
    else{
        mIsPersistent = OMX_TRUE;
    }


    LOG_DEBUG("mConsumer %p set buffer size %d/%d, count %d",mConsumer.get(), sVideoFmt.nFrameWidth, sVideoFmt.nFrameHeight, nFrameBuffer);

    mConsumer->setDefaultBufferSize(sVideoFmt.nFrameWidth, sVideoFmt.nFrameHeight);

    wp<BufferQueue::ConsumerListener> listener = static_cast<BufferQueue::ConsumerListener*>(mListener.get());

    sp<IConsumerListener> proxy;

    /*
    if (!mIsPersistent) {
        proxy = new BufferQueue::ProxyConsumerListener(listener);
    } else {
        proxy = new PersisProxyListener(mConsumer, listener);
    }
    */
    proxy = new BufferQueue::ProxyConsumerListener(listener);

    mInitCheck = mConsumer->consumerConnect(proxy, false);
    if (mInitCheck != NO_ERROR) {
        LOG_ERROR("Error connecting to BufferQueue: %s (%d)",
                strerror(-mInitCheck), mInitCheck);
        return OMX_ErrorUndefined;
    }

    LOG_DEBUG("InitBuffers GraphicBufferListener %p", this);
    LOG_DEBUG("InitBuffers mConsumer %p", mConsumer.get());
    LOG_DEBUG("InitBuffers proxy %p strong count %d", proxy.get(),
        (static_cast<RefBase*>(proxy.get()))->getStrongCount());

    return OMX_ErrorNone;

}

OMX_ERRORTYPE SurfaceSource::DeinitBuffers()
{
    LOG_DEBUG("%s", __FUNCTION__);

    LOG_DEBUG("remaining encoding buffers %d",mEncodingBuffers.size());

    while(mEncodingBuffers.size() > 0){
        ReleaseBuffer(mEncodingBuffers[0].mBuf,
                    mEncodingBuffers[0].nFrameNumber,
                    mEncodingBuffers[0].spGraphicBuffer);

        LOG_DEBUG("    released buffer mBuf %d, num %d", mEncodingBuffers[0].mBuf, mEncodingBuffers[0].nFrameNumber);

        mEncodingBuffers.removeItemsAt(0);
    }

    if (mConsumer != NULL && !mIsPersistent) {
        LOG_DEBUG("consumerDisconnect");
        status_t err = mConsumer->consumerDisconnect();
        if (err != NO_ERROR) {
            LOG_ERROR("consumerDisconnect failed: %d", err);
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceSource::OpenDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("%s mListener is %s", __FUNCTION__, (mListener == NULL) ? "NULL" : "Not NULL");

    if(mListener == NULL){
        GraphicBufferListener *listener = FSL_NEW(GraphicBufferListener, (this));
        mListener = listener;
    }

    InitBuffers();

	return ret;
}

OMX_ERRORTYPE SurfaceSource::CloseDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("%s", __FUNCTION__);

    DeinitBuffers();

	return ret;
}

OMX_ERRORTYPE SurfaceSource::SetSensorMode()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE SurfaceSource::SetVideoFormat()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE SurfaceSource::SetWhiteBalance()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE SurfaceSource::SetDigitalZoom()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE SurfaceSource::SetExposureValue()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE SurfaceSource::SetRotation()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	return ret;
}

OMX_ERRORTYPE SurfaceSource::StartDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}


OMX_ERRORTYPE SurfaceSource::StopDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE SurfaceSource::SendBufferToDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 nIndex = 0;
    OMX_PTR pBuffer;

    fsl_osal_mutex_lock(lock);

    LOG_DEBUG("%s", __FUNCTION__);

    pBuffer = ((METADATA_BUFFER *)(pOutBufferHdr->pBuffer))->pPhysicAddress;

    if(pBuffer == NULL){
        LOG_DEBUG("    pPhysicAddress is null");
        fsl_osal_mutex_unlock(lock);
        return ret;
    }

	ret = SearchBuffer(pBuffer, &nIndex);
    if (ret != OMX_ErrorNone){
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorNone;
    }

    //mListener->mConsumer->releaseBuffer(mEncodingBuffers[nIndex].mBuf, mEncodingBuffers[nIndex].nFrameNumber,
    //        EGL_NO_DISPLAY, EGL_NO_SYNC_KHR, Fence::NO_FENCE);
    ReleaseBuffer(mEncodingBuffers[nIndex].mBuf,
                mEncodingBuffers[nIndex].nFrameNumber,
                mEncodingBuffers[nIndex].spGraphicBuffer);

    LOG_DEBUG("    released buffer mBuf %d, num %d", mEncodingBuffers[nIndex].mBuf, mEncodingBuffers[nIndex].nFrameNumber);

    mEncodingBuffers.removeItemsAt(nIndex);

    fsl_osal_mutex_unlock(lock);

	return ret;
}

OMX_ERRORTYPE SurfaceSource::ReleaseBuffer(int frame_id, uint64_t frame_num,
    const sp<GraphicBuffer> buffer)
{
    if(!mIsPersistent) {
        mConsumer->releaseBuffer(frame_id, frame_num, EGL_NO_DISPLAY, 
            EGL_NO_SYNC_KHR, Fence::NO_FENCE);
    } else {
        mConsumer->detachBuffer(frame_id);

        if (OK == mConsumer->attachBuffer(&frame_id, buffer))
            mConsumer->releaseBuffer(frame_id, 0, EGL_NO_DISPLAY, 
                EGL_NO_SYNC_KHR, Fence::NO_FENCE);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceSource::GetOneFrameFromDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
    //LOG_DEBUG("%s", __FUNCTION__);

    while(nFramesAvailable <= 0){
        LOG_DEBUG("    no available frame!");
        fsl_osal_sleep(10000);
        if (OMX_TRUE == bHasCmdToProcess() || EOS.bEnabled == OMX_TRUE) {
            ((METADATA_BUFFER *)(pOutBufferHdr->pBuffer))->pPhysicAddress = 0;
            pOutBufferHdr->nFilledLen = 0;
            pOutBufferHdr->nTimeStamp = INVALID_TS;
            return ret;
        }
    }
    fsl_osal_mutex_lock(lock);

    BufferItem item;
    status_t err = mConsumer->acquireBuffer(&item, 0);
    if (err == BufferQueue::NO_BUFFER_AVAILABLE) {
        LOG_ERROR("GetOneFrameFromDevice: frame is not available");
        fsl_osal_mutex_unlock(lock);
		return OMX_ErrorUndefined;
    } else if (err != OK) {
        LOG_ERROR("GetOneFrameFromDevice: acquireBuffer returned err=%d", err);
        fsl_osal_mutex_unlock(lock);
		return OMX_ErrorUndefined;
    }

    nFramesAvailable--;

    err = item.mFence->waitForever("SurfaceSource::GetOneFrameFromDevice");
    if (err != OK) {
        LOG_ERROR("failed to wait for buffer fence: %d", err);
        fsl_osal_mutex_unlock(lock);
		return OMX_ErrorUndefined;
    }

    LOG_DEBUG("acquired buffer slot %d, num %d, ts %lld, rest buf %d",
             item.mBuf,item.mFrameNumber, item.mTimestamp/1000, nFramesAvailable);

    // If this is the first time we're seeing this buffer, add it to slot table.
    if (item.mGraphicBuffer != NULL) {
        LOG_INFO("    setting mBufferSlot %d", item.mBuf);
        mBufferSlot[item.mBuf] = item.mGraphicBuffer;
    }

    EncodingBuffer encbuf;
    encbuf.nFrameNumber = item.mFrameNumber;
    encbuf.spGraphicBuffer = mBufferSlot[item.mBuf];
    encbuf.mBuf = item.mBuf;
    mEncodingBuffers.add(encbuf);

    GraphicBuffer *buffer = encbuf.spGraphicBuffer.get();
    private_handle_t* pGrallocHandle = (private_handle_t*)buffer->handle;

    LOG_DEBUG("buffer format is 0x%x, phys %p, base %p, size %d, stride %d, %d/%d",
        pGrallocHandle->format, pGrallocHandle->phys, pGrallocHandle->base, pGrallocHandle->size,
        pGrallocHandle->stride, pGrallocHandle->width, pGrallocHandle->height);

    if(0){ // debug
        static int iiCnt=0;
        static FILE *pfTest = NULL;
        iiCnt ++;
        if (iiCnt==1) {
            pfTest = fopen("/data/DumpData.rgb", "wb");
            if(pfTest == NULL)
                printf("Unable to open test file! \n");
        }
        if(iiCnt == 5 && pfTest != NULL) {
            if(pGrallocHandle->size > 0)  {
                printf("dump data %d\n", pGrallocHandle->size );
                fwrite((void *)pGrallocHandle->base, sizeof(char), pGrallocHandle->size, pfTest);
                fflush(pfTest);
                fclose(pfTest);
                pfTest = NULL;
           }
         }
    }

    if(bFormatSent == OMX_FALSE){

        OMX_COLOR_FORMATTYPE colorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
        switch(pGrallocHandle->format) {
            case HAL_PIXEL_FORMAT_YCbCr_420_SP:
                colorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
                break;
            case HAL_PIXEL_FORMAT_YCbCr_420_P:
                colorFormat = OMX_COLOR_FormatYUV420Planar;
                break;
            case HAL_PIXEL_FORMAT_RGBA_8888:
            case HAL_PIXEL_FORMAT_RGBX_8888:
                colorFormat = OMX_COLOR_Format32bitRGBA8888;
                break;
            default:
                LOG_ERROR("Not supported color format %d!", pGrallocHandle->format);
                break;
        }
        LOG_DEBUG("colorFormat is %x", colorFormat);

        OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
		OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
		sPortDef.nPortIndex = CAPTURED_FRAME_PORT;
		ports[CAPTURED_FRAME_PORT]->GetPortDefinition(&sPortDef);
        sPortDef.format.video.eColorFormat = colorFormat;
		ports[CAPTURED_FRAME_PORT]->SetPortDefinition(&sPortDef);

        ComponentBase::SendEvent(OMX_EventPortSettingsChanged,
            CAPTURED_FRAME_PORT, 0, NULL);

        // first PortSettingEvent will be catched by GMComponent. Need to send second event
        // to let GMRecorder get it and set to Video Encoder.
        ComponentBase::SendEvent(OMX_EventPortSettingsChanged,
            CAPTURED_FRAME_PORT, 0, NULL);

        bFormatSent = OMX_TRUE;
    }

    ((METADATA_BUFFER *)(pOutBufferHdr->pBuffer))->pPhysicAddress = \
        (OMX_PTR) pGrallocHandle->phys;
    pOutBufferHdr->nOffset = 0;
    pOutBufferHdr->nFilledLen = 4;
    pOutBufferHdr->nTimeStamp = item.mTimestamp / 1000;;

	nCurTS = pOutBufferHdr->nTimeStamp;

    //LOG_DEBUG("%s end", __FUNCTION__);

    fsl_osal_mutex_unlock(lock);
	return ret;
}

OMX_TICKS SurfaceSource::GetDelayofFrame()
{
	int64_t readTimeUs = systemTime() / 1000;

	return readTimeUs - nCurTS;
}

OMX_TICKS SurfaceSource::GetFrameTimeStamp()
{
    return nCurTS;
}

OMX_ERRORTYPE SurfaceSource::SearchBuffer(
    OMX_PTR pAddr,
    OMX_U32 *pIndex)
{
    OMX_U32 i = 0;
    GraphicBuffer *buffer;
    private_handle_t* pGrallocHandle;

    //LOG_DEBUG("mCodecBuffers size is %d", mCodecBuffers.size());

    for (i = 0; i < mEncodingBuffers.size() - 1; i++) {
        buffer = mEncodingBuffers[i].spGraphicBuffer.get();
        pGrallocHandle = (private_handle_t*)buffer->handle;
        if(pAddr == (OMX_PTR) pGrallocHandle->phys)
            break;
    }

    if(i >= mEncodingBuffers.size()){
        LOG_ERROR("    can't find physical buffer %p in mEncodingBuffers[]", pAddr);
        return OMX_ErrorUndefined;
    }

    *pIndex = i;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceSource::InstanceGetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("%s index 0x%x", __FUNCTION__ ,(int)nParamIndex);

    switch ((int)nParamIndex) {
		case OMX_IndexParamBufferProducer:
            OMX_PARAM_BUFFER_PRODUCER *pBufferProducer;

            if(mProducer == NULL){
                LOG_ERROR("mProducer is NULL, can't get it from SurfaceSource");
                ret = OMX_ErrorUndefined;
                break;
            }
            pBufferProducer = (OMX_PARAM_BUFFER_PRODUCER*)pStructure;
            OMX_CHECK_STRUCT(pBufferProducer, OMX_PARAM_BUFFER_PRODUCER, ret);
            pBufferProducer->pBufferProducer = (OMX_PTR)&(mProducer);
            LOG_DEBUG("pBufferProducer is %p",pBufferProducer->pBufferProducer);
            break;

        default:
            LOG_DEBUG("OMX_ErrorUnsupportedIndex %d\n", nParamIndex);
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }
    return ret;
}

OMX_ERRORTYPE SurfaceSource::InstanceSetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
		case OMX_IndexParamBufferConsumer:
            {
            LOG_DEBUG("SurfaceSource SetParam bufferConsumer %p", pStructure);

            sp<IGraphicBufferConsumer> * p = (sp<IGraphicBufferConsumer> *)pStructure;

            //mBufferConsumer = *p;
            mConsumer = *p;

            LOG_DEBUG("SurfaceSource mConsumer is %s", (mConsumer == NULL) ? "NULL" : "not NULL");
            if(mConsumer != NULL)
                LOG_DEBUG("SurfaceSource mConsumer pointer set to %p", mConsumer.get());

            mIsPersistent = OMX_TRUE;

            //if(mListener != NULL && mListener->mConsumer != NULL)
            //    LOG_ERROR("SetParam BufferConsumer: mListener->mConsumer is not null!");

            }
            break;

        default:
            {
            LOG_DEBUG("OMX_ErrorUnsupportedIndex %d\n", nParamIndex);
            ret = OMX_ErrorUnsupportedIndex;
            }
            break;
    }
    return ret;
}

SurfaceSource::GraphicBufferListener::GraphicBufferListener(SurfaceSource *ssource)
{
    LOG_DEBUG("%s", __FUNCTION__ );

    //mConsumer = bufferConsumer;
    //mIsPersistent = OMX_FALSE;

    mSurfaceSource = ssource;
    //InitBuffers();
};

SurfaceSource::GraphicBufferListener::~GraphicBufferListener(){
    LOG_DEBUG("%s", __FUNCTION__);
    //DeinitBuffers();

    //LOG_DEBUG("proxy %p strong count %d", proxy.get(),
    //    (static_cast<RefBase*>(proxy.get()))->getStrongCount());
};

void SurfaceSource::GraphicBufferListener::onFrameAvailable(const BufferItem& item){
    fsl_osal_mutex_lock(mSurfaceSource->lock);

    mSurfaceSource->nFramesAvailable++;
    LOG_DEBUG("%s total available %d", __FUNCTION__, mSurfaceSource->nFramesAvailable);

    fsl_osal_mutex_unlock(mSurfaceSource->lock);

}

void SurfaceSource::GraphicBufferListener::onSidebandStreamChanged() {
    LOG_DEBUG("%s", __FUNCTION__);
}

void SurfaceSource::GraphicBufferListener::onBuffersReleased(){
    fsl_osal_mutex_lock(mSurfaceSource->lock);

    uint64_t mask = 0;
    if (mSurfaceSource->mConsumer->getReleasedBuffers(&mask) != NO_ERROR) {
        LOG_WARNING("on buffers released: can't get released buffer set");
        mask = 0xffffffffffffffffULL;
    }

    LOG_DEBUG("on buffers released: mast: %x,%x" , int(mask>>32), int(mask & 0xFFFFFFFF));

    for (int i = 0; i < BufferQueue::NUM_BUFFER_SLOTS; i++) {
        if (0 != (mask & 0x01))
            mSurfaceSource->mBufferSlot[i] = NULL;

        mask >>= 1;
    }

    fsl_osal_mutex_unlock(mSurfaceSource->lock);
}

SurfaceSource::PersisProxyListener::PersisProxyListener(
        const wp<IGraphicBufferConsumer> &consumer,
        const wp<ConsumerListener>& consumerListener) :
    mConsumerListener(consumerListener),
    mConsumer(consumer)
{
    LOG_DEBUG("%s %p", __FUNCTION__, this);
}

SurfaceSource::PersisProxyListener::~PersisProxyListener()
{
    LOG_DEBUG("%s", __FUNCTION__);
}

void SurfaceSource::PersisProxyListener::onBuffersReleased() {
    sp<ConsumerListener> csmlistener(mConsumerListener.promote());
    LOG_DEBUG("%s listener is %s", __FUNCTION__, csmlistener == NULL ? "NULL" : "not NULL");
    if (csmlistener != NULL) {
        csmlistener->onBuffersReleased();
    }
}

void SurfaceSource::PersisProxyListener::onSidebandStreamChanged() {
    sp<ConsumerListener> csmlistener(mConsumerListener.promote());
    LOG_DEBUG("%s listener is %s", __FUNCTION__, csmlistener == NULL ? "NULL" : "not NULL");
    if (csmlistener != NULL) {
        csmlistener->onSidebandStreamChanged();
    }
}

void SurfaceSource::PersisProxyListener::onFrameAvailable(
        const BufferItem& itm) {
    sp<ConsumerListener> csmlistener(mConsumerListener.promote());
    if (csmlistener != NULL) {
        LOG_DEBUG("%s listener is !NULL", __FUNCTION__);
        csmlistener->onFrameAvailable(itm);
    } else {
        LOG_DEBUG("%s listener is NULL", __FUNCTION__);

        sp<IGraphicBufferConsumer> consumer(mConsumer.promote());
        if (consumer == NULL) {
            return;
        }
        BufferItem bi;
        status_t err = consumer->acquireBuffer(&bi, 0);
        if (err != OK) {
            LOG_ERROR("PersisProxyListener acquireBuffer failed %d", err);
            return;
        }

        err = consumer->detachBuffer(bi.mBuf);
        if (err != OK) {
            LOG_ERROR("PersisProxyListener detachBuffer failed %d", err);
            return;
        }

        err = consumer->attachBuffer(&bi.mBuf, bi.mGraphicBuffer);
        if (err != OK) {
            LOG_ERROR("PersisProxyListener attachBuffer failed %d", err);
            return;
        }

        err = consumer->releaseBuffer(bi.mBuf, 0,
                EGL_NO_DISPLAY, EGL_NO_SYNC_KHR, bi.mFence);
        if (err != OK) {
            LOG_ERROR("PersisProxyListener releaseBuffer failed %d", err);
        }
    }
}

void SurfaceSource::PersisProxyListener::onFrameReplaced(
        const BufferItem& itm) {
    sp<ConsumerListener> csmlistener(mConsumerListener.promote());
    LOG_DEBUG("%s listener is %s", __FUNCTION__, csmlistener == NULL ? "NULL" : "not NULL");
    if (csmlistener != NULL) {
        csmlistener->onFrameReplaced(itm);
    }
}



/**< C style functions to expose entry point for the shared library */
	extern "C" {
		OMX_ERRORTYPE SurfaceSourceInit(OMX_IN OMX_HANDLETYPE pHandle)
		{
			OMX_ERRORTYPE ret = OMX_ErrorNone;
			SurfaceSource *obj = NULL;
			ComponentBase *base = NULL;

			obj = FSL_NEW(SurfaceSource, ());
			if(obj == NULL)
				return OMX_ErrorInsufficientResources;

			base = (ComponentBase*)obj;
			ret = base->ConstructComponent(pHandle);
			if(ret != OMX_ErrorNone)
				return ret;

			return ret;
		}
	}


}

/* File EOF */
