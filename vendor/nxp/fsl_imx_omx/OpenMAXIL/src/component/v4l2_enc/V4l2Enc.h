/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
#ifndef V4L2ENC_H
#define V4L2ENC_H
#include "V4l2Filter.h"
#include "FrameConverter.h"
#include "IsiColorConvert.h"
#include "DmaBuffer.h"


typedef enum
{
	V4L2_ENC_STATE_IDLE=0,
	V4L2_ENC_STATE_INIT,
	V4L2_ENC_STATE_QUEUE_INPUT,
    V4L2_ENC_STATE_START_INPUT,
    V4L2_ENC_STATE_WAIT_RES,
    V4L2_ENC_STATE_RES_CHANGED,
    V4L2_ENC_STATE_FLUSHED,
    V4L2_ENC_STATE_RUN,
    V4L2_ENC_STATE_END,
    V4L2_ENC_STATE_START_ENCODE,
    V4L2_ENC_STATE_STOP_ENCODE,
}V4l2EncState;

class V4l2Enc : public V4l2Filter {
public:
    explicit V4l2Enc();
    OMX_ERRORTYPE DoAllocateBuffer(OMX_PTR *buffer, OMX_U32 nSize,OMX_U32 nPortIndex) override;
    OMX_ERRORTYPE DoFreeBuffer(OMX_PTR buffer,OMX_U32 nPortIndex) override;
    OMX_ERRORTYPE DoUseBuffer(OMX_PTR buffer,OMX_U32 nSize,OMX_U32 nPortIndex) override;
    OMX_ERRORTYPE FlushComponent(OMX_U32 nPortIndex) override;

    friend void *filterThreadHandler(void *arg);

    OMX_ERRORTYPE HandleFormatChanged(OMX_U32 nPortIndex);
    OMX_ERRORTYPE HandleErrorEvent();
    OMX_ERRORTYPE HandleEOSEvent(OMX_U32 nPortIndex);
    OMX_ERRORTYPE HandleInObjBuffer();
    OMX_ERRORTYPE HandleOutObjBuffer();

    OMX_ERRORTYPE ReturnBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr,OMX_U32 nPortIndex);
    OMX_ERRORTYPE HandlePreProcessBuffer(DmaBufferHdr *buf);
private:
    OMX_ERRORTYPE CreateObjects() override;
    OMX_ERRORTYPE SetRoleFormat(OMX_STRING role);
    OMX_ERRORTYPE QueryVideoProfile(OMX_PTR pComponentParameterStructure);
    OMX_ERRORTYPE SetDefaultPortSetting() override;
    OMX_ERRORTYPE ProcessInit();
    OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure) override;
    OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure) override;
    OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure) override;
    OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure) override;
    OMX_ERRORTYPE CheckIfNeedPreProcess();
    V4l2EncInputParam sInputParam;
    OMX_U32 nProfile;
    OMX_U32 nLevel;

    OMX_S32 nMaxDurationMsThr;  // control the speed of data consumed by decoder: -1 -> no threshold
    OMX_S32 nMaxBufCntThr;      // control the speed of data consumed by decoder: -1 -> no threshold
    OMX_BOOL bSendPortSettingChanged;

    IsiColorConvert *pPreProcess;//pre process thread
    OMX_BOOL bEnabledPreProcess;
    OMX_BOOL bCheckPreProcess;

    FrameConverter *pCodecDataConverter;
    OMX_BOOL bEnabledAVCCConverter;

    OMX_BOOL bRefreshIntra;
    OMX_BOOL bStoreMetaData;
    OMX_BOOL bInsertSpsPps2IDR;

    OMX_PTR pCodecData;
    OMX_U32 nCodecDataLen;
    OMX_BOOL bSendCodecData;
    OMX_BUFFERHEADERTYPE *pCodecDataBufferHdr;

    OMX_BOOL bInputEOS;
    OMX_BOOL bOutputEOS;

    OMX_BOOL bOutputStarted;
    OMX_BOOL bNewSegment;

    DmaBuffer * pDmaInputBuffer;
    OMX_BOOL bUseDmaInputBuffer;

    DmaBuffer * pDmaBuffer;//for color convert
    OMX_BOOL bUseDmaBuffer;//for color convert
    OMX_BOOL bDmaBufferAllocated;//for color convert
    OMX_BOOL bAllocateFailed;//for color convert

    OMX_COLOR_FORMATTYPE eDmaBufferFormat;//for color convert
    OMX_U32 nDmaBufferCnt;//for color convert
    OMX_U32 nDmaBufferSize[3];//for color convert
    OMX_U32 nDmaPlanes;

    V4l2EncState eState;
    OMX_CONFIG_ROTATIONTYPE Rotation;

    OMX_U32 nErrCnt;

    OMX_ERRORTYPE PortFormatChanged(OMX_U32 nPortIndex) override;
    OMX_ERRORTYPE DoIdle2Loaded() override;
    OMX_ERRORTYPE DoLoaded2Idle() override;
    OMX_ERRORTYPE ProcessDataBuffer() override;
    OMX_ERRORTYPE ProcessInputBuffer();
    OMX_ERRORTYPE ProcessInBufferFlags(OMX_BUFFERHEADERTYPE *pInBufferHdr);
    OMX_ERRORTYPE ProcessOutputBuffer();

    void dumpInputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr);
    void dumpOutputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr);
    void dumpPreProcessBuffer(DmaBufferHdr *pBufferHdr);
    OMX_ERRORTYPE PrepareForPreprocess();
    OMX_ERRORTYPE ProcessPreBuffer();
};
#endif
