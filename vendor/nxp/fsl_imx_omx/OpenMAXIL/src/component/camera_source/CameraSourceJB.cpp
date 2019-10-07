/**
 *  Copyright (c) 2013, Freescale Semiconductor, Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include <cutils/log.h>
#include "gralloc_priv.h"
#include <binder/IPCThreadState.h>
#include <media/hardware/MetadataBufferType.h>
#include "CameraSourceJB.h"
#if (ANDROID_VERSION >= JELLY_BEAN_43)
#include "gui/IGraphicBufferProducer.h"
#include <utils/String16.h>
#endif

using android::IPCThreadState;

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_WARNING
#define LOG_WARNING printf
#endif

#define NO_ERROR 0

struct CamSrcListener : public CameraListener {
    CamSrcListener(CameraSource *source);

    virtual void notify(int msg, int e1, int e2);
    virtual void postData(int msg, const sp<IMemory> &data, camera_frame_metadata_t *metadata);

    virtual void postDataTimestamp(
            nsecs_t ts, int msg, const sp<IMemory>& data);

    virtual ~CamSrcListener();
protected:

private:
    CameraSource *mSource;

    CamSrcListener(const CamSrcListener &);

    CamSrcListener &operator=(const CamSrcListener &);
};

CamSrcListener::CamSrcListener(CameraSource *source) {
		mSource = source;
}

CamSrcListener::~CamSrcListener() {
}

void CamSrcListener::notify(int msg, int e1, int e2) {
    LOG_LOG("listener notify %d, %d, %d", msg, e1, e2);
}

void CamSrcListener::postData(int msg, const sp<IMemory> &data, camera_frame_metadata_t *metadata) {
	LOG_LOG("listener postData %d, ptr:%p, size:%d", msg, data->pointer(), data->size());
}

void CamSrcListener::postDataTimestamp(
        nsecs_t ts, int msg, const sp<IMemory>& data) {

	LOG_LOG("listener postData %d, dataPtr:%p, ptr:%p, size:%d",
			msg, (OMX_PTR)(&data), data->pointer(), data->size());

	mSource->dataCallbackTimestamp((OMX_TICKS)ts/1000, (OMX_S32)msg, (OMX_PTR)(&data));
}

static OMX_S32 getColorFormat(const char* colorFormat) {
    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV420P)) {
       return OMX_COLOR_FormatYUV420Planar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV422SP)) {
       return OMX_COLOR_FormatYUV422SemiPlanar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV420SP)) {
        return OMX_COLOR_FormatYUV420SemiPlanar;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_YUV422I)) {
        return OMX_COLOR_FormatYCbYCr;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_RGB565)) {
       return OMX_COLOR_Format16bitRGB565;
    }

    if (!strcmp(colorFormat, CameraParameters::PIXEL_FORMAT_ANDROID_OPAQUE)) {
        return OMX_COLOR_FormatAndroidOpaque;
    }

	LOG_ERROR("Uknown color format (%s), please add it to "
			"CameraSource::getColorFormat", colorFormat);

	return OMX_COLOR_FormatUnused;
}

CameraSource::CameraSource()
{
	OMX_U32 i;

	fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_source.camera.sw-based");
	ComponentVersion.s.nVersionMajor = 0x0;
	ComponentVersion.s.nVersionMinor = 0x1;
	ComponentVersion.s.nRevision = 0x0;
	ComponentVersion.s.nStep = 0x0;
	role_cnt = 1;
	role[0] = (OMX_STRING)"video_source.camera";

	nFrameBufferMin = 1;
	nFrameBufferActual = 3;
	nFrameBuffer = nFrameBufferActual;
	PreviewPortSupplierType = OMX_BufferSupplyInput;
	CapturePortSupplierType = OMX_BufferSupplyOutput;
	fsl_osal_memset(&sVideoFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
	sVideoFmt.nFrameWidth = 176;
	sVideoFmt.nFrameHeight = 144;
	sVideoFmt.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
	sVideoFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

	OMX_INIT_STRUCT(&sRectIn, OMX_CONFIG_RECTTYPE);
	sRectIn.nWidth = sVideoFmt.nFrameWidth;
	sRectIn.nHeight = sVideoFmt.nFrameHeight;
	sRectIn.nTop = 0;
	sRectIn.nLeft = 0;
	OMX_INIT_STRUCT(&sRectOut, OMX_CONFIG_RECTTYPE);
	sRectOut.nWidth = sVideoFmt.nFrameWidth;
	sRectOut.nHeight = sVideoFmt.nFrameHeight;
	sRectOut.nTop = 0;
	sRectOut.nLeft = 0;

	for(i=0; i<FRAME_BUFFER_QUEUE_SIZE; i++) {
		mVideoBufferPhy[i] = NULL;
		mIMemory[i] = 0;
	}

	mCamera = NULL;
	mCameraRecordingProxy = NULL;
#if (ANDROID_VERSION == JELLY_BEAN_42)
	mPreviewSurface = NULL;
#endif
	mPreviewSurface2 = NULL;
	mDeathNotifier = NULL;
	mListenerPtr = NULL;
	mProxyListenerPtr = NULL;
	mStarted = OMX_FALSE;
	bFrameBufferInit = OMX_FALSE;
	nSourceFrames = 0;
	nIMemoryIndex = 0;
	nCurTS = 0;
	lock = NULL;
	stringClientName = NULL;
    ReceivedBufferQueue = NULL;
    mFlags = 0;
}

OMX_ERRORTYPE CameraSource::InitSourceComponent()
{
	if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
		LOG_ERROR("Create mutex for camera device failed.\n");
		return OMX_ErrorInsufficientResources;
	}

	/** Create queue for frame buffer. */
	ReceivedBufferQueue = FSL_NEW(Queue, ());
	if (ReceivedBufferQueue == NULL) {
		LOG_ERROR("Can't get memory.\n");
		return OMX_ErrorInsufficientResources;
	}

	if (ReceivedBufferQueue->Create(FRAME_BUFFER_QUEUE_SIZE, \
                sizeof(RECEIVED_VIDEO_BUFFER), E_FSL_OSAL_TRUE) != QUEUE_SUCCESS) {
		LOG_ERROR("Can't create frame buffer queue.\n");
		return OMX_ErrorInsufficientResources;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE CameraSource::DeInitSourceComponent()
{
	LOG_DEBUG("Frame buffer queue free\n");
	if(ReceivedBufferQueue != NULL)
		ReceivedBufferQueue->Free();
	LOG_DEBUG("Frame buffer queue delete\n");
	FSL_DELETE(ReceivedBufferQueue);

	if(lock)
		fsl_osal_mutex_destroy(lock);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE CameraSource::PortFormatChanged(OMX_U32 nPortIndex)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_BOOL bFmtChanged = OMX_FALSE;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = nPortIndex;
    ports[nPortIndex]->GetPortDefinition(&sPortDef);

    if (bStoreMetaData == OMX_FALSE) {
        sPortDef.nBufferSize = sPortDef.format.video.nFrameWidth
            * sPortDef.format.video.nFrameHeight
            * pxlfmt2bpp(sVideoFmt.eColorFormat) / 8;
        ret = ports[nPortIndex]->SetPortDefinition(&sPortDef);
        if (ret != OMX_ErrorNone)
            return ret;
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
		}

		sRectIn.nWidth = sVideoFmt.nFrameWidth;
		sRectIn.nHeight = sVideoFmt.nFrameHeight;
		sRectOut.nWidth = sVideoFmt.nFrameWidth;
		sRectOut.nHeight = sVideoFmt.nFrameHeight;
		sVideoFmt.xFramerate = sPortDef.format.video.xFramerate;

		printf("Set resolution: width: %d height: %d\n", \
                sPortDef.format.video.nFrameWidth, \
				sPortDef.format.video.nFrameHeight);

		if(bFmtChanged && mCamera > 0) {
			CloseDevice();
			OpenDevice();
		}
	}

    return ret;
}

