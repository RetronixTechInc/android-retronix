/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "V4l2Object.h"
#include <sys/mman.h>
#include <poll.h>

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_LOG
#define LOG_LOG printf
#endif

#define FRMAE_WIDTH_LIMITATION 4096
#define FRMAE_HEIGHT_LIMITATION 3072
#define BUFFER_NUM_LIMITATION 32

OMX_ERRORTYPE V4l2Object::Create(OMX_S32 dev,enum v4l2_buf_type type,OMX_U32 plane_num)
{
    OMX_ERRORTYPE ret;

    OMX_U32 formatNum = 0;

    mFd = dev;
    eBufferType = type;
    bBufferRequired = OMX_FALSE;
    bMultiPlane = OMX_FALSE;
    bRunning = OMX_FALSE;

    if(V4L2_TYPE_IS_MULTIPLANAR(eBufferType))
        bMultiPlane = OMX_TRUE;

    nPlanesNum = plane_num;
    nBufferCount = 1;
    nBufferNum = 0;

    nCnt = 0;
    nBufferSize[0] = 0;
    nBufferSize[1] = 0;
    nBufferSize[2] = 0;
    sMutex = NULL;

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&sMutex, fsl_osal_mutex_normal)) {
        ret = OMX_ErrorInsufficientResources;
        return ret;
    }

    pQueue = FSL_NEW(Queue, ());
    pOutQueue = FSL_NEW(Queue, ());


    if(pQueue == NULL || pOutQueue == NULL){
        return OMX_ErrorInsufficientResources;
    }

    if(pQueue->Create(V4L2_OBJECT_MAX_BUFFER_COUNT, sizeof(OMX_U32), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
        LOG_ERROR("V4l2Object create output queue failed");
        return OMX_ErrorInsufficientResources;
    }

    if(pOutQueue->Create(V4L2_OBJECT_MAX_BUFFER_COUNT, sizeof(OMX_U32), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
        LOG_ERROR("V4l2Object create output queue failed");
        return OMX_ErrorInsufficientResources;
    }

    ret = QueryFormat(&formatNum);
    if(formatNum == 0){
        LOG_ERROR("no format found under this type");
        ret = OMX_ErrorBadParameter;
        return ret;
    }
    pBufferMap = NULL;
    LOG_LOG("[%p] V4l2Object::V4l2Object,type=%d,plane_num=%d,FD=%d \n",this,type,plane_num,dev);
    bChange = OMX_FALSE;
    bMetadataBuffer = OMX_FALSE;
    return ret;
}
OMX_ERRORTYPE V4l2Object::ReleaseResource()
{
    struct v4l2_fmtdesc * pFormat;

    if(sMutex != NULL)
        fsl_osal_mutex_lock(sMutex);

    if(pQueue){
        pQueue->Free();
        FSL_DELETE(pQueue);
    }

    if(pOutQueue){
        pOutQueue->Free();
        FSL_DELETE(pOutQueue);
    }

    if(sMutex != NULL)
        fsl_osal_mutex_unlock(sMutex);

    if(pBufferMap)
       FSL_FREE(pBufferMap);

    while(formatList.GetNodeCnt() > 0){
        pFormat = formatList.GetNode(0);
        formatList.Remove(pFormat);
        FSL_FREE(pFormat);
    }

    if(sMutex != NULL)
        fsl_osal_mutex_destroy(sMutex);

    LOG_LOG("[%p] ReleaseResource\n",this);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::QueryFormat(OMX_U32 * num)
{
    OMX_U32 i = 0;
    struct v4l2_fmtdesc sFmt;
    struct v4l2_fmtdesc * pFormat;

    if(num == NULL)
        return OMX_ErrorBadParameter;

    while(OMX_TRUE){
        fsl_osal_memset((fsl_osal_ptr)&sFmt, 0, sizeof(struct v4l2_fmtdesc));
        sFmt.type = eBufferType;
        sFmt.index = i;
        if(ioctl(mFd,VIDIOC_ENUM_FMT,&sFmt) < 0)
            break;
        pFormat = (struct v4l2_fmtdesc *)FSL_MALLOC(sizeof(struct v4l2_fmtdesc));
        if(pFormat == NULL)
            break;

        fsl_osal_memcpy((void*)pFormat,&sFmt,sizeof(v4l2_fmtdesc));
        formatList.Add(pFormat);
        LOG_LOG("***QueryFormat add format %x\n",pFormat->pixelformat);
        i++;
    }
    *num = i;
    LOG_DEBUG("QueryFormat num=%d\n",i);
    return OMX_ErrorNone;
}
OMX_BOOL V4l2Object::IsFormatSupported(OMX_U32 in_format)
{
    OMX_U32 i = 0;
    struct v4l2_fmtdesc * pFormat;

    for(i = 0; i < formatList.GetNodeCnt(); i++){
        pFormat = formatList.GetNode(i);
        if(pFormat->pixelformat == in_format)
            return OMX_TRUE;
    }
    return OMX_FALSE;
}

OMX_ERRORTYPE V4l2Object::SetFormat(V4l2ObjectFormat * pFormat)
{
    int result = 0;
    struct v4l2_format format;

    if(pFormat == NULL)
        return OMX_ErrorBadParameter;

    if(!IsFormatSupported(pFormat->format)){
        LOG_ERROR("[%p]V4l2Object::SetFormat format not supported,format=%x \n",this,pFormat->format);
        return OMX_ErrorFormatNotDetected;
    }
    fsl_osal_memset(&format, 0, sizeof(struct v4l2_format));
    format.type = eBufferType;

    if(bMultiPlane){
        OMX_U32 i;
        format.fmt.pix_mp.pixelformat = pFormat->format;
        format.fmt.pix_mp.width = pFormat->width;
        format.fmt.pix_mp.height = pFormat->height;
        format.fmt.pix_mp.num_planes = nPlanesNum;

        format.fmt.pix_mp.field = V4L2_FIELD_NONE;
        //format.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;

        //TODO: set stride for multi planer.

        if(nPlanesNum == 1){
            format.fmt.pix_mp.plane_fmt[0].bytesperline = pFormat->stride;
            format.fmt.pix_mp.plane_fmt[0].sizeimage = pFormat->bufferSize[0];
            nBufferSize[0] = pFormat->bufferSize[0];
            nBufferSize[1] = 0;
            nBufferSize[2] = 0;
        }
        else if(nPlanesNum == 2){
            format.fmt.pix_mp.plane_fmt[0].bytesperline = pFormat->stride;
            format.fmt.pix_mp.plane_fmt[1].bytesperline = pFormat->stride;

            if(pFormat->plane_num == 1){
                format.fmt.pix_mp.plane_fmt[0].sizeimage = pFormat->width * pFormat->height;
                //assume yuv 420 
                format.fmt.pix_mp.plane_fmt[1].sizeimage = format.fmt.pix_mp.plane_fmt[0].sizeimage / 2;
                format.fmt.pix_mp.plane_fmt[2].sizeimage = 0;

            }else{
                format.fmt.pix_mp.plane_fmt[0].sizeimage = pFormat->bufferSize[0];
                format.fmt.pix_mp.plane_fmt[1].sizeimage = pFormat->bufferSize[1];
                format.fmt.pix_mp.plane_fmt[2].sizeimage = pFormat->bufferSize[2];
            }
            nBufferSize[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
            nBufferSize[1] = format.fmt.pix_mp.plane_fmt[1].sizeimage;
            nBufferSize[2] = format.fmt.pix_mp.plane_fmt[2].sizeimage;

            LOG_DEBUG("set buffer size=%d,%d",format.fmt.pix_mp.plane_fmt[0].sizeimage,format.fmt.pix_mp.plane_fmt[1].sizeimage);

        }

    }else{
        format.fmt.pix.pixelformat = pFormat->format;
        format.fmt.pix.width = pFormat->width;
        format.fmt.pix.height = pFormat->height;
        format.fmt.pix.bytesperline = pFormat->stride;
        format.fmt.pix.sizeimage = pFormat->bufferSize[0];

        format.fmt.pix.field = V4L2_FIELD_NONE;
        format.fmt.pix.colorspace = V4L2_COLORSPACE_REC709;
        
        nBufferSize[0] = pFormat->bufferSize[0];
        nBufferSize[1] = 0;
        nBufferSize[2] = 0;
    }

#if 0
    result = ioctl (mFd, VIDIOC_TRY_FMT, &format);
    if(result < 0)
        return OMX_ErrorUnsupportedSetting;
#endif
    result = ioctl (mFd, VIDIOC_S_FMT, &format);

    LOG_DEBUG("[%p] SetFormat ret=%d width=%d,height=%d,stride=%d,buffersize=%d,format=%x\n",this,result,pFormat->width,pFormat->height,pFormat->stride,pFormat->bufferSize[0],pFormat->format);

    if(result != 0)
        return OMX_ErrorUnsupportedSetting;


    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::GetFormat(V4l2ObjectFormat * pFormat)
{

    int result = 0;
    struct v4l2_format format;
    OMX_U32 out_format = 0;
    OMX_U32 i = 0;

    if(pFormat == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_memset(&format, 0, sizeof(struct v4l2_format));
    format.type = eBufferType;

    result = ioctl (mFd, VIDIOC_G_FMT, &format);
    if(result < 0)
        return OMX_ErrorUndefined;

    pFormat->bufferSize[0] = 0;
    pFormat->bufferSize[1] = 0;
    pFormat->bufferSize[2] = 0;

    if(bMultiPlane){

        pFormat->format = format.fmt.pix_mp.pixelformat;
        pFormat->width = format.fmt.pix_mp.width;
        pFormat->height = format.fmt.pix_mp.height;
        pFormat->stride = format.fmt.pix_mp.plane_fmt[0].bytesperline;
        
        if(nPlanesNum == 1){
            pFormat->bufferSize[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
            nBufferSize[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
        }
        else if(nPlanesNum == 2){
            pFormat->stride = format.fmt.pix_mp.plane_fmt[0].bytesperline;
            pFormat->bufferSize[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
            pFormat->bufferSize[1] = format.fmt.pix_mp.plane_fmt[1].sizeimage;
            LOG_DEBUG("get buffer size=%d,%d\n",format.fmt.pix_mp.plane_fmt[0].sizeimage,
                format.fmt.pix_mp.plane_fmt[1].sizeimage);
            nBufferSize[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
            nBufferSize[1] = format.fmt.pix_mp.plane_fmt[1].sizeimage;
        }
        pFormat->plane_num = nPlanesNum;

    }else{

        pFormat->width = format.fmt.pix.width;
        pFormat->height = format.fmt.pix.height;
        pFormat->format = format.fmt.pix.pixelformat;
        pFormat->stride = format.fmt.pix.bytesperline;
        pFormat->bufferSize[0] = format.fmt.pix.sizeimage;
        pFormat->plane_num = 1;
    }
    LOG_DEBUG("[%p] GetFormat SUCCESS width=%d,height=%d,size[0]=%d,size[1]=%d,format=%x,stride=%d\n",this
        ,pFormat->width,pFormat->height,pFormat->bufferSize[0],pFormat->bufferSize[1],pFormat->format,pFormat->stride);
    LOG_LOG("[%p] GetFormat field=%d\n",this,format.fmt.pix_mp.field);

    if(pFormat->width > FRMAE_WIDTH_LIMITATION || pFormat->height > FRMAE_HEIGHT_LIMITATION){
        LOG_ERROR("StreamCorrupt width %d, height %d, stride %d", pFormat->width, pFormat->height, pFormat->stride);
        return OMX_ErrorStreamCorrupt;
    }
    if(pFormat->bufferSize[0] == 0 && pFormat->bufferSize[1] == 0 && pFormat->stride == 0){
        LOG_ERROR("StreamCorrupt bufferSize[0] %d, bufferSize[1] %d, stride %d",
    		pFormat->bufferSize[0], pFormat->bufferSize[1], pFormat->stride);
        return OMX_ErrorStreamCorrupt;
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::SetCrop(OMX_CONFIG_RECTTYPE *sCrop)
{
    int result = 0;
    struct v4l2_crop crop;

    if(sCrop == NULL)
        return OMX_ErrorBadParameter;

    crop.type = eBufferType;
    crop.c.left = sCrop->nLeft;
    crop.c.top = sCrop->nTop;
    crop.c.width = sCrop->nWidth;
    crop.c.height = sCrop->nHeight;

    result = ioctl (mFd, VIDIOC_S_CROP, &crop);
    if(result < 0)
        return OMX_ErrorUnsupportedSetting;

    LOG_DEBUG("[%p] SetCrop SUCCESS width=%d,height=%d",this,sCrop->nWidth,sCrop->nHeight);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::GetCrop(OMX_CONFIG_RECTTYPE *sCrop)
{
    int result = 0;
    struct v4l2_crop crop;

    if(sCrop == NULL)
        return OMX_ErrorBadParameter;

    crop.type = eBufferType;

    result = ioctl (mFd, VIDIOC_G_CROP, &crop);
    if(result < 0)
        return OMX_ErrorUnsupportedSetting;

    sCrop->nLeft = crop.c.left;
    sCrop->nTop = crop.c.top;
    sCrop->nWidth = crop.c.width;
    sCrop->nHeight = crop.c.height;
    return OMX_ErrorNone;

}
OMX_ERRORTYPE V4l2Object::GetMinimumBufferCount(OMX_U32 *num)
{
    int result = 0;
    struct v4l2_control ctl;

    fsl_osal_memset(&ctl, 0, sizeof(struct v4l2_control));
 
    if(eBufferType == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE || eBufferType == 
        V4L2_BUF_TYPE_VIDEO_CAPTURE)
        ctl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    else if(eBufferType == V4L2_BUF_TYPE_VIDEO_OUTPUT || eBufferType == 
        V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
        ctl.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT;
    else
        return OMX_ErrorUndefined;

    
    result = ioctl(mFd, VIDIOC_G_CTRL, &ctl);
    LOG_LOG("[%p] GetMinimumBufferCount ret=%d num=%d\n",this,result,ctl.value);

    if(result < 0)
        return OMX_ErrorUndefined;

    if(ctl.value > BUFFER_NUM_LIMITATION){
        LOG_ERROR("StreamCorrupt Minimum buffer count %d", ctl.value);
        return OMX_ErrorStreamCorrupt;
    }

    *num = ctl.value;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Object::SetBufferCount(OMX_U32 count, enum v4l2_memory type,OMX_U32 planes_num)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    int result = 0;
    struct v4l2_requestbuffers buf_req;

    if(count > V4L2_OBJECT_MAX_BUFFER_COUNT)
        return OMX_ErrorOverflow;

    //free buffers
    if(bBufferRequired){
        buf_req.count = 0;
        buf_req.type = eBufferType;
        buf_req.memory = eIOType;
        
        result = ioctl(mFd, VIDIOC_REQBUFS, &buf_req);
        LOG_DEBUG("[%p] SetBufferCount VIDIOC_REQBUFS count=0,memory=%d,type=%d,ret=%d\n",this,eIOType,eBufferType,result);

        //delete buffer map
        if(pBufferMap)
            FSL_FREE(pBufferMap);
    }
    if(count == 0)
        return ret;

    buf_req.count = count;
    buf_req.type = eBufferType;
    buf_req.memory = type;

    if(buf_req.count > V4L2_OBJECT_MAX_BUFFER_COUNT)
        buf_req.count = V4L2_OBJECT_MAX_BUFFER_COUNT;

    result = ioctl(mFd, VIDIOC_REQBUFS, &buf_req);
    LOG_DEBUG("[%p] SetBufferCount VIDIOC_REQBUFS count=%d,memory=%d, type=%d,ret=%d\n",this,buf_req.count,type,eBufferType,result);

    if(result < 0)
        return OMX_ErrorUndefined;

    eIOType = buf_req.memory;
    nBufferCount = buf_req.count;

    if(planes_num > 0)
        nPlanesNum = planes_num;

    nBufferNum = 0;
    bBufferRequired = OMX_TRUE;
    LOG_DEBUG("[%p] SetBufferCount count=%d,type=%d,planes_num=%d\n",this,count,type,planes_num);

    fsl_osal_memset(pIndex,0,V4L2_OBJECT_MAX_BUFFER_COUNT * sizeof(OMX_U8*));

    ret = BufferMap_Create(V4L2_OBJECT_MAX_BUFFER_COUNT/*nBufferCount*/);

    return ret;
}
OMX_ERRORTYPE V4l2Object::BufferMap_Create(OMX_U32 count)
{
    pBufferMap = (V4l2ObjectMap *)FSL_MALLOC(count * sizeof(V4l2ObjectMap));
    if(pBufferMap == NULL)
        return OMX_ErrorInsufficientResources;

    fsl_osal_memset(pBufferMap, 0, count * sizeof(V4l2ObjectMap));

    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Object::BufferMap_Update(OMX_BUFFERHEADERTYPE *inputBuffer,OMX_U8* pBuf,OMX_U32 index)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 i = 0;

    if(inputBuffer == NULL || pBuf == NULL || index > V4L2_OBJECT_MAX_BUFFER_COUNT)
        return OMX_ErrorBadParameter;

    i = index;
    pBufferMap[i].vaddr = pBuf;
    pBufferMap[i].omxBuffer = inputBuffer;

    pBufferMap[i].v4l2Buffer.index = i;
    pBufferMap[i].v4l2Buffer.type = eBufferType;
    pBufferMap[i].v4l2Buffer.field = V4L2_FIELD_NONE;

    LOG_LOG("[%p]BufferMap_Update omxBuf=%p,vaddr=%p,index=%d",this,inputBuffer,pBuf,index);
    if(eIOType == V4L2_MEMORY_USERPTR)
        ret = BufferMap_UpdateUserPtr(inputBuffer,pBuf,index);
    else if(eIOType == V4L2_MEMORY_DMABUF){
        ret = BufferMap_UpdateDmaBuf(inputBuffer,pBuf,index);
    }

    return ret;
}
OMX_ERRORTYPE V4l2Object::BufferMap_UpdateDmaBuf(DmaBufferHdr *inputBuffer,OMX_U8* pBuf,OMX_U32 index)
{
    OMX_U32 i = 0;

    if(inputBuffer == NULL || pBuf == NULL || index > V4L2_OBJECT_MAX_BUFFER_COUNT)
        return OMX_ErrorBadParameter;

    i = index;
    pBufferMap[i].vaddr = pBuf;
    pBufferMap[i].dmaBuffer = inputBuffer;
 
    pBufferMap[i].v4l2Buffer.index = i;
    pBufferMap[i].v4l2Buffer.type = eBufferType;
    pBufferMap[i].v4l2Buffer.field = V4L2_FIELD_NONE; 
    pBufferMap[i].v4l2Buffer.length = inputBuffer->num_of_plane;

    if(inputBuffer->num_of_plane == 2){
        pBufferMap[i].fd[0] = inputBuffer->plane[0].fd;
        pBufferMap[i].fd[1] = inputBuffer->plane[1].fd;
    }else
        return OMX_ErrorBadParameter;

    pBufferMap[i].v4l2Buffer.m.planes = &pBufferMap[i].v4l2Plane[0];
    if(bMultiPlane){

        for (OMX_U32 j = 0; j < inputBuffer->num_of_plane; j++){
            pBufferMap[i].v4l2Plane[j].m.fd = inputBuffer->plane[j].fd;
            pBufferMap[i].v4l2Plane[j].length = inputBuffer->plane[j].size;
            pBufferMap[i].v4l2Plane[j].data_offset = 0;
            LOG_LOG("[%p]BufferMap_Update index=%d, j=%d,len=%d,fd=%d",this,index, j,pBufferMap[i].v4l2Plane[j].length, pBufferMap[i].v4l2Plane[j].m.fd);

        }

    }else
        return OMX_ErrorBadParameter;

    pBufferMap[i].v4l2Buffer.memory = V4L2_MEMORY_DMABUF;

    LOG_LOG("[%p]BufferMap_Update DmaBufferHdr=%p,vaddr=%p,index=%d",this,inputBuffer,pBuf,index);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::BufferMap_UpdateUserPtr(OMX_BUFFERHEADERTYPE *inputBuffer,OMX_U8* pBuf,OMX_U32 index)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 i = 0;

    if(inputBuffer == NULL || pBuf == NULL || index > V4L2_OBJECT_MAX_BUFFER_COUNT)
        return OMX_ErrorBadParameter;

    i = index;

    pBufferMap[i].v4l2Buffer.m.planes = &pBufferMap[i].v4l2Plane[0];
    if(bMultiPlane){
        if(1 == nPlanesNum){
            pBufferMap[i].v4l2Plane[0].m.userptr = (unsigned long)pBuf;
            pBufferMap[i].v4l2Plane[0].length = inputBuffer->nAllocLen;
            LOG_LOG("BufferMap_UpdateUserPtr userptr=%p",pBufferMap[i].v4l2Plane[0].m.userptr);
        }else if(2 == nPlanesNum){
            pBufferMap[i].v4l2Plane[0].m.userptr = (unsigned long)pBuf;
            pBufferMap[i].v4l2Plane[0].length = nBufferSize[0];
            pBufferMap[i].v4l2Plane[1].m.userptr = (unsigned long)(pBuf+nBufferSize[0]);
            pBufferMap[i].v4l2Plane[1].length = nBufferSize[1];
            pBufferMap[i].mmap_addr[0] = (OMX_PTR)pBufferMap[i].v4l2Plane[0].m.userptr;
            pBufferMap[i].mmap_addr[1] = (OMX_PTR)pBufferMap[i].v4l2Plane[1].m.userptr;
            LOG_LOG("BufferMap_UpdateUserPtr nPlanesNum2 userptr=%p",pBufferMap[i].v4l2Plane[0].m.userptr);

        }
        pBufferMap[i].v4l2Buffer.length = nPlanesNum;
    }else{
        pBufferMap[i].v4l2Buffer.m.userptr = (unsigned long)pBuf;
        pBufferMap[i].v4l2Buffer.length = inputBuffer->nAllocLen;
    }

    pBufferMap[i].v4l2Buffer.memory = V4L2_MEMORY_USERPTR;

    LOG_DEBUG("[%p]Update userptr buffer, index=%d,ptr=%p planes=%p\n",this,i,pBuf,pBufferMap[i].v4l2Buffer.m.planes);

    return ret;
}
OMX_ERRORTYPE V4l2Object::BufferMap_UpdateDmaBuf(OMX_BUFFERHEADERTYPE *inputBuffer,OMX_U8* pBuf,OMX_U32 index)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 i = 0;

    if(inputBuffer == NULL || pBuf == NULL || index > V4L2_OBJECT_MAX_BUFFER_COUNT)
        return OMX_ErrorBadParameter;

    i = index;

    pBufferMap[i].v4l2Buffer.m.planes = &pBufferMap[i].v4l2Plane[0];
    if(bMultiPlane){
        if(1 == nPlanesNum){
            pBufferMap[i].v4l2Plane[0].m.fd = (unsigned long)pBuf;
            pBufferMap[i].v4l2Plane[0].length = nBufferSize[0];
            LOG_LOG("BufferMap_UpdateDmaBuf fd=%d",pBufferMap[i].v4l2Plane[0].m.fd);

            if((inputBuffer->nFlags & OMX_BUFFERFLAG_EOS) && inputBuffer->nFilledLen == 0){
                pBufferMap[i].v4l2Plane[0].m.fd = -1;
                pBufferMap[i].v4l2Plane[0].length = 0;
            }
        }else if(2 == nPlanesNum){
            //for encoder output device, use same fd for plane[0] and plane[1], set data_offset to length of plane[0]
            pBufferMap[i].v4l2Plane[0].m.fd = (unsigned long)pBuf;
            pBufferMap[i].v4l2Plane[0].data_offset = 0;
            pBufferMap[i].v4l2Plane[0].length = nBufferSize[0];
            pBufferMap[i].v4l2Plane[1].m.fd = (unsigned long)pBuf;
            pBufferMap[i].v4l2Plane[1].data_offset = nBufferSize[0];
            pBufferMap[i].v4l2Plane[1].length = nBufferSize[0] + nBufferSize[1];
            LOG_LOG("BufferMap_UpdateDmaBuf nPlanesNum2 fd0=%d,nBufferSize[0]=%d,nBufferSize[1]=%d",pBufferMap[i].v4l2Plane[0].m.fd, nBufferSize[0], nBufferSize[1]);

        }
        pBufferMap[i].v4l2Buffer.length = nPlanesNum;
    }else{
        pBufferMap[i].v4l2Buffer.m.fd = (unsigned long)pBuf;
        pBufferMap[i].v4l2Buffer.length = nBufferSize[0];
    }

    pBufferMap[i].v4l2Buffer.memory = V4L2_MEMORY_DMABUF;

    LOG_DEBUG("[%p]Update dma buffer, index=%d,fd=%d\n",this,i,pBuf);
    return ret;
}
OMX_ERRORTYPE V4l2Object::BufferIndex_FindIndex(OMX_U8* pBuf,OMX_U32 *index)
{
    OMX_U32 i = 0;
    OMX_BOOL bGot = OMX_FALSE;

    if(index == NULL || pBuf == NULL)
        return OMX_ErrorBadParameter;

    for(i = 0; i < V4L2_OBJECT_MAX_BUFFER_COUNT;i++){
        if(pIndex[i] != NULL && pIndex[i] == pBuf){
            LOG_LOG("[%p]BufferIndex_FindIndex index=%d,%p",this,i,pIndex[i]);
            bGot = OMX_TRUE;
            *index = i;
            break;
        }
    }

    if(bGot)
        return OMX_ErrorNone;
    else
        return OMX_ErrorUndefined;
}
OMX_ERRORTYPE V4l2Object::BufferIndex_GetEmptyIndex(OMX_U32 *index)
{
    OMX_U32 i = 0;
    OMX_BOOL bGot = OMX_FALSE;

    if(index == NULL)
        return OMX_ErrorBadParameter;

    for(i = 0; i < V4L2_OBJECT_MAX_BUFFER_COUNT;i++){
        if(pIndex[i] == NULL){
            bGot = OMX_TRUE;
            *index = i;
            break;
        }
    }

    if(bGot)
        return OMX_ErrorNone;
    else
        return OMX_ErrorUndefined;
}
OMX_ERRORTYPE V4l2Object::BufferIndex_AddIndex(OMX_U8* pBuf,OMX_U32 index)
{
    if(pIndex[index] == NULL && pBuf != NULL){
        pIndex[index] = pBuf;
        LOG_LOG("[%p]BufferIndex_AddIndex [%d,%p]",this,index,pBuf);
        return OMX_ErrorNone;
    }else
        return OMX_ErrorUndefined;
}
OMX_ERRORTYPE V4l2Object::BufferIndex_RemoveIndex(OMX_U32 index)
{
    pIndex[index] = NULL;
    LOG_LOG("[%p]BufferIndex_RemoveIndex [%d]",this,index);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::SetBuffer(OMX_BUFFERHEADERTYPE * inputBuffer,OMX_U32 flags)
{
    int result = 0;
    OMX_U8* pVaddr = NULL;
    OMX_U32 nBufLen = 0;
    OMX_U32 index = 0;
    OMX_ERRORTYPE ret;

    if(inputBuffer == NULL || inputBuffer->pBuffer == NULL){
        LOG_ERROR("[%p]SetBuffer error,%p \n",this,inputBuffer);
        return OMX_ErrorBadParameter;
    }else
        LOG_LOG("[%p]SetBuffer hdr=%p,buf=%p,len=%d,size=%d,ts=%lld", this,inputBuffer, inputBuffer->pBuffer,inputBuffer->nFilledLen,nBufferSize[0],inputBuffer->nTimeStamp);

    fsl_osal_mutex_lock(sMutex);

    pVaddr = inputBuffer->pBuffer;
    nBufLen = inputBuffer->nFilledLen;

    if((flags & V4L2_OBJECT_FLAG_METADATA_BUFFER) && nBufLen > 0){
        pVaddr = (OMX_U8*)((OMX_U64 *)(inputBuffer->pBuffer))[0];
        nBufLen = nBufferSize[0] + nBufferSize[1] + nBufferSize[2];
        LOG_LOG("[%p]SetBuffer metadataBuf %p,len=%d\n",this,pVaddr,nBufLen);
    }
    if(flags & V4L2_OBJECT_FLAG_NATIVE_BUFFER){
        OMX_S32 fd = 0;
        OMX_PTR addr=NULL;
        if(OMX_ErrorNone == GetFdAndAddr( inputBuffer->pBuffer, &fd,&addr) && fd > 0)
            pVaddr = reinterpret_cast<OMX_U8*>(fd);
    }

    if((flags & V4L2_OBJECT_FLAG_METADATA_BUFFER) || (flags & V4L2_OBJECT_FLAG_NATIVE_BUFFER)){
        //if run as encoder, fd count is more than the openmax input port buffer count,
        //so use inputBuffer pointer for buffer index key word
        ret = BufferIndex_FindIndex((OMX_U8*)inputBuffer,&index);
        if(ret != OMX_ErrorNone){
            if(OMX_ErrorNone == BufferIndex_GetEmptyIndex(&index))
                ret = BufferIndex_AddIndex((OMX_U8*)(OMX_U8*)inputBuffer,index);
        }

        if(ret != OMX_ErrorNone){
            LOG_ERROR("[%p]Set Buffer err 1\n",this);
            fsl_osal_mutex_unlock(sMutex);
            return ret;
        }

        ret = BufferMap_Update(inputBuffer,pVaddr,index);
        if(ret != OMX_ErrorNone){
            LOG_ERROR("[%p]Set Buffer err 2\n",this);
            fsl_osal_mutex_unlock(sMutex);
            return ret;
        }
        pBufferMap[index].vaddr = (OMX_U8*)inputBuffer;

    }else{
        ret = BufferIndex_FindIndex(pVaddr,&index);

        if(ret != OMX_ErrorNone){
            if(OMX_ErrorNone == BufferIndex_GetEmptyIndex(&index))
                ret = BufferIndex_AddIndex(pVaddr,index);
        }

        if(ret != OMX_ErrorNone){
            LOG_ERROR("[%p]Set Buffer err 1\n",this);
            fsl_osal_mutex_unlock(sMutex);
            return ret;
        }

        ret = BufferMap_Update(inputBuffer,pVaddr,index);
        if(ret != OMX_ErrorNone){
            LOG_ERROR("[%p]Set Buffer err 2\n",this);
            fsl_osal_mutex_unlock(sMutex);
            return ret;
        }
    }
    


    if(inputBuffer->nTimeStamp != -1){
        //set buffer timestamp 
        pBufferMap[index].v4l2Buffer.timestamp.tv_sec = inputBuffer->nTimeStamp/1000000;
        pBufferMap[index].v4l2Buffer.timestamp.tv_usec = inputBuffer->nTimeStamp -
            pBufferMap[index].v4l2Buffer.timestamp.tv_sec*1000000;
        pBufferMap[index].v4l2Buffer.flags = (V4L2_BUF_FLAG_TIMESTAMP_MASK | V4L2_BUF_FLAG_TIMESTAMP_COPY);
    }

    //update buffer length
    if(bMultiPlane){
        OMX_U32 i = 0;
        //bytes used may be not correct for v4l2Plane[1] & v4l2Plane[2]
        if(1 == nPlanesNum){
            pBufferMap[index].v4l2Plane[0].bytesused = nBufLen;
            pBufferMap[index].v4l2Plane[0].data_offset = 0;

        }else if(2 == nPlanesNum){

            pBufferMap[index].v4l2Plane[0].bytesused = nBufferSize[0];
            pBufferMap[index].v4l2Plane[1].bytesused = nBufferSize[1];
            //for encoder output device, use same fd for plane[0] and plane[1], set data_offset to length of plane[0]
            //data_offset is included in the bytesused.
            if(pBufferMap[index].v4l2Plane[1].data_offset == nBufferSize[0]){
                pBufferMap[index].v4l2Plane[1].bytesused = nBufferSize[0] + nBufferSize[1];
            }
        }else{
            fsl_osal_mutex_unlock(sMutex);
            return OMX_ErrorNotImplemented;
        }

    }else{
        pBufferMap[index].v4l2Buffer.bytesused = nBufLen;
    }

    if(flags & V4L2_OBJECT_FLAG_KEYFRAME)
        pBufferMap[index].v4l2Buffer.flags |= V4L2_BUF_FLAG_KEYFRAME;
    else
        pBufferMap[index].v4l2Buffer.flags &= ~V4L2_BUF_FLAG_KEYFRAME;

    if(inputBuffer->nFlags & OMX_BUFFERFLAG_EOS)
        pBufferMap[index].v4l2Buffer.flags |= V4L2_BUF_FLAG_LAST;

    LOG_LOG("VIDIOC_QBUF len=%d,type=%d plane=%p,ptr=%p,size=%d\n",nBufLen,pBufferMap[index].v4l2Buffer.type,pBufferMap[index].v4l2Buffer.m.planes,pBufferMap[index].v4l2Buffer.m.planes[0].m.mem_offset,pBufferMap[index].v4l2Buffer.m.planes[0].length);
    result = ioctl(mFd, VIDIOC_QBUF, &pBufferMap[index].v4l2Buffer);
    if(result < 0){
        LOG_ERROR("[%p] VIDIOC_QBUF failed ret=%d,index=%d,pVaddr=%p\n",this,result,index,pBufferMap[index].vaddr);
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorUndefined;
    }
    LOG_LOG("VIDIOC_QBUF end\n");
    pQueue->Add(&index);
    LOG_LOG("[%p]SetBuffer index=%d ts=%lld,size=%d\n",this,index,inputBuffer->nTimeStamp,nBufLen);

    fsl_osal_mutex_unlock(sMutex);

    if(!bRunning)
        Start();

    return OMX_ErrorNone;
}

OMX_U32 V4l2Object::ProcessBuffer(OMX_U32 flags)
{
    int result = 0;
    OMX_U32 ret = 0;
    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane stV4lPlane[V4L2_OBJECT_MAX_PLANES];

    OMX_U32 outIndex = 0;
    OMX_U32 outLen = 0;
    OMX_S64 ts = 0;

    //simulate the port setting changed event for coda decoder on input thread
    //for coda encoder, please disable this macro
    //#define TEST_RESOLUTION_CHANGE
    #ifdef TEST_RESOLUTION_CHANGE
    if(nCnt == 0 && eBufferType == V4L2_BUF_TYPE_VIDEO_OUTPUT && !bChange){
        ret = V4L2OBJECT_RESOLUTION_CHANGED;
        outLen = 0;
        bChange = OMX_TRUE;
        return ret;
    }
    #endif

    if(!bRunning)
        return V4L2OBJECT_TIMEOUT;

    bProcessing = OMX_TRUE;
    stV4lBuf.type = eBufferType;
    stV4lBuf.memory = eIOType;
    if(bMultiPlane){
        stV4lBuf.length = nPlanesNum;
        stV4lBuf.m.planes = stV4lPlane;
    }

    fsl_osal_mutex_lock(sMutex);
    if(!bRunning){
        bProcessing = OMX_FALSE;
        fsl_osal_mutex_unlock(sMutex);
        return V4L2OBJECT_ERROR;
    }

    LOG_LOG("[%p]call VIDIOC_DQBUF BEGIN,flags=%x\n",this,flags);
    result = ioctl(mFd, VIDIOC_DQBUF, &stV4lBuf);
    if(result < 0){
        LOG_ERROR("[%p]dequeue buffer failed,ret=%d,q size=%d,out size=%d\n",this,result,pQueue->Size(),pOutQueue->Size());
        bProcessing = OMX_FALSE;
        fsl_osal_mutex_unlock(sMutex);
        return V4L2OBJECT_ERROR;
    }
    LOG_LOG("[%p]call VIDIOC_DQBUF END\n",this);

    outIndex = stV4lBuf.index;

    if(bMultiPlane){
        OMX_U32 i = 0;
        for (i = 0; i < stV4lBuf.length; i++)
        {
            outLen += stV4lBuf.m.planes[i].bytesused;
            pBufferMap[outIndex].v4l2Plane[i].bytesused = stV4lBuf.m.planes[i].bytesused;
            LOG_LOG("VIDIOC_DQBUF i=%d,len=%d \n",i,stV4lBuf.m.planes[i].bytesused);
        }
    }else{
        outLen = stV4lBuf.bytesused;
    }
    nCnt ++;

    if(pBufferMap[outIndex].omxBuffer != NULL){
        pBufferMap[outIndex].omxBuffer->nFlags = 0;

        LOG_LOG("[%p]ProcessBuffer dq buf index=%d,len=%d,flags=%x,nCnt=%d,ptr=0x%x,len=%d,offset=%d\n",this,outIndex,outLen,stV4lBuf.flags,nCnt,stV4lBuf.m.planes[0].m.mem_offset,stV4lBuf.m.planes[0].length,stV4lBuf.m.planes[0].data_offset);
        ts = (OMX_S64)stV4lBuf.timestamp.tv_sec *1000000;
        ts += stV4lBuf.timestamp.tv_usec;
        pBufferMap[outIndex].omxBuffer->nTimeStamp = ts;

        if(!(flags & V4L2_OBJECT_FLAG_METADATA_BUFFER))
            pBufferMap[outIndex].omxBuffer->nFilledLen = outLen;

        if(outLen > 0)
            pBufferMap[outIndex].omxBuffer->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;

        if(stV4lBuf.flags & V4L2_BUF_FLAG_KEYFRAME)
            pBufferMap[outIndex].omxBuffer->nFlags |= OMX_BUFFERFLAG_SYNCFRAME;

        if(stV4lBuf.flags & V4L2_BUF_FLAG_LAST)
            pBufferMap[outIndex].omxBuffer->nFlags |= OMX_BUFFERFLAG_EOS;

    }else if(outLen > 0){
        pBufferMap[outIndex].dmaBuffer->bReadyForProcess = OMX_TRUE;
        if(stV4lBuf.field == V4L2_FIELD_INTERLACED)
            pBufferMap[outIndex].dmaBuffer->flag |= DMA_BUF_INTERLACED;
        else
            pBufferMap[outIndex].dmaBuffer->flag = 0;
            //pBufferMap[outIndex].dmaBuffer->flag &= ~DMA_BUF_INTERLACED;

        if(stV4lBuf.flags & V4L2_BUF_FLAG_LAST)
           pBufferMap[outIndex].dmaBuffer->flag |= DMA_BUF_EOS;

    }

    
    pOutQueue->Add(&outIndex);
    bProcessing = OMX_FALSE;
    LOG_LOG("[%p] ProcessBuffer END ret=%d ts=%lld,size=%d\n",this,ret,ts,outLen);

    fsl_osal_mutex_unlock(sMutex);

    return ret;

}
OMX_BOOL V4l2Object::HasEmptyBuffer()
{
    if(pQueue->Size() < nBufferCount){
        return OMX_TRUE;
    }else{
        return OMX_FALSE;
    }
}
OMX_BOOL V4l2Object::HasOutputBuffer()
{
    if(pOutQueue == NULL)
        return OMX_FALSE;

    if(pOutQueue->Size() > 0){
        return OMX_TRUE;
    }else{
        return OMX_FALSE;
    }
}
OMX_BOOL V4l2Object::HasBuffer()
{
    if(pQueue == NULL)
        return OMX_FALSE;

    if(pQueue->Size() > 0){
        return OMX_TRUE;
    }else{
        return OMX_FALSE;
    }
}
OMX_ERRORTYPE V4l2Object::GetOutputBuffer(OMX_BUFFERHEADERTYPE ** bufHdlr)
{
    OMX_U32 index = 0;
    OMX_U32 i = 0;
    OMX_U32 temp = 0;
    if(bufHdlr == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_mutex_lock(sMutex);

    if(pOutQueue->Size()==0 || pQueue->Size()==0){
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }

    pOutQueue->Get(&index);

    for(i = 1; i <= pQueue->Size(); i++){
        if(pQueue->Access(&temp, i) == QUEUE_SUCCESS){
            if(temp == index){
                pQueue->Get(&temp,i);
                break;
            }
        }
    }

#if 0
    //copy mmap buffer
    if(2 == nPlanesNum && pBufferMap[index].omxBuffer->nFilledLen > 0){
        OMX_U32 offset = pBufferMap[index].v4l2Plane[0].bytesused;
        fsl_osal_memcpy(pBufferMap[index].omxBuffer->pBuffer+offset,pBufferMap[index].mmap_addr[1]
        ,pBufferMap[index].v4l2Plane[1].bytesused);
    }
#endif
    *bufHdlr = pBufferMap[index].omxBuffer;
    LOG_LOG("[%p]GetOutputBuffer index=%d,bufHdlr=%p,buf=%p\n",this,index,pBufferMap[index].omxBuffer,pBufferMap[index].omxBuffer->pBuffer);

    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Object::GetBuffer(OMX_BUFFERHEADERTYPE ** bufHdlr)
{
    OMX_U32 index = 0;

    if(bufHdlr == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_mutex_lock(sMutex);
    if(pQueue->Size()==0){
        LOG_ERROR("[%p]can not get output buffer\n",this);
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }


    pQueue->Get(&index);

    LOG_LOG("[%p]GetBuffer index=%d\n",this,index);

    *bufHdlr = pBufferMap[index].omxBuffer;
    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}


OMX_PTR V4l2Object::AllocateBuffer(OMX_U32 nSize)
{
    int result = 0;
    OMX_U8 * ptr = NULL;
    eIOType = V4L2_MEMORY_MMAP;
    OMX_U32 index;
    OMX_U32 i = 0;
    struct v4l2_buffer stV4lBuf;
    struct v4l2_plane planes[V4L2_OBJECT_MAX_PLANES];

    if(!bBufferRequired)
        return NULL;

    if(nBufferNum > nBufferCount)
        return NULL;

    if(OMX_ErrorNone != BufferIndex_GetEmptyIndex(&index)){
        return NULL;
    }

    if(bMultiPlane){
        stV4lBuf.type = eBufferType;
        stV4lBuf.memory = V4L2_MEMORY_MMAP;
        stV4lBuf.index = index;
        stV4lBuf.length = nPlanesNum;
        stV4lBuf.m.planes = planes;
        result = ioctl(mFd, VIDIOC_QUERYBUF, &stV4lBuf);
        if(result < 0)
            return NULL;

        if(1 == nPlanesNum){
            planes[0].length = nSize;

            ptr = (OMX_U8 *)mmap(NULL, planes[0].length,
                PROT_READ | PROT_WRITE, /* recommended */
                MAP_SHARED,             /* recommended */
                mFd, planes[0].m.mem_offset);
            if(ptr != MAP_FAILED){
                pBufferMap[index].mmap_addr[i] = ptr;
            }
            LOG_LOG("offset=%x,ptr=%p\n",stV4lBuf.m.planes[0].m.mem_offset,ptr);
        }else if(2 == nPlanesNum){
            planes[0].length = nSize*2/3;
            planes[1].length = nSize/3;
            if(nSize == nBufferSize[0] + nBufferSize[1])
            {
                planes[0].length = nBufferSize[0];
                planes[1].length = nBufferSize[1];
                LOG_LOG("update buffer size for 2 planer %d,%d \n",nBufferSize[0],nBufferSize[1]);
            }

            ptr = (OMX_U8 *)mmap(NULL,planes[0].length,
                PROT_READ | PROT_WRITE, /* recommended */
                MAP_SHARED,             /* recommended */
                mFd, planes[0].m.mem_offset);
            if(ptr != MAP_FAILED){
                pBufferMap[index].mmap_addr[0] = ptr;
            }
            LOG_LOG("offset 0=%x,ptr=%p\n",stV4lBuf.m.planes[0].m.mem_offset,ptr);



            ptr = (OMX_U8 *)mmap(NULL, planes[1].length,
                PROT_READ | PROT_WRITE, /* recommended */
                MAP_SHARED,             /* recommended */
                mFd, planes[1].m.mem_offset);
            if(ptr != MAP_FAILED){
                pBufferMap[index].mmap_addr[1] = ptr;
            }
            LOG_LOG("offset 1=%x,ptr=%p\n",stV4lBuf.m.planes[1].m.mem_offset,ptr);
            nBufferSize[0] = planes[0].length;
            nBufferSize[1] = planes[1].length;
        }
        if(MAP_FAILED == ptr)
            return NULL;

        //only return mmap_addr[0]
        fsl_osal_memcpy(&pBufferMap[index].v4l2Buffer,&stV4lBuf,sizeof(struct v4l2_buffer));
        fsl_osal_memcpy(&pBufferMap[index].v4l2Plane,&planes,V4L2_OBJECT_MAX_PLANES * sizeof(struct v4l2_plane));
        pBufferMap[index].v4l2Buffer.m.planes = &pBufferMap[index].v4l2Plane[0];

        ptr = (OMX_U8 *)pBufferMap[index].mmap_addr[0];

        LOG_DEBUG("[%p] AllocateBuffer multi %p\n",this,ptr);

    }else{

        //set buffer size and index
        stV4lBuf.type = eBufferType;
        stV4lBuf.memory = V4L2_MEMORY_MMAP;
        stV4lBuf.index = index;
        stV4lBuf.length = nSize;
        result = ioctl(mFd, VIDIOC_QUERYBUF, &stV4lBuf);
        if(result < 0)
            return NULL;

        ptr = (OMX_U8 *)mmap(NULL, stV4lBuf.length,
            PROT_READ | PROT_WRITE, /* recommended */
            MAP_SHARED,             /* recommended */
            mFd, stV4lBuf.m.offset);

        if(MAP_FAILED == ptr)
            return NULL;
        fsl_osal_memcpy(&pBufferMap[index].v4l2Buffer,&stV4lBuf,sizeof(struct v4l2_buffer));
        LOG_DEBUG("[%p] AllocateBuffer single %p\n",this,ptr);
        nBufferSize[0] = nSize;

    }

    pBufferMap[index].vaddr = ptr;
    nBufferNum ++;

    BufferIndex_AddIndex(ptr,index);
    
    LOG_DEBUG("[%p] AllocateBuffer index=%d,bufferNum=%d\n",this,index,nBufferNum);

    return (OMX_PTR)pBufferMap[index].vaddr;

}
OMX_ERRORTYPE V4l2Object::FreeBuffer(OMX_PTR buffer)
{
    OMX_U32 index = 0;
    OMX_U32 len;
    if(nBufferNum == 0)
        return OMX_ErrorResourcesLost;

    if(OMX_ErrorNone != BufferIndex_FindIndex((OMX_U8*)buffer,&index))
        return OMX_ErrorResourcesLost;

    if(bMultiPlane){
        OMX_U32 i = 0;
        for(i = 0; i < nPlanesNum; i++){
            len = pBufferMap[index].v4l2Buffer.m.planes[i].length;
            if(pBufferMap[index].mmap_addr[i] != NULL)
                munmap(pBufferMap[index].mmap_addr[i],len);
            fsl_osal_memset(&pBufferMap[index].v4l2Plane[i],0,sizeof(struct v4l2_plane));
        }
    }else{
        len = pBufferMap[index].v4l2Buffer.length;
        if(pBufferMap[index].vaddr != NULL)
            munmap(pBufferMap[index].vaddr, len);
    }

    fsl_osal_memset(&pBufferMap[index].v4l2Buffer,0,sizeof(struct v4l2_buffer));
    nBufferNum --;
    pBufferMap[index].vaddr = NULL;
    LOG_DEBUG("[%p] FreeBuffer index=%d,ptr=%p",this,index,buffer);

    BufferIndex_RemoveIndex(index);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::Flush(void)
{
    OMX_U32 i = 0;
    LOG_DEBUG("[%p] flush BEGIN\n",this);
    Stop();
    while(pOutQueue->Size() > 0){
        pOutQueue->Get(&i);
    }
    LOG_DEBUG("[%p] flush END\n",this);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Object::Start(void)
{
    if(!bRunning){
        int result = 0;
        fsl_osal_mutex_lock(sMutex);

        result = ioctl(mFd, VIDIOC_STREAMON, &eBufferType);

        if(result == 0)
            bRunning = OMX_TRUE;
        fsl_osal_mutex_unlock(sMutex);

        LOG_LOG("[%p]V4l2Object::Start VIDIOC_STREAMON.ret=%d\n",this,result);
    }
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::Stop(void)
{
    OMX_U32 index = 0;

    if(bRunning){
        int result = 0;
        fsl_osal_mutex_lock(sMutex);
        LOG_LOG("[%p] call VIDIOC_STREAMOFF",this);
        result = ioctl(mFd, VIDIOC_STREAMOFF, &eBufferType);
        //when reach eos, the ioctl return -1, ignore the result
        bRunning = OMX_FALSE;
        fsl_osal_mutex_unlock(sMutex);

        LOG_LOG("[%p]V4l2Object::Stop VIDIOC_STREAMOFF ret=%d\n",this,result);

    }
    while(OMX_TRUE){
        if(!bProcessing)
            break;
        fsl_osal_sleep(1000);
    }
    LOG_LOG("[%p]V4l2Object::Stop END\n",this);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::Select()
{
    OMX_BOOL bRead = OMX_TRUE;
    struct timeval  tv;
    fd_set      fds;
    int r;
    FD_ZERO(&fds);
    FD_SET(mFd, &fds);
    // Timeout
    tv.tv_sec = 2;
    tv.tv_usec = 0;


    if(V4L2_TYPE_IS_OUTPUT(eBufferType))
        bRead = OMX_FALSE;
       
    LOG_LOG("[%p]Select Begin read=%d\n",this,bRead);

    while(OMX_TRUE){
        if(bRead)
            r = select(mFd + 1, &fds, NULL, NULL, &tv);
        else
            r = select(mFd + 1, NULL, &fds, NULL, &tv);
        if(r < 0){
            break;
        }else if(r == 0){
            return OMX_ErrorTimeout;
        }else if(FD_ISSET(mFd,&fds)){
            break;
        }
    }
    LOG_LOG("[%p]Select END\n",this);
    return OMX_ErrorNone;
}
OMX_BOOL V4l2Object::isMMap()
{
    if(eIOType == V4L2_MEMORY_MMAP)
        return OMX_TRUE;
    else
        return OMX_FALSE;
}
OMX_ERRORTYPE V4l2Object::SetBuffer(DmaBufferHdr * inputBuffer,OMX_U32 flags)
{
    int result = 0;
    OMX_U8* pVaddr = NULL;
    OMX_U32 nBufLen = 0;
    OMX_U32 index = 0;
    OMX_ERRORTYPE ret;

    if(inputBuffer == NULL){
        LOG_ERROR("[%p]SetBuffer error");
        return OMX_ErrorBadParameter;
    }else
        LOG_LOG("[%p]SetBuffer fd0=%d,fd1=%d,size[0]=%d,size[1]=%d", this, 
            inputBuffer->plane[0].fd, inputBuffer->plane[1].fd, inputBuffer->plane[0].size, inputBuffer->plane[1].size);

    fsl_osal_mutex_lock(sMutex);

    pVaddr = (OMX_U8*)(uintptr_t)inputBuffer->plane[0].fd;

    ret = BufferIndex_FindIndex(pVaddr,&index);

    if(ret != OMX_ErrorNone){
        if(OMX_ErrorNone == BufferIndex_GetEmptyIndex(&index))
            ret = BufferIndex_AddIndex(pVaddr,index);
    }

    if(ret != OMX_ErrorNone){
        LOG_ERROR("[%p]Set Buffer err 1\n",this);
        fsl_osal_mutex_unlock(sMutex);
        return ret;
    }
    
    ret = BufferMap_UpdateDmaBuf(inputBuffer,pVaddr,index);
    if(ret != OMX_ErrorNone){
        LOG_ERROR("[%p]Set Buffer err 2\n",this);
        fsl_osal_mutex_unlock(sMutex);
        return ret;
    }

    //update buffer length
    if(bMultiPlane){
        OMX_U32 i = 0;
        //bytes used may be not correct for v4l2Plane[1] & v4l2Plane[2]
        if(1 == nPlanesNum){
            pBufferMap[index].v4l2Plane[0].bytesused = nBufLen;
            pBufferMap[index].v4l2Plane[0].data_offset = 0;

        }else if(2 == nPlanesNum){

            pBufferMap[index].v4l2Plane[0].bytesused = pBufferMap[index].v4l2Plane[0].length;
            pBufferMap[index].v4l2Plane[1].bytesused = pBufferMap[index].v4l2Plane[1].length;
            //TODO: set to 0 when encoder driver support 0 bytesused
            //pBufferMap[index].v4l2Plane[0].bytesused = 0;
            //pBufferMap[index].v4l2Plane[1].bytesused = 0;

        }else{
            fsl_osal_mutex_unlock(sMutex);
            return OMX_ErrorNotImplemented;
        }

    }else{
        pBufferMap[index].v4l2Buffer.bytesused = nBufLen;
    }

    if(flags & V4L2_OBJECT_FLAG_KEYFRAME)
        pBufferMap[index].v4l2Buffer.flags |= V4L2_BUF_FLAG_KEYFRAME;
    else
        pBufferMap[index].v4l2Buffer.flags &= ~V4L2_BUF_FLAG_KEYFRAME;

    if(inputBuffer->flag & DMA_BUF_EOS)
        pBufferMap[index].v4l2Buffer.flags |= V4L2_BUF_FLAG_LAST;


    LOG_LOG("[%p] VIDIOC_QBUF len=%d,type=%d,planes=%d index=%d, fd0=%d, size=%d, fd1=%d,size=%d\n",
        this,
        nBufLen,
        pBufferMap[index].v4l2Buffer.type,
        pBufferMap[index].v4l2Buffer.length,
        pBufferMap[index].v4l2Buffer.index,
        pBufferMap[index].v4l2Buffer.m.planes[0].m.fd,
        pBufferMap[index].v4l2Buffer.m.planes[0].length,
        pBufferMap[index].v4l2Buffer.m.planes[1].m.fd,
        pBufferMap[index].v4l2Buffer.m.planes[1].length
        );
    result = ioctl(mFd, VIDIOC_QBUF, &pBufferMap[index].v4l2Buffer);
    if(result < 0){
        LOG_ERROR("[%p] VIDIOC_QBUF failed ret=%d,index=%d\n",this,result,index);
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorUndefined;
    }
    LOG_LOG("VIDIOC_QBUF end\n");
    pQueue->Add(&index);
    LOG_LOG("[%p]SetBuffer DMA index=%d end\n",this,index);

    fsl_osal_mutex_unlock(sMutex);

    if(!bRunning && !(flags & V4L2_OBJECT_FLAG_NO_AUTO_START))
        Start();

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::GetOutputBuffer(DmaBufferHdr ** bufHdlr)
{
    OMX_U32 index = 0;
    OMX_U32 i = 0;
    OMX_U32 temp = 0;
    if(bufHdlr == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_mutex_lock(sMutex);

    if(pOutQueue->Size()==0 || pQueue->Size()==0){
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }

    pOutQueue->Get(&index);

    for(i = 1; i <= pQueue->Size(); i++){
        if(pQueue->Access(&temp, i) == QUEUE_SUCCESS){
            if(temp == index){
                pQueue->Get(&temp,i);
                break;
            }
        }
    }

    *bufHdlr = pBufferMap[index].dmaBuffer;
    LOG_LOG("[%p]GetOutputBuffer index=%d,buf=%p\n",this,index,pBufferMap[index].dmaBuffer);

    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Object::GetBuffer(DmaBufferHdr ** bufHdlr)
{
    OMX_U32 index = 0;

    if(bufHdlr == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_mutex_lock(sMutex);
    if(pQueue->Size()==0){
        LOG_ERROR("[%p]can not get output buffer\n",this);
        fsl_osal_mutex_unlock(sMutex);
        return OMX_ErrorNoMore;
    }

    pQueue->Get(&index);

    LOG_LOG("[%p]GetBuffer dma index=%d\n",this,index);

    *bufHdlr = pBufferMap[index].dmaBuffer;
    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}

