/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2013-2015 Freescale Semiconductor, Inc.
 *
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

#include "UvcDevice.h"
#include <poll.h>

#define UVC_DEFAULT_PREVIEW_FPS (15)
#define UVC_DEFAULT_PREVIEW_W   (320)
#define UVC_DEFAULT_PREVIEW_H   (240)
#define UVC_DEFAULT_PICTURE_W   (320)
#define UVC_DEFAULT_PICTURE_H   (240)

status_t UvcDevice::initParameters(CameraParameters& params,
                                  int              *supportRecordingFormat,
                                  int               rfmtLen,
                                  int              *supportPictureFormat,
                                  int               pfmtLen)
{

    if ((supportRecordingFormat == NULL) || (rfmtLen == 0) ||
        (supportPictureFormat == NULL) || (pfmtLen == 0)) {
        FLOGE("UvcDevice: initParameters invalid parameters");
        return BAD_VALUE;
    }

    // first read sensor format.
    int ret = 0, index = 0;
    int sensorFormat[MAX_SENSOR_FORMAT];
	sensorFormat[0] = v4l2_fourcc('Y', 'U', 'Y', 'V');
    index           = 1;


    // second check match sensor format with vpu support format and picture
    // format.
    mPreviewPixelFormat = getMatchFormat(supportRecordingFormat,
                                         rfmtLen,
                                         sensorFormat,
                                         index);
    mPicturePixelFormat = getMatchFormat(supportPictureFormat,
                                         pfmtLen,
                                         sensorFormat,
                                         index);

#ifdef NO_GPU
    mPreviewPixelFormat = HAL_PIXEL_FORMAT_RGB_888;
#endif

    ALOGI("mPreviewPixelFormat 0x%x, mPicturePixelFormat 0x%x", mPreviewPixelFormat, mPicturePixelFormat);
    setPreviewStringFormat(mPreviewPixelFormat);
    ret = setSupportedPreviewFormats(supportRecordingFormat,
                                     rfmtLen,
                                     sensorFormat,
                                     index);
    if (ret) {
        FLOGE("setSupportedPreviewFormats failed");
        return ret;
    }

    index = 0;
    char TmpStr[20];
    int  previewCnt = 0, pictureCnt = 0;
    struct v4l2_frmsizeenum vid_frmsize;
    struct v4l2_frmivalenum vid_frmval;

    memset(TmpStr, 0, 20);
	
    memset(&vid_frmsize, 0, sizeof(struct v4l2_frmsizeenum));
    vid_frmsize.index        = index++;
	vid_frmsize.pixel_format = v4l2_fourcc('Y', 'U', 'Y', 'V');
	vid_frmsize.discrete.width = UVC_DEFAULT_PREVIEW_W;
	vid_frmsize.discrete.height = UVC_DEFAULT_PREVIEW_H;
	

    FLOG_RUNTIME("enum frame size w:%d, h:%d",
                 vid_frmsize.discrete.width, vid_frmsize.discrete.height);
	
    memset(&vid_frmval, 0, sizeof(struct v4l2_frmivalenum));
    vid_frmval.index        = 0;
    vid_frmval.pixel_format = vid_frmsize.pixel_format;
    vid_frmval.width        = vid_frmsize.discrete.width;
    vid_frmval.height       = vid_frmsize.discrete.height;


    FLOG_RUNTIME("vid_frmval denominator:%d, numeraton:%d",
                 vid_frmval.discrete.denominator,
                 vid_frmval.discrete.numerator);

    vid_frmval.discrete.denominator = 30;
    vid_frmval.discrete.numerator   = 1;


    sprintf(TmpStr,
            "%dx%d",
            vid_frmsize.discrete.width,
            vid_frmsize.discrete.height);
	
    if (pictureCnt == 0)
        strncpy((char *)mSupportedPictureSizes,
                TmpStr,
                CAMER_PARAM_BUFFER_SIZE);

    pictureCnt++;

    if (vid_frmval.discrete.denominator /
        vid_frmval.discrete.numerator > 15) {
        if (previewCnt == 0)
            strncpy((char *)mSupportedPreviewSizes,
                    TmpStr,
                    CAMER_PARAM_BUFFER_SIZE);
        else {
            strncat(mSupportedPreviewSizes,
                    PARAMS_DELIMITER,
                    CAMER_PARAM_BUFFER_SIZE);
            strncat(mSupportedPreviewSizes,
                    TmpStr,
                    CAMER_PARAM_BUFFER_SIZE);
        }
        previewCnt++;
    }


    strcpy(mSupportedFPS, "15,30");
    FLOGI("SupportedPictureSizes is %s", mSupportedPictureSizes);
    FLOGI("SupportedPreviewSizes is %s", mSupportedPreviewSizes);
    FLOGI("SupportedFPS is %s", mSupportedFPS);

    mParams.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES,
                mSupportedPictureSizes);
    mParams.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES,
                mSupportedPreviewSizes);

    mParams.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,
                mSupportedFPS);
    mParams.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE,
                "(12000,17000),(25000,33000)");
    // Align the default FPS RANGE to the UVC_DEFAULT_PREVIEW_FPS
    mParams.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "12000,17000");

    mParams.setPreviewSize(UVC_DEFAULT_PREVIEW_W, UVC_DEFAULT_PREVIEW_H);
    mParams.setPictureSize(UVC_DEFAULT_PICTURE_W, UVC_DEFAULT_PICTURE_H);
    mParams.setPreviewFrameRate(UVC_DEFAULT_PREVIEW_FPS);

    params = mParams;
    return NO_ERROR;
}