OMX_ERRORTYPE CameraSource::OpenDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	int64_t token = IPCThreadState::self()->clearCallingIdentity();
	if (cameraPtr == NULL) {
#if (ANDROID_VERSION == JELLY_BEAN_42)
		mCamera = Camera::connect(nCameraId);
#elif (ANDROID_VERSION >= JELLY_BEAN_43)
		stringClientName = FSL_NEW(android::String16, ((const char16_t*)clientName));
		if(stringClientName == NULL)
			return OMX_ErrorInsufficientResources;
		mCamera = Camera::connect(nCameraId, *stringClientName, clientUID);
#endif
		if (mCamera == 0) {
			IPCThreadState::self()->restoreCallingIdentity(token);
			LOG_ERROR("Camera connection could not be established.");
			return OMX_ErrorHardware;
		}
		mFlags &= ~FLAGS_HOT_CAMERA;
		mCamera->lock();
	} else {
		sp<ICamera> *pCamera;
		pCamera = (sp<ICamera> *)cameraPtr;
		mFlags &= ~FLAGS_HOT_CAMERA;
		mCamera = Camera::create(*pCamera);
		if (mCamera == 0) {
			IPCThreadState::self()->restoreCallingIdentity(token);
			LOG_ERROR("Unable to connect to camera");
			return OMX_ErrorHardware;
		}
		mFlags |= FLAGS_HOT_CAMERA;

		sp<ICameraRecordingProxy> *pCameraProxy;
		if (cameraProxyPtr == NULL) {
			LOG_ERROR("camera proxy is NULL");
			return OMX_ErrorHardware;
		}
		pCameraProxy = (sp<ICameraRecordingProxy> *)cameraProxyPtr;
		mCameraRecordingProxy = *pCameraProxy;
		mDeathNotifier = new DeathNotifier();
#if (ANDROID_VERSION < MARSH_MALLOW_600)
		mCameraRecordingProxy->asBinder()->linkToDeath(mDeathNotifier);
#else
		android::IInterface::asBinder(mCameraRecordingProxy)->linkToDeath(mDeathNotifier);
#endif
		mCamera->lock();
	}
	IPCThreadState::self()->restoreCallingIdentity(token);

	ret = SetDevice();
	if (ret != OMX_ErrorNone)
		return ret;

	return ret;
}

