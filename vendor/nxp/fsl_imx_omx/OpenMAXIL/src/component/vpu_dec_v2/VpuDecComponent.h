/**
 *  Copyright (c) 2010-2016, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file VpuDecComponent.h
 *  @brief Class definition of VpuDecoder Component
 *  @ingroup VpuDecoder
 */

#ifndef VpuDecoder_h
#define VpuDecoder_h

#include "VideoFilter.h"
#include "vpu_wrapper.h"

#ifdef MX6X
#include "linux/ipu.h"
typedef struct ipu_task IpuTask;
#else
typedef enum {
	MED_MOTION = 0,
	LOW_MOTION = 1,
	HIGH_MOTION = 2,
} ipu_motion_sel;
typedef OMX_S32 IpuTask;
#endif  //ifdef MX6X

#define VPU_DEC_MAX_NUM_MEM	(30)
typedef fsl_osal_sem VPUCompSemaphor;
#define VPU_COMP_SEM_INIT(ppsem)    fsl_osal_sem_init((ppsem), 0/*process shared*/,1/*number*/)
#define VPU_COMP_SEM_INIT_PROCESS(ppsem)    fsl_osal_sem_init_process((ppsem), 1/*process shared*/,1/*number*/)
#define VPU_COMP_SEM_DESTROY(psem)  fsl_osal_sem_destroy((psem))
#define VPU_COMP_SEM_DESTROY_PROCESS(psem)  fsl_osal_sem_destroy_process((psem))
#define VPU_COMP_SEM_LOCK(psem)     fsl_osal_sem_wait((psem))
#define VPU_COMP_SEM_TRYLOCK(psem)     fsl_osal_sem_trywait((psem))
#define VPU_COMP_SEM_UNLOCK(psem)   fsl_osal_sem_post((psem))

typedef struct
{
	//virtual mem info
	OMX_S32 nVirtNum;
	OMX_U32 virtMem[VPU_DEC_MAX_NUM_MEM];

	//phy mem info
	OMX_S32 nPhyNum;
	OMX_U32 phyMem_virtAddr[VPU_DEC_MAX_NUM_MEM];
	OMX_U32 phyMem_phyAddr[VPU_DEC_MAX_NUM_MEM];
	OMX_U32 phyMem_cpuAddr[VPU_DEC_MAX_NUM_MEM];
	OMX_U32 phyMem_size[VPU_DEC_MAX_NUM_MEM];
}VpuDecoderMemInfo;

typedef enum
{
	VPU_COM_FRM_OWNER_NULL=0,
	VPU_COM_FRM_OWNER_DEC,
	VPU_COM_FRM_OWNER_POST,
}VpuDecoderFrmOwner;

typedef enum
{
	VPU_COM_FRM_STATE_FREE=0,
	VPU_COM_FRM_STATE_OUT,
}VpuDecoderFrmState;

typedef struct
{
	OMX_S32 nFrmNum;
	VpuDecoderFrmOwner eFrmOwner[VPU_DEC_MAX_NUM_MEM];
	VpuDecoderFrmState eFrmState[VPU_DEC_MAX_NUM_MEM];
	OMX_U32 nFrm_virtAddr[VPU_DEC_MAX_NUM_MEM];
	OMX_U32 nFrm_phyAddr[VPU_DEC_MAX_NUM_MEM];
	//OMX_U32 nFrm_cpuAddr[VPU_DEC_MAX_NUM_MEM];
	//OMX_U32 nFrm_size[VPU_DEC_MAX_NUM_MEM];
	VpuDecOutFrameInfo outFrameInfo[VPU_DEC_MAX_NUM_MEM];
	OMX_S32 nPostIndxMapping[VPU_DEC_MAX_NUM_MEM];  //mapping between decIndx and PostIndx: to align with memset(0),it store the decIndx+1
}VpuDecoderFrmPoolInfo;

typedef struct
{
	OMX_PTR pVirtAddr[VPU_DEC_MAX_NUM_MEM];
	VpuDecOutFrameInfo outFrameInfo[VPU_DEC_MAX_NUM_MEM];
}VpuDecoderOutMapInfo;

typedef struct
{
	OMX_S32 nIpuFd;
	IpuTask sIpuTask;
}VpuDecoderIpuHandle;