status_t UvcDevice::setParameters(CameraParameters& params)
{
    int  w, h;
    int  framerate, local_framerate;
    int  max_zoom, zoom, max_fps, min_fps;
    char tmp[128];
	
    Mutex::Autolock lock(mLock);

    max_zoom = params.getInt(CameraParameters::KEY_MAX_ZOOM);
    zoom     = params.getInt(CameraParameters::KEY_ZOOM);
    if (zoom > max_zoom) {
        FLOGE("Invalid zoom setting, zoom %d, max zoom %d", zoom, max_zoom);
        return BAD_VALUE;
    }

    if (!((strcmp(params.getPreviewFormat(), "yuv420sp") == 0) ||
          (strcmp(params.getPreviewFormat(), "yuv420p") == 0) ||
          (strcmp(params.getPreviewFormat(), "yuv422i-yuyv") == 0) ||
          (strcmp(params.getPreviewFormat(), "rgb888") == 0) )) {
        FLOGE("Only yuv420sp or yuv420pis supported, but input format is %s",
              params.getPreviewFormat());
        return BAD_VALUE;
    }
    else {
        mPreviewPixelFormat = convertStringToPixelFormat(
                                 params.getPreviewFormat());
    }

    if (strcmp(params.getPictureFormat(), "jpeg") != 0) {
        FLOGE("Only jpeg still pictures are supported");
        return BAD_VALUE;
    }

    params.getPreviewSize(&w, &h);
    sprintf(tmp, "%dx%d", w, h);
    FLOGI("Set preview size: %s", tmp);
    if (strstr(mSupportedPreviewSizes, tmp) == NULL) {
        FLOGE("The preview size w %d, h %d is not corrected", w, h);
        return BAD_VALUE;
    }

    params.getPictureSize(&w, &h);
    sprintf(tmp, "%dx%d", w, h);
    FLOGI("Set picture size: %s", tmp);
    if (strstr(mSupportedPictureSizes, tmp) == NULL) {
        FLOGE("The picture size w %d, h %d is not corrected", w, h);
        return BAD_VALUE;
    }

    local_framerate = mParams.getPreviewFrameRate();
    FLOGI("get local frame rate:%d FPS", local_framerate);
    if ((local_framerate > 30) || (local_framerate < 0)) {
        FLOGE("The framerate is not corrected");
        local_framerate = 15;
    }

    framerate = params.getPreviewFrameRate();
    FLOGI("Set frame rate:%d FPS", framerate);
    if ((framerate > 30) || (framerate < 0)) {
        FLOGE("The framerate is not corrected");
        return BAD_VALUE;
    }
    else if (local_framerate != framerate) {
        if (framerate == 15) {
            params.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "12000,17000");
        }
        else if (framerate == 30) {
            params.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "25000,33000");
        }
    }

    int actual_fps = 15;
    params.getPreviewFpsRange(&min_fps, &max_fps);
    FLOGI("FPS range: %d - %d", min_fps, max_fps);
    if ((max_fps < 1000) || (min_fps < 1000) || (max_fps > 33000) ||
        (min_fps > 33000)) {
        FLOGE("The fps range from %d to %d is error", min_fps, max_fps);
        return BAD_VALUE;
    }
    actual_fps = min_fps > 15000 ? 30 : 15;
    FLOGI("setParameters: actual_fps=%d", actual_fps);
    params.setPreviewFrameRate(actual_fps);

    mParams = params;
    return NO_ERROR;
}