OMX_ERRORTYPE CameraSource::CloseDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	if (mCamera != 0) {
		int64_t token = IPCThreadState::self()->clearCallingIdentity();
		LOG_LOG("Disconnect camera");
		if ((mFlags & FLAGS_HOT_CAMERA) == 0) {
			LOG_LOG("Camera was cold when we started, stopping preview");
			mCamera->stopPreview();
            mCamera->disconnect();
		}
		LOG_DEBUG("unlock camera.\n");
		mCamera->unlock();
		mCamera.clear();
		mCamera = NULL;
		IPCThreadState::self()->restoreCallingIdentity(token);
	}

	if (mCameraRecordingProxy != 0) {
#if (ANDROID_VERSION < MARSH_MALLOW_600)
		mCameraRecordingProxy->asBinder()->unlinkToDeath(mDeathNotifier);
#else
		android::IInterface::asBinder(mCameraRecordingProxy)->unlinkToDeath(mDeathNotifier);
#endif
		mCameraRecordingProxy.clear();
	}
	mFlags = 0;

	if(stringClientName != NULL)
		FSL_DELETE(stringClientName);

	return ret;
}

OMX_ERRORTYPE CameraSource::SetDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	{
		// Set the actual video recording frame size
		int64_t token = IPCThreadState::self()->clearCallingIdentity();
		CameraParameters params(mCamera->getParameters());
		params.setVideoSize(sRectIn.nWidth, sRectIn.nHeight);
		params.setPreviewFrameRate(sVideoFmt.xFramerate / Q16_SHIFT);
		String8 s = params.flatten();
		if (NO_ERROR != mCamera->setParameters(s)) {
			LOG_ERROR("Could not change settings."
					" Someone else is using camera %d?", nCameraId);
			IPCThreadState::self()->restoreCallingIdentity(token);
			return OMX_ErrorHardware;
		}
		CameraParameters newCameraParams(mCamera->getParameters());

		// Check on video frame size
		int frameWidth = 0, frameHeight = 0;
		newCameraParams.getVideoSize(&frameWidth, &frameHeight);
		if (frameWidth  < 0 || frameWidth  != (int)sRectIn.nWidth ||
				frameHeight < 0 || frameHeight != (int)sRectIn.nHeight) {
			LOG_ERROR("Failed to set the video frame size to %dx%d",
					sRectIn.nWidth, sRectIn.nHeight);
			IPCThreadState::self()->restoreCallingIdentity(token);
			return OMX_ErrorHardware;
		}

		// Check on video frame rate
		OMX_S32 frameRate = newCameraParams.getPreviewFrameRate();
		if (frameRate < 0 || (frameRate - sVideoFmt.xFramerate / Q16_SHIFT) != 0) {
			LOG_ERROR("Failed to set frame rate to %d fps. The actual "
					"frame rate is %d", sVideoFmt.xFramerate / Q16_SHIFT, frameRate);
		}

		// This CHECK is good, since we just passed the lock/unlock
		// check earlier by calling mCamera->setParameters().
		LOG_DEBUG("previewSurface: %p\n", previewSurface);
        if (previewSurface) {
#if (ANDROID_VERSION == JELLY_BEAN_42)
            sp<Surface> *pSurface;
            pSurface = (sp<Surface> *)previewSurface;
            mPreviewSurface2 = *pSurface;
            if (NO_ERROR != mCamera->setPreviewDisplay(mPreviewSurface2)) {
                LOG_ERROR("setPreviewDisplay failed.\n");
            }
#elif (ANDROID_VERSION == JELLY_BEAN_43)
            sp<android::IGraphicBufferProducer> * pProducer = (sp<android::IGraphicBufferProducer> *) previewSurface;
            if (NO_ERROR != mCamera->setPreviewTexture(*pProducer)){
                LOG_ERROR("setPreviewTexture failed.\n");
            }

#elif (ANDROID_VERSION >= KITKAT_44)
            sp<android::IGraphicBufferProducer> * pProducer = (sp<android::IGraphicBufferProducer> *) previewSurface;
            if (NO_ERROR != mCamera->setPreviewTarget(*pProducer)){
                LOG_ERROR("setPreviewTarget failed.\n");
            }
#endif
        }
		IPCThreadState::self()->restoreCallingIdentity(token);
	}

	{
		int64_t token = IPCThreadState::self()->clearCallingIdentity();
		String8 s = mCamera->getParameters();
		IPCThreadState::self()->restoreCallingIdentity(token);

		printf("params: \"%s\"\n", s.string());

		int width, height, stride, sliceHeight;
		CameraParameters params(s);
		params.getVideoSize(&width, &height);

		OMX_S32 frameRate = params.getPreviewFrameRate();

		const char *colorFormatStr = params.get(CameraParameters::KEY_VIDEO_FRAME_FORMAT);
		if (colorFormatStr != NULL) {
			OMX_S32 colorFormat = getColorFormat(colorFormatStr);
		}
	}

	{
		int64_t token = IPCThreadState::self()->clearCallingIdentity();

        if (nFrameBuffer > 0) {
            // This could happen for CameraHAL1 clients; thus the failure is
            // not a fatal error
			if (NO_ERROR != mCamera->sendCommand(CAMERA_CMD_SET_VIDEO_BUFFER_COUNT, nFrameBuffer, 0)) {
                LOG_DEBUG("Failed to set video buffer count to %d", nFrameBuffer);
            }
        }

        if (mFlags & FLAGS_HOT_CAMERA) {
			ProxyListener *mProxyListener;
			mProxyListener = FSL_NEW(ProxyListener, (this));
			mProxyListenerPtr = (OMX_PTR)mProxyListener;

			if (NO_ERROR != mCamera->storeMetaDataInBuffers(true)) {
				LOG_WARNING("Camera set derect input fail.\n");
				return OMX_ErrorNone;
			}

			mCamera->unlock();
			mCamera.clear();

			mCameraRecordingProxy->startRecording(mProxyListener);
		} else {
			CamSrcListener *mListener;
			mListener = FSL_NEW(CamSrcListener, (this));
			mListenerPtr = (OMX_PTR)mListener;

			mCamera->setListener(mListener);
			if (NO_ERROR != mCamera->startRecording()) {
				LOG_ERROR("stopRecording failed.\n");
			}

			if (NO_ERROR != mCamera->storeMetaDataInBuffers(true)) {
				LOG_WARNING("Camera set derect input fail.\n");
				return OMX_ErrorNone;
			}
		}
		IPCThreadState::self()->restoreCallingIdentity(token);

	}

	return ret;
}

