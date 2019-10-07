/**
 *  Copyright (c) 2011-2016, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#include "SurfaceRender.h"
#include "ui/GraphicBufferMapper.h"
#include "ui/Rect.h"
#if (ANDROID_VERSION <= ICS)
#include "Surface.h"
#endif

#define ASSERT(exp)	if(!(exp)) {printf("%s: %d : assert condition !!!\r\n",__FUNCTION__,__LINE__);}

using android::GraphicBufferMapper;
using android::Rect;

SurfaceRender::SurfaceRender()
{
    OMX_U32 i;

    LOG_DEBUG("initializing SurfaceRender\n");

    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_render.surface.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"video_render.surface";

    nFrameBufferMin = 3;
    nFrameBufferActual = 3;
    nFrameBuffer = nFrameBufferActual;
    TunneledSupplierType = OMX_BufferSupplyInput;
    fsl_osal_memset(&sVideoFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    sVideoFmt.nFrameWidth = 320;
    sVideoFmt.nFrameHeight = 240;
    sVideoFmt.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    sVideoFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

    OMX_INIT_STRUCT(&sRectIn, OMX_CONFIG_RECTTYPE);
    sRectIn.nWidth = sVideoFmt.nFrameWidth;
    sRectIn.nHeight = sVideoFmt.nFrameHeight;
    sRectIn.nTop = 0;
    sRectIn.nLeft = 0;
    eRotation = ROTATE_NONE;

    for(i=0; i<MAX_SURFACE_BUFFER; i++)
        fsl_osal_memset(&fb_data[i], 0, sizeof(FB_DATA));

    bFrameBufferInit = OMX_FALSE;
    nAllocating = 0;
    lock = NULL;

    OMX_INIT_STRUCT(&sCapture, OMX_CONFIG_CAPTUREFRAME);
    sCapture.eType = CAP_NONE;
    bCaptureFrameDone = OMX_FALSE;
    EnqueueBufferIdx = -1;
    nMinUndequeuedBuffers = 2;
    OMX_INIT_STRUCT(&sOutputMode, OMX_CONFIG_OUTPUTMODE);

    nativeWindow = NULL;
    bSuspend = OMX_FALSE;
    bFirstRender = OMX_TRUE;
    mBufferUsage = 0;
    nEnqueuedBuffers = 0;
    nFrameNumber = 0;
#if (ANDROID_VERSION >= JELLY_BEAN_42)
    mScalingMode = NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW;
#endif

}

OMX_ERRORTYPE SurfaceRender::InitRenderComponent()
{
    LOG_DEBUG("SurfaceRender::InitRenderComponent()\n");

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
        LOG_ERROR("Create mutex for surface device failed.\n");
        return OMX_ErrorInsufficientResources;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::DeInitRenderComponent()
{
    LOG_DEBUG("SurfaceRender::DeInitRenderComponent()\n");

    if(lock){
        fsl_osal_mutex_destroy(lock);
        lock = NULL;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::RenderGetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    LOG_DEBUG("SurfaceRender::RenderGetConfig()\n");

    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexOutputMode:
            {
                LOG_DEBUG("OMX_IndexOutputMode\n");
                OMX_CONFIG_OUTPUTMODE *pOutputMode;
                pOutputMode = (OMX_CONFIG_OUTPUTMODE*)pStructure;
                CHECK_STRUCT(pOutputMode, OMX_CONFIG_OUTPUTMODE, ret);
                sOutputMode.bSetupDone = OMX_TRUE;
                fsl_osal_memcpy(pOutputMode, &sOutputMode, sizeof(OMX_CONFIG_OUTPUTMODE));
            }
            break;
        case OMX_IndexConfigCaptureFrame:
            {
                LOG_DEBUG("OMX_IndexConfigCaptureFrame\n");
                OMX_CONFIG_CAPTUREFRAME *pCapture;
                pCapture = (OMX_CONFIG_CAPTUREFRAME*)pStructure;
                CHECK_STRUCT(pCapture, OMX_CONFIG_CAPTUREFRAME, ret);
                pCapture->bDone = bCaptureFrameDone;
                //if(sCapture.nFilledLen == 0)
                //    ret = OMX_ErrorUndefined;
            }
            break;
        case OMX_IndexSysSleep:
            {
                LOG_DEBUG("OMX_IndexSysSleep\n");
                OMX_CONFIG_SYSSLEEP *pSysSleep;
                pSysSleep = (OMX_CONFIG_SYSSLEEP *)pStructure;
                CHECK_STRUCT(pSysSleep, OMX_CONFIG_SYSSLEEP, ret);
                pSysSleep->bSleep = bSuspend;
            }
            break;

        default:
            LOG_DEBUG("OMX_ErrorUnsupportedIndex %d\n", nParamIndex);
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}


OMX_ERRORTYPE SurfaceRender::RenderSetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("SurfaceRender::RenderSetConfig\n");

    switch ((int)nParamIndex) {
        case OMX_IndexOutputMode:
            {
                LOG_DEBUG("OMX_IndexOutputMode\n");
                OMX_CONFIG_OUTPUTMODE *pOutputMode;
                pOutputMode = (OMX_CONFIG_OUTPUTMODE*)pStructure;
                CHECK_STRUCT(pOutputMode, OMX_CONFIG_OUTPUTMODE, ret);
                fsl_osal_memcpy(&sOutputMode, pOutputMode, sizeof(OMX_CONFIG_OUTPUTMODE));

                if(pOutputMode->sRectIn.nWidth <= 0 || pOutputMode->sRectIn.nHeight <= 0){
                    LOG_ERROR("Invalide sRectIn\n");
                    break;
                }

                if(fsl_osal_memcmp(&sRectIn, &pOutputMode->sRectIn, sizeof(OMX_CONFIG_RECTTYPE)) != 0){
                    fsl_osal_memcpy(&sRectIn, &pOutputMode->sRectIn, sizeof(OMX_CONFIG_RECTTYPE));
                    LOG_DEBUG("sRectIn: w %d, h %d, left %d, top %d\n", sRectIn.nWidth, sRectIn.nHeight, sRectIn.nLeft, sRectIn.nTop);
                    SetDeviceInputCrop();
                }
            }
            break;
        case OMX_IndexConfigCaptureFrame:
            {
                LOG_DEBUG("OMX_IndexConfigCaptureFrame\n");

                OMX_CONFIG_CAPTUREFRAME *pCapture;
                pCapture = (OMX_CONFIG_CAPTUREFRAME*)pStructure;
                CHECK_STRUCT(pCapture, OMX_CONFIG_CAPTUREFRAME, ret);
                sCapture.eType = pCapture->eType;
                sCapture.pBuffer = pCapture->pBuffer;
                LOG_DEBUG("ConfigCaptureFrame: eType %d, pBuffer %p\n", sCapture.eType, sCapture.pBuffer);
                bCaptureFrameDone = OMX_FALSE;
                if (sCapture.eType == CAP_SNAPSHOT)
                {
                    CaptureFrame(EnqueueBufferIdx);
                    bCaptureFrameDone = OMX_TRUE;
                }
            }
            break;
        case OMX_IndexSysSleep:
            {
                LOG_DEBUG("OMX_IndexSysSleep\n");

                OMX_CONFIG_SYSSLEEP *pSysSleep;
                pSysSleep = (OMX_CONFIG_SYSSLEEP *)pStructure;
                CHECK_STRUCT(pSysSleep, OMX_CONFIG_SYSSLEEP, ret);
                bSuspend = pSysSleep->bSleep;
                if (bSuspend)
                    {}
                else
                    {}
            }
            break;
        case OMX_IndexParamSurface:
            {
                LOG_DEBUG("OMX_IndexParamSurface\n");
                fsl_osal_mutex_lock(lock);

                sp<ANativeWindow> *pNativWin = (sp<ANativeWindow> *)pStructure;
                if(nativeWindow != *pNativWin){
                    LOG_DEBUG("surface changed!!!\n");
                    nativeWindow = *pNativWin;
                }

                fsl_osal_mutex_unlock(lock);
            }
            break;
        case OMX_IndexParamBufferUsage:
            {
                fsl_osal_mutex_lock(lock);
                OMX_U32 *pUsage = (OMX_U32*) pStructure;
                mBufferUsage = *pUsage;
                fsl_osal_mutex_unlock(lock);
            }
            break;
        case OMX_IndexConfigScalingMode:
            {
                OMX_S32 *pMode;
                pMode = (OMX_S32 *)pStructure;
                mScalingMode = *pMode;
                SetScalingMode();
            }
            break;
        default:
            LOG_DEBUG("unknown index: %d\n",nParamIndex);
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}
OMX_ERRORTYPE SurfaceRender::SetDeviceRotation()
{

    OMX_S32 transform = 0;
    if(nativeWindow == NULL){
        LOG_ERROR("nativeWindow is NULL!\n");
        return OMX_ErrorUndefined;
    }
    fsl_osal_mutex_lock(lock);

    switch(eRotation){
        case ROTATE_90_LEFT:
            transform = HAL_TRANSFORM_ROT_90;
            break;
        case ROTATE_180:
            transform = HAL_TRANSFORM_ROT_180;
            break;
        case ROTATE_90_RIGHT:
            transform = HAL_TRANSFORM_ROT_270;
            break;
        default:
            break;
    }
    LOG_DEBUG("SurfaceRender: SetDeviceRotation: %0x\n", transform);
    if (transform) {
        native_window_set_buffers_transform(nativeWindow.get(), transform);
    }

    fsl_osal_mutex_unlock(lock);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE SurfaceRender::PortFormatChanged(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_BOOL bFmtChanged = OMX_FALSE;

    if(nPortIndex != IN_PORT)
        return OMX_ErrorBadPortIndex;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    ports[IN_PORT]->GetPortDefinition(&sPortDef);

    LOG_DEBUG("SurfaceRender::PortFormatChanged, old w/h %d/%d, color %d bufcnt %d, new w/d %d/%d, color %d, bufcnt %d\n",
        sVideoFmt.nFrameWidth, sVideoFmt.nFrameHeight, sVideoFmt.eColorFormat, nFrameBuffer,
        sPortDef.format.video.nFrameWidth, sPortDef.format.video.nFrameHeight, sPortDef.format.video.eColorFormat, sPortDef.nBufferCountActual);

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
    if(nFrameBuffer != sPortDef.nBufferCountActual ) {
        nFrameBuffer = sPortDef.nBufferCountActual ;
        bFmtChanged = OMX_TRUE;
    }

    if(nFrameBuffer  > MAX_SURFACE_BUFFER){
        LOG_ERROR("buffer count overflow: nFrameBuffer %d\n", nFrameBuffer);
        nFrameBuffer  = MAX_SURFACE_BUFFER ;
    }

    if(bFmtChanged ) {
        LOG_DEBUG("====== format changed !!! ========\n");
        if(bFrameBufferInit==OMX_TRUE){
            CloseDevice();
            OpenDevice();
        }
    }

    return ret;
}

OMX_ERRORTYPE SurfaceRender::DoAllocateBuffer(
        OMX_PTR *buffer,
        OMX_U32 nSize,
        OMX_U32 nPortIndex)
{
    OMX_U32 i = 0;

    //LOG_DEBUG("SurfaceRender: DoAllocateBuffer, nSize %d\n", nSize);

    while(bFrameBufferInit != OMX_TRUE) {
        usleep(10000);
        i++;
        if(i >= 10)
            return OMX_ErrorHardware;
    }

    if(nAllocating >= nFrameBuffer ) {
        LOG_ERROR("Requested buffer number beyond system can provide: [%:%]\n", nAllocating, nFrameBuffer );
        return OMX_ErrorInsufficientResources;
    }

    if(nAllocating >= nFrameBuffer ) {
        LOG_ERROR("Requested buffer number beyond vpu maximum: [%:%]\n", nAllocating, nFrameBuffer );
    }
    // nLength is not provided by hwc
/*
    if(nSize > fb_data[nAllocating].nLength){
        LOG_ERROR("Requested buffer length beyond system can provide: [nSize %d, nAllocating %d, bufferLen %d]\n", nSize, nAllocating, fb_data[nAllocating].nLength);
        return OMX_ErrorInsufficientResources;
    }
*/
    *buffer = fb_data[nAllocating].pVirtualAddr;
    nAllocating ++;

    LOG_DEBUG("SurfaceRender: DoAllocateBuffer end, nAllocating %d\n", nAllocating);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::DoFreeBuffer(
        OMX_PTR buffer,
        OMX_U32 nPortIndex)
{
    LOG_DEBUG("SurfaceRender: DoFreeBuffer\n");
    nAllocating --;
    return OMX_ErrorNone;
}


