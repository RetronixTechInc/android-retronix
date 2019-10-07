/**
 *  Copyright (c) 2014-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file LibavVideoDec.h
 *  @brief Class definition of LibavVideoDec Component
 *  @ingroup LibavVideoDec
 */

#ifndef LibavVideoDec_h
#define LibavVideoDec_h

#include "VideoFilter.h"
#include "ShareLibarayMgr.h"
#include "LibavDecInterfaceMgr.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/log.h"
}

typedef enum {
    LIBAV_DEC_INIT,
    LIBAV_DEC_RUN,
    LIBAV_DEC_STOP
} LIBAV_DEC_STATE;

class LibavVideoDec : public VideoFilter {
    public:
        LibavVideoDec();

    private:
        OMX_PTR pInBuffer;
        OMX_U32 nInputSize;
        OMX_U32 nInputOffset;
        OMX_BOOL bInEos;
        OMX_PTR pOutBuffer;
        OMX_S32 nPadWidth;
        OMX_S32 nPadHeight;
        OMX_S32 nFrameSize;
        OMX_PTR pFrameBuffer;
        OMX_CONFIG_RECTTYPE sOutCrop;
        OMX_BOOL bNeedInput;
        LibavDecInterface *ILibavDec;
        LibavDecInterfaceMgr * libavDecMgr;

        LIBAV_DEC_STATE eDecState;
        OMX_ERRORTYPE SetDefaultSetting();

        OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);

        OMX_ERRORTYPE SetInputBuffer(OMX_PTR pBuffer, OMX_S32 nSize, OMX_BOOL bLast);
        OMX_ERRORTYPE SetOutputBuffer(OMX_PTR pBuffer);
        OMX_ERRORTYPE InitFilterComponent();
        OMX_ERRORTYPE DeInitFilterComponent();
        OMX_ERRORTYPE SetRoleFormat(OMX_STRING role);
        OMX_ERRORTYPE InitFilter();
        OMX_ERRORTYPE DeInitFilter();
        FilterBufRetCode FilterOneBuffer();
        OMX_ERRORTYPE GetOutputBuffer(OMX_PTR *ppBuffer,OMX_S32* pOutSize);
        OMX_ERRORTYPE DetectOutputFmt();
        OMX_ERRORTYPE ProcessQOS();
        OMX_ERRORTYPE FlushInputBuffer();
        OMX_ERRORTYPE FlushOutputBuffer();

        FilterBufRetCode DecodeOneFrame();
        OMX_ERRORTYPE SetCropInfo(OMX_CONFIG_RECTTYPE *sCrop);
        OMX_ERRORTYPE GetCropInfo(OMX_CONFIG_RECTTYPE *sCrop);

		OMX_U32 nChromaAddrAlign;	//the address of Y,Cb,Cr may need to alignment.
        OMX_U8 cRole[OMX_MAX_STRINGNAME_SIZE];
        OMX_PTR pClock;
        OMX_VIDEO_CODINGTYPE CodingType;
        enum AVCodecID codecID;
        AVCodecContext * codecContext;
        AVFrame* picture;
        OMX_BOOL bNeedReportOutputFormat;
        OMX_BOOL bLibavHoldOutputBuffer;
        OMX_BOOL bOutEos;
        OMX_U32 nInvisibleFrame;
        OMX_U8 nFramesToCheckDiscarding;
        OMX_S32 nFrameWidthStride;//user may register frames with specified width stride
        OMX_S32 nFrameHeightStride;//user may register frames with specified height stride
        OMX_S32 nFrameMaxCnt;//user may register frames with specified count

};

#endif
/* File EOF */
