/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef V4L2FILTER_H
#define V4L2FILTER_H

#include "ComponentBase.h"
#include "Mem.h"
#include "OMX_ImageConvert.h"
#include "V4l2Device.h"
#include "V4l2Object.h"
#include "VThread.h" 


#define NUM_PORTS 2
#define IN_PORT   0
#define OUT_PORT  1

#define COLOR_FORMAT_FLAG_SINGLE_PLANE 0x01
#define COLOR_FORMAT_FLAG_2_PLANE       0x02

#define Align(ptr,align)    (((OMX_U32)(ptr)+(align)-1)/(align)*(align))

#define ASSERT(exp) if(!(exp)) {printf("%s: %d : assert condition !!!\r\n",__FUNCTION__,__LINE__);}

class V4l2Filter : public ComponentBase {
public:
    explicit V4l2Filter();

    OMX_U32 ConvertOmxColorFormatToV4l2Format(OMX_COLOR_FORMATTYPE color_format,OMX_U32 flag);
    OMX_U32 ConvertOmxCodecFormatToV4l2Format(OMX_VIDEO_CODINGTYPE codec_format);
    OMX_BOOL ConvertV4l2FormatToOmxColorFormat(OMX_U32 v4l2_format,OMX_COLOR_FORMATTYPE *color_format);
    OMX_BOOL ConvertV4l2FormatToOmxCodecFormat(OMX_U32 v4l2_format,OMX_VIDEO_CODINGTYPE *codec_format);

    OMX_ERRORTYPE ComponentReturnBuffer(OMX_U32 nPortIndex) override;
    OMX_ERRORTYPE SetDefaultSetting();
    OMX_ERRORTYPE updateCropInfo(OMX_U32 nPortIndex);

    void ParseVpuLogLevel(const char* name);
protected:
    OMX_VIDEO_PORTDEFINITIONTYPE sInFmt;
    OMX_VIDEO_PORTDEFINITIONTYPE sOutFmt;
    OMX_U32 nInPortFormatCnt;
    OMX_COLOR_FORMATTYPE eInPortPormat[MAX_PORT_FORMAT_NUM];
    OMX_U32 nOutPortFormatCnt;
    OMX_COLOR_FORMATTYPE eOutPortPormat[MAX_PORT_FORMAT_NUM];
    OMX_U32 nInBufferCnt;
    OMX_U32 nInBufferSize;
    OMX_U32 nOutBufferCnt;
    OMX_U32 nOutBufferSize;
    OMX_S32 nInBufferNum;
    OMX_S32 nOutBufferNum;


    OMX_U8 cRole[OMX_MAX_STRINGNAME_SIZE];
    OMX_VIDEO_CODINGTYPE CodingType;

    V4l2DEV_TYPE eDevType;
    OMX_S32 nFd;

    V4l2Dev* pV4l2Dev;
    
    V4l2Object *inObj;
    V4l2Object *outObj;

    VThread *inThread;
    fsl_osal_mutex sMutex;//mutex for port buffer operation

    OMX_U32 nInputPlane;//set in child class
    OMX_U32 nOutputPlane;//set in child class

    OMX_BOOL bSetInputBufferCount;
    OMX_BOOL bSetOutputBufferCount;
    
    OMX_U32 nInputCnt;//used for debug
    OMX_U32 nOutputCnt;//used for debug
    OMX_U32 nDebugFlag;

    OMX_CONFIG_RECTTYPE sOutputCrop;

    OMX_VIDEO_WMVFORMATTYPE eWmvFormat;
    
    OMX_U32 nWidthAlign;//set by child class
    OMX_U32 nHeightAlign;//set by child class

    OMX_PTR hTsHandle;// ENABLE_TS_MANAGER

private:
    OMX_U8 devName[32];

    OMX_ERRORTYPE InitComponent() override;
    OMX_ERRORTYPE DeInitComponent() override;
    virtual OMX_ERRORTYPE CreateObjects() = 0;

    virtual OMX_ERRORTYPE SetDefaultPortSetting() = 0;
};

#endif//V4L2FILTER_H
/* File EOF */

