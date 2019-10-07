/**
 *  Copyright (c) 2015, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file SurfaceSource.h
 *  @brief Class definition of SurfaceSource Component
 *  @ingroup SurfaceSource
 */

#ifndef SurfaceSource_h
#define SurfaceSource_h

#include <gui/IGraphicBufferProducer.h>
#include <gui/BufferQueue.h>
#include <utils/RefBase.h>

#include <gui/Surface.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include "VideoSource.h"


using android::sp;
using android::wp;
using android::Surface;
using android::IBinder;
using android::String8;

#define FRAME_BUFFER_QUEUE_SIZE 32

namespace android {


class SurfaceSource : public VideoSource {
    public:
        SurfaceSource();
        virtual ~SurfaceSource();


	private:

        struct EncodingBuffer {
            uint64_t nFrameNumber;

            int mBuf;

            sp<GraphicBuffer> spGraphicBuffer;
        };

		OMX_ERRORTYPE InitSourceComponent();
        OMX_ERRORTYPE DeInitSourceComponent();
        OMX_ERRORTYPE OpenDevice();
        OMX_ERRORTYPE CloseDevice();

		OMX_ERRORTYPE SetSensorMode();
		OMX_ERRORTYPE PortFormatChanged(OMX_U32 nPortIndex);
		OMX_ERRORTYPE SetVideoFormat();
		OMX_ERRORTYPE SetWhiteBalance();
		OMX_ERRORTYPE SetDigitalZoom();
		OMX_ERRORTYPE SetExposureValue();
		OMX_ERRORTYPE SetRotation();
		OMX_ERRORTYPE StartDevice();
        OMX_ERRORTYPE StopDevice();
        OMX_ERRORTYPE SendBufferToDevice();
        OMX_ERRORTYPE GetOneFrameFromDevice();
        OMX_TICKS GetDelayofFrame();
        OMX_TICKS GetFrameTimeStamp();

        OMX_ERRORTYPE SearchBuffer(OMX_PTR pAddr, OMX_U32 *pIndex);

        OMX_ERRORTYPE InstanceGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE InstanceSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);

        OMX_ERRORTYPE InitBuffers();
        OMX_ERRORTYPE DeinitBuffers();
        OMX_ERRORTYPE ReleaseBuffer(
            int id,
            uint64_t frameNum,
            const sp<GraphicBuffer> buffer);

        fsl_osal_mutex lock;
        OMX_U32     nFrameBuffer;
		OMX_TICKS   nCurTS;
        OMX_BOOL    bFormatSent;
        OMX_U32     nFramesAvailable;

        Vector<EncodingBuffer> mEncodingBuffers;
        sp<GraphicBuffer> mBufferSlot[BufferQueue::NUM_BUFFER_SLOTS];

        sp<IGraphicBufferProducer> mProducer;
        sp<IGraphicBufferConsumer> mConsumer;
        OMX_BOOL mIsPersistent;


        class GraphicBufferListener : public BufferQueue::ConsumerListener {
            public:
              GraphicBufferListener(SurfaceSource *ssource);
              virtual ~GraphicBufferListener();

            private:
                SurfaceSource * mSurfaceSource;
                virtual void onFrameAvailable(const BufferItem& item);
                virtual void onBuffersReleased();
                virtual void onSidebandStreamChanged() ;

        };

        sp <GraphicBufferListener> mListener;

        class PersisProxyListener : public BnConsumerListener {
             public:
                 PersisProxyListener(
                         const wp<IGraphicBufferConsumer> &consumer,
                         const wp<ConsumerListener>& consumerListener);
                 virtual ~PersisProxyListener();
                 virtual void onFrameAvailable(const BufferItem& item) override;
                 virtual void onFrameReplaced(const BufferItem& item) override;
                 virtual void onBuffersReleased() override;
                 virtual void onSidebandStreamChanged() override;
              private:

                 wp<ConsumerListener> mConsumerListener;

                 wp<IGraphicBufferConsumer> mConsumer;
         };


};

}

#endif
/* File EOF */
