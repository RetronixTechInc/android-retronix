/**
 *  Copyright 2018-2019 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef V4L2DEC_H
#define V4L2DEC_H
#include "V4l2Filter.h"
#include "FrameParser.h"
#include "Process3.h"
#include "DmaBuffer.h"

typedef enum
{
	V4L2_DEC_STATE_IDLE=0,
	V4L2_DEC_STATE_INIT,
	V4L2_DEC_STATE_QUEUE_INPUT,
    V4L2_DEC_STATE_START_INPUT,
    V4L2_DEC_STATE_WAIT_RES,
    V4L2_DEC_STATE_RES_CHANGED,
    V4L2_DEC_STATE_FLUSHED,
    V4L2_DEC_STATE_RUN,
    V4L2_DEC_STATE_END,
}V4l2DecState;

class V4l2Dec : public V4l2Filter {
public:
    explicit V4l2Dec();
    OMX_ERRORTYPE DoAllocateBuffer(OMX_PTR *buffer, OMX_U32 nSize,OMX_U32 nPortIndex) override; 
    OMX_ERRORTYPE DoFreeBuffer(OMX_PTR buffer,OMX_U32 nPortIndex) override;
    OMX_ERRORTYPE DoUseBuffer(OMX_PTR buffer,OMX_U32 nSize,OMX_U32 nPortIndex) override;
    OMX_ERRORTYPE FlushComponent(OMX_U32 nPortIndex) override;

    friend void *filterThreadHandler(void *arg);

    OMX_ERRORTYPE HandleFormatChanged(OMX_U32 nPortIndex);
    OMX_ERRORTYPE HandleFormatChangedForIon(OMX_U32 nPortIndex);
    OMX_ERRORTYPE HandleErrorEvent();
    OMX_ERRORTYPE HandleEOSEvent();
    OMX_ERRORTYPE HandleInObjBuffer();
    OMX_ERRORTYPE HandleOutObjBuffer();
    OMX_ERRORTYPE HandleSkipEvent();

    OMX_ERRORTYPE ReturnBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr,OMX_U32 nPortIndex);
private:
    OMX_S32 nMaxDurationMsThr;  // control the speed of data consumed by decoder: -1 -> no threshold
    OMX_S32 nMaxBufCntThr;      // control the speed of data consumed by decoder: -1 -> no threshold
    OMX_BOOL bSendPortSettingChanged;

    FrameParser *pParser;
    OMX_BOOL bEnabledFrameParser;

    Process3 *pPostProcess;
    OMX_BOOL bEnabledPostProcess;//only used in decoder

    DmaBuffer * pDmaBuffer;
    OMX_BOOL bUseDmaBuffer;

    OMX_COLOR_FORMATTYPE eDmaBufferFormat;
    OMX_U32 nDmaBufferCnt;
    OMX_U32 nDmaBufferSize[3];

    DmaBuffer * pDmaOutputBuffer;
    OMX_BOOL bUseDmaOutputBuffer;

    OMX_PTR pCodecData;
    OMX_U32 nCodecDataLen;
    OMX_BOOL bReceiveFrame;
    OMX_BOOL bNeedCodecData;

    V4l2DecState eState;

    OMX_BOOL bAndroidNativeBuffer;
    OMX_BOOL bAdaptivePlayback;

    OMX_BOOL bInputEOS;
    OMX_BOOL bOutputEOS;

    OMX_BOOL bOutputStarted;
    OMX_BOOL bNewSegment;

    OMX_BOOL bDmaBufferAllocated;
    OMX_BOOL bAllocateFailed;

    OMX_BOOL bRetryHandleEOS;
    OMX_BOOL bReceiveOutputEOS;
    OMX_U32 nErrCnt;
    OMX_U32 nOutObjBufferCnt;

    OMX_ERRORTYPE CreateObjects() override;
    OMX_ERRORTYPE SetRoleFormat(OMX_STRING role);
    OMX_ERRORTYPE QueryVideoProfile(OMX_PTR pComponentParameterStructure);
    OMX_ERRORTYPE SetDefaultPortSetting() override;
    OMX_ERRORTYPE ProcessInit();
    OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure) override;
    OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure) override;
    OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure) override;
    OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex,OMX_PTR pComponentParameterStructure) override;
    OMX_ERRORTYPE CheckIfNeedFrameParser();
    OMX_BOOL CheckIfNeedPostProcess();
    OMX_ERRORTYPE AllocateDmaBuffer();

    OMX_ERRORTYPE PortFormatChanged(OMX_U32 nPortIndex) override;
    OMX_ERRORTYPE DoIdle2Loaded() override;
    OMX_ERRORTYPE DoLoaded2Idle() override;
    OMX_ERRORTYPE ProcessDataBuffer() override;
    OMX_ERRORTYPE ProcessInputBuffer();
    OMX_ERRORTYPE ProcessInBufferFlags(OMX_BUFFERHEADERTYPE *pInBufferHdr);
    OMX_ERRORTYPE ProcessOutputBuffer();
    OMX_ERRORTYPE ProcessPostBuffer();

    void dumpInputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr);
    void dumpOutputBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr);
    void dumpPostProcessBuffer(DmaBufferHdr *pBufferHdr);
};
#endif