OMX_ERRORTYPE CameraSource::SetSensorMode()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE CameraSource::SetVideoFormat()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE CameraSource::SetWhiteBalance()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE CameraSource::SetDigitalZoom()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE CameraSource::SetExposureValue()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	return ret;
}

OMX_ERRORTYPE CameraSource::SetRotation()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	return ret;
}

OMX_ERRORTYPE CameraSource::StartDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

	mStarted = OMX_TRUE;

	return ret;
}

void CameraSource::releaseRecordingFrame(const sp<IMemory>& frame)
{
	if (mCameraRecordingProxy != NULL) {
		mCameraRecordingProxy->releaseRecordingFrame(frame);
	} else if (mCamera != NULL) {
		int64_t token = IPCThreadState::self()->clearCallingIdentity();
		mCamera->releaseRecordingFrame(frame);
		IPCThreadState::self()->restoreCallingIdentity(token);
	}
}

OMX_ERRORTYPE CameraSource::releaseQueuedFrames()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 nIndex = 0;

	for (nIndex = 0; nIndex < FRAME_BUFFER_QUEUE_SIZE; nIndex ++) {
		if (mIMemory[nIndex] != 0) {
            LOG_DEBUG("releaseRecordingFrame index: %d\n", nIndex);
			if (mCameraRecordingProxy != NULL) {
				mCameraRecordingProxy->releaseRecordingFrame(mIMemory[nIndex]);
			} else if (mCamera != NULL) {
				mCamera->releaseRecordingFrame(mIMemory[nIndex]);
			}
			mIMemory[nIndex] = 0;
		}
	}
	return ret;
}