PixelFormat UvcDevice::getMatchFormat(int *sfmt,
                                     int  slen,
                                     int *dfmt,
                                     int  dlen)
{
    if ((sfmt == NULL) || (slen == 0) || (dfmt == NULL) || (dlen == 0)) {
        FLOGE("setSupportedPreviewFormats invalid parameters");
        return 0;
    }

    PixelFormat matchFormat = 0;
    bool live               = true;
    for (int i = 0; i < slen && live; i++) {
        for (int j = 0; j < dlen; j++) {
            FLOG_RUNTIME("sfmt[%d]=%c%c%c%c, dfmt[%d]=%c%c%c%c",
                         i,
                         sfmt[i] & 0xFF,
                         (sfmt[i] >> 8) & 0xFF,
                         (sfmt[i] >> 16) & 0xFF,
                         (sfmt[i] >> 24) & 0xFF,
                         j,
                         dfmt[j] & 0xFF,
                         (dfmt[j] >> 8) & 0xFF,
                         (dfmt[j] >> 16) & 0xFF,
                         (dfmt[j] >> 24) & 0xFF);
            if (sfmt[i] == dfmt[j]) {
                matchFormat = convertV4L2FormatToPixelFormat(dfmt[j]);
                live        = false;
                break;
            }
        }
    }

    return matchFormat;
}


status_t UvcDevice::setSupportedPreviewFormats(int *sfmt,
                                              int  slen,
                                              int *dfmt,
                                              int  dlen)
{
    if ((sfmt == NULL) || (slen == 0) || (dfmt == NULL) || (dlen == 0)) {
        FLOGE("setSupportedPreviewFormats invalid parameters");
        return BAD_VALUE;
    }

    char fmtStr[FORMAT_STRING_LEN];
    memset(fmtStr, 0, FORMAT_STRING_LEN);
    for (int i = 0; i < slen; i++) {
        for (int j = 0; j < dlen; j++) {
            // should report VPU support format.
            if (sfmt[i] == dfmt[j]) {
                if (sfmt[i] == v4l2_fourcc('Y', 'U', '1', '2')) {
                    strcat(fmtStr, "yuv420p");
                    strcat(fmtStr, ",");
                }
                else if (sfmt[i] == v4l2_fourcc('N', 'V', '1', '2')) {
                    strcat(fmtStr, "yuv420sp");
                    strcat(fmtStr, ",");
                }
                else if (sfmt[i] == v4l2_fourcc('Y', 'U', 'Y', 'V')) {
                    strcat(fmtStr, "yuv422i-yuyv");
                    strcat(fmtStr, ",");
                }
            }
        }
    }
    mParams.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, fmtStr);

    return NO_ERROR;
}

status_t UvcDevice::setPreviewStringFormat(PixelFormat format)
{
    const char *pformat = NULL;

    if (format == HAL_PIXEL_FORMAT_YCbCr_420_P) {
        pformat = "yuv420p";
    }
    else if (format == HAL_PIXEL_FORMAT_YCbCr_420_SP) {
        pformat = "yuv420sp";
    }
    else if (format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
        pformat = "yuv422i-yuyv";
    }
    else if (format == HAL_PIXEL_FORMAT_RGB_888) {
        pformat = "rgb888";
    }
    else {
        FLOGE("format %d is not supported", format);
        return BAD_VALUE;
    }

    mParams.setPreviewFormat(pformat);
	mParams.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, "yuv420sp");

    return NO_ERROR;
}


