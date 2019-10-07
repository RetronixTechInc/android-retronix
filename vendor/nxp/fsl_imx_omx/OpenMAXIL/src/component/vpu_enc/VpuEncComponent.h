/**
 *  Copyright (c) 2010-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 *  Copyright 2018 NXP
 */

/**
 *  @file VpuEncComponent.h
 *  @brief Class definition of VpuEncoder Component
 *  @ingroup VpuEncoder
 */

#ifndef VpuEncoder_h
#define VpuEncoder_h

#include "vpu_wrapper.h"
#include "VideoFilter.h"

#ifdef MX6X
#include "linux/ipu.h"
typedef struct ipu_task IpuTask;
typedef OMX_S32 G2DSurface;
#else
#include "g2d.h"
typedef OMX_S32 IpuTask;
typedef struct g2d_surface G2DSurface;
#endif  //ifdef MX6X

typedef void * VpuEncHandle;
typedef void * VpuEncPPHandle;  // preprocess handle

typedef struct {
	VpuCodStd eFormat;
    OMX_COLOR_FORMATTYPE eColorFormat;
	int nPicWidth;
	int nPicHeight;
	int nWidthStride;
	int nHeightStride;
	int nRotAngle;
	int nFrameRate;
	int nBitRate;			/*unit: bps*/
	int nGOPSize;
	int nChromaInterleave;
	VpuEncMirrorDirection sMirror;
	int nQuantParam;

	int nEnableAutoSkip;
	int nIDRPeriod;		//for H.264
	int nRefreshIntra;	//IDR for H.264
	int nIntraFreshNum;
	OMX_BOOL bEnabledSPSIDR; //SPS/PPS is added for every IDR frame
	int nRcIntraQP;		//0: auto; >0: qp value
} VpuEncInputParam;

typedef struct
{
	OMX_S32 nIpuFd;
	IpuTask sIpuTask;
}VpuEncoderIpuHandle;

typedef struct
{
    void * g2dHandle;
    G2DSurface sInput;
    G2DSurface sOutput;
}VpuEncoderGpuHandle;

typedef enum
{
	VPU_ENC_COM_STATE_NONE=0,
	VPU_ENC_COM_STATE_LOADED,
	VPU_ENC_COM_STATE_OPENED,
	//VPU_ENC_COM_STATE_WAIT_FRM,
	VPU_ENC_COM_STATE_DO_INIT,
	VPU_ENC_COM_STATE_DO_DEC,
	VPU_ENC_COM_STATE_DO_OUT,
	//VPU_ENC_COM_STATE_EOS,
	//VPU_ENC_COM_STATE_RE_WAIT_FRM,
}VpuEncoderState;

typedef enum
{
	PRE_PROCESS_STATE_NONE=0,
	PRE_PROCESS_STATE_IDLE,
	PRE_PROCESS_STATE_RUN,
}PreProcessState;

typedef enum
{
	PRE_PROCESS_CMD_NONE=0,
	PRE_PROCESS_CMD_RUN,
	PRE_PROCESS_CMD_FLUSH_IN,
	PRE_PROCESS_CMD_FLUSH_OUT,
	PRE_PROCESS_CMD_STOP,
}PreProcessCmd;

#define VPU_ENC_MAX_NUM_MEM	(36)
typedef struct
{
	//virtual mem info
	OMX_S32 nVirtNum;
	OMX_U32 virtMem[VPU_ENC_MAX_NUM_MEM];

	//phy mem info
	OMX_S32 nPhyNum;
	OMX_U32 phyMem_virtAddr[VPU_ENC_MAX_NUM_MEM];
	OMX_U32 phyMem_phyAddr[VPU_ENC_MAX_NUM_MEM];
	OMX_U32 phyMem_cpuAddr[VPU_ENC_MAX_NUM_MEM];
	OMX_U32 phyMem_size[VPU_ENC_MAX_NUM_MEM];
}VpuEncoderMemInfo;

typedef struct
{
	OMX_S32 nNum;
	OMX_U32 virtAddr[VPU_ENC_MAX_NUM_MEM];
	OMX_U32 phyAddr[VPU_ENC_MAX_NUM_MEM];
	OMX_U32 size[VPU_ENC_MAX_NUM_MEM];
	OMX_U32 flag[VPU_ENC_MAX_NUM_MEM];
}VpuEncoderPreProcessMemInfo;

