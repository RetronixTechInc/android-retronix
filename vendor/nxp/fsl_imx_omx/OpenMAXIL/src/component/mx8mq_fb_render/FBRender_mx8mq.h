/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file FBRender_mx8mq.h
 *  @brief Class definition of FBRender Component
 *  @ingroup FBRender
 */

#ifndef FBRender_mx8mq_h
#define FBRender_mx8mq_h

#include <stdint.h>
#include <linux/ipu.h>
#include "VideoRender.h"
#include <cutils/native_handle.h>


class FBRender : public VideoRender {
    public:
        FBRender();
    private:
        OMX_BOOL bInitDev;
        fsl_osal_mutex lock;
        int mFb;
        OMX_CONFIG_RECTTYPE sRectOut;
        OMX_CONFIG_OUTPUTMODE sOutputMode;
        OMX_BOOL bCaptureFrameDone;
        OMX_CONFIG_CAPTUREFRAME sCapture;
        OMX_PTR pShowFrame;
        OMX_U32 nFrameLen;
        OMX_BOOL bSuspend;

        native_handle_t* hSidebandHandle;
        OMX_BUFFERHEADERTYPE* preBufferHdr[2];
        struct fb_var_screeninfo sOriFBInfo;
        struct fb_var_screeninfo sCurFBInfo;

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
        OMX_ERRORTYPE FBInit();
        OMX_ERRORTYPE FBDeInit();
};

#endif
/* File EOF */