status_t UvcDevice::registerCameraFrames(CameraFrame *pBuffer,
                                             int        & num)
{
    status_t ret = NO_ERROR;

    if ((pBuffer == NULL) || (num <= 0)) {
        FLOGE("requestCameraBuffers invalid pBuffer");
        return BAD_VALUE;
    }

		
    mVideoInfo->rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mVideoInfo->rb.memory = V4L2_MEMORY_MMAP;
    mVideoInfo->rb.count  = num;
	
    ret = ioctl(mCameraHandle, VIDIOC_REQBUFS, &mVideoInfo->rb);
    if (ret < 0) {
        FLOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));		
        return ret;
    }


    for (int i = 0; i < num; i++) { 	
		
        memset(&mVideoInfo->buf, 0, sizeof(struct v4l2_buffer));
        mVideoInfo->buf.index    = i;
        mVideoInfo->buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;		
        mVideoInfo->buf.memory   = V4L2_MEMORY_MMAP;
			
	    ret = ioctl(mCameraHandle, VIDIOC_QUERYBUF, &mVideoInfo->buf);
	    if (ret < 0) {
	        FLOGE("Unable to query buffer (%s)", strerror(errno));
	        return ret;
	    }

		mMapedBuf[i].length = mVideoInfo->buf.length;
		mMapedBuf[i].offset = (size_t)mVideoInfo->buf.m.offset;
		mMapedBuf[i].start = (unsigned char*)mmap(NULL, mMapedBuf[i].length,
				 PROT_READ | PROT_WRITE, MAP_SHARED,
				 mCameraHandle, mMapedBuf[i].offset);			

	//	ALOGE("maped idx %d, len %d, phy 0x%x, vir 0x%x",
		//i, mMapedBuf[i].length, mMapedBuf[i].offset, mMapedBuf[i].start);	
    }


	for (int i = 0; i < num; i++) {         // Associate each Camera buffer
		CameraFrame *buffer = pBuffer + i;
		
        buffer->setObserver(this);
        mPreviewBufs.add((int)buffer, i);
		mMapedBufVector.add((int)&mMapedBuf[i], i);
	}
		
    mPreviewBufferSize  = pBuffer->mSize;
    mPreviewBufferCount = num;
		
    return ret;
}



status_t UvcDevice::initialize(const CameraInfo& info)
{
	status_t ret = NO_ERROR;

	ret = DeviceAdapter::initialize(info);
	if(ret == NO_ERROR)
		pDevPath = info.devPath;
		
    return ret;
}


