/**
 *  Copyright (c) 2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file CameraSource.h
 *  @brief Class definition of CameraSource Component
 *  @ingroup CameraSource
 */

#ifndef CameraSource_h
#define CameraSource_h

#include <camera/Camera.h>
#include <camera/ICameraRecordingProxyListener.h>
#include <camera/CameraParameters.h>
#if (ANDROID_VERSION == JELLY_BEAN_42)
#include <gui/ISurface.h>
#endif
#include <gui/Surface.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include "VideoSource.h"

using android::sp;
using android::wp;
#if (ANDROID_VERSION == JELLY_BEAN_42)
using android::ISurface;
#endif
using android::Surface;
using android::Camera;
using android::ICamera;
using android::ICameraRecordingProxy;
using android::BnCameraRecordingProxyListener;
using android::IBinder;
using android::CameraListener;
using android::IMemory;
using android::CameraParameters;
using android::String8;

typedef struct {
    OMX_PTR pPhysicAdd;
    OMX_TICKS nTimeStamp;
}RECEIVED_VIDEO_BUFFER;

#define FRAME_BUFFER_QUEUE_SIZE 32

class CameraSource : public VideoSource {
    public:
        CameraSource();
		void dataCallbackTimestamp(OMX_TICKS timestamp, OMX_S32 msgType, OMX_PTR dataPtr);
	private:
		class ProxyListener: public BnCameraRecordingProxyListener {
			public:
				ProxyListener(CameraSource *source);
				virtual void dataCallbackTimestamp(int64_t timestampUs, int32_t msgType,
						const sp<IMemory> &data);

			private:
				CameraSource *mSource;
		};

		// isBinderAlive needs linkToDeath to work.
		class DeathNotifier: public IBinder::DeathRecipient {
			public:
				DeathNotifier() {}
				virtual void binderDied(const wp<IBinder>& who);
		};

		enum CameraFlags {
			FLAGS_SET_CAMERA = 1L << 0,
			FLAGS_HOT_CAMERA = 1L << 1,
		};

		OMX_ERRORTYPE InitSourceComponent();
        OMX_ERRORTYPE DeInitSourceComponent();
        OMX_ERRORTYPE OpenDevice();
        OMX_ERRORTYPE CloseDevice();
        OMX_ERRORTYPE SetDevice();
		OMX_ERRORTYPE SetSensorMode();
		OMX_ERRORTYPE PortFormatChanged(OMX_U32 nPortIndex);
		OMX_ERRORTYPE SetVideoFormat();
		OMX_ERRORTYPE SetWhiteBalance();
		OMX_ERRORTYPE SetDigitalZoom();
		OMX_ERRORTYPE SetExposureValue();
		OMX_ERRORTYPE SetRotation();
		OMX_ERRORTYPE StartDevice();
		void releaseRecordingFrame(const sp<IMemory>& frame);
		OMX_ERRORTYPE releaseQueuedFrames();
        OMX_ERRORTYPE StopDevice();
        OMX_ERRORTYPE SendBufferToDevice();
        OMX_ERRORTYPE GetOneFrameFromDevice();
        OMX_TICKS GetDelayofFrame();
        OMX_TICKS GetFrameTimeStamp();
        OMX_ERRORTYPE SetDeviceRotation();
        OMX_ERRORTYPE SetDeviceInputCrop();
        OMX_ERRORTYPE SetDeviceDisplayRegion();

        OMX_ERRORTYPE SearchBuffer(OMX_PTR pAddr, OMX_U32 *pIndex);
        OMX_ERRORTYPE SearchAddBuffer(OMX_PTR pAddr, OMX_U32 *pIndex);

		sp<Camera> mCamera;
		sp<ICameraRecordingProxy> mCameraRecordingProxy;
		sp<DeathNotifier> mDeathNotifier;
#if (ANDROID_VERSION == JELLY_BEAN_42)
		sp<ISurface> mPreviewSurface;
#endif
        sp<Surface> mPreviewSurface2;
		List<sp<IMemory> > VideoFrameList;
		sp<IMemory> mIMemory[FRAME_BUFFER_QUEUE_SIZE];
		OMX_PTR mVideoBufferPhy[FRAME_BUFFER_QUEUE_SIZE];
		Queue *ReceivedBufferQueue;
		OMX_PTR mListenerPtr;
		OMX_PTR mProxyListenerPtr;
		OMX_S32 mFlags;
		OMX_S32 nIMemoryIndex;
        OMX_CONFIG_RECTTYPE sRectIn;
        OMX_CONFIG_RECTTYPE sRectOut;
        OMX_BOOL bFrameBufferInit;
		OMX_BOOL mStarted;
        OMX_U32 nFrameBuffer;
        OMX_U32 nSourceFrames;
        fsl_osal_mutex lock;
		OMX_TICKS nCurTS;
        android::String16 * stringClientName;

};

#endif
/* File EOF */