OMX_ERRORTYPE CameraSource::StopDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;

    fsl_osal_mutex_lock(lock);
	if (mStarted == OMX_TRUE) {
		mStarted = OMX_FALSE;

		int64_t token = IPCThreadState::self()->clearCallingIdentity();
		releaseQueuedFrames();
		if (mFlags & FLAGS_HOT_CAMERA) {
			mCameraRecordingProxy->stopRecording();
		} else {
			mCamera->setListener(NULL);
			LOG_DEBUG("CamSrcListener removed.\n");
			if (mListenerPtr) {
				CamSrcListener *mListener = (CamSrcListener *)mListenerPtr;
				mListenerPtr = NULL;
			}
			printf("stopRecording.\n");
			mCamera->stopRecording();
		}
		IPCThreadState::self()->restoreCallingIdentity(token);
	}

    fsl_osal_mutex_unlock(lock);

	return ret;
}

OMX_ERRORTYPE CameraSource::SendBufferToDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PTR pBuffer;
    OMX_U32 nIndex = 0;

    pBuffer = ((METADATA_BUFFER *)(pOutBufferHdr->pBuffer))->pPhysicAddress;
	ret = SearchBuffer(pBuffer, &nIndex);
    if (ret != OMX_ErrorNone)
        return OMX_ErrorNone;

    fsl_osal_mutex_lock(lock);

	if (mIMemory[nIndex] != 0) {
		LOG_DEBUG("releaseRecordingFrame index: %d\n", nIndex);
		releaseRecordingFrame(mIMemory[nIndex]);
		mIMemory[nIndex] = 0;
	}

    OMX_U32 iCnt = 0;
	for (nIndex = 0; nIndex < FRAME_BUFFER_QUEUE_SIZE; nIndex ++) {
		if (mIMemory[nIndex] != 0) {
            iCnt ++;
		}
	}
    LOG_DEBUG("recorder hold buffer: %d\n", iCnt);

    fsl_osal_mutex_unlock(lock);

	return ret;
}

