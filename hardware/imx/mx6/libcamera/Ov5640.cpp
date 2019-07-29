/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2012-2016 Freescale Semiconductor, Inc.
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

#include "Ov5640.h"

#define DEFAULT_PREVIEW_FPS (15)
#define DEFAULT_PREVIEW_W   (640)
#define DEFAULT_PREVIEW_H   (480)
#define DEFAULT_PICTURE_W   (640)
#define DEFAULT_PICTURE_H   (480)


status_t Ov5640::initParameters(CameraParameters& params,
                                  int              *supportRecordingFormat,
                                  int               rfmtLen,
                                  int              *supportPictureFormat,
                                  int               pfmtLen)
{
    if (mCameraHandle < 0) {
        FLOGE("OvDevice: initParameters sensor has not been opened");
        return BAD_VALUE;
    }
    if ((supportRecordingFormat == NULL) || (rfmtLen == 0) ||
        (supportPictureFormat == NULL) || (pfmtLen == 0)) {
        FLOGE("OvDevice: initParameters invalid parameters");
        return BAD_VALUE;
    }

    // first read sensor format.
    int ret = 0, index = 0;
    int sensorFormat[MAX_SENSOR_FORMAT];
#if 0
    struct v4l2_fmtdesc vid_fmtdesc;
    while (ret == 0) {
        vid_fmtdesc.index = index;
        vid_fmtdesc.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret               = ioctl(mCameraHandle, VIDIOC_ENUM_FMT, &vid_fmtdesc);
        FLOG_RUNTIME("index:%d,ret:%d, format:%c%c%c%c", index, ret,
                     vid_fmtdesc.pixelformat & 0xFF,
                     (vid_fmtdesc.pixelformat >> 8) & 0xFF,
                     (vid_fmtdesc.pixelformat >> 16) & 0xFF,
                     (vid_fmtdesc.pixelformat >> 24) & 0xFF);
        if (ret == 0) {
            sensorFormat[index++] = vid_fmtdesc.pixelformat;
        }
    }
#endif // if 0

    // v4l2 does not support enum format, now hard code here.
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
    while (ret == 0) {
        memset(TmpStr, 0, 20);
        memset(&vid_frmsize, 0, sizeof(struct v4l2_frmsizeenum));
        vid_frmsize.index        = index++;
		vid_frmsize.pixel_format = v4l2_fourcc('Y', 'U', 'Y', 'V');
        ret                      = ioctl(mCameraHandle,
                                         VIDIOC_ENUM_FRAMESIZES,
                                         &vid_frmsize);
        if (ret == 0) {
            FLOG_RUNTIME("enum frame size w:%d, h:%d",
                         vid_frmsize.discrete.width, vid_frmsize.discrete.height);
			
            //omit large resolution
			if((vid_frmsize.discrete.width > 640) || (vid_frmsize.discrete.height > 480))
			{
				continue;
			}

            //Omit 176x144, since cts verifier, "Camera Video" test will use 176x144 for camera.
            //But 320x240 for SoftAVCEncoder, will not pass
            if((vid_frmsize.discrete.width == 176) && (vid_frmsize.discrete.height == 144))
			{
				continue;
			}

            memset(&vid_frmval, 0, sizeof(struct v4l2_frmivalenum));
            vid_frmval.index        = 0;
            vid_frmval.pixel_format = vid_frmsize.pixel_format;
            vid_frmval.width        = vid_frmsize.discrete.width;
            vid_frmval.height       = vid_frmsize.discrete.height;

            // ret = ioctl(mCameraHandle, VIDIOC_ENUM_FRAMEINTERVALS,
            // &vid_frmval);
            // v4l2 does not support, now hard code here.
            if (ret == 0) {
                FLOG_RUNTIME("vid_frmval denominator:%d, numeraton:%d",
                             vid_frmval.discrete.denominator,
                             vid_frmval.discrete.numerator);
                if ((vid_frmsize.discrete.width > 1920) ||
                    (vid_frmsize.discrete.height > 1080)) {
                    vid_frmval.discrete.denominator = 15;
                    vid_frmval.discrete.numerator   = 1;
                }
                else {
                    vid_frmval.discrete.denominator = 30;
                    vid_frmval.discrete.numerator   = 1;
                }

                sprintf(TmpStr,
                        "%dx%d",
                        vid_frmsize.discrete.width,
                        vid_frmsize.discrete.height);
                if (pictureCnt == 0)
                    strncpy((char *)mSupportedPictureSizes,
                            TmpStr,
                            CAMER_PARAM_BUFFER_SIZE);
                else {
                    strncat(mSupportedPictureSizes,
                            PARAMS_DELIMITER,
                            CAMER_PARAM_BUFFER_SIZE);
                    strncat(mSupportedPictureSizes,
                            TmpStr,
                            CAMER_PARAM_BUFFER_SIZE);
                }
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
            }
        }
    } // end while

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
                "(12000,30000),(15000,15000),(30000,30000)");
    // Align the default FPS RANGE to the DEFAULT_PREVIEW_FPS
    mParams.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "12000,17000");

    mParams.setPreviewSize(DEFAULT_PREVIEW_W, DEFAULT_PREVIEW_H);
    mParams.setPictureSize(DEFAULT_PICTURE_W, DEFAULT_PICTURE_H);
    mParams.setPreviewFrameRate(DEFAULT_PREVIEW_FPS);

    params = mParams;
    return NO_ERROR;
}

status_t Ov5640::setParameters(CameraParameters& params)
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
          (strcmp(params.getPreviewFormat(), "yuv422i-yuyv") == 0))) {
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
            params.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "15000,15000");
        }
        else if (framerate == 30) {
            params.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "30000,30000");
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