typedef enum
{
	VPU_COM_STATE_NONE=0,
	VPU_COM_STATE_LOADED,
	VPU_COM_STATE_OPENED,
	VPU_COM_STATE_WAIT_FRM,
	VPU_COM_STATE_DO_INIT,
	VPU_COM_STATE_DO_DEC,
	VPU_COM_STATE_DO_OUT,
	VPU_COM_STATE_EOS,
	VPU_COM_STATE_RE_WAIT_FRM,
	VPU_COM_STATE_WAIT_POST_EOS,
}VpuDecoderState;

typedef enum
{
	POST_PROCESS_STATE_NONE=0,
	POST_PROCESS_STATE_IDLE,
	POST_PROCESS_STATE_RUN,
}PostProcessState;

typedef enum
{
	POST_PROCESS_CMD_NONE=0,
	POST_PROCESS_CMD_RUN,
	POST_PROCESS_CMD_FLUSH_IN,
	POST_PROCESS_CMD_FLUSH_OUT,
	POST_PROCESS_CMD_STOP,
}PostProcessCmd;

typedef enum
{
	VPU_COM_CAPABILITY_FILEMODE=0x1,
	VPU_COM_CAPABILITY_TILE=0x2,
	VPU_COM_CAPABILITY_FRMSIZE=0x4,
}VpuDecoderCapability;

class VpuDecoder : public VideoFilter {
	public:
		friend void *PostThread(void *arg);
		VpuDecoder();
		OMX_S32 DoGetBitstream(OMX_U32 nLen, OMX_U8 *pBuffer, OMX_S32 *pEndOfFrame);
	private:
		VPUCompSemaphor psemaphore;		//in fact, it is one ptr
		VpuMemInfo sMemInfo;				// required by vpu wrapper
		VpuDecoderMemInfo sVpuMemInfo;		// memory info for vpu wrapper itself
		VpuDecoderMemInfo sAllocMemInfo;	// memory info for AllocateOutputBuffer()
		VpuDecoderFrmPoolInfo sFramePoolInfo;// frame pool info for all output frames: decode + post-process

		VpuDecInitInfo sInitInfo;		// seqinit info
		VpuDecHandle nHandle;		// pointer to vpu object

		OMX_U8 cRole[OMX_MAX_STRINGNAME_SIZE];

		//sWmv9DecObjectType sDecObj;
		VpuCodStd eFormat;
		OMX_S32 nPadWidth;
		OMX_S32 nPadHeight;
		//OMX_S32 nFrameSize;
		//OMX_PTR pFrameBuffer;
		OMX_CONFIG_RECTTYPE sOutCrop;
		OMX_CONFIG_SCALEFACTORTYPE sDispRatio;

		OMX_PTR pInBuffer;
		OMX_S32 nInSize;
		OMX_BOOL bInEos;

		//OMX_PTR pOutBuffer;
		//OMX_BOOL bOutLast;

		//OMX_BOOL bFrameBufReady;
		VpuDecoderState eVpuDecoderState;

		OMX_PTR pLastOutVirtBuf;		//record the last output frame

		OMX_S32 nFreeOutBufCntDec;		//frame buffer numbers which can be used by vpu
		OMX_S32 nFreeOutBufCntPost;		//frame buffer numbers which can be used by post-process

		OMX_S32 nNeedFlush;		//flush automatically according return value from vpu wrapper
		OMX_S32 nNeedSkip;			//output one skip to get one timestamp

		OMX_PARAM_MEM_OPERATOR sMemOperator;
		OMX_DECODE_MODE ePlayMode;
		OMX_U32 nChromaAddrAlign;	//the address of Y,Cb,Cr may need to alignment.

		OMX_PTR pClock;
		VpuVersionInfo sVpuVer;		//vpu version info
		OMX_U32 nCapability;			//vpu capability

		OMX_S32 nMaxDurationMsThr;	// control the speed of data consumed by decoder: -1 -> no threshold
		OMX_S32 nMaxBufCntThr;		// control the speed of data consumed by decoder: -1 -> no threshold