OMX_ERRORTYPE CameraSource::GetOneFrameFromDevice()
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
    RECEIVED_VIDEO_BUFFER sReceivedBuffer;

    OMX_U32 nIndex;

	if (mFlags & FLAGS_HOT_CAMERA) {
		if(mCameraRecordingProxy <= 0)
			return OMX_ErrorNotReady;
	} else {
		if(mCamera <= 0)
			return OMX_ErrorNotReady;
	}

    if(ReceivedBufferQueue->Size() <= 0) {
        fsl_osal_sleep(10000);
        ((METADATA_BUFFER *)(pOutBufferHdr->pBuffer))->pPhysicAddress = 0;
        pOutBufferHdr->nFilledLen = 0;
        pOutBufferHdr->nTimeStamp = INVALID_TS;
        return ret;
    }

	if (ReceivedBufferQueue->Get(&sReceivedBuffer) != QUEUE_SUCCESS) {
		LOG_ERROR("Can't get output port buffer item.\n");
		return OMX_ErrorUndefined;
	}

    ((METADATA_BUFFER *)(pOutBufferHdr->pBuffer))->pPhysicAddress = \
        sReceivedBuffer.pPhysicAdd;
    pOutBufferHdr->nOffset = 0;
    pOutBufferHdr->nFilledLen = 4;
    pOutBufferHdr->nTimeStamp = sReceivedBuffer.nTimeStamp;
    LOG_DEBUG("pOutBufferHdr buffer allocate len: %d, pBuffer[0]: %x, ts %lld\n", \
            pOutBufferHdr->nAllocLen, ((OMX_U32 *)(pOutBufferHdr->pBuffer))[0], pOutBufferHdr->nTimeStamp);

	nCurTS = pOutBufferHdr->nTimeStamp;

	return ret;
}

OMX_TICKS CameraSource::GetDelayofFrame()
{
	int64_t readTimeUs = systemTime() / 1000;

	return readTimeUs - nCurTS;
}

OMX_TICKS CameraSource::GetFrameTimeStamp()
{
    return nCurTS;
}