status_t UvcDevice::setDeviceConfig(int         width,
                                        int         height,
                                        PixelFormat /*format*/,
                                        int         fps)
{		
	status_t ret = NO_ERROR;	

    if (mCameraHandle <= 0) {

		 if (pDevPath != NULL) {
            mCameraHandle = open(pDevPath, O_RDWR);
        }

		if (mCameraHandle <= 0) { 
        	FLOGE("setDeviceConfig: UvcDevice:: uninitialized");
        	return BAD_VALUE;
		}
	}
	
    if ((width == 0) || (height == 0)) {
        FLOGE("setDeviceConfig: invalid parameters");
        return BAD_VALUE;
    }    


    int vformat;
    vformat = v4l2_fourcc('Y', 'U', 'Y', 'V');

    if ((width > 1920) || (height > 1080)) {
        fps = 15;
    }
    FLOGI("Width * Height %d x %d format %d, fps: %d",
          width,
          height,
          vformat,
          fps);

    mVideoInfo->width       = width;
    mVideoInfo->height      = height;
    mVideoInfo->framesizeIn = (width * height << 1);
    mVideoInfo->formatIn    = vformat;

    mVideoInfo->param.type =
        V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mVideoInfo->param.parm.capture.timeperframe.numerator   = 1;
    mVideoInfo->param.parm.capture.timeperframe.denominator = fps;
    mVideoInfo->param.parm.capture.capturemode              = getCaptureMode(
        width,
        height);

	ret = ioctl(mCameraHandle, VIDIOC_S_PARM, &mVideoInfo->param);
    if (ret < 0) {
        FLOGE("Open: VIDIOC_S_PARM Failed: %s", strerror(errno));
        return ret;
    }

	memset(&mVideoInfo->format, 0, sizeof(mVideoInfo->format));
    mVideoInfo->format.type                 = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mVideoInfo->format.fmt.pix.width        = width & 0xFFFFFFF8;
    mVideoInfo->format.fmt.pix.height       = height & 0xFFFFFFF8;
    mVideoInfo->format.fmt.pix.pixelformat  = vformat;
    mVideoInfo->format.fmt.pix.priv         = 0;
    mVideoInfo->format.fmt.pix.sizeimage    = 0;
    mVideoInfo->format.fmt.pix.bytesperline = 0;


	FLOGI("Open: VIDIOC_S_FMT, w %d, h %d, pixel 0x%x",
		mVideoInfo->format.fmt.pix.width, mVideoInfo->format.fmt.pix.height,
		mVideoInfo->format.fmt.pix.pixelformat);
	
    ret = ioctl(mCameraHandle, VIDIOC_S_FMT, &mVideoInfo->format);
	if (ret < 0) {
        FLOGE("Open: VIDIOC_S_FMT Failed: %s", strerror(errno));
        return ret;
    }

    return ret;

}


status_t UvcDevice::startDeviceLocked()
{

    status_t ret = NO_ERROR;
	unsigned int phyAddr = 0;

    FSL_ASSERT(!mPreviewBufs.isEmpty());	
    FSL_ASSERT(mBufferProvider != NULL);
	FSL_ASSERT(!mMapedBufVector.isEmpty());	

    int queueableBufs = mBufferProvider->maxQueueableBuffers();
    FSL_ASSERT(queueableBufs > 0);

    for (int i = 0; i < queueableBufs; i++) {
		//FLOGI("i %d, num %d", i, queueableBufs);
		MemmapBuf *pMapdBuf = (MemmapBuf *)mMapedBufVector.keyAt(i);
		phyAddr = (unsigned int)pMapdBuf->offset;		
		mVideoInfo->buf.index    = i;
        mVideoInfo->buf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        mVideoInfo->buf.memory   = V4L2_MEMORY_MMAP;
        mVideoInfo->buf.m.offset = phyAddr;

		ALOGI("VIDIOC_QBUF, idx %d, phyAddr 0x%x", i, phyAddr);
        ret = ioctl(mCameraHandle, VIDIOC_QBUF, &mVideoInfo->buf);
	    if (ret < 0) {
            FLOGE("VIDIOC_QBUF Failed, %s, mCameraHandle %d", strerror(errno), mCameraHandle);
            return BAD_VALUE;
        }

        mQueued++;
    }


    enum v4l2_buf_type bufType;
    if (!mVideoInfo->isStreamOn) {
        bufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl(mCameraHandle, VIDIOC_STREAMON, &bufType);
		ALOGI("VIDIOC_STREAMON, ret %d", ret);
        if (ret < 0) {
            FLOGE("VIDIOC_STREAMON failed: %s", strerror(errno));
            return ret;
        }

        mVideoInfo->isStreamOn = true;
    }

    mDeviceThread = new DeviceThread(this);
		
    FLOGI("Created device thread");
	
    return ret;
}


status_t UvcDevice::stopDeviceLocked()
{
    status_t ret = NO_ERROR;

	ret = DeviceAdapter::stopDeviceLocked();
	if (ret != 0) {
        FLOGE("call %s failed", __FUNCTION__);
        return ret;
    }

	for (int i = 0; i < mPreviewBufferCount; i++) {
        if (mMapedBuf[i].start!= NULL && mMapedBuf[i].length > 0) {
            munmap(mMapedBuf[i].start, mMapedBuf[i].length);
        }
    }

	if (mCameraHandle > 0) {
        close(mCameraHandle);
        mCameraHandle = -1;
    }

    return ret;
}