class VpuEncoder : public VideoFilter {
	public:
		friend void *PreThread(void *arg);
		VpuEncoder();
		//OMX_S32 DoGetBitstream(OMX_U32 nLen, OMX_U8 *pBuffer, OMX_S32 *pEndOfFrame);
	private:
		VpuEncoderMemInfo sVpuEncMemInfo;		// memory info for vpu : component internal structure/vpu_EncRegisterFrameBuffer()
		VpuEncoderMemInfo sEncAllocInMemInfo;	// memory info for AllocateInputBuffer()
		VpuEncoderMemInfo sEncAllocOutMemInfo;	// memory info for AllocateOutputBuffer() and output of pre-process
		VpuEncoderMemInfo sEncOutFrameInfo;		// memory info for output frames: it may be overlapped with sEncAllocOutMemInfo
		VpuEncoderPreProcessMemInfo sEncPreProcessInFrameInfo;	// memory info for pre-process input
		VpuEncoderPreProcessMemInfo sEncPreProcessOutFrameInfo;	// memory info for pre-process output, it is released through sEncAllocOutMemInfo
		OMX_PARAM_MEM_OPERATOR sMemOperator;

		VpuEncInitInfo sEncInitInfo;	// seqinit info
		VpuEncHandle nHandle;		// pointer to vpu object

		OMX_U8 cRole[OMX_MAX_STRINGNAME_SIZE];

		//VpuCodStd eFormat;
		//OMX_S32 nPadWidth;
		//OMX_S32 nPadHeight;

		OMX_PTR pInBufferPhy;
		OMX_PTR pInBufferVirt;
		OMX_S32 nInSize;
		OMX_BOOL bInEos;
		OMX_PTR pOutBufferPhy;
		OMX_S32 nOutSize;
		OMX_S32 nOutGOPFrameCnt;	// used for H.264: [1,2,...,GOPSize]
        OMX_BOOL bStoreMetaData;    // indicate store meta data in buffer.

        VpuColourDesc sColorDesc;

		VpuEncoderState eVpuEncoderState;

		VpuEncInputParam sVpuEncInputPara;

		OMX_BOOL bEnabledPreProcess;	//pre-process is enalbed or not: mainly for RGBA - YUV
        VpuEncPPHandle preProcessHandle;
		fsl_osal_ptr pPreThreadId;
		PreProcessState ePreState;	//pre thread write, encode thread read
		PreProcessCmd ePreCmd;	//pre thread read/clear, encode thread write
		Queue* pPreInQueue;		//rgb frame index: encode thread add, pre thread get
		Queue* pPreOutQueue;		//yuv frame index: encode thread get, pre thread add
		Queue* pPreInReturnQueue;  //rgb frame index: encode thread get, pre thread add
		Queue* pPreOutReturnQueue;//yuv frame index: encode thread add, pre thread get
		fsl_osal_sem pPreCmdSem;	//encode thread wait, pre thread pre
		fsl_osal_mutex pPreMutex;       //sync mutex between pre and encode thread
		pthread_cond_t sPreCond;        //sync condition between pre and encode thread
		OMX_BOOL bPreWaitingTasks;//pre thread are waiting tasks from encode thread
		OMX_PTR pPreReturnedInput; //record the last buffer addr (in pPreInReturnQueue)
		OMX_S32 nPreOutIndex;	      //record the last out index (in pPreOutQueue)
		OMX_BOOL bPreEos;			//pre-process has received eos

		OMX_ERRORTYPE SetRoleFormat(OMX_STRING role);
		OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);

		/* virtual function implementation */
		OMX_ERRORTYPE InitFilterComponent();
		OMX_ERRORTYPE DeInitFilterComponent();
		OMX_ERRORTYPE SetInputBuffer(OMX_PTR pBuffer, OMX_S32 nSize, OMX_BOOL bLast);
		OMX_ERRORTYPE SetOutputBuffer(OMX_PTR pBuffer);
		OMX_ERRORTYPE InitFilter();
		OMX_ERRORTYPE DeInitFilter();
		FilterBufRetCode FilterOneBuffer();
		OMX_ERRORTYPE GetReturnedInputDataPtr(OMX_PTR* ppInput);
		OMX_ERRORTYPE GetOutputBuffer(OMX_PTR *ppBuffer);
		OMX_ERRORTYPE GetOutputBuffer(OMX_PTR *ppBuffer,OMX_S32* pOutSize);
		OMX_ERRORTYPE FlushInputBuffer();
		OMX_ERRORTYPE FlushOutputBuffer();

		OMX_PTR AllocateInputBuffer(OMX_U32 nSize);	//implement this api to support direct input
		OMX_ERRORTYPE FreeInputBuffer(OMX_PTR pBuffer);
		OMX_PTR AllocateOutputBuffer(OMX_U32 nSize);
		OMX_ERRORTYPE FreeOutputBuffer(OMX_PTR pBuffer);

		//OMX_ERRORTYPE GetOutputBufferSize(OMX_S32 *pOutSize);

		OMX_ERRORTYPE PreThreadRun();
		OMX_ERRORTYPE PreThreadStop();
		OMX_ERRORTYPE PreThreadFlushInput();
		OMX_ERRORTYPE PreThreadFlushOutput();
};

#endif	// #ifdef VpuEncoder_h
/* File EOF */