		OMX_U32 nOutBufferCntPost;	//used for post-porcess: <= nutBufferCnt-nOutBufferCntDec
		OMX_U32 nOutBufferCntDec;	//used for vpu: <= nutBufferCnt-nOutBufferCntPost
		OMX_BOOL bEnabledPostProcess;	//post-process is enalbed or not: mainly for deinterlace
		ipu_motion_sel nPostMotion;     // post process motion: LOW/MED/HIGH
		VpuDecoderIpuHandle sIpuHandle;
		fsl_osal_ptr pPostThreadId;
		PostProcessState ePostState;	//post thread write, decoder thread read
		PostProcessCmd ePostCmd;	//post thread read/clear, decoder thread write
		Queue* pPostInQueue;		//vpu frame index(low 16bits): decode thread add, post thread get
		Queue* pPostOutQueue;		//post/vpu frame index(low 16bits): decode thread get, post thread add
		Queue* pPostInReturnQueue;  //vpu frame index: decode thread get, post thread add
		Queue* pPostOutReturnQueue;//post frame index: decode thread add, post thread get
		fsl_osal_sem pPostCmdSem;	//decode thread wait, post thread post
		fsl_osal_mutex pPostMutex;       //sync mutex between post and decode thread
		pthread_cond_t sPostCond;        //sync condition between post and decode thread
		OMX_BOOL bPostWaitingTasks;//post thread are waiting tasks from decode thread

		OMX_S32 nFrameWidthStride;	//user may register frames with specified width stride
		OMX_S32 nFrameHeightStride;	//user may register frames with specified height stride
		OMX_S32 nFrameMaxCnt;		//user may register frames with specified count
		OMX_BOOL bReorderDisabled;

        OMX_U32 nRegisterFramePhyAddr;
        OMX_BOOL bHasCodecColorDesc;
        VpuColourDesc sDecoderColorDesc;
        VpuColourDesc sParserColorDesc;
        OMX_CONFIG_VPU_STATIC_HDR_INFO sStaticHDRInfo;
        OMX_U32 nYOffset;
        OMX_U32 nUVOffset;
		OMX_ERRORTYPE SetRoleFormat(OMX_STRING role);
		OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE GetInputDataDepthThreshold(OMX_S32* pDurationThr, OMX_S32* pBufCntThr);

		/* virtual function implementation */
		OMX_ERRORTYPE InitFilterComponent();
		OMX_ERRORTYPE DeInitFilterComponent();
		OMX_ERRORTYPE SetInputBuffer(OMX_PTR pBuffer, OMX_S32 nSize, OMX_BOOL bLast);
		OMX_ERRORTYPE SetOutputBuffer(OMX_PTR pBuffer);
		OMX_ERRORTYPE InitFilter();
		OMX_ERRORTYPE DeInitFilter();
		FilterBufRetCode FilterOneBuffer();
		OMX_ERRORTYPE GetDecBuffer(OMX_PTR *ppBuffer,OMX_S32 * pOutStuffSize,OMX_S32* pOutFrmSize);
		OMX_ERRORTYPE GetPostMappedDecBuffer(OMX_PTR pPostBuf,OMX_PTR *ppDecBuffer);
		OMX_ERRORTYPE GetOutputBuffer(OMX_PTR *ppBuffer,OMX_S32 * pOutSize);
		OMX_ERRORTYPE FlushFilter();
		OMX_ERRORTYPE FlushInputBuffer();
		OMX_ERRORTYPE FlushOutputBuffer();

		OMX_PTR AllocateInputBuffer(OMX_U32 nSize);
		OMX_ERRORTYPE FreeInputBuffer(OMX_PTR pBuffer);
		OMX_PTR AllocateOutputBuffer(OMX_U32 nSize);
		OMX_ERRORTYPE FreeOutputBuffer(OMX_PTR pBuffer);

		OMX_ERRORTYPE SetDefaultSetting();
		FilterBufRetCode ProcessVpuInitInfo();
		OMX_ERRORTYPE PostThreadRun();
		OMX_ERRORTYPE PostThreadStop();
		OMX_ERRORTYPE PostThreadFlushInput();
		OMX_ERRORTYPE PostThreadFlushOutput();
		OMX_ERRORTYPE ReleaseVpuSource();
        OMX_ERRORTYPE GetCropInfo(OMX_CONFIG_RECTTYPE *sCrop);
        OMX_ERRORTYPE SetCropInfo(OMX_CONFIG_RECTTYPE *sCrop);
        OMX_ERRORTYPE CheckDropB(VpuDecHandle InHandle,OMX_TICKS nTimeStamp,OMX_PTR pClock,VPUCompSemaphor psem);
        OMX_ERRORTYPE ConfigVpu(VpuDecHandle InHandle,VpuDecConfig config,OMX_S32 param,VPUCompSemaphor psem);
        OMX_ERRORTYPE PortFormatChanged(OMX_U32 nPortIndex);
        OMX_BOOL DefaultOutputBufferNeeded();
};

#endif
/* File EOF */