status_t UvcDevice::fillCameraFrame(CameraFrame *frame)
{
    status_t ret = NO_ERROR;
	unsigned int phyAddr = 0;

    if (!mVideoInfo->isStreamOn) {
        return NO_ERROR;
    }

    int i = mPreviewBufs.valueFor((unsigned int)frame);
    if (i < 0) {
        return BAD_VALUE;
    }


	MemmapBuf *pMemmapBuf = (MemmapBuf *) mMapedBufVector.keyAt(i);
	phyAddr = (unsigned int)pMemmapBuf->offset;
//    FLOGI("==== VIDIOC_QBUF idx %d, phy 0x%x", i, phyAddr);

    struct v4l2_buffer cfilledbuffer;
    memset(&cfilledbuffer, 0, sizeof (struct v4l2_buffer));
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_MMAP;
    cfilledbuffer.index    = i;
    cfilledbuffer.m.offset = phyAddr;

    ret = ioctl(mCameraHandle, VIDIOC_QBUF, &cfilledbuffer);
    if (ret < 0) {
        FLOGE("fillCameraFrame: VIDIOC_QBUF Failed");
        return BAD_VALUE;
    }
    mQueued++;
	
    return ret;
}

CameraFrame * UvcDevice::acquireCameraFrame()
{
    int ret;	
	int n;
	struct v4l2_buffer cfilledbuffer;
	struct pollfd fdListen;
	int pollCount = 0;
	
dopoll:
	pollCount++;

	memset(&fdListen, 0, sizeof(fdListen));
	fdListen.fd = mCameraHandle;
	fdListen.events = POLLIN;	

    n = poll(&fdListen, 1, MAX_DEQUEUE_WAIT_TIME);

//	FLOGI("poll, n %d, use %lld ms, fd %d, event 0x%x, rEvent 0x%x, POLLIN 0x%x", 
	//	n, (pst - pre)/1000000, mCameraHandle, fdListen.events, fdListen.revents, POLLIN);

    if(n < 0) {
        FLOGE("Error!Query the V4L2 Handler state error.");
    }
    else if(n == 0) {
        FLOGI("Warning!Time out wait for V4L2 capture reading operation!");
    }
    else if(fdListen.revents & POLLIN) {
		memset(&cfilledbuffer, 0, sizeof (cfilledbuffer));
	    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	    cfilledbuffer.memory = V4L2_MEMORY_MMAP;
		
	    /* DQ */
	    ret = ioctl(mCameraHandle, VIDIOC_DQBUF, &cfilledbuffer);
	    if (ret < 0) {
	        FLOGE("GetFrame: VIDIOC_DQBUF Failed, fd  %d, erro %s", mCameraHandle, strerror(errno));
	        return NULL;
	    }
	    mDequeued++;
	
	    int index = cfilledbuffer.index;
	    FSL_ASSERT(!mPreviewBufs.isEmpty(), "mPreviewBufs is empty");		
		FSL_ASSERT(!mMapedBufVector.isEmpty(), "mMapedBufVector is empty");

		CameraFrame *camFrame = (CameraFrame *)mPreviewBufs.keyAt(index);
		MemmapBuf *pMapedBuf = (MemmapBuf *)mMapedBufVector.keyAt(index);

        // ALOGI("VIDIOC_DQBUF, idx %d, copy src 0x%x, %d, dst 0x%x, %d", index,
        //      pMapedBuf->start, pMapedBuf->length,
        //      camFrame->mVirtAddr, camFrame->mSize);

        uint32_t copySize = (pMapedBuf->length <= camFrame->mSize) ? pMapedBuf->length : camFrame->mSize;
		memcpy(camFrame->mVirtAddr, pMapedBuf->start, copySize);

	    return camFrame;
    }
	else {
        FLOGW("Poll the V4L2 Handler, revent 0x%x, pollCount %d", fdListen.revents, pollCount);
		usleep(10000);

	if(pollCount <= 3)
		goto dopoll;
		
    }

	return NULL;

}


void UvcDevice::onBufferDestroy()
{
    mPreviewBufs.clear();
	mMapedBufVector.clear();	
}


