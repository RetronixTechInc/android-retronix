#include "IsiColorConvert.h"
#include <linux/videodev2.h>

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_LOG
#define LOG_LOG printf
#endif

#define MIN_CAPTURE_BUFFER_CNT (3)

#define INPUT_BUFFER_PLANE (1)
#define DMA_BUFFER_PLANE (2)

void *colorConvertThreadHandler(void *arg)
{
    IsiColorConvert *base = (IsiColorConvert*)arg;
    OMX_S32 ret = 0;
    OMX_S32 poll_ret = 0;
    OMX_BOOL call_callback = OMX_FALSE;

    LOG_LOG("[%p]colorConvertThreadHandler BEGIN \n",base);
    poll_ret = base->pColorDev->Poll(base->nFd);
    LOG_LOG("[%p]colorConvertThreadHandler END ret=%x \n",base,poll_ret);


    if(poll_ret & V4L2_DEV_POLL_RET_OUTPUT){
        LOG_LOG("V4L2_DEV_POLL_RET_OUTPUT \n");
        base->inObj->ProcessBuffer(V4L2_OBJECT_FLAG_METADATA_BUFFER);
        call_callback = OMX_TRUE;
    }

    if(poll_ret & V4L2_DEV_POLL_RET_CAPTURE){
        LOG_LOG("V4L2_DEV_POLL_RET_CAPTURE \n");
        base->outObj->ProcessBuffer(0);
        call_callback = OMX_TRUE;
    }

    if(call_callback && base->pCallback != NULL)
        base->pCallback(base, base->pAppData);
    return NULL;
}

