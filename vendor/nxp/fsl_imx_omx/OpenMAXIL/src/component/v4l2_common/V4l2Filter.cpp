/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "V4l2Filter.h"
#ifdef ENABLE_TS_MANAGER
#include "Tsm_wrapper.h"
#endif

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_LOG
#define LOG_LOG printf
#endif

typedef struct{
OMX_U32 v4l2_format;
OMX_U32 omx_format;
OMX_U32 flag;
}V4L2_FORMAT_TABLE;

static V4L2_FORMAT_TABLE codec_format_table[]={
    /* compressed formats */
    {V4L2_PIX_FMT_JPEG, OMX_VIDEO_CodingMJPEG,0},//V4L2_PIX_FMT_MJPEG
    {V4L2_PIX_FMT_MPEG2, OMX_VIDEO_CodingMPEG2,0},
    {V4L2_PIX_FMT_MPEG4, OMX_VIDEO_CodingMPEG4,0},
    {V4L2_PIX_FMT_H263, OMX_VIDEO_CodingH263,0},
    {V4L2_PIX_FMT_H264,  OMX_VIDEO_CodingAVC,0},
    {V4L2_PIX_FMT_VP8, OMX_VIDEO_CodingVP8,0},
    {V4L2_PIX_FMT_VC1_ANNEX_G, OMX_VIDEO_CodingWMV9,0},
    //{V4L2_PIX_FMT_VC1_ANNEX_L, OMX_VIDEO_WMVFormatWVC1,0},
    {v4l2_fourcc('H', 'E', 'V', 'C'),OMX_VIDEO_CodingHEVC,0},
    {v4l2_fourcc('H', 'E', 'V', 'C'),11,0},//backup for vpu test
    {v4l2_fourcc('D', 'I', 'V', 'X'), OMX_VIDEO_CodingXVID,0},
    {v4l2_fourcc('V', 'P', '6', '0'), OMX_VIDEO_CodingVP6,0},
    {v4l2_fourcc('D', 'I', 'V', 'X'), OMX_VIDEO_CodingDIVX,0},
    {v4l2_fourcc('D', 'I', 'V', 'X'), OMX_VIDEO_CodingDIV3,0},
    {v4l2_fourcc('D', 'I', 'V', 'X'), OMX_VIDEO_CodingDIV4,0},
    {v4l2_fourcc('R', 'V', '0', '0'), OMX_VIDEO_CodingRV,0},
};

//need add more format
static V4L2_FORMAT_TABLE color_format_table[]={
    //test for nv 12: V4L2_PIX_FMT_NV12
    {V4L2_PIX_FMT_NV12, OMX_COLOR_FormatAndroidOpaque, COLOR_FORMAT_FLAG_2_PLANE},
    {V4L2_PIX_FMT_NV12, OMX_COLOR_FormatYCbYCr,   COLOR_FORMAT_FLAG_2_PLANE},
    {V4L2_PIX_FMT_NV12, OMX_COLOR_Format16bitRGB565,   COLOR_FORMAT_FLAG_2_PLANE},
    {V4L2_PIX_FMT_NV12, OMX_COLOR_FormatYUV420SemiPlanar,COLOR_FORMAT_FLAG_2_PLANE},
    {V4L2_PIX_FMT_NV12, OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled,COLOR_FORMAT_FLAG_2_PLANE},
    //{V4L2_PIX_FMT_YUV420 , OMX_COLOR_FormatYUV420Planar,COLOR_FORMAT_FLAG_2_PLANE},
    //{V4L2_PIX_FMT_NV12 , OMX_COLOR_FormatYUV420SemiPlanar,COLOR_FORMAT_FLAG_SINGLE_PLANE},
    //{V4L2_PIX_FMT_NV12 , OMX_COLOR_FormatYUV420SemiPlanar,COLOR_FORMAT_FLAG_2_PLANE},
};