OMX_ERRORTYPE SurfaceRender::OpenDevice()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_DEBUG("SurfaceRender::OpenDevice\n");

    ret = BufferInit();
    if(ret != OMX_ErrorNone)
        return ret;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::CloseDevice()
{
    LOG_DEBUG("SurfaceRender::CloseDevice\n");
    BufferDeInit();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::SetScalingMode()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int err;

#if (ANDROID_VERSION >= JELLY_BEAN_42)
    LOG_DEBUG("SurfaceRender: SetScalingMode: %d\n", mScalingMode);

    if(nativeWindow == NULL){
        LOG_ERROR("nativeWindow is NULL!\n");
        return OMX_ErrorUndefined;
    }
    fsl_osal_mutex_lock(lock);

    err = native_window_set_scaling_mode(
            nativeWindow.get(), mScalingMode);
    if (err != 0) {
        LOG_DEBUG("Failed to set scaling mode: %d", err);
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorUndefined;
    }

    fsl_osal_mutex_unlock(lock);
#endif

    return ret;
}

OMX_ERRORTYPE SurfaceRender::SetDeviceInputCrop()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int err;

    LOG_DEBUG("SurfaceRender: SetDeviceInputCrop, sRectIn: left %d, top %d, width %d, height %d\n",
            sRectIn.nLeft, sRectIn.nTop, sRectIn.nWidth, sRectIn.nHeight);

    if(nativeWindow == NULL){
        LOG_ERROR("nativeWindow is NULL!\n");
        return OMX_ErrorUndefined;
    }
    fsl_osal_mutex_lock(lock);

    android_native_rect_t rect;
    rect.left = sRectIn.nLeft;
    rect.right = sRectIn.nLeft + sRectIn.nWidth;
    rect.top = sRectIn.nTop;
    rect.bottom = sRectIn.nTop + sRectIn.nHeight;
    err = native_window_set_crop(nativeWindow.get(), &rect);
    if(err != 0){
        LOG_ERROR("setCrop fail\n");
        ret = OMX_ErrorUndefined;
    }

    fsl_osal_mutex_unlock(lock);

    return ret;
}


