/**
 *  Copyright (c) 2014-2016, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef SOFTHEVCDEC_H
#define SOFTHEVCDEC_H

#include "VideoFilter.h"

extern "C" {
#include "ihevc_typedefs.h"
#include "iv.h"
#include "ivd.h"
#include "ithread.h"
#include "ihevcd_cxa.h"
}

typedef enum {
    HEVC_DEC_IDLE,
    HEVC_DEC_INIT,
    HEVC_DEC_PREPARE_FOR_FRAME,
    HEVC_DEC_RUN,
    HEVC_DEC_FLUSHING,
} HEVC_DEC_STATE;

class SoftHevcDec : public VideoFilter {
    public:
        SoftHevcDec();

    private:
        OMX_PTR pInBuffer;
        OMX_U32 nInputSize;
        OMX_U32 nInputOffset;

        OMX_PTR pOutBuffer;
        OMX_PTR pInternalBuffer;

        OMX_U32 nWidth;
        OMX_U32 nHeight;

        OMX_U32 nNumCores; //Number of cores to be uesd by the codec
        OMX_U32 nNalLen; //nal len size for hevc format

        OMX_CONFIG_RECTTYPE sOutCrop;
        HEVC_DEC_STATE eDecState;

        OMX_BOOL bInEos;// EOS is receieved on input port
        OMX_BOOL bOutEos;

        OMX_BOOL bNeedCopyCodecData;//if true, copy codec data for the first input buffer
        OMX_BOOL bCodecDataParsed;// parse codec data from hevc to bytestream format for only once

        OMX_BOOL bInFlush;        // codec is flush mode

        // The input stream has changed to a different resolution, which is still supported by the
        // codec. So the codec is switching to decode the new resolution.
        OMX_BOOL bChangingResolution;
        OMX_S32 nFrameWidthStride;//user may register frames with specified width stride
        OMX_S32 nFrameHeightStride;//user may register frames with specified height stride
        OMX_S32 nFrameMaxCnt;//user may register frames with specified count

        OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE SetInputBuffer(OMX_PTR pBuffer, OMX_S32 nSize, OMX_BOOL bLast);
        OMX_ERRORTYPE SetOutputBuffer(OMX_PTR pBuffer);

        OMX_ERRORTYPE InitFilterComponent();
        OMX_ERRORTYPE DeInitFilterComponent();
        OMX_ERRORTYPE InitFilter();
        OMX_ERRORTYPE DeInitFilter();
        OMX_ERRORTYPE ProcessQOS();

        FilterBufRetCode FilterOneBuffer();
        OMX_ERRORTYPE GetOutputBuffer(OMX_PTR *ppBuffer,OMX_S32* pOutSize);

        OMX_ERRORTYPE FlushInputBuffer();
        OMX_ERRORTYPE FlushOutputBuffer();

        FilterBufRetCode PrepareForFrame();
        FilterBufRetCode DecodeOneFrame();
        FilterBufRetCode DecodeFlushFrame();

        OMX_ERRORTYPE DetectOutputFmt(OMX_U32 width, OMX_U32 height);
        OMX_ERRORTYPE SetCropInfo(OMX_CONFIG_RECTTYPE *sCrop);
        OMX_ERRORTYPE GetCropInfo(OMX_CONFIG_RECTTYPE *sCrop);
        OMX_ERRORTYPE SetDefaultSetting();
        OMX_U8 cRole[OMX_MAX_STRINGNAME_SIZE];
        OMX_PTR pClock;
        OMX_VIDEO_CODINGTYPE CodingType;

        iv_obj_t *mCodecCtx;         // Codec context
        IVD_FRAME_SKIP_MODE_T skipMode; //skip mode for qos

        OMX_ERRORTYPE initDecoder();
        OMX_ERRORTYPE deInitDecoder();

        void logVersion();
        OMX_ERRORTYPE setNumCores();
        OMX_ERRORTYPE setDisplayStride(OMX_U32 stride);
        OMX_ERRORTYPE resetDecoder();
        void setDecodeArgs(ivd_video_decode_ip_t *ps_dec_ip,
            ivd_video_decode_op_t *ps_dec_op,OMX_PTR inBuffer,OMX_U32 inSize,OMX_PTR outBuffer);
        OMX_ERRORTYPE setFlushMode();
        OMX_ERRORTYPE ParseCodecInfo();
        OMX_U32 parseNALSize(OMX_U8 *data);

        OMX_ERRORTYPE AllocateFlushBuffer();

};

#endif
/* File EOF */
