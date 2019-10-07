/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef FRAMECONVERTER_H
#define FRAMECONVERTER_H
#include "ComponentBase.h"
#include "Mem.h"

class FrameConverter{
public:
    OMX_ERRORTYPE Create(OMX_VIDEO_CODINGTYPE format);
    OMX_ERRORTYPE ConvertToCodecData(OMX_U8* pInData, OMX_U32 nSize,
        OMX_U8* pOutData,OMX_U32*nOutSize,OMX_U32 * nConsumeLen);
    OMX_ERRORTYPE ConvertToData(OMX_U8* pData, OMX_U32 nSize);
    OMX_ERRORTYPE GetSpsPpsPtr(OMX_U8** pData,OMX_U32 *outLen);
    OMX_ERRORTYPE CheckSpsPps(OMX_U8* pInData, OMX_U32 nSize, OMX_U32* nConsumeLen);
    OMX_ERRORTYPE Destroy();
private:
    OMX_VIDEO_CODINGTYPE nFormat;
    OMX_BOOL FindStartCode(OMX_U8* pData, OMX_U32 nSize,OMX_U8** ppStart);
    
    OMX_U8* pSpsPps;
    OMX_U32 nLen;
    
};
#endif