OMX_ERRORTYPE SurfaceRender::WriteDevice(
        OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds((uint32_t)sVideoFmt.nFrameWidth, (uint32_t)sVideoFmt.nFrameHeight);
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 nIndex;
    android_native_buffer_t *buf;
    int err;
    void *dst;

    //LOG_DEBUG("SurfaceRender: WriteDevice,Hdr %p,pBuffer %x, FilledLen %d, ts %lld \n",pBufferHdr, pBufferHdr->pBuffer, pBufferHdr->nFilledLen, pBufferHdr->nTimeStamp);

    fsl_osal_mutex_lock(lock);

    if(SearchBuffer(pBufferHdr, &nIndex) != OMX_ErrorNone){
        LOG_ERROR("can not find pBufferHdr %p in fb_data[] \n", pBufferHdr);
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorUndefined;
    }

    fb_data[nIndex].owner = OWNER_RENDER;

    if(sCapture.eType == CAP_THUMBNAL) {
        //Still output the frame as thumbnail even if it's corrupted
        if(pBufferHdr->nFilledLen == 0)
            pBufferHdr->nFilledLen = pBufferHdr->nAllocLen;
        CaptureFrame(nIndex);
        bCaptureFrameDone = OMX_TRUE;
        ports[IN_PORT]->SendBuffer(pBufferHdr);
        fb_data[nIndex].owner = OWNER_VPU;
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorNone;
    }

    if(pBufferHdr->nFilledLen == 0) {
        ports[0]->SendBuffer(pBufferHdr);
        fb_data[nIndex].owner = OWNER_VPU;
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorNone;
    }

    if(bSuspend == OMX_TRUE) {
        ports[0]->SendBuffer(pBufferHdr);
        fb_data[nIndex].owner = OWNER_VPU;
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorNone;
    }

    //printf("SurfaceRender: SearchBuffer ok, nIndex %d\n",nIndex);

    //OMX_TICKS * pTs =  (OMX_TICKS *)fb_data[nIndex].pVirtualAddr;
    //*pTs = pBufferHdr->nTimeStamp;
    //printf("%lld\n",*pTs);
	if(fb_data[nIndex].bReturned != OMX_TRUE) {
                mapper.unlock(fb_data[nIndex].pNativeBufferT->handle);
        if (bFirstRender) {
#if (ANDROID_VERSION >= JELLY_BEAN_42)
			SendEvent(OMX_EventRenderingStart, 0, 0, NULL);
#endif
            bFirstRender = OMX_FALSE;
        }

        // browser can't apply crop info for first few frames, need to clean garbage data manually
        if(nFrameNumber < 8){
            if(sRectIn.nLeft != 0 || sRectIn.nTop !=0
                || sRectIn.nWidth != sVideoFmt.nFrameWidth
                || sRectIn.nHeight != sVideoFmt.nFrameHeight)
            {
                LOG_DEBUG("clean crop area before rendering buffer %d %d/%d/%d/%d, %d/%d",
                    nIndex, sRectIn.nLeft,sRectIn.nTop, sRectIn.nWidth, sRectIn.nHeight, sVideoFmt.nFrameWidth, sVideoFmt.nFrameHeight);
                CleanCropContent(nIndex);
            }
            nFrameNumber++;
        }

#if (ANDROID_VERSION <= ICS)
		if ((err = nativeWindow->queueBuffer(nativeWindow.get(), fb_data[nIndex].pNativeBufferT)) != 0) {
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
		if ((err = nativeWindow->queueBuffer(nativeWindow.get(), fb_data[nIndex].pNativeBufferT, -1)) != 0) {
#endif

			LOG_ERROR("Surface::queueBuffer returned error %d", err);
			ports[0]->SendBuffer(pBufferHdr);
			fb_data[nIndex].owner = OWNER_VPU;
			fsl_osal_mutex_unlock(lock);
			return OMX_ErrorUndefined;
		}
	} else {
		nEnqueuedBuffers--;
		fb_data[nIndex].bReturned = OMX_FALSE;
	}

#if 0
    { // for debug
        char status[20] ;
        int k , j;
        memset(status, 0, 20);
        for(k = 0,j=0; k<nFrameBuffer; k++){
            status[j++] = '0' + fb_data[k].owner;
            if(((k+1)%5)==0){
                status[j++] = ',';
            }
        }
        status[j]= '\0';
        LOG_DEBUG("status: %s\n", status);
    }
#endif

    //printf("SurfaceRender: queueBuffer ok, nIndex %d\n",nIndex);
    fb_data[nIndex].owner = OWNER_SURFACE;
    nEnqueuedBuffers++;
    EnqueueBufferIdx = nIndex;

    if(nEnqueuedBuffers > 2){
#if (ANDROID_VERSION <= ICS)
        if ((err = nativeWindow->dequeueBuffer(nativeWindow.get(), &buf)) != 0) {
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
        if ((err = native_window_dequeue_buffer_and_wait(nativeWindow.get(), &buf)) != 0) {
#endif
            LOG_ERROR("Surface::dequeueBuffer returned error %d", err);
            fsl_osal_mutex_unlock(lock);
            return OMX_ErrorNone;//OMX_ErrorUndefined;
        }

        //printf("SurfaceRender: dequeueBuffer ok \n");

        nEnqueuedBuffers--;

#if (ANDROID_VERSION <= ICS)
        if((err = nativeWindow->lockBuffer(nativeWindow.get(), buf)) != 0){
            LOG_ERROR("Surface::lockBuffer returned error %d", err);
            nativeWindow->cancelBuffer(nativeWindow.get(), buf);
            nEnqueuedBuffers--;
            fsl_osal_mutex_unlock(lock);
            return OMX_ErrorUndefined;
        }
#endif
        mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst);
        //printf("SurfaceRender: lockBuffer ok \n");

        if(((err = SearchBuffer(buf, &nIndex)) == OMX_ErrorNone)
            &&(fb_data[nIndex].pOMXBufferHdr != NULL)
            ){
            if(fb_data[nIndex].bReturned != OMX_TRUE)
                ports[IN_PORT]->SendBuffer(fb_data[nIndex].pOMXBufferHdr);
            else
                fb_data[nIndex].bReturned = OMX_FALSE;
            fb_data[nIndex].owner = OWNER_VPU;
            //printf("SurfaceRender: send buffer to vpu ok \n");
        }
        else{
            LOG_ERROR("can not find corresponding bufferHdr in fb_data before SendBuffer to vpu\n");

#if (ANDROID_VERSION <= ICS)
            nativeWindow->cancelBuffer(nativeWindow.get(), buf);
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
            nativeWindow->cancelBuffer(nativeWindow.get(), buf, -1);
#endif
            nEnqueuedBuffers--;
        }
    }

    // when seeking in pause state, previous frame has been flushed in FlushComponent(), here no chance to call SendBuffer() and
    // OMX_EventMark will not be sent , and then seek action fails. So we add this SendEventMark() to trigger OMX_EventMark.
    if(pBufferHdr->hMarkTargetComponent != NULL){
        ports[0]->SendEventMark(pBufferHdr);
    }

    fsl_osal_mutex_unlock(lock);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::CaptureFrame(OMX_U32 nIndex)
{
    LOG_DEBUG("SurfaceRender::CaptureFrame(%d)\n", nIndex);

    if(sCapture.pBuffer == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_memcpy(sCapture.pBuffer, fb_data[nIndex].pVirtualAddr, fb_data[nIndex].pOMXBufferHdr->nFilledLen);
    sCapture.nFilledLen = fb_data[nIndex].pOMXBufferHdr->nFilledLen;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::BufferInit()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_S32 cr_left, cr_top, cr_right, cr_bottom;
    OMX_BOOL bSetCrop = OMX_FALSE;
    OMX_U32 i;
    android_native_buffer_t *buf;
    void *dst;
    int err;
    int color_Format = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    private_handle_t *prvHandle ;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds((uint32_t)sVideoFmt.nFrameWidth, (uint32_t)sVideoFmt.nFrameHeight);

    LOG_DEBUG("SurfaceRender: BufferInit\n");

    if(nativeWindow == NULL) {
        LOG_ERROR("nativeWindow is NULL!\n");
        return OMX_ErrorUndefined;
    }

    int usage = GRALLOC_USAGE_HW_TEXTURE;
#ifdef HWC_RENDER
    usage |= GRALLOC_USAGE_EXTERNAL_DISP;
#endif
    if(mBufferUsage & BUFFER_SW_READ_NEVER)
        usage |= GRALLOC_USAGE_SW_READ_NEVER;
    else if(mBufferUsage & BUFFER_SW_READ_OFTEN)
        usage |= GRALLOC_USAGE_SW_READ_OFTEN;
    else {
    }

    if(mBufferUsage & BUFFER_SW_WRITE_NEVER)
        usage |= GRALLOC_USAGE_SW_WRITE_NEVER;
    else if(mBufferUsage & BUFFER_SW_WRITE_OFTEN)
        usage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
    else {
    }

    if(mBufferUsage & BUFFER_PHY_CONTINIOUS)
        usage |= GRALLOC_USAGE_FORCE_CONTIGUOUS;

    err = native_window_set_usage(nativeWindow.get(), usage );
    if(err != 0){
        LOG_ERROR("set_usage fail , err %d\n", err);
        return OMX_ErrorUndefined;
    }

    LOG_DEBUG("set geometry: width %d, height %d, color %d\n", sVideoFmt.nFrameWidth, sVideoFmt.nFrameHeight, sVideoFmt.eColorFormat);

    switch(sVideoFmt.eColorFormat ){
        case OMX_COLOR_FormatYUV420SemiPlanar:
                color_Format = HAL_PIXEL_FORMAT_YCbCr_420_SP;
                break;

        case OMX_COLOR_FormatYUV420Planar:
            color_Format = HAL_PIXEL_FORMAT_YCbCr_420_P;
                break;

        case OMX_COLOR_Format16bitRGB565:
            color_Format = HAL_PIXEL_FORMAT_RGB_565;
                        break;

        case OMX_COLOR_FormatYUV422Planar:
            color_Format = HAL_PIXEL_FORMAT_YCbCr_422_P;
                        break;
#ifdef MX6X
       case OMX_COLOR_FormatYUV422SemiPlanar:
            color_Format = HAL_PIXEL_FORMAT_YCbCr_422_SP;
                        break;
#endif
        default:
	    LOG_ERROR("Not supported color format %d by surface!", sVideoFmt.eColorFormat);
	    return OMX_ErrorUndefined;
    }
    SetDeviceRotation();

#if (ANDROID_VERSION >= JELLY_BEAN_42)
    SetScalingMode();
#endif

    // force to RGB for debug
    //color_Format = HAL_PIXEL_FORMAT_RGB_565;

    LOG_DEBUG("color_Format: %d\n", color_Format);


//  see system\core\include\system\window.h
//  XXX: This function is deprecated.  The native_window_set_buffers_dimensions
//  and native_window_set_buffers_format functions should be used instead.

#if (ANDROID_VERSION <= KITKAT_44)
    err = native_window_set_buffers_geometry(
            nativeWindow.get(),
            sVideoFmt.nFrameWidth,
            sVideoFmt.nFrameHeight,
            color_Format);
    if(err != 0){
        LOG_DEBUG("set_buffers_geometry fail , err %d\n", err);
        return OMX_ErrorUndefined;
    }
#else
    err = native_window_set_buffers_dimensions(
            nativeWindow.get(),
            sVideoFmt.nFrameWidth,
            sVideoFmt.nFrameHeight);
    if(err != 0){
        LOG_DEBUG("native_window_set_buffers_dimensions fail , err %d\n", err);
        return OMX_ErrorUndefined;
    }

    err = native_window_set_buffers_format(
        nativeWindow.get(),
        color_Format);
    if(err != 0){
        LOG_DEBUG("native_window_set_buffers_format fail , err %d\n", err);
        return OMX_ErrorUndefined;
    }
#endif


    // do not need to query, it's fixed to 2
    /*
    err = nativeWindow->query(nativeWindow.get(), NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &nMinUndequeuedBuffers);
    if (err != 0) {
        LOG_ERROR("NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS query failed: %s (%d)",err);
        nMinUndequeuedBuffers = 2; // default
    }

    LOG_DEBUG("query: MinUndequeuedBuffers %d\n", nMinUndequeuedBuffers);
    */

    LOG_DEBUG("set buffer count %d\n", nFrameBuffer );

    err = native_window_set_buffer_count(nativeWindow.get(), nFrameBuffer );
    if(err != 0){
        LOG_DEBUG("set_buffer_count fail , err %d\n", err);
        return OMX_ErrorUndefined;
    }

    for(i=0; i<MAX_SURFACE_BUFFER; i++)
        fsl_osal_memset(&fb_data[i], 0, sizeof(FB_DATA));

    for(i=0; i<nFrameBuffer ; i++) {

        LOG_DEBUG("dequeueBuffer %d\n", i);

#if (ANDROID_VERSION <= ICS)
        if ((err = nativeWindow->dequeueBuffer(nativeWindow.get(), &buf)) != 0) {
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
        if ((err = native_window_dequeue_buffer_and_wait(nativeWindow.get(), &buf)) != 0) {
#endif

            LOG_ERROR("Surface::dequeueBuffer returned error %d", err);
            ret = OMX_ErrorInsufficientResources;
            break;
        }
        prvHandle = (private_handle_t *)buf->handle;
        if(prvHandle == NULL) {
            LOG_ERROR("nativeWindow buffer prvHandle is NULL.\n");
            ret = OMX_ErrorUndefined;
            break;
        }
        LOG_DEBUG("prvHandle %p, phys %x, nLength %d\n", prvHandle, prvHandle->phys, prvHandle->size);

        fb_data[i].nLength = prvHandle->size;
        fb_data[i].pPhyiscAddr = (OMX_PTR)(unsigned long)prvHandle->phys;
        fb_data[i].pNativeBufferT = buf;
        fb_data[i].owner = OWNER_VPU;

        //LOG_DEBUG("mapper.lock %d\n", i);

        dst = NULL;
        mapper.lock(prvHandle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst);
        if(dst == NULL) {
            LOG_ERROR("mapper.lock: %d failed.\n", i);
            ret = OMX_ErrorUndefined;
            break;
        }
        fb_data[i].pVirtualAddr = dst;
        AddHwBuffer(fb_data[i].pPhyiscAddr, fb_data[i].pVirtualAddr);

        // this clean up job is done in CleanCropContent
        /*
        // Green bar appears at bottom of screen when some streaming video start to play in browser.
        // First few frames are already displayed before browser uses crop info to adjust video display area,
        // so the crop area filled with 0 is displayed out at beginning.
        // Need to fill buffer with black to avoid it.
        if(sRectIn.nLeft != 0 || sRectIn.nTop !=0
            || sRectIn.nWidth != sVideoFmt.nFrameWidth
            || sRectIn.nHeight != sVideoFmt.nFrameHeight)
        {
            LOG_DEBUG("clean buffer %d content because of cropping %d/%d/%d/%d, %d/%d",
                i, sRectIn.nLeft,sRectIn.nTop, sRectIn.nWidth, sRectIn.nHeight, sVideoFmt.nFrameWidth, sVideoFmt.nFrameHeight);
            CleanBufferContent(i);
        }
        */

        // need to see whether or not commenting this affects...
        /*
        printf("lockBuffer %d\n", i);
        if((err = nativeWindow->lockBuffer(nativeWindow.get(), buf)) != 0){
            LOG_ERROR("Surface::lockBuffer returned error %d", err);
            ret = OMX_ErrorUndefined;
            break;
        }
        */

    }

    LOG_DEBUG("dequeueBuffer finished, ret %x\n", ret);

    if(ret != OMX_ErrorNone) {
        BufferDeInit();
        return ret;
    }

    SetDeviceInputCrop();

    LOG_DEBUG("SetDeviceInputCrop finished\n");

    nEnqueuedBuffers = 0;
    bFrameBufferInit = OMX_TRUE;

    LOG_DEBUG("BufferInit finished\n");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::BufferDeInit()
{
    OMX_U32 i;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();

    LOG_DEBUG("SurfaceRender: BufferDeInit\n");

    if(nativeWindow == NULL){
        LOG_DEBUG("nativeWindow is NULL!\n");
        return OMX_ErrorUndefined;
    }

    for(i = 0; i < MAX_SURFACE_BUFFER ; i++) {
        if(fb_data[i].pVirtualAddr != NULL) {
            RemoveHwBuffer(fb_data[i].pVirtualAddr);
            private_handle_t * prvHandle = (private_handle_t *)fb_data[i].pNativeBufferT->handle;
            mapper.unlock(prvHandle);
        }
        if(fb_data[i].pNativeBufferT != NULL && fb_data[i].owner != OWNER_SURFACE){

#if (ANDROID_VERSION <= ICS)
            nativeWindow->cancelBuffer(nativeWindow.get(), fb_data[i].pNativeBufferT);
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
            nativeWindow->cancelBuffer(nativeWindow.get(), fb_data[i].pNativeBufferT, -1);
#endif
        }
    }

    for(i=0; i<MAX_SURFACE_BUFFER; i++)
        fsl_osal_memset(&fb_data[i], 0, sizeof(FB_DATA));

    bFrameBufferInit = OMX_FALSE;
    nAllocating = 0;

    LOG_DEBUG("SurfaceRender: BufferDeInit finished\n");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::SearchBuffer(
        OMX_BUFFERHEADERTYPE *pBufferHdr,
        OMX_U32 *pIndex)
{
    OMX_U32 nIndex = 0;
    OMX_U32 i;
    OMX_BOOL bFound = OMX_FALSE;

    for(i=0; i<nFrameBuffer ; i++) {
        if(fb_data[i].pVirtualAddr == pBufferHdr->pBuffer) {
            nIndex = i;
            bFound = OMX_TRUE;
            if(fb_data[i].pOMXBufferHdr == NULL)
                fb_data[i].pOMXBufferHdr = pBufferHdr;
            break;
        }
    }

    if(bFound != OMX_TRUE)
        return OMX_ErrorUndefined;

    *pIndex = nIndex;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE SurfaceRender::SearchBuffer(
        android_native_buffer_t *pNativeBufferT,
        OMX_U32 *pIndex)
{
    OMX_U32 nIndex = 0;
    OMX_U32 i;
    OMX_BOOL bFound = OMX_FALSE;

    for(i=0; i<nFrameBuffer ; i++) {
        if(fb_data[i].pNativeBufferT == pNativeBufferT) {
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

OMX_U8 SurfaceRender::GetDequeuedBufferNumber()
{
    OMX_U8 bufferNumber = 0;
    OMX_S32 i;

    for(i = 0; i < (OMX_S32)nFrameBuffer; i++){
        if(fb_data[i].owner != OWNER_SURFACE)
            bufferNumber++;

    }
    return bufferNumber;
}

OMX_ERRORTYPE SurfaceRender::FlushComponent(
                OMX_U32 nPortIndex)
{
    OMX_U32 nIndex;
    android_native_buffer_t *buf;
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds((uint32_t)sVideoFmt.nFrameWidth, (uint32_t)sVideoFmt.nFrameHeight);
    void *dst;
    int err;

    LOG_DEBUG("SurfaceRender FlushCompoment \n");
    LOG_DEBUG("dequeuedBufferNumber is %d\n", GetDequeuedBufferNumber());

    int undequeuedBuffer = nFrameBuffer - GetDequeuedBufferNumber();
	LOG_DEBUG("undequeuedBuffer: %d\n", undequeuedBuffer);

    fsl_osal_mutex_lock(lock);

    for(int i = 0; i < undequeuedBuffer ; i++){
		// workround for last frame can dequeue, but address is wrong.
		if (undequeuedBuffer - i > 1) {

#if (ANDROID_VERSION <= ICS)
           if ((err = nativeWindow->dequeueBuffer(nativeWindow.get(), &buf)) != 0) {
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
           if ((err = native_window_dequeue_buffer_and_wait(nativeWindow.get(), &buf)) != 0) {
#endif
                LOG_ERROR("Surface::dequeueBuffer returned error %d", err);

#ifdef MX5X
				for(int j = 0; j < (int)nFrameBuffer ; j++) {
					if(fb_data[j].owner == OWNER_RENDER || fb_data[j].owner == OWNER_SURFACE) {
						ports[IN_PORT]->SendBuffer(fb_data[j].pOMXBufferHdr);
						fb_data[j].owner = OWNER_VPU;
						fb_data[j].bReturned = OMX_TRUE;
					}
				}
#endif

				fsl_osal_mutex_unlock(lock);
				return OMX_ErrorNone;//OMX_ErrorUndefined;
			}
                        mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &dst);
		} else {
#ifdef MX5X
			for(int j = 0; j < (int)nFrameBuffer ; j++) {
				if(fb_data[j].owner == OWNER_RENDER || fb_data[j].owner == OWNER_SURFACE) {
					ports[IN_PORT]->SendBuffer(fb_data[j].pOMXBufferHdr);
					fb_data[j].owner = OWNER_VPU;
					fb_data[j].bReturned = OMX_TRUE;
				}
			}
#endif

			fsl_osal_mutex_unlock(lock);
			return OMX_ErrorNone;//OMX_ErrorUndefined;
		}

        LOG_DEBUG("SurfaceRender: dequeueBuffer ok \n");

        nEnqueuedBuffers--;
        /*
        if((err = nativeWindow->lockBuffer(nativeWindow.get(), buf)) != 0){
            LOG_ERROR("Surface::lockBuffer returned error %d", err);
            nativeWindow->cancelBuffer(nativeWindow.get(), buf);
            nEnqueuedBuffers--;
            fsl_osal_mutex_unlock(lock);
            return OMX_ErrorUndefined;
        }

        printf("SurfaceRender: lockBuffer ok \n");
        */

        if(((err = SearchBuffer(buf, &nIndex)) == OMX_ErrorNone)
            &&(fb_data[nIndex].pOMXBufferHdr != NULL)
            ){
            ports[IN_PORT]->SendBuffer(fb_data[nIndex].pOMXBufferHdr);
            fb_data[nIndex].owner = OWNER_VPU;
        }
        else{
            LOG_ERROR("can not find corresponding bufferHdr in fb_data before SendBuffer to vpu\n");

#if (ANDROID_VERSION <= ICS)
            nativeWindow->cancelBuffer(nativeWindow.get(), buf);
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
            nativeWindow->cancelBuffer(nativeWindow.get(), buf, -1);
#endif
            nEnqueuedBuffers--;
        }
    }

    fsl_osal_mutex_unlock(lock);

    return OMX_ErrorNone;
}

OMX_U32 SurfaceRender::GetDeviceDropFames()
{
    if(nativeWindow == NULL)
        return 0;

    int nDropedFrames = 0;
    #if (ANDROID_VERSION < MARSH_MALLOW_600)
    if(0 != nativeWindow->query(nativeWindow.get(), NATIVE_WINDOW_GET_FRAME_LOST, &nDropedFrames))
        return 0;
    #endif

    LOG_DEBUG("nDeviceDropCnt: %d\n", nDropedFrames);

    return nDropedFrames;
}

OMX_ERRORTYPE SurfaceRender::CleanBufferContent(OMX_U32 index)
{
    OMX_U32 y_length = sVideoFmt.nFrameWidth * sVideoFmt.nFrameHeight;

    switch(sVideoFmt.eColorFormat ){
        case OMX_COLOR_FormatYUV420SemiPlanar:
        case OMX_COLOR_FormatYUV420Planar:
        {
            ASSERT(fb_data[index].nLength >= y_length *3/2);
            fsl_osal_memset(fb_data[index].pVirtualAddr, 0x10, y_length);
            fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + y_length, 0x80, y_length/2);
            break;
        }

        case OMX_COLOR_Format16bitRGB565:
        {
            ASSERT(fb_data[index].nLength >= y_length *2);
            fsl_osal_memset(fb_data[index].pVirtualAddr, 0, y_length*2);
            break;
        }

        case OMX_COLOR_FormatYUV422Planar:
        case OMX_COLOR_FormatYUV422SemiPlanar:
        {
            ASSERT(fb_data[index].nLength >= y_length *2);
            fsl_osal_memset(fb_data[index].pVirtualAddr, 0x10, y_length);
            fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + y_length, 0x80, y_length);
            break;
        }

        default:
            LOG_ERROR("Not supported color format %d by surface!", sVideoFmt.eColorFormat);
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;

}

void SurfaceRender::CleanCropContent(OMX_U32 index)
{
    OMX_U32 y_length = sVideoFmt.nFrameWidth * sVideoFmt.nFrameHeight;

    LOG_DEBUG("color format is %d" ,sVideoFmt.eColorFormat );

    switch(sVideoFmt.eColorFormat ){
        case OMX_COLOR_FormatYUV420SemiPlanar:
        {
            ASSERT(fb_data[index].nLength >= y_length *3/2);
            if(sVideoFmt.nFrameWidth > sRectIn.nLeft + sRectIn.nWidth){
                for(OMX_U32 i = 0; i < sRectIn.nTop + sRectIn.nHeight ; i++){
                    OMX_U32 crop_start_y = sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth;
                    OMX_U32 crop_len_y = sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth);
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_y , 0x10, crop_len_y);
                }
                for(OMX_U32 i = 0; i < (sRectIn.nTop + sRectIn.nHeight)/2 ; i++){
                    OMX_U32 crop_start_uv = sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth;
                    OMX_U32 crop_len_uv = sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth);
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + y_length + crop_start_uv , 0x80, crop_len_uv);
                }
            }
            if(sVideoFmt.nFrameHeight > sRectIn.nTop + sRectIn.nHeight){
                OMX_U32 crop_start_y = (sRectIn.nTop + sRectIn.nHeight) * sVideoFmt.nFrameWidth;
                OMX_U32 crop_start_uv = y_length + crop_start_y/2;
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_y , 0x10, y_length - crop_start_y);
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_uv, 0x80, (y_length - crop_start_y)/2);
            }
            break;
        }

        case OMX_COLOR_FormatYUV420Planar:
        {
            ASSERT(fb_data[index].nLength >= y_length *3/2);
            if(sVideoFmt.nFrameWidth > sRectIn.nLeft + sRectIn.nWidth){
                for(OMX_U32 i = 0; i < sRectIn.nTop + sRectIn.nHeight ; i++){
                    OMX_U32 crop_start_y = sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth;
                    OMX_U32 crop_len_y = sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth);
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_y , 0x10, crop_len_y);
                }
                for(OMX_U32 i = 0; i < (sRectIn.nTop + sRectIn.nHeight)/ 2 ; i++){
                    OMX_U32 crop_start_uv = (sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth ) / 2;
                    OMX_U32 crop_len_uv = (sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth) ) / 2;
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + y_length + crop_start_uv , 0x80, crop_len_uv);
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + y_length /4 * 5 + crop_start_uv , 0x80, crop_len_uv);
                }
            }
            if(sVideoFmt.nFrameHeight > sRectIn.nTop + sRectIn.nHeight){
                OMX_U32 crop_start_y = (sRectIn.nTop + sRectIn.nHeight) * sVideoFmt.nFrameWidth;
                OMX_U32 crop_start_u = y_length + crop_start_y/4;
                OMX_U32 crop_start_v = y_length / 4 * 5 + crop_start_y/4;
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_y , 0x10, y_length - crop_start_y);
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_u, 0x80, (y_length - crop_start_y)/4);
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_v, 0x80, (y_length - crop_start_y)/4);
            }
            break;
        }

        case OMX_COLOR_Format16bitRGB565:
        {
            ASSERT(fb_data[index].nLength >= y_length *2);
            if(sVideoFmt.nFrameWidth > sRectIn.nLeft + sRectIn.nWidth){
                for(OMX_U32 i = 0; i < sRectIn.nTop + sRectIn.nHeight ; i++){
                    OMX_U32 crop_start = (sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth) * 2;
                    OMX_U32 crop_len = (sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth)) * 2;
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start , 0, crop_len);
                }
            }
            if(sVideoFmt.nFrameHeight > sRectIn.nTop + sRectIn.nHeight){
                OMX_U32 crop_start = ((sRectIn.nTop + sRectIn.nHeight) * sVideoFmt.nFrameWidth) * 2;
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start , 0, fb_data[index].nLength - crop_start);
            }
            break;
        }

        case OMX_COLOR_FormatYUV422Planar:
        {
            ASSERT(fb_data[index].nLength >= y_length *2);
            if(sVideoFmt.nFrameWidth > sRectIn.nLeft + sRectIn.nWidth){
                for(OMX_U32 i = 0; i < sRectIn.nTop + sRectIn.nHeight ; i++){
                    OMX_U32 crop_start_y = sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth;
                    OMX_U32 crop_len_y = sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth);
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_y , 0x10, crop_len_y);
                }
                for(OMX_U32 i = 0; i < (sRectIn.nTop + sRectIn.nHeight) ; i++){
                    OMX_U32 crop_start_uv = (sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth ) / 2;
                    OMX_U32 crop_len_uv = (sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth) ) / 2;
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + y_length + crop_start_uv , 0x80, crop_len_uv);
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + y_length /2 * 3 + crop_start_uv , 0x80, crop_len_uv);
                }
            }
            if(sVideoFmt.nFrameHeight > sRectIn.nTop + sRectIn.nHeight){
                OMX_U32 crop_start_y = (sRectIn.nTop + sRectIn.nHeight) * sVideoFmt.nFrameWidth;
                OMX_U32 crop_start_u = y_length + crop_start_y/2;
                OMX_U32 crop_start_v = y_length / 2 * 3 + crop_start_y/2;
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_y , 0x10, y_length - crop_start_y);
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_u, 0x80, (y_length - crop_start_y)/2);
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_v, 0x80, (y_length - crop_start_y)/2);
            }
            break;
        }

        case OMX_COLOR_FormatYUV422SemiPlanar:
        {
            ASSERT(fb_data[index].nLength >= y_length * 2);
            if(sVideoFmt.nFrameWidth > sRectIn.nLeft + sRectIn.nWidth){
                for(OMX_U32 i = 0; i < sRectIn.nTop + sRectIn.nHeight ; i++){
                    OMX_U32 crop_start_y = sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth;
                    OMX_U32 crop_len_y = sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth);
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_y , 0x10, crop_len_y);
                }
                for(OMX_U32 i = 0; i < (sRectIn.nTop + sRectIn.nHeight); i++){
                    OMX_U32 crop_start_uv = sVideoFmt.nFrameWidth * i + sRectIn.nLeft + sRectIn.nWidth;
                    OMX_U32 crop_len_uv = sVideoFmt.nFrameWidth - (sRectIn.nLeft + sRectIn.nWidth);
                    fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + y_length + crop_start_uv , 0x80, crop_len_uv);
                }
            }
            if(sVideoFmt.nFrameHeight > sRectIn.nTop + sRectIn.nHeight){
                OMX_U32 crop_start_y = (sRectIn.nTop + sRectIn.nHeight) * sVideoFmt.nFrameWidth;
                OMX_U32 crop_start_uv = y_length + crop_start_y;
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_y , 0x10, y_length - crop_start_y);
                fsl_osal_memset((OMX_U8*)fb_data[index].pVirtualAddr + crop_start_uv, 0x80, y_length - crop_start_y);
            }
            break;
        }

        default:
            LOG_ERROR("Not supported color format %d by surface!", sVideoFmt.eColorFormat);
            break;
    }

    return;

}


/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE SurfaceRenderInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        SurfaceRender *obj = NULL;
        ComponentBase *base = NULL;

        LOG_DEBUG("SurfaceRenderInit\n");

        obj = FSL_NEW(SurfaceRender, ());
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

