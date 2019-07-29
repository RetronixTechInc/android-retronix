/*
 * Copyright (C) 2009 The Android Open Source Project
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

/* Copyright (C) 2011-2015 Freescale Semiconductor, Inc. */

#ifndef OMX_RECORDER_H_
#define OMX_RECORDER_H_

#include <media/MediaRecorderBase.h>
#include <utils/String8.h>
#include <camera/ICamera.h>
#include <gui/IGraphicBufferProducer.h>

namespace android {

struct OMXRecorder : public MediaRecorderBase {
    OMXRecorder(const String16 &opPackageName);
    virtual ~OMXRecorder();

    virtual status_t init();
    virtual status_t setAudioSource(audio_source_t as);
    virtual status_t setVideoSource(video_source vs);
    virtual status_t setOutputFormat(output_format of);
    virtual status_t setAudioEncoder(audio_encoder ae);
    virtual status_t setVideoEncoder(video_encoder ve);
    virtual status_t setVideoSize(int width, int height);
    virtual status_t setVideoFrameRate(int frames_per_second);
	virtual status_t setCamera(const sp<ICamera>& camera);
	virtual status_t setCamera(const sp<ICamera>& camera, const sp<ICameraRecordingProxy>& proxy);
    virtual status_t setPreviewSurface(const sp<IGraphicBufferProducer>& surface);
    virtual status_t setOutputFile(const char *path);
    virtual status_t setOutputFile(int fd, int64_t offset, int64_t length);
    virtual status_t setParameters(const String8& params);
    virtual status_t setListener(const sp<IMediaRecorderClient>& listener);
    virtual status_t setClientName(const String16& clientName);
    virtual status_t prepare();
    virtual status_t start();
    virtual status_t pause();
    virtual status_t stop();
    virtual status_t close();
    virtual status_t reset();
    virtual status_t getMaxAmplitude(int *max);
    virtual status_t dump(int fd, const Vector<String16>& args) const;
    status_t ProcessEvent(int msg, int ext1, int ext2);
    // Querying a SurfaceMediaSourcer
    virtual status_t setInputSurface(const sp<IGraphicBufferConsumer>& surface);
    virtual sp<IGraphicBufferProducer> querySurfaceMediaSource() const;

private:
    // Encoding parameter handling utilities
    status_t setParameter(const String8 &key, const String8 &value);

	sp<IMediaRecorderClient> mListener;
    void                *recorder;
	void                *cameraPtr;
	void                *cameraProxyPtr;
	sp<ICamera>			mCamera; 
	sp<ICameraRecordingProxy> mCameraProxy; 
    sp<IGraphicBufferProducer> mPreviewSurface;
    String16 mClientName;
    uid_t mClientUid;
    sp<IGraphicBufferConsumer> mBufferConsumer;

    OMXRecorder(const OMXRecorder &);
    OMXRecorder &operator=(const OMXRecorder &);
};

}  // namespace android

#endif  // OMX_RECORDER_H_

