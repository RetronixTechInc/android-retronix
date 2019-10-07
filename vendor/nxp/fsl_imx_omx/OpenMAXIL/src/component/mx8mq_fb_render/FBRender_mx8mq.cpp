/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */

#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/mxcfb.h>

#include "FBRender_mx8mq.h"

#define PAGE_SHIFT      12
#ifndef PAGE_SIZE
#define PAGE_SIZE       (1 << PAGE_SHIFT)
#endif

#define DISP_WIDTH 1024
#define DISP_HEIGHT 768

#define ABS(a) (((a)>0)?(a):-(a))

static struct sigaction default_act;
static void segfault_signal_handler(int signum);

static void RegistSignalHandler()
{
    struct sigaction act;

    act.sa_handler = segfault_signal_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    fsl_osal_memset(&default_act, 0, sizeof(struct sigaction));
    sigaction(SIGSEGV, &act, &default_act);

    return;
}

static void UnRegistSignalHandler()
{
    struct sigaction act;

    sigaction(SIGSEGV, &default_act, NULL);

    return;
}

static void segfault_signal_handler(int signum)
{
    default_act.sa_handler(SIGSEGV);

    return;
}


FBRender::FBRender()
{
    OMX_U32 i;

    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_render.mx8mqfb.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"video_render.mx8mqfb";

    nFrameBufferMin = 2;
    nFrameBufferActual = 2;
    TunneledSupplierType = OMX_BufferSupplyInput;
    fsl_osal_memset(&sVideoFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    sVideoFmt.nFrameWidth = 320;
    sVideoFmt.nFrameHeight = 240;
    sVideoFmt.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    sVideoFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

    eRotation = ROTATE_NONE;
    OMX_INIT_STRUCT(&sRectIn, OMX_CONFIG_RECTTYPE);
    sRectIn.nWidth = sVideoFmt.nFrameWidth;
    sRectIn.nHeight = sVideoFmt.nFrameHeight;
    sRectIn.nTop = 0;
    sRectIn.nLeft = 0;
    OMX_INIT_STRUCT(&sRectOut, OMX_CONFIG_RECTTYPE);
    sRectOut.nWidth = DISP_WIDTH;
    sRectOut.nHeight = DISP_HEIGHT;
    sRectOut.nTop = 0;
    sRectOut.nLeft = 0;

    bInitDev = OMX_FALSE;
    lock = NULL;
    pShowFrame = NULL;
    nFrameLen = 0;
    bSuspend = OMX_FALSE;

    OMX_INIT_STRUCT(&sCapture, OMX_CONFIG_CAPTUREFRAME);
    sCapture.eType = CAP_NONE;

    mFb = -1;
    bCaptureFrameDone = OMX_FALSE;
    hSidebandHandle = NULL;
    fsl_osal_memset(preBufferHdr, 0, sizeof(preBufferHdr));
    fsl_osal_memset(&sOutputMode, 0, sizeof(OMX_CONFIG_OUTPUTMODE));
    fsl_osal_memset(&sOriFBInfo, 0, sizeof(fb_var_screeninfo));
    fsl_osal_memset(&sCurFBInfo, 0, sizeof(fb_var_screeninfo));
}

OMX_ERRORTYPE FBRender::InitRenderComponent()
{
    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
        LOG_ERROR("Create mutex for v4l device failed.\n");
        return OMX_ErrorInsufficientResources;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::DeInitRenderComponent()
{
    CloseDevice();

    if(lock)
        fsl_osal_mutex_destroy(lock);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::RenderGetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexOutputMode:
            {
                OMX_CONFIG_OUTPUTMODE *pOutputMode;
                pOutputMode = (OMX_CONFIG_OUTPUTMODE*)pStructure;
                CHECK_STRUCT(pOutputMode, OMX_CONFIG_OUTPUTMODE, ret);
                sOutputMode.bSetupDone = OMX_TRUE;
                fsl_osal_memcpy(pOutputMode, &sOutputMode, sizeof(OMX_CONFIG_OUTPUTMODE));
            }
            break;
        case OMX_IndexConfigCaptureFrame:
            {
                OMX_CONFIG_CAPTUREFRAME *pCapture;
                pCapture = (OMX_CONFIG_CAPTUREFRAME*)pStructure;
                CHECK_STRUCT(pCapture, OMX_CONFIG_CAPTUREFRAME, ret);
                pCapture->bDone = bCaptureFrameDone;
                if(sCapture.nFilledLen == 0)
                    ret = OMX_ErrorUndefined;
            }
            break;
        case OMX_IndexSysSleep:
            {
                OMX_CONFIG_SYSSLEEP *pSysSleep = (OMX_CONFIG_SYSSLEEP *)pStructure;
                CHECK_STRUCT(pSysSleep, OMX_CONFIG_SYSSLEEP, ret);
                pSysSleep->bSleep = bSuspend;
            }
            break;
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}

OMX_ERRORTYPE FBRender::RenderSetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexOutputMode:
            {
                OMX_CONFIG_OUTPUTMODE *pOutputMode;
                pOutputMode = (OMX_CONFIG_OUTPUTMODE*)pStructure;
                CHECK_STRUCT(pOutputMode, OMX_CONFIG_OUTPUTMODE, ret);
                fsl_osal_memcpy(&sOutputMode, pOutputMode, sizeof(OMX_CONFIG_OUTPUTMODE));
                fsl_osal_memcpy(&sRectIn, &pOutputMode->sRectIn, sizeof(OMX_CONFIG_RECTTYPE));
                fsl_osal_memcpy(&sRectOut, &pOutputMode->sRectOut, sizeof(OMX_CONFIG_RECTTYPE));
                eRotation = pOutputMode->eRotation;
                ResetDevice();
            }
            break;
        case OMX_IndexConfigCaptureFrame:
            {
                OMX_CONFIG_CAPTUREFRAME *pCapture;
                pCapture = (OMX_CONFIG_CAPTUREFRAME*)pStructure;
                CHECK_STRUCT(pCapture, OMX_CONFIG_CAPTUREFRAME, ret);
                sCapture.eType = pCapture->eType;
                sCapture.pBuffer = pCapture->pBuffer;
                bCaptureFrameDone = OMX_FALSE;
                if (sCapture.eType == CAP_SNAPSHOT) {
                    if(pShowFrame == NULL) {
                        ret = OMX_ErrorUndefined;
                        break;
                    }
                    fsl_osal_memcpy(sCapture.pBuffer, pShowFrame, nFrameLen);
                    sCapture.nFilledLen = nFrameLen;
                    bCaptureFrameDone = OMX_TRUE;
                }
            }
            break;
        case OMX_IndexSysSleep:
            OMX_CONFIG_SYSSLEEP *pSysSleep;
            pSysSleep = (OMX_CONFIG_SYSSLEEP *)pStructure;
            bSuspend = pSysSleep->bSleep;
            fsl_osal_mutex_lock(lock);
            if(OMX_TRUE == bSuspend) {
                CloseDevice();
            }
            else {
                OMX_STATETYPE eState = OMX_StateInvalid;
                GetState(&eState);
                if(eState == OMX_StatePause)
                    StartDeviceInPause();
            }
            fsl_osal_mutex_unlock(lock);
            break;
        case OMX_IndexParamConfigureVideoTunnelMode:
            hSidebandHandle = (native_handle_t*)pStructure;
            break;
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}

OMX_ERRORTYPE FBRender::PortFormatChanged(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_BOOL bFmtChanged = OMX_FALSE;

    if(nPortIndex != IN_PORT)
        return OMX_ErrorBadPortIndex;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    ports[IN_PORT]->GetPortDefinition(&sPortDef);

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

    if(bFmtChanged == OMX_TRUE)
        ResetDevice();

    return ret;
}

OMX_ERRORTYPE FBRender::OpenDevice()
{
    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::ResetDevice()
{
    fsl_osal_mutex_lock(lock);


    CloseDevice();

    OMX_STATETYPE eState = OMX_StateInvalid;
    GetState(&eState);
    if(eState == OMX_StatePause && bSuspend != OMX_TRUE)
        StartDeviceInPause();

    fsl_osal_mutex_unlock(lock);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::CloseDevice()
{

    if(bInitDev != OMX_TRUE)
        return OMX_ErrorNone;

    DeInit();
    bInitDev = OMX_FALSE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::StartDeviceInPause()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(bInitDev != OMX_FALSE)
        return OMX_ErrorNone;

    if(pShowFrame == NULL)
        return OMX_ErrorBadParameter;

    OMX_PTR pFrame = NULL;
    GetHwBuffer(pShowFrame, &pFrame);

    ret = Init();
    if(ret != OMX_ErrorNone)
        return ret;

    bInitDev = OMX_TRUE;

    ret = RenderFrame(pFrame);
    if(ret != OMX_ErrorNone)
        return ret;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::WriteDevice(
        OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    fsl_osal_mutex_lock(lock);

    if(sCapture.eType == CAP_THUMBNAL) {
        fsl_osal_memcpy(sCapture.pBuffer, pBufferHdr->pBuffer, pBufferHdr->nFilledLen);
        sCapture.nFilledLen = pBufferHdr->nFilledLen;
        bCaptureFrameDone = OMX_TRUE;
        ports[0]->SendBuffer(pBufferHdr);
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorNone;
    }

    if(pBufferHdr->nFilledLen == 0) {
        ports[0]->SendBuffer(pBufferHdr);
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorNone;
    }

    pShowFrame = pBufferHdr->pBuffer;
    nFrameLen = pBufferHdr->nFilledLen;

    if(bSuspend == OMX_TRUE) {
        ports[0]->SendBuffer(pBufferHdr);
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorNone;
    }

    OMX_PTR pFrame = NULL;
    GetHwBuffer(pBufferHdr->pBuffer, &pFrame);

    if(bInitDev != OMX_TRUE) {
        ret = Init();
        if(ret != OMX_ErrorNone) {
            ports[0]->SendBuffer(pBufferHdr);
            fsl_osal_mutex_unlock(lock);
            return ret;
        }
        bInitDev = OMX_TRUE;
    }

    ret = RenderFrame(pFrame);
    if(ret != OMX_ErrorNone) {
        ports[0]->SendBuffer(pBufferHdr);
        fsl_osal_mutex_unlock(lock);
        return ret;
    }

    /* fb1 hold one buffer not return to decoder to make video smooth */
    if (preBufferHdr[0] != NULL) {
        ports[0]->SendBuffer(preBufferHdr[0]);
    }
    preBufferHdr[0]  = preBufferHdr[1];
    preBufferHdr[1] = pBufferHdr;

    fsl_osal_mutex_unlock(lock);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::Init()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = FBInit();
    if(ret != OMX_ErrorNone)
        return ret;

    RegistSignalHandler();

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::DeInit()
{
    FBDeInit();
    UnRegistSignalHandler();

    return OMX_ErrorNone;
}


OMX_ERRORTYPE FBRender::FBInit()
{
    int  retval = 0;
#ifdef ANDROID_BUILD
    OMX_STRING device_name = (OMX_STRING)"/dev/graphics/fb1";
#else
    OMX_STRING device_name = (OMX_STRING)"/dev/fb1";
#endif

    mFb = open(device_name, O_RDWR | O_NONBLOCK, 0);
    if(mFb < 0) {
        LOG_ERROR("Open device: %s failed.\n", device_name);
        return OMX_ErrorHardware;
    }

    retval = ioctl(mFb, FBIOGET_VSCREENINFO, &sOriFBInfo);
    if (retval < 0) {
        close(mFb);
        return OMX_ErrorHardware;
    }

    fsl_osal_memcpy(&sCurFBInfo, &sOriFBInfo, sizeof(fb_var_screeninfo));

    OMX_U32 videoFmt, bits;
    if(sVideoFmt.eColorFormat == OMX_COLOR_FormatYUV420Planar) {
        videoFmt = V4L2_PIX_FMT_YUV420;
        bits = 8;
    }
    else if(sVideoFmt.eColorFormat == OMX_COLOR_FormatYUV420SemiPlanar) {
        videoFmt = V4L2_PIX_FMT_NV12;
        bits = 8;
    }
    else if(sVideoFmt.eColorFormat == OMX_COLOR_FormatYUV422Planar) {
        videoFmt = V4L2_PIX_FMT_YUV422P;
        bits = 16;
    }
    else{
        LOG_ERROR("Unsupported color format : %d \n",sVideoFmt.eColorFormat);
        return OMX_ErrorHardware;
    }

    sCurFBInfo.xoffset = 0;
    sCurFBInfo.yoffset = 0;
    sCurFBInfo.bits_per_pixel = bits;
    sCurFBInfo.grayscale = videoFmt;
    sCurFBInfo.nonstd = videoFmt;
    sCurFBInfo.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
    sCurFBInfo.xres = sVideoFmt.nFrameWidth;
    sCurFBInfo.yres = sVideoFmt.nFrameHeight;
    sCurFBInfo.yres_virtual = sCurFBInfo.yres;
    sCurFBInfo.xres_virtual = sCurFBInfo.xres;

    retval = ioctl(mFb, FBIOPUT_VSCREENINFO, &sCurFBInfo);
    if (retval < 0) {
        close(mFb);
        return OMX_ErrorHardware;
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::FBDeInit()
{
    int  retval = 0;

    for (OMX_U32 i = 0; i < sizeof(preBufferHdr)/sizeof(preBufferHdr[0]); i++) {
        ports[0]->SendBuffer(preBufferHdr[i]);
        preBufferHdr[i] = NULL;
    }

    if(mFb) {
        retval = ioctl(mFb, FBIOPUT_VSCREENINFO, &sOriFBInfo);
        if (retval < 0)
            LOG_ERROR("Fail to set fb vscreen info ro original.");

        close(mFb);
        mFb = 0;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FBRender::RenderFrame(OMX_PTR pFrame)
{
    int  retval = 0;
    unsigned long addr = (unsigned long)pFrame;

    sCurFBInfo.xoffset = 0;
    sCurFBInfo.yoffset = 0;
    sCurFBInfo.reserved[0] = static_cast<OMX_U32>(addr);
    if (sizeof(addr) > 32)
        sCurFBInfo.reserved[1] = static_cast<OMX_U32>(addr >> 32);
    sCurFBInfo.reserved[2] = 0;
    sCurFBInfo.activate = FB_ACTIVATE_VBL;
    retval = ioctl(mFb, FBIOPAN_DISPLAY, &sCurFBInfo);
    if (retval < 0) {
        LOG_ERROR("updateScreen: FBIOPAN_DISPLAY failed, retval:%d", retval);
        return OMX_ErrorUndefined;
    }

    int fenceFd = (sCurFBInfo.reserved[3] == 0 ? -1 : sCurFBInfo.reserved[3]);
    if(fenceFd > 0) {
        close(fenceFd);
    }

    return OMX_ErrorNone;
}


/**< C style functions to expose entry point for the shared library */
    extern "C" {
        OMX_ERRORTYPE MX8MQFBRenderInit(OMX_IN OMX_HANDLETYPE pHandle)
        {
            OMX_ERRORTYPE ret = OMX_ErrorNone;
            FBRender *obj = NULL;
            ComponentBase *base = NULL;

            obj = FSL_NEW(FBRender, ());
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
