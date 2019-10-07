/**
 *  Copyright (c) 2011-2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file IpulibRender.h
 *  @brief Class definition of IpulibRender Component
 *  @ingroup IpulibRender
 */

#ifndef IpulibRender_h
#define IpulibRender_h

#include <stdint.h>
#include <linux/ipu.h>
#include "VideoRender.h"
#include <cutils/native_handle.h>

#define MAX_FB_BUFFERS 3

typedef struct {
    OMX_U32 rotation;
    OMX_U32 left;
    OMX_U32 top;
    OMX_U32 right;
    OMX_U32 bottom;
    OMX_U32 state;
}SURFACE_REGION;

typedef enum{
    TRANSCATION_STATE_NONE,
    TRANSCATION_STATE_PREPARE,
    TRANSCATION_STATE_START
}TRANSCATION_STATE;

class IpulibRender : public VideoRender {
    public:
        IpulibRender();
        friend void *ConfigScreenThreadFunc(fsl_osal_ptr arg);
    private:
        OMX_BOOL bInitDev;
        fsl_osal_mutex lock;
        int mIpuFd;
        struct ipu_task mTask;
        int mFb;
        OMX_CONFIG_RECTTYPE sRectOut;
        OMX_CONFIG_OUTPUTMODE sOutputMode;
        OMX_BOOL bCaptureFrameDone;
        OMX_CONFIG_CAPTUREFRAME sCapture;
        OMX_PTR pShowFrame;
        OMX_U32 nFrameLen;
        OMX_BOOL bSuspend;

        OMX_U32 nFBWidth;
        OMX_U32 nFBHeight;
        OMX_PTR pFbVAddrBase;
        OMX_PTR pFbPAddrBase;
        OMX_S32 nFbSize;
        OMX_PTR pFbPAddr[MAX_FB_BUFFERS];
        OMX_U32 nNextFb;
        native_handle_t* hSidebandHandle;
        OMX_BOOL bStopThread;
        OMX_PTR pThread;

        OMX_ERRORTYPE InitRenderComponent();
        OMX_ERRORTYPE DeInitRenderComponent();
        OMX_ERRORTYPE RenderGetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE RenderSetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE PortFormatChanged(OMX_U32 nPortIndex);
        OMX_ERRORTYPE OpenDevice();
        OMX_ERRORTYPE CloseDevice();
        OMX_ERRORTYPE ResetDevice();
        OMX_ERRORTYPE StartDeviceInPause();
        OMX_ERRORTYPE WriteDevice(OMX_BUFFERHEADERTYPE *pBufferHdr);

        OMX_ERRORTYPE Init();
        OMX_ERRORTYPE DeInit();
        OMX_ERRORTYPE RenderFrame(OMX_PTR pFrame);
        OMX_ERRORTYPE IpuInit();
        OMX_ERRORTYPE IpuDeInit();
        OMX_ERRORTYPE IpuProcessBuffer(OMX_PTR pFrame);
        OMX_ERRORTYPE FBInit();
        OMX_ERRORTYPE FBDeInit();
        OMX_ERRORTYPE FBShowFrame();
        OMX_ERRORTYPE SetFBAlpha(OMX_U32 alpha);
        OMX_ERRORTYPE CheckScreenConfig(OMX_BOOL *bTrans);
};

#endif
/* File EOF */