void CameraSource::dataCallbackTimestamp(OMX_TICKS timestamp, OMX_S32 msgType, OMX_PTR dataPtr)
{
    RECEIVED_VIDEO_BUFFER sReceivedBuffer;
    OMX_U32 *pTempBuffer;
    OMX_U32 nMetadataBufferType;
    private_handle_t* pPrivateHandle;
    buffer_handle_t  tBufHandle;
    sp<IMemory> *pData;
	pData = (sp<IMemory> *)dataPtr;
    OMX_U32 nIndex = 0;

    fsl_osal_mutex_lock(lock);

	if (mStarted == OMX_FALSE) {
		printf("Haven't start, Drop frame.\n");
		releaseRecordingFrame(*pData);
		fsl_osal_mutex_unlock(lock);
		return;
	}

    pTempBuffer = (OMX_U32 *) ((*pData)->pointer());
    LOG_DEBUG("IMemory pointer is %x, size %d", pTempBuffer, (*pData)->size());

    nMetadataBufferType = *pTempBuffer;
    LOG_DEBUG("MetadataBufferType is %d (0-CameraSource,1-GrallocSource,2-ANWBuffer)", nMetadataBufferType);

    pTempBuffer++;

#if (ANDROID_VERSION < MARSH_MALLOW_600)
    tBufHandle =  *((buffer_handle_t *)pTempBuffer);
#else
    if(nMetadataBufferType == android::kMetadataBufferTypeANWBuffer){
        ANativeWindowBuffer *buffer = (ANativeWindowBuffer *)*pTempBuffer;
        if(buffer == 0){
            LOG_ERROR("No valid ANWBuffer from metadata!");
            fsl_osal_mutex_unlock(lock);
            return;
        }
        tBufHandle = buffer->handle;
    }else{
        tBufHandle =  *((buffer_handle_t *)pTempBuffer);
    }
#endif

    pPrivateHandle = (private_handle_t*) tBufHandle;
    if(pPrivateHandle == 0){
        LOG_ERROR("No valid private handle from metadata!");
        fsl_osal_mutex_unlock(lock);
        return;
    }

    LOG_DEBUG("%s Gralloc=0x%x, phys = 0x%x", __FUNCTION__, pPrivateHandle,
            pPrivateHandle->phys);

    SearchAddBuffer((OMX_PTR)pPrivateHandle->phys, &nIndex);
    if(nIndex >= FRAME_BUFFER_QUEUE_SIZE)
    {
        LOG_ERROR("Can't find physical buffer in mIMemory!");
        fsl_osal_mutex_unlock(lock);
        return;
    }
    LOG_DEBUG("Got frame from Camera index: %d\n", nIndex);
    mIMemory[nIndex] = *pData;

    sReceivedBuffer.pPhysicAdd = (OMX_PTR)pPrivateHandle->phys;
    sReceivedBuffer.nTimeStamp = timestamp;

    if (ReceivedBufferQueue->Add(&sReceivedBuffer) != QUEUE_SUCCESS) {
        LOG_ERROR("Can't add frame buffer queue. Queue size: %d, max queue size: %d \n", \
                ReceivedBufferQueue->Size(), FRAME_BUFFER_QUEUE_SIZE);
    }

    fsl_osal_mutex_unlock(lock);
}

OMX_ERRORTYPE CameraSource::SearchBuffer(
        OMX_PTR pAddr,
        OMX_U32 *pIndex)
{
    OMX_U32 nIndex = 0;
    OMX_U32 i;
    OMX_BOOL bFound = OMX_FALSE;

    for(i=0; i<FRAME_BUFFER_QUEUE_SIZE; i++) {
        if(mVideoBufferPhy[i] == pAddr) {
            nIndex = i;
            bFound = OMX_TRUE;
            break;
        }
    }

    if(bFound != OMX_TRUE)
        return OMX_ErrorUndefined;

    *pIndex = nIndex;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE CameraSource::SearchAddBuffer(
        OMX_PTR pAddr,
        OMX_U32 *pIndex)
{
    OMX_U32 nIndex = 0;
    OMX_U32 i;

    for(i=0; i<FRAME_BUFFER_QUEUE_SIZE; i++) {
        if(mVideoBufferPhy[i] == pAddr) {
            nIndex = i;
            break;
        }
        if (mVideoBufferPhy[i] == NULL) {
            nIndex = i;
            mVideoBufferPhy[i] = pAddr;
            break;
        }
    }

    *pIndex = nIndex;

    return OMX_ErrorNone;
}

CameraSource::ProxyListener::ProxyListener(CameraSource *source) {
	mSource = source;
}

void CameraSource::ProxyListener::dataCallbackTimestamp(
		nsecs_t timestamp, int32_t msgType, const sp<IMemory>& dataPtr) {
	mSource->dataCallbackTimestamp((OMX_TICKS)timestamp/1000, (OMX_S32)msgType, (OMX_PTR)(&dataPtr));
}

void CameraSource::DeathNotifier::binderDied(const wp<IBinder>& who) {
	LOG_DEBUG("Camera recording proxy died");
}

/**< C style functions to expose entry point for the shared library */
	extern "C" {
		OMX_ERRORTYPE CameraSourceInit(OMX_IN OMX_HANDLETYPE pHandle)
		{
			OMX_ERRORTYPE ret = OMX_ErrorNone;
			CameraSource *obj = NULL;
			ComponentBase *base = NULL;

			obj = FSL_NEW(CameraSource, ());
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