V4l2Filter::V4l2Filter()
{
    bInContext = OMX_FALSE;
    nPorts = NUM_PORTS;
    nFd = -1;
    inThread = NULL;
    inObj = NULL;
    outObj = NULL;
    pV4l2Dev = NULL;
    sMutex = NULL;
}
OMX_U32 V4l2Filter::ConvertOmxColorFormatToV4l2Format(OMX_COLOR_FORMATTYPE color_format,OMX_U32 flag)
{
    OMX_U32 i=0;
    OMX_BOOL bGot=OMX_FALSE;
    OMX_U32 out = 0;
    for(i = 0; i < sizeof(color_format_table)/sizeof(V4L2_FORMAT_TABLE);i++){
        if(color_format == color_format_table[i].omx_format && (flag & color_format_table[i].flag)){
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
OMX_U32 V4l2Filter::ConvertOmxCodecFormatToV4l2Format(OMX_VIDEO_CODINGTYPE codec_format)
{
    OMX_U32 i=0;
    OMX_BOOL bGot=OMX_FALSE;
    OMX_U32 out = 0;
    for(i = 0; i < sizeof(codec_format_table)/sizeof(V4L2_FORMAT_TABLE);i++){
        if(codec_format == codec_format_table[i].omx_format){
            bGot = OMX_TRUE;
            out = codec_format_table[i].v4l2_format;
            break;
        }
    }
    if(bGot && eWmvFormat != 0){
        if(eWmvFormat == OMX_VIDEO_WMVFormatWVC1)
            out = V4L2_PIX_FMT_VC1_ANNEX_L;
    }

    if(bGot)
        return out;
    else
        return 0;
}
OMX_BOOL V4l2Filter::ConvertV4l2FormatToOmxColorFormat(OMX_U32 v4l2_format,OMX_COLOR_FORMATTYPE *color_format)
{
    OMX_U32 i=0;
    OMX_BOOL bGot=OMX_FALSE;
    OMX_U32 out = 0;
    for(i = 0; i < sizeof(color_format_table)/sizeof(V4L2_FORMAT_TABLE);i++){
        if(v4l2_format == color_format_table[i].v4l2_format){
            bGot = OMX_TRUE;
            *color_format = (OMX_COLOR_FORMATTYPE)color_format_table[i].omx_format;
            break;
        }
    }

    return bGot;
}
OMX_BOOL V4l2Filter::ConvertV4l2FormatToOmxCodecFormat(OMX_U32 v4l2_format,OMX_VIDEO_CODINGTYPE *codec_format)
{
    OMX_U32 i=0;
    OMX_BOOL bGot=OMX_FALSE;
    OMX_U32 out = 0;
    for(i = 0; i < sizeof(codec_format_table)/sizeof(V4L2_FORMAT_TABLE);i++){
        if(v4l2_format == codec_format_table[i].v4l2_format){
            bGot = OMX_TRUE;
            *codec_format = (OMX_VIDEO_CODINGTYPE)codec_format_table[i].omx_format;
            break;
        }
    }

    return bGot;
}

OMX_ERRORTYPE V4l2Filter::ComponentReturnBuffer(OMX_U32 nPortIndex)
{
    LOG_LOG("ComponentReturnBuffer port=%d\n",nPortIndex);

    if(nPortIndex == IN_PORT) {
        FlushComponent(IN_PORT);
    }else if(nPortIndex == OUT_PORT) {
        FlushComponent(OUT_PORT);
    }
    
    return OMX_ErrorNone;
}

OMX_ERRORTYPE V4l2Filter::InitComponent()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    pV4l2Dev = FSL_NEW(V4l2Dev,());
    if(pV4l2Dev == NULL)
        return OMX_ErrorInsufficientResources;

    ret = pV4l2Dev->LookupNode(eDevType,&devName[0]);
    LOG_LOG("[%p]InitComponent LookupNode ret=%x\n",this,ret);

    if(ret != OMX_ErrorNone)
        return ret;

    nFd = pV4l2Dev->Open(eDevType, &devName[0]);
    LOG_LOG("pV4l2Dev->Open path=%s,fd=%d",devName, nFd);

    if(nFd < 0)
        return OMX_ErrorInsufficientResources;

    ret = CreateObjects();
    if(ret != OMX_ErrorNone)
        return ret;

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&sMutex, fsl_osal_mutex_normal)) {
        ret = OMX_ErrorInsufficientResources;
        return ret;
    }

    ret = SetDefaultSetting();
    if(ret != OMX_ErrorNone)
        return ret;

    LOG_LOG("[%p]InitComponent ret=%d\n",this, ret);
    return ret;
}
OMX_ERRORTYPE V4l2Filter::DeInitComponent()
{

    if(inObj){
        inObj->ReleaseResource();
        FSL_DELETE(inObj);
    }

    if(outObj){
        outObj->ReleaseResource();
        FSL_DELETE(outObj);
    }

    if(pV4l2Dev){
        pV4l2Dev->Close(nFd);
        FSL_DELETE(pV4l2Dev);
    }

    if(sMutex != NULL)
        fsl_osal_mutex_destroy(sMutex);

    LOG_LOG("[%p]DeInitComponent success\n",this);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE V4l2Filter::SetDefaultSetting()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    
    OMX_U32 bufCnt = 0;
    V4l2ObjectFormat sInFormat;
    V4l2ObjectFormat sOutFormat; 
    ret = SetDefaultPortSetting();

    if(ret != OMX_ErrorNone)
        return ret;

    //input port
    if(OMX_ErrorNone == inObj->GetMinimumBufferCount(&bufCnt)){
        if(bufCnt == 0)
            bufCnt = 1;
        if(nInBufferCnt < bufCnt)
            nInBufferCnt = bufCnt;
    }

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = IN_PORT;
    sPortDef.eDir = OMX_DirInput;
    sPortDef.eDomain = OMX_PortDomainVideo;

    sInFormat.width = sInFmt.nFrameWidth;
    sInFormat.height = sInFmt.nFrameHeight;
    sInFormat.stride = sInFmt.nStride;
    sInFormat.plane_num = 1;
    sInFormat.bufferSize[0] = nInBufferSize;

    if(sInFmt.eColorFormat != OMX_COLOR_FormatUnused)
        sInFormat.format = ConvertOmxColorFormatToV4l2Format(sInFmt.eColorFormat,COLOR_FORMAT_FLAG_2_PLANE);
    else if(sInFmt.eCompressionFormat != OMX_VIDEO_CodingUnused)
        sInFormat.format = ConvertOmxCodecFormatToV4l2Format(sInFmt.eCompressionFormat);

    ret = inObj->SetFormat(&sInFormat);
    if(ret != OMX_ErrorNone)
        ret = OMX_ErrorNone;

    ret = inObj->GetFormat(&sInFormat);

    if(ret == OMX_ErrorNone){
        if(sInFormat.width > 0)
            sInFmt.nFrameWidth = sInFormat.width;
        if(sInFormat.height > 0)
            sInFmt.nFrameHeight = sInFormat.height;
        if(sInFormat.stride > 0)
            sInFmt.nStride = sInFormat.stride;
        if(sInFormat.bufferSize[0] > 0)
            nInBufferSize = sInFormat.bufferSize[0];
    }


    fsl_osal_memcpy(&sPortDef.format.video, &sInFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
    sPortDef.format.video.nStride = sPortDef.format.video.nFrameWidth;
    sPortDef.format.video.nSliceHeight= sPortDef.format.video.nFrameHeight;

    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_TRUE;
    sPortDef.nBufferCountMin = nInBufferCnt;
    sPortDef.nBufferCountActual = nInBufferCnt;
    sPortDef.nBufferSize = nInBufferSize;

    ret = ports[IN_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for in port failed.\n");
        return ret;
    }
    LOG_DEBUG("default in format w=%d,h=%d, bufferSize=%d format=%d\n",sInFmt.nFrameWidth,sInFmt.nFrameHeight,nInBufferSize,sPortDef.format.video.eCompressionFormat);
    for (OMX_U32 i=0; i<nInPortFormatCnt; i++) {
        OMX_VIDEO_PARAM_PORTFORMATTYPE sPortFormat;
        OMX_INIT_STRUCT(&sPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
        sPortFormat.nPortIndex = IN_PORT;
        sPortFormat.nIndex = i;
        sPortFormat.eCompressionFormat = sPortDef.format.video.eCompressionFormat;
        sPortFormat.eColorFormat = eInPortPormat[i];
        sPortFormat.xFramerate = sPortDef.format.video.xFramerate;
        LOG_DEBUG("Set support color format: %d\n", eInPortPormat[i]);
        ret = ports[IN_PORT]->SetPortFormat(&sPortFormat);
        if(ret != OMX_ErrorNone) {
            LOG_ERROR("Set port format for in port failed.\n");
            return ret;
        }
    }

    //output port
    if(OMX_ErrorNone == outObj->GetMinimumBufferCount(&bufCnt)){
        if(bufCnt == 0)
            bufCnt = 3;
        if(nOutBufferCnt < bufCnt)
            nOutBufferCnt = bufCnt;
    }

    sOutFormat.width = sOutFmt.nFrameWidth;
    sOutFormat.height = sOutFmt.nFrameHeight;
    sOutFormat.stride= sOutFmt.nStride;
    sOutFormat.plane_num = 1;
    sOutFormat.bufferSize[0] = nOutBufferSize;
    if(sOutFmt.eColorFormat != OMX_COLOR_FormatUnused)
        sOutFormat.format = ConvertOmxColorFormatToV4l2Format(sOutFmt.eColorFormat,COLOR_FORMAT_FLAG_2_PLANE);
    else if(sOutFmt.eCompressionFormat != OMX_VIDEO_CodingUnused)
        sOutFormat.format = ConvertOmxCodecFormatToV4l2Format(sOutFmt.eCompressionFormat);

    ret = outObj->SetFormat(&sOutFormat);
    if(ret != OMX_ErrorNone)
        ret = OMX_ErrorNone;

    sPortDef.nPortIndex = OUT_PORT;
    sPortDef.eDir = OMX_DirOutput;
    sPortDef.nBufferCountMin = nOutBufferCnt;
    sPortDef.nBufferCountActual = nOutBufferCnt;

    fsl_osal_memcpy(&sPortDef.format.video, &sOutFmt, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));

    sPortDef.format.video.nStride = sPortDef.format.video.nFrameWidth;
    sPortDef.format.video.nSliceHeight= sPortDef.format.video.nFrameHeight;
    sPortDef.nBufferSize=nOutBufferSize;

    ret = ports[OUT_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for out port failed.\n");
        return ret;
    }
    LOG_DEBUG("default out format w=%d,h=%d,format=%d\n",sOutFmt.nFrameWidth,sOutFmt.nFrameHeight,sPortDef.format.video.eColorFormat);

    for (OMX_U32 i=0; i<nOutPortFormatCnt; i++) {
        OMX_VIDEO_PARAM_PORTFORMATTYPE sPortFormat;
        OMX_INIT_STRUCT(&sPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
        sPortFormat.nPortIndex = OUT_PORT;
        sPortFormat.nIndex = i;
        sPortFormat.eCompressionFormat = sPortDef.format.video.eCompressionFormat;
        sPortFormat.eColorFormat = eOutPortPormat[i];
        sPortFormat.xFramerate = sPortDef.format.video.xFramerate;
        LOG_DEBUG("Set support color format: %d\n", eOutPortPormat[i]);
        ret = ports[OUT_PORT]->SetPortFormat(&sPortFormat);
        if(ret != OMX_ErrorNone) {
            LOG_ERROR("Set port format for in port failed.\n");
            return ret;
        }
    }

    nInBufferNum = 0;
    nOutBufferNum = 0;

    bSetInputBufferCount = OMX_FALSE;
    bSetOutputBufferCount = OMX_FALSE;

    nOutputCnt = 0;
    nInputCnt = 0;

    eWmvFormat = (enum OMX_VIDEO_WMVFORMATTYPE)0;

    LOG_LOG("SetDefaultSetting SUCCESS\n");
    return ret;
}


OMX_ERRORTYPE V4l2Filter::updateCropInfo(OMX_U32 nPortIndex){
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_VIDEO_PORTDEFINITIONTYPE *pFmt = NULL;

    if(nPortIndex == OUT_PORT){
        pFmt = &sOutFmt;
    }else if(nPortIndex == IN_PORT){
        pFmt = &sInFmt;
    }else{
        ret = OMX_ErrorBadParameter;
        return ret;
    }

    if(sOutputCrop.nWidth != pFmt->nFrameWidth ||
        sOutputCrop.nHeight != pFmt->nFrameHeight){

        sOutputCrop.nWidth = pFmt->nFrameWidth;
        sOutputCrop.nHeight = pFmt->nFrameHeight;
        sOutputCrop.nLeft = 0;
        sOutputCrop.nTop = 0;
        LOG_LOG("updateCropInfo w=%d,h=%d",sOutputCrop.nWidth,sOutputCrop.nHeight);

        ret = outObj->SetCrop(&sOutputCrop);
        if(ret != OMX_ErrorNone)
           return ret;

        ret = outObj->GetCrop(&sOutputCrop);
    }

    return ret;
}
void V4l2Filter::ParseVpuLogLevel(const char* name)
{
    int level=0;
    FILE* fpVpuLog;

    if(name == NULL)
        return;
    
    fpVpuLog=fopen(name, "r");
    if (NULL==fpVpuLog){
        return;
    }

    char symbol;
    int readLen = 0;

    readLen = fread(&symbol,1,1,fpVpuLog);
    if(feof(fpVpuLog) != 0){
        ;
    }
    else{
        level=atoi(&symbol);
        if((level<0) || (level>255)){
            level=0;
        }
    }
    fclose(fpVpuLog);

    nDebugFlag=level;
    LOG_LOG("ParseVpuLogLevel name=%s, flag=%x",name, nDebugFlag);
    return;
}