IsiColorConvert::IsiColorConvert()
{
    pColorDev = NULL;
    inObj = NULL;
    outObj = NULL;
    pThread = NULL;
    pCallback = NULL;
    pAppData = NULL;

    bPrepared = OMX_FALSE;
    bCreated = OMX_FALSE;
    bEos = OMX_FALSE;
    nFd = -1;
    fsl_osal_memset(devName, 0, 32);
}
OMX_ERRORTYPE IsiColorConvert::Create()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(bCreated)
        return ret;
    
    if(pColorDev == NULL){
        pColorDev = FSL_NEW(V4l2Dev,());
        if(pColorDev == NULL)
            return OMX_ErrorInsufficientResources;
    }

    ret = pColorDev->LookupNode(V4L2_DEV_TYPE_ISI,&devName[0]);

    if(ret != OMX_ErrorNone)
        return ret;

    nFd = pColorDev->Open(V4L2_DEV_TYPE_ISI, &devName[0]);
    LOG_DEBUG("pColorDev->Open path=%s,fd=%d",devName, nFd);

    if(nFd < 0)
        return OMX_ErrorInsufficientResources;

    if(inObj == NULL)
        inObj = FSL_NEW(V4l2Object,());

    if(outObj == NULL)
        outObj = FSL_NEW(V4l2Object,());

    if(inObj == NULL || outObj == NULL)
        return OMX_ErrorInsufficientResources;

    if(pColorDev->isV4lBufferTypeSupported(nFd,V4L2_DEV_TYPE_ISI,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)){
        ret = inObj->Create(nFd,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,INPUT_BUFFER_PLANE);
        LOG_DEBUG("HandleStart create output ret=%x",ret);
    }
    if(ret != OMX_ErrorNone)
        return ret;

    if(pColorDev->isV4lBufferTypeSupported(nFd,V4L2_DEV_TYPE_ISI,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)){
        ret = outObj->Create(nFd,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,DMA_BUFFER_PLANE);
        LOG_DEBUG("HandleStart create capture ret=%x",ret);
    }
    if(ret != OMX_ErrorNone)
        return ret;

    pThread = FSL_NEW(VThread,());
    if(pThread == NULL)
        return OMX_ErrorInsufficientResources;

    ret = pThread->create(this,OMX_FALSE,colorConvertThreadHandler);
    if(ret != OMX_ErrorNone)
        return ret;

    bCreated = OMX_TRUE;
    nCnt = 0;
    nInputCnt = 0;
    nOutputCnt = 0;
    nInputBufferCnt = 0;
    nOutputBufferCnt = 0;
    return ret;
}
OMX_ERRORTYPE IsiColorConvert::SetCallbackFunc(ISI_CALLBACKTYPE *callback, OMX_PTR appData)
{
    if(callback == NULL || appData == NULL)
        return OMX_ErrorBadParameter;

    pCallback = callback;
    pAppData = appData;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE IsiColorConvert::Flush()
{
    inObj->Flush();
    outObj->Flush();
    if(pCallback)
        pCallback(this, pAppData);

    nInputCnt = 0;
    nOutputCnt = 0;
    nCnt = 0;
    return OMX_ErrorNone;
}

typedef struct{
OMX_U32 v4l2_format;
OMX_U32 omx_format;
}V4L2_FORMAT_TABLE;

static V4L2_FORMAT_TABLE color_format_table[]={
{v4l2_fourcc('R', 'G', 'B', 'A'), OMX_COLOR_Format32bitRGBA8888},
{V4L2_PIX_FMT_RGB565, OMX_COLOR_Format16bitRGB565},
{V4L2_PIX_FMT_NV12, OMX_COLOR_FormatYUV420SemiPlanar},
};

OMX_U32 IsiColorConvert::getV4l2Format(OMX_U32 color_format)
{
    OMX_U32 i=0;
    OMX_BOOL bGot=OMX_FALSE;
    OMX_U32 out = 0;
    for(i = 0; i < sizeof(color_format_table)/sizeof(V4L2_FORMAT_TABLE);i++){
        if(color_format == color_format_table[i].omx_format){
            bGot = OMX_TRUE;
            out = color_format_table[i].v4l2_format;
            break;
        }
    }

    if(bGot)
        return out;
    else
        return 0;
}
OMX_ERRORTYPE IsiColorConvert::setParam()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    V4l2ObjectFormat in_fmt;
    V4l2ObjectFormat out_fmt;
    
    in_fmt.format = getV4l2Format(sInFormat.format);
    in_fmt.width = sInFormat.width;
    in_fmt.height = sInFormat.height;
    in_fmt.stride = sInFormat.stride;
    in_fmt.plane_num = 1;
    in_fmt.bufferSize[0] = sInFormat.bufferSize;
 
    ret = inObj->SetFormat(&in_fmt);
    if(ret != OMX_ErrorNone)
        return ret;

    ret = inObj->GetFormat(&in_fmt);
    if(ret != OMX_ErrorNone)
        return ret;

    out_fmt.format = getV4l2Format(sOutFormat.format);
    out_fmt.width = sOutFormat.width;
    out_fmt.height = sOutFormat.height;
    out_fmt.stride = sOutFormat.stride;
    out_fmt.plane_num = 1;
    out_fmt.bufferSize[0] = sOutFormat.bufferSize;

    ret = outObj->SetFormat(&out_fmt);
    if(ret != OMX_ErrorNone)
        return ret;

    ret = outObj->GetFormat(&out_fmt);
    if(ret != OMX_ErrorNone)
        return ret;

    LOG_DEBUG("setParam width=%d,height=%d ret=%d",out_fmt.width, out_fmt.height, ret);
    return ret;
}
OMX_ERRORTYPE IsiColorConvert::prepare()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    //encoder input buffer count
    ret = inObj->SetBufferCount(nInputBufferCnt, V4L2_MEMORY_DMABUF,INPUT_BUFFER_PLANE);
    if(ret != OMX_ErrorNone)
        return ret;

    //encoder dma buffer count
    ret = outObj->SetBufferCount(nOutputBufferCnt, V4L2_MEMORY_DMABUF,DMA_BUFFER_PLANE);

    LOG_DEBUG("IsiColorConvert::prepare");
    return ret;
}
IsiColorConvert::~IsiColorConvert()
{
    if(pThread){
        pThread->destroy();
        FSL_DELETE(pThread);
    }

    if(inObj){
        inObj->ReleaseResource();
        FSL_DELETE(inObj);
    }

    if(outObj){
        outObj->ReleaseResource();
        FSL_DELETE(outObj);
    }

    if(pColorDev){
        pColorDev->Close(nFd);
        FSL_DELETE(pColorDev);
    }

}
OMX_ERRORTYPE IsiColorConvert::AddInputFrame(OMX_BUFFERHEADERTYPE *inBufHdlr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 flag = V4L2_OBJECT_FLAG_METADATA_BUFFER;
    LOG_DEBUG("IsiColorConvert::AddInputFrame BEGIN");

    if(inBufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(!bPrepared){

        ret = setParam();
        if(ret != OMX_ErrorNone)
            return ret;

        ret = prepare();
        if(ret != OMX_ErrorNone)
            return ret;
        bPrepared = OMX_TRUE;

    }

    if(inBufHdlr->nFlags & OMX_BUFFERFLAG_SYNCFRAME)
        flag |= V4L2_OBJECT_FLAG_KEYFRAME;

    if((inBufHdlr->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) && 0 == inBufHdlr->nFilledLen){
        bEos = OMX_TRUE;
        return ret;
    }

    ret = inObj->SetBuffer(inBufHdlr, flag);

    if(ret == OMX_ErrorNone)
        nInputCnt++;
    LOG_DEBUG("IsiColorConvert::AddInputFrame flag=%x,nInputCnt=%d",flag,nInputCnt);
    return ret;
}
OMX_ERRORTYPE IsiColorConvert::GetInputBuffer(OMX_BUFFERHEADERTYPE **inBufHdlr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 flag = 0;

    if(inBufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(inObj->HasBuffer()){
        ret = inObj->GetBuffer(inBufHdlr);
    }else
        ret = OMX_ErrorNoMore;

    LOG_DEBUG("IsiColorConvert::GetInputBuffer flag=%x",flag);
    return ret;
}
OMX_ERRORTYPE IsiColorConvert::GetInputReturnBuffer(OMX_BUFFERHEADERTYPE **inBufHdlr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 flag = 0;

    if(inBufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(inObj->HasOutputBuffer()){
        ret = inObj->GetOutputBuffer(inBufHdlr);
    }else
        ret = OMX_ErrorNoMore;

    LOG_DEBUG("IsiColorConvert::GetInputReturnBuffer flag=%x",flag);
    return ret;
}
OMX_ERRORTYPE IsiColorConvert::AddOutputFrame(DmaBufferHdr *outBufHdlr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 flag = V4L2_OBJECT_FLAG_METADATA_BUFFER;
    LOG_DEBUG("IsiColorConvert::AddOutputFrame BEGIN");

    if(outBufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(!bPrepared){
        
        ret = setParam();
        if(ret != OMX_ErrorNone)
            return ret;

        ret = prepare();
        if(ret != OMX_ErrorNone)
            return ret;
        bPrepared = OMX_TRUE;
    }

    ret = outObj->SetBuffer(outBufHdlr,V4L2_OBJECT_FLAG_NO_AUTO_START);
    if(ret != OMX_ErrorNone)
        return ret;

    nCnt ++;
    //start when output has 3 frames according to driver requires
    if(MIN_CAPTURE_BUFFER_CNT == nCnt){
        outObj->Start();
        pThread->start();
    }

    LOG_DEBUG("IsiColorConvert::AddOutputFrame");
    return ret;
}
OMX_ERRORTYPE IsiColorConvert::GetOutputBuffer(DmaBufferHdr **outBufHdlr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 flag = 0;

    if(outBufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(outObj->HasBuffer()){
        ret = outObj->GetBuffer(outBufHdlr);
    }else
        ret = OMX_ErrorNoMore;

    LOG_DEBUG("IsiColorConvert::GetOutputBuffer ret=%x",ret);
    return ret;
}
OMX_ERRORTYPE IsiColorConvert::GetOutputReturnBuffer(DmaBufferHdr **outBufHdlr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 flag = 0;

    if(outBufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(outObj->HasOutputBuffer()){
        ret = outObj->GetOutputBuffer(outBufHdlr);
        if(ret == OMX_ErrorNone)
            nOutputCnt++;
    }else
        ret = OMX_ErrorNoMore;

    LOG_DEBUG("IsiColorConvert::GetOutputReturnBuffer ret=%x, nOutputCnt=%d",ret,nOutputCnt);
    return ret;
}
OMX_ERRORTYPE IsiColorConvert::ConfigInput(ISI_FORMAT* fmt)
{
    if(fmt == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_memcpy(&sInFormat, fmt, sizeof(PROCESS3_FORMAT));
    LOG_DEBUG("Process3::ConfigInput in w=%d,h=%d,stride=%d",sInFormat.width,sInFormat.height,sInFormat.stride);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE IsiColorConvert::ConfigOutput(ISI_FORMAT* fmt)
{
    if(fmt == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_memcpy(&sOutFormat, fmt, sizeof(PROCESS3_FORMAT));
    LOG_DEBUG("Process3::ConfigOutput in w=%d,h=%d,stride=%d",sOutFormat.width,sOutFormat.height,sOutFormat.stride);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE IsiColorConvert::SetBufferCount(OMX_U32 in_cnt, OMX_U32 out_cnt)
{
    if(in_cnt < 1 || out_cnt < MIN_CAPTURE_BUFFER_CNT)
        return OMX_ErrorBadParameter;

    nInputBufferCnt = in_cnt;
    nOutputBufferCnt = out_cnt;
    return OMX_ErrorNone;
}

OMX_BOOL IsiColorConvert::GetEos()
{
    if(bEos && nInputCnt > 0 && nOutputCnt > 0 && nOutputCnt >= nInputCnt){
        bEos = OMX_FALSE;
        return OMX_TRUE;
    }
    return OMX_FALSE;
}
