/**
 *  Copyright (c) 2010-2016, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 *  Copyright 2018 NXP
 */
#include "VpuEncComponent.h"
#include "Mem.h"
#include "sys/ioctl.h"
#include "fcntl.h"

#define VPU_ENC_GUESS_OUTLENGTH	//no size in SetOutputBuffer(), so we guess one value
//#define VPU_ENC_WORKAROUND_WIFI_INTRA_QUALITY //we need to set qp value for I frame manually to avoid dither in static video

//#define VPU_ENC_COMP_API_DBGLOG
#ifdef VPU_ENC_COMP_API_DBGLOG
#define VPU_ENC_COMP_API_LOG	printf
#define VPU_ENC_COMP_LOG(...)//	printf
#define VPU_ENC_API(...)//	printf
#define VPU_ENC_LOG(...)// printf
typedef int INT32;				//avoid compiler warning in printf
typedef unsigned int UINT32;		//avoid compiler warning in printf
#else
#define VPU_ENC_COMP_API_LOG(...)
#define VPU_ENC_COMP_LOG(...)
#define VPU_ENC_API(...)
#define VPU_ENC_LOG(...)
#endif

//#define VPU_ENC_COMP_ERR_DBGLOG
#ifdef VPU_ENC_COMP_ERR_DBGLOG
#define VPU_ENC_COMP_ERR_LOG	printf
//#define VPU_ENC_ERROR	printf
typedef int INT32;				//avoid compiler warning in printf
typedef unsigned int UINT32;		//avoid compiler warning in printf
#define ASSERT(exp)	if(!(exp)) {printf("%s: %d : assert condition !!!\r\n",__FUNCTION__,__LINE__);}
#else
typedef int INT32;				//avoid compiler warning in printf
typedef unsigned int UINT32;		//avoid compiler warning in printf
#define VPU_ENC_COMP_ERR_LOG LOG_ERROR //(...)
//#define VPU_ENC_ERROR(...)
#define ASSERT(...)
#endif

#ifdef NULL
#undef NULL
#define NULL 0
#endif

#define Align(ptr,align)	(((OMX_U32)ptr+(align)-1)/(align)*(align))
//#define MemAlign(mem,align)	((((OMX_U32)mem)%(align))==0)
//#define MemNotAlign(mem,align)	((((OMX_U32)mem)%(align))!=0)

#define ENC_MAX_FRAME_NUM		(VPU_ENC_MAX_NUM_MEM)
//#define ENC_FRAME_SURPLUS	(1)

#define DEFAULT_ENC_FRM_WIDTH		(320)
#define DEFAULT_ENC_FRM_HEIGHT		(240)
#define DEFAULT_ENC_FRM_RATE			(30 * Q16_SHIFT)
#define DEFAULT_ENC_FRM_BITRATE		(256 * 1024)

#define DEFAULT_ENC_BUF_IN_CNT		0x2
#define DEFAULT_ENC_BUF_IN_SIZE		(DEFAULT_ENC_FRM_WIDTH*DEFAULT_ENC_FRM_HEIGHT*3/2)
#define DEFAULT_ENC_BUF_OUT_CNT		0x3
#define DEFAULT_ENC_BUF_OUT_SIZE		(1024*1024)	//FIXME: set one big enough value !!!

#define VPU_ENC_COMP_NAME_AVCENC	"OMX.Freescale.std.video_encoder.avc.hw-based"
#define VPU_ENC_COMP_NAME_MPEG4ENC	"OMX.Freescale.std.video_encoder.mpeg4.hw-based"
#define VPU_ENC_COMP_NAME_H263ENC	"OMX.Freescale.std.video_encoder.h263.hw-based"
#define VPU_ENC_COMP_NAME_VP8ENC	"OMX.Freescale.std.video_encoder.vp8.hw-based"

#define VPU_ENC_VP8_BITRATE_THRESHOLD    (20000000) // 20Mbps

//#define PRE_PROCESS_BY_PASS_TEST  //only for pipeline test
#define PRE_FLAG_EOS			(0x1)
#define PRE_MAX_QUEUE_SIZE	(VPU_ENC_MAX_NUM_MEM)
#define PRE_OUT_BUF_NUM		(3)
#define PRE_THRESHOLD_MIN_INPUT		(1)
#define IPU_FOURCC(a,b,c,d) (((OMX_U32)(a)<<0)|((OMX_U32)(b)<<8)|((OMX_U32)(c)<<16)|((OMX_U32)(d)<<24))

OMX_S32 MemMallocVpuBlock(VpuMemInfo* pMemBlock,VpuEncoderMemInfo* pVpuMem,OMX_PARAM_MEM_OPERATOR* pMemOp);

//#define VPU_ENC_COMP_PRE_DEBUG
#ifdef VPU_ENC_COMP_PRE_DEBUG
#include <sys/time.h>
static struct timeval time_beg;
static struct timeval time_end;
static unsigned long long total_time;
static unsigned long long total_cnt;
static void time_init()
{
	total_time=0;
	total_cnt=0;
	return;
}
static void time_start()
{
	gettimeofday(&time_beg, 0);
	return;
}
static void time_stop()
{
	unsigned int tm_1, tm_2;
	gettimeofday(&time_end, 0);

	tm_1 = time_beg.tv_sec * 1000000 + time_beg.tv_usec;
	tm_2 = time_end.tv_sec * 1000000 + time_end.tv_usec;
	total_cnt++;
	total_time = total_time + (tm_2-tm_1);
	printf("pre-process time: %d (us) \r\n",(tm_2-tm_1));
	return;
}
static void time_report()
{
	double avg;
	if(total_cnt)
	{
		avg=total_time/total_cnt;
		printf(" average pre-process time: %f (ms)\r\n",avg/1000);
	}
	return;
}
#define TIMER_INIT	time_init()
#define TIMER_START time_start()
#define TIMER_STOP	time_stop()
#define TIMER_REPORT	time_report()
#define VPU_ENC_COMP_PRE_LOG printf
#else
#define TIMER_INIT
#define TIMER_START
#define TIMER_STOP
#define TIMER_REPORT
#define VPU_ENC_COMP_PRE_LOG
#endif

VpuEncRetCode VPUCom_GetMem_Wrapper(VpuMemDesc* pInOutMem,OMX_PARAM_MEM_OPERATOR* pMemOp)
{
	VpuEncRetCode ret;
	if((pMemOp->pfMalloc==NULL) || (pMemOp->pfFree==NULL))
	{
		//use default method
		ret=VPU_EncGetMem(pInOutMem);
	}
	else
	{
		OMX_MEM_DESC sOmxMem;
		sOmxMem.nSize=pInOutMem->nSize;
		if(OMX_TRUE==pMemOp->pfMalloc(&sOmxMem))
		{
			pInOutMem->nPhyAddr=sOmxMem.nPhyAddr;
			pInOutMem->nVirtAddr=sOmxMem.nVirtAddr;
			pInOutMem->nCpuAddr=sOmxMem.nCpuAddr;
			ret=VPU_ENC_RET_SUCCESS;
		}
		else
		{
			ret=VPU_ENC_RET_FAILURE;
		}
	}
	return ret;
}

VpuEncRetCode VPUCom_FreeMem_Wrapper(VpuMemDesc* pInMem,OMX_PARAM_MEM_OPERATOR* pMemOp)
{
	VpuEncRetCode ret;
	if((pMemOp->pfMalloc==NULL) || (pMemOp->pfFree==NULL))
	{
		//use default method
		ret=VPU_EncFreeMem(pInMem);
	}
	else
	{
		OMX_MEM_DESC sOmxMem;
		sOmxMem.nSize=pInMem->nSize;
		sOmxMem.nPhyAddr=pInMem->nPhyAddr;
		sOmxMem.nVirtAddr=pInMem->nVirtAddr;
		sOmxMem.nCpuAddr=pInMem->nCpuAddr;
		if(OMX_TRUE==pMemOp->pfFree(&sOmxMem))
		{
			ret=VPU_ENC_RET_SUCCESS;
		}
		else
		{
			ret=VPU_ENC_RET_FAILURE;
		}
	}
	return ret;
}

VpuColorFormat VPUCom_ConvertColorFmtOmx2Vpu(OMX_COLOR_FORMATTYPE omxColorFmt)
{
  VpuColorFormat vpuColorFmt = VPU_COLOR_420;

  switch((int)omxColorFmt) {
    case OMX_COLOR_FormatYUV420SemiPlanar:
    case OMX_COLOR_FormatYUV420Planar:
        vpuColorFmt = VPU_COLOR_420;
        break;
    case OMX_COLOR_FormatYCbYCr:
        vpuColorFmt = VPU_COLOR_422H;
    default:
      VPU_ENC_COMP_LOG("unknown omx color format %d\n", omxColorFmt);
      break;
  }

  return vpuColorFmt;
}

VpuEncRetCode VPUCom_Init(VpuEncInputParam* pVpuEncInputPara,VpuEncoderMemInfo* pVpuMem,OMX_PARAM_MEM_OPERATOR* pMemOp,VpuEncHandle *pOutHandle, VpuEncInitInfo * pOutInitInfo)
{
	VpuEncRetCode ret;
	VpuMemInfo sMemInfo;
	VpuEncOpenParamSimp sEncOpenParam;
	//query memory
	ret=VPU_EncQueryMem(&sMemInfo);
	if (ret!=VPU_ENC_RET_SUCCESS)
	{
		VPU_ENC_COMP_ERR_LOG("%s: vpu query memory failure: ret=0x%X \r\n",__FUNCTION__,ret);
		return VPU_ENC_RET_FAILURE;
	}
	//malloc memory for vpu
	if(0==MemMallocVpuBlock(&sMemInfo,pVpuMem,pMemOp))
	{
		VPU_ENC_COMP_ERR_LOG("%s: malloc memory failure: \r\n",__FUNCTION__);
		return VPU_ENC_RET_FAILURE;
	}

	//clear 0 firstly
#if 0 //1 !!!!
	fsl_osal_memset(&sEncOpenParam,0,sizeof(VpuEncOpenParam));
#else
	fsl_osal_memset(&sEncOpenParam,0,sizeof(VpuEncOpenParamSimp));
#endif
	sEncOpenParam.eFormat=pVpuEncInputPara->eFormat;
	sEncOpenParam.nPicWidth= pVpuEncInputPara->nPicWidth;
	sEncOpenParam.nPicHeight=pVpuEncInputPara->nPicHeight;
	sEncOpenParam.nRotAngle=pVpuEncInputPara->nRotAngle;
	sEncOpenParam.nFrameRate=pVpuEncInputPara->nFrameRate;
	sEncOpenParam.nBitRate=pVpuEncInputPara->nBitRate/1000;	// bps => kbps !!!
	sEncOpenParam.nGOPSize=pVpuEncInputPara->nGOPSize;
	sEncOpenParam.nIntraRefresh=pVpuEncInputPara->nIntraFreshNum;
	sEncOpenParam.nIntraQP=pVpuEncInputPara->nRcIntraQP;
	sEncOpenParam.nChromaInterleave=pVpuEncInputPara->nChromaInterleave;
	sEncOpenParam.sMirror=pVpuEncInputPara->sMirror;
    sEncOpenParam.eColorFormat = VPUCom_ConvertColorFmtOmx2Vpu(pVpuEncInputPara->eColorFormat);

	if(1==sEncOpenParam.nChromaInterleave){
		//for reduce the bus loading, we set tile format for vpu internal frame buffers.
		sEncOpenParam.nMapType=1;
		sEncOpenParam.nLinear2TiledEnable=1;
	}

	//open vpu
	ret=VPU_EncOpenSimp(pOutHandle, &sMemInfo,&sEncOpenParam);
	if (ret!=VPU_ENC_RET_SUCCESS)
	{
		VPU_ENC_COMP_ERR_LOG("%s: vpu open failure: ret=0x%X \r\n",__FUNCTION__,ret);
		return VPU_ENC_RET_FAILURE;
	}

	//set default config
	ret=VPU_EncConfig(*pOutHandle, VPU_ENC_CONF_NONE, NULL);
	if(VPU_ENC_RET_SUCCESS!=ret)
	{
		VPU_ENC_COMP_ERR_LOG("%s: vpu config failure: config=0x%X, ret=%d \r\n",__FUNCTION__,(UINT32)VPU_ENC_CONF_NONE,ret);
		//VPU_EncClose(nHandle);
		return VPU_ENC_RET_FAILURE;
	}

	if(pVpuEncInputPara->nIntraFreshNum>0){
		ret=VPU_EncConfig(*pOutHandle, VPU_ENC_CONF_INTRA_REFRESH, &pVpuEncInputPara->nIntraFreshNum);
		if(VPU_ENC_RET_SUCCESS!=ret){
			VPU_ENC_COMP_ERR_LOG("%s: vpu config failure: config=0x%X, ret=%d \r\n",__FUNCTION__,(UINT32)VPU_ENC_CONF_INTRA_REFRESH,ret);
			return VPU_ENC_RET_FAILURE;
		}
	}

	if(OMX_TRUE==pVpuEncInputPara->bEnabledSPSIDR){
		ret=VPU_EncConfig(*pOutHandle, VPU_ENC_CONF_ENA_SPSPPS_IDR, NULL);
		if(VPU_ENC_RET_SUCCESS!=ret){
			VPU_ENC_COMP_ERR_LOG("%s: vpu config failure: config=0x%X, ret=%d \r\n",__FUNCTION__,(UINT32)VPU_ENC_CONF_ENA_SPSPPS_IDR,ret);
			return VPU_ENC_RET_FAILURE;
		}
	}

	if(pVpuEncInputPara->nRcIntraQP>0){
		//fixme: this setting not work, so we still need to transfer intra qp in open operation
		ret=VPU_EncConfig(*pOutHandle, VPU_ENC_CONF_RC_INTRA_QP, &pVpuEncInputPara->nRcIntraQP);
		if(VPU_ENC_RET_SUCCESS!=ret){
			VPU_ENC_COMP_ERR_LOG("%s: vpu config failure: config=0x%X, ret=%d \r\n",__FUNCTION__,(UINT32)VPU_ENC_CONF_RC_INTRA_QP,ret);
			return VPU_ENC_RET_FAILURE;
		}
	}

	//get initinfo
	ret=VPU_EncGetInitialInfo(*pOutHandle,pOutInitInfo);
	if(VPU_ENC_RET_SUCCESS!=ret)
	{
		VPU_ENC_COMP_ERR_LOG("%s: init vpu failure \r\n",__FUNCTION__);
		return VPU_ENC_RET_FAILURE;
	}

	return VPU_ENC_RET_SUCCESS;
}

OMX_S32 MemFreeBlock(VpuEncoderMemInfo* pEncMem,OMX_PARAM_MEM_OPERATOR* pMemOp, OMX_BOOL bRemoveHw)
{
	OMX_S32 i;
	VpuMemDesc vpuMem;
	VpuEncRetCode vpuRet;
	OMX_S32 retOk=1;

	//free virtual mem
	//for(i=0;i<pEncMem->nVirtNum;i++)
	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		if(pEncMem->virtMem[i])
		{
			fsl_osal_ptr ptr=(fsl_osal_ptr)pEncMem->virtMem[i];
			FSL_FREE(ptr);
			pEncMem->virtMem[i]=NULL;
		}
	}

	pEncMem->nVirtNum=0;

	//free physical mem
	//for(i=0;i<pEncMem->nPhyNum;i++)
	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		if(pEncMem->phyMem_phyAddr[i])
		{
			vpuMem.nPhyAddr=pEncMem->phyMem_phyAddr[i];
			vpuMem.nVirtAddr=pEncMem->phyMem_virtAddr[i];
			vpuMem.nCpuAddr=pEncMem->phyMem_cpuAddr[i];
			vpuMem.nSize=pEncMem->phyMem_size[i];
			vpuRet=VPUCom_FreeMem_Wrapper(&vpuMem,pMemOp);
			if(vpuRet!=VPU_ENC_RET_SUCCESS)
			{
				VPU_ENC_COMP_LOG("%s: free vpu memory failure : ret=0x%X \r\n",__FUNCTION__,vpuRet);
				retOk=0;
			}
			if(bRemoveHw){
				if(OMX_ErrorNone!=RemoveHwBuffer((OMX_PTR)pEncMem->phyMem_virtAddr[i])){
					VPU_ENC_COMP_ERR_LOG("%s: remove hw buffer failure \r\n",__FUNCTION__);
				}
			}
			pEncMem->phyMem_phyAddr[i]=NULL;
			pEncMem->phyMem_virtAddr[i]=NULL;
			pEncMem->phyMem_cpuAddr[i]=NULL;
			pEncMem->phyMem_size[i]=0;
		}
	}
	pEncMem->nPhyNum=0;

	return retOk;
}

OMX_S32 MemAddPhyBlock(VpuMemDesc* pInMemDesc,VpuEncoderMemInfo* pOutEncMem)
{
	OMX_S32 i;

	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		// insert value into empty node
		if(NULL==pOutEncMem->phyMem_phyAddr[i])
		{
			pOutEncMem->phyMem_phyAddr[i]=pInMemDesc->nPhyAddr;
			pOutEncMem->phyMem_virtAddr[i]=pInMemDesc->nVirtAddr;
			pOutEncMem->phyMem_cpuAddr[i]=pInMemDesc->nCpuAddr;
			pOutEncMem->phyMem_size[i]=pInMemDesc->nSize;
			pOutEncMem->nPhyNum++;
			return 1;
		}
	}
	VPU_ENC_COMP_ERR_LOG("error: buffer items(%d) are full \r\n",VPU_ENC_MAX_NUM_MEM);
	return 0;
}

OMX_S32 MemRemovePhyBlock(VpuMemDesc* pInMemDesc,VpuEncoderMemInfo* pOutEncMem)
{
	OMX_S32 i;
	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		// clear matched node
		if(pInMemDesc->nPhyAddr==pOutEncMem->phyMem_phyAddr[i])
		{
			pOutEncMem->phyMem_phyAddr[i]=NULL;
			pOutEncMem->phyMem_virtAddr[i]=NULL;
			pOutEncMem->phyMem_cpuAddr[i]=NULL;
			pOutEncMem->phyMem_size[i]=0;
			pOutEncMem->nPhyNum--;
			return 1;
		}
	}

	return 0;
}

OMX_S32 MemQueryPhyBlock(OMX_PTR pInVirtAddr,VpuMemDesc* pOutMemDesc,VpuEncoderMemInfo* pInEncMem)
{
	OMX_S32 i;
	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		// find matched node
		if((OMX_U32)pInVirtAddr==pInEncMem->phyMem_virtAddr[i])
		{
			pOutMemDesc->nPhyAddr=pInEncMem->phyMem_phyAddr[i];
			pOutMemDesc->nVirtAddr=pInEncMem->phyMem_virtAddr[i];
			pOutMemDesc->nCpuAddr=pInEncMem->phyMem_cpuAddr[i];
			pOutMemDesc->nSize=pInEncMem->phyMem_size[i];
			return 1;
		}
	}

	return 0;
}

OMX_S32 MemMallocVpuBlock(VpuMemInfo* pMemBlock,VpuEncoderMemInfo* pVpuMem,OMX_PARAM_MEM_OPERATOR* pMemOp)
{
	OMX_S32 i;
	OMX_U8 * ptr=NULL;
	OMX_S32 size;

	for(i=0;i<pMemBlock->nSubBlockNum;i++)
	{
		size=pMemBlock->MemSubBlock[i].nAlignment+pMemBlock->MemSubBlock[i].nSize;
		if(pMemBlock->MemSubBlock[i].MemType==VPU_MEM_VIRT)
		{
			ptr=(OMX_U8*)FSL_MALLOC(size);
			if(ptr==NULL)
			{
				VPU_ENC_COMP_LOG("%s: get virtual memory failure, size=%d \r\n",__FUNCTION__,(INT32)size);
				goto failure;
			}
			pMemBlock->MemSubBlock[i].pVirtAddr=(OMX_U8*)Align(ptr,pMemBlock->MemSubBlock[i].nAlignment);

			//record virtual base addr
			pVpuMem->virtMem[pVpuMem->nVirtNum]=(OMX_U32)ptr;
			pVpuMem->nVirtNum++;
		}
		else// if(memInfo.MemSubBlock[i].MemType==VPU_MEM_PHY)
		{
			VpuMemDesc vpuMem;
			VpuEncRetCode ret;
			vpuMem.nSize=size;
			ret=VPUCom_GetMem_Wrapper(&vpuMem,pMemOp);
			if(ret!=VPU_ENC_RET_SUCCESS)
			{
				VPU_ENC_COMP_LOG("%s: get vpu memory failure, size=%d, ret=0x%X \r\n",__FUNCTION__,(INT32)size,ret);
				goto failure;
			}
			pMemBlock->MemSubBlock[i].pVirtAddr=(OMX_U8*)Align(vpuMem.nVirtAddr,pMemBlock->MemSubBlock[i].nAlignment);
			pMemBlock->MemSubBlock[i].pPhyAddr=(OMX_U8*)Align(vpuMem.nPhyAddr,pMemBlock->MemSubBlock[i].nAlignment);

			//record physical base addr
			pVpuMem->phyMem_phyAddr[pVpuMem->nPhyNum]=(OMX_U32)vpuMem.nPhyAddr;
			pVpuMem->phyMem_virtAddr[pVpuMem->nPhyNum]=(OMX_U32)vpuMem.nVirtAddr;
			pVpuMem->phyMem_cpuAddr[pVpuMem->nPhyNum]=(OMX_U32)vpuMem.nCpuAddr;
			pVpuMem->phyMem_size[pVpuMem->nPhyNum]=size;
			pVpuMem->nPhyNum++;
		}
	}

	return 1;

failure:
	MemFreeBlock(pVpuMem,pMemOp,OMX_FALSE);
	return 0;

}


OMX_S32 OutFrameBufRegister(OMX_PTR pInVirtAddr,VpuEncoderMemInfo* pOutEncMem,OMX_S32* pOutIsPhy)
{
	OMX_S32 i;
	OMX_PTR pPhyAddr;

	//get physical info from resource manager
	if(OMX_ErrorNone!=GetHwBuffer(pInVirtAddr,&pPhyAddr))
	{
		return -1;
	}

	if(pPhyAddr==NULL)
	{
		//1 virtual space ??
		for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
		{
			//insert into empty node
			if(NULL==pOutEncMem->virtMem[i])
			{
				pOutEncMem->virtMem[i]=(OMX_U32)pInVirtAddr;
				pOutEncMem->nVirtNum++;
				*pOutIsPhy=0;
				return pOutEncMem->nPhyNum;
			}
		}
	}
	else
	{
		//physical space
		for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
		{
			//insert into empty node
			if(NULL==pOutEncMem->phyMem_phyAddr[i])
			{
				pOutEncMem->phyMem_phyAddr[i]=(OMX_U32)pPhyAddr;
				pOutEncMem->phyMem_virtAddr[i]=(OMX_U32)pInVirtAddr;
				//pOutEncMem->phyMem_cpuAddr[i]=
				//pOutEncMem->phyMem_size[i]=
				pOutEncMem->nPhyNum++;
				*pOutIsPhy=1;
				return pOutEncMem->nPhyNum;
			}
		}
	}

	return -1;
}

OMX_S32 OutFrameBufExist(OMX_PTR pInVirtAddr,VpuEncoderMemInfo* pInEncMem, OMX_S32* pOutIsPhy)
{
	OMX_S32 i;
	OMX_PTR pPhyAddr;

	//get physical info from resource manager
	if(OMX_ErrorNone!=GetHwBuffer(pInVirtAddr,&pPhyAddr))
	{
		return 0;
	}

	if(pPhyAddr==NULL)
	{
		//1 virtual space ??
		for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
		{
			//search matched node
			if((OMX_U32)pInVirtAddr==pInEncMem->virtMem[i])
			{
				*pOutIsPhy=0;
				return 1;
			}
		}
	}
	else
	{
		//physical space
		for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
		{
			//search matched node
			if((OMX_U32)pPhyAddr==pInEncMem->phyMem_phyAddr[i])
			{
				*pOutIsPhy=1;
				return 1;
			}
		}
	}
	return 0;
}

OMX_S32 OutFrameBufClear(VpuEncoderMemInfo* pOutEncMem)
{
	OMX_S32 i;
	//clear all node
	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		pOutEncMem->phyMem_virtAddr[i]=NULL;

		pOutEncMem->phyMem_phyAddr[i]=NULL;
		pOutEncMem->phyMem_virtAddr[i]=NULL;
		pOutEncMem->phyMem_cpuAddr[i]=NULL;
		pOutEncMem->phyMem_size[i]=0;
	}
	pOutEncMem->nVirtNum=0;
	pOutEncMem->nPhyNum=0;
	return 1;
}

OMX_S32 OutFrameBufPhyFindValid(VpuEncoderMemInfo* pInMem, OMX_U32* pOutPhy,OMX_U32* pOutVirt,OMX_U32* pOutLen)
{
	OMX_S32 i;

	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		//find one non-empty physical node
		if(NULL!=pInMem->phyMem_phyAddr[i])
		{
			*pOutPhy=pInMem->phyMem_phyAddr[i];
			*pOutVirt=pInMem->phyMem_virtAddr[i];

#ifdef VPU_ENC_GUESS_OUTLENGTH
		//1 TODO: in fact, we do know the size value since only address is transfered through SetOutputBuffer()
		//give one big enough value, Now, it is only used by assert checking in encode frame : header length <= output buffer size
		*pOutLen = DEFAULT_ENC_BUF_OUT_SIZE;
#else
		*pOutLen=pInMem->phyMem_size[i];
#endif

			return i;
		}
	}
	return -1;
}

OMX_S32 OutFrameBufVirtFindValidAndClear(VpuEncoderMemInfo* pInMem, OMX_U32* pOutVirt)
{
	OMX_S32 i;

	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		//find one non-empty physical node and return its virtual address
		if(NULL!=pInMem->phyMem_phyAddr[i])
		{
			*pOutVirt=pInMem->phyMem_virtAddr[i];
			//clear the node
			pInMem->phyMem_virtAddr[i]=NULL;
			pInMem->phyMem_phyAddr[i]=NULL;
			pInMem->phyMem_virtAddr[i]=NULL;
			pInMem->phyMem_cpuAddr[i]=NULL;
			pInMem->phyMem_size[i]=0;
			pInMem->nPhyNum--;
			return i;
		}
	}
	return -1;
}

OMX_S32 OutFrameBufPhyClear(VpuEncoderMemInfo* pOutDecMem,OMX_PTR pInPhyAddr,OMX_PTR* ppOutVirtAddr)
{
	//find node according physical address and (1) return virtual address (2) clear node
	OMX_S32 i;

	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++)
	{
		if((OMX_U32)pInPhyAddr==pOutDecMem->phyMem_phyAddr[i])
		{
			//return virtual address
			*ppOutVirtAddr=(OMX_PTR)(pOutDecMem->phyMem_virtAddr[i]);
			//clear specified physical node
			pOutDecMem->phyMem_virtAddr[i]=NULL;
			pOutDecMem->phyMem_phyAddr[i]=NULL;
			pOutDecMem->phyMem_virtAddr[i]=NULL;
			pOutDecMem->phyMem_cpuAddr[i]=NULL;
			pOutDecMem->phyMem_size[i]=0;

			pOutDecMem->nPhyNum--;
			return 1;
		}
	}

	return -1;
}

OMX_S32 OutFrameBufCreateRegisterFrame(
	VpuFrameBuffer* pOutRegisterFrame,OMX_S32 nInCnt,OMX_S32 nWidth,OMX_S32 nHeight,
	VpuEncoderMemInfo* pOutEncMemInfo, OMX_PARAM_MEM_OPERATOR* pMemOp,OMX_S32 nInRot,OMX_S32* pOutSrcStride,OMX_S32 nInAlign)
{
	OMX_S32 i;
	VpuEncRetCode ret;
	OMX_S32 yStride;
	OMX_S32 uvStride;
	OMX_S32 ySize;
	OMX_S32 uvSize;
	OMX_S32 mvSize;
	VpuMemDesc vpuMem;
	OMX_U8* ptr;
	OMX_U8* ptrVirt;
	OMX_S32 nPadW;
	OMX_S32 nPadH;

	nPadW=Align(nWidth,16);
	nPadH=Align(nHeight,16);
	if((nInRot==90)||(nInRot==270))
	{
		yStride=nPadH;
		ySize=yStride*nPadW;
	}
	else
	{
		yStride=nPadW;
		ySize=yStride*nPadH;
	}
	uvStride=yStride/2;

	uvSize=ySize/4;
	mvSize=0;//ySize/4;	//1 set 0 ??

	//we need to align the Y/Cb/Cr address
	if(nInAlign>1){
		ySize=Align(ySize,nInAlign);
		uvSize=Align(uvSize,nInAlign);
	}

	for(i=0;i<nInCnt;i++)
	{
		vpuMem.nSize=ySize+uvSize*2+mvSize+nInAlign;
		ret=VPUCom_GetMem_Wrapper(&vpuMem,pMemOp);
		if(VPU_ENC_RET_SUCCESS!=ret)
		{
			VPU_ENC_COMP_ERR_LOG("%s: vpu malloc frame buf failure: ret=0x%X \r\n",__FUNCTION__,ret);
			return -1;//OMX_ErrorInsufficientResources;
		}

		ptr=(OMX_U8*)vpuMem.nPhyAddr;
		ptrVirt=(OMX_U8*)vpuMem.nVirtAddr;

		/*align the base address*/
		if(nInAlign>1){
			ptr=(unsigned char*)Align(ptr,nInAlign);
			ptrVirt=(unsigned char*)Align(ptrVirt,nInAlign);
		}

		/* fill stride info */
		pOutRegisterFrame[i].nStrideY=yStride;
		pOutRegisterFrame[i].nStrideC=uvStride;

		/* fill phy addr*/
		pOutRegisterFrame[i].pbufY=ptr;
		pOutRegisterFrame[i].pbufCb=ptr+ySize;
		pOutRegisterFrame[i].pbufCr=ptr+ySize+uvSize;
		pOutRegisterFrame[i].pbufMvCol=ptr+ySize+uvSize*2;

		/* fill virt addr */
		pOutRegisterFrame[i].pbufVirtY=ptrVirt;
		pOutRegisterFrame[i].pbufVirtCb=ptrVirt+ySize;
		pOutRegisterFrame[i].pbufVirtCr=ptrVirt+ySize+uvSize;
		pOutRegisterFrame[i].pbufVirtMvCol=ptrVirt+ySize+uvSize*2;

		/*now, only consider frame tile*/
		pOutRegisterFrame[i].pbufY_tilebot=0;
		pOutRegisterFrame[i].pbufCb_tilebot=0;
		pOutRegisterFrame[i].pbufVirtY_tilebot=0;
		pOutRegisterFrame[i].pbufVirtCb_tilebot=0;

		//record memory info for release
		pOutEncMemInfo->phyMem_phyAddr[pOutEncMemInfo->nPhyNum]=vpuMem.nPhyAddr;
		pOutEncMemInfo->phyMem_virtAddr[pOutEncMemInfo->nPhyNum]=vpuMem.nVirtAddr;
		pOutEncMemInfo->phyMem_cpuAddr[pOutEncMemInfo->nPhyNum]=vpuMem.nCpuAddr;
		pOutEncMemInfo->phyMem_size[pOutEncMemInfo->nPhyNum]=vpuMem.nSize;
		pOutEncMemInfo->nPhyNum++;
	}

	*pOutSrcStride=nWidth;//nPadW;
	return i;
}

OMX_PTR AllocateBuffer(OMX_U32 nSize,VpuEncoderMemInfo* pInMemInfo,OMX_PARAM_MEM_OPERATOR* pMemOp)
{
	VpuEncRetCode ret;
	VpuMemDesc vpuMem;

	//malloc physical memory through vpu
	vpuMem.nSize=nSize;
	ret=VPUCom_GetMem_Wrapper(&vpuMem,pMemOp);
	if(VPU_ENC_RET_SUCCESS!=ret)
	{
		VPU_ENC_COMP_ERR_LOG("%s: vpu malloc frame buf failure: size=%d, ret=0x%X \r\n",__FUNCTION__,(INT32)nSize,ret);
		return (OMX_PTR)NULL;//OMX_ErrorInsufficientResources;
	}

	//record memory for release
	if(0==MemAddPhyBlock(&vpuMem, pInMemInfo))
	{
		VPUCom_FreeMem_Wrapper(&vpuMem,pMemOp);
		VPU_ENC_COMP_ERR_LOG("%s:add phy block failure \r\n",__FUNCTION__);
		return (OMX_PTR)NULL;
	}

	//register memory info into resource manager
	if(OMX_ErrorNone!=AddHwBuffer((OMX_PTR)vpuMem.nPhyAddr, (OMX_PTR)vpuMem.nVirtAddr))
	{
		MemRemovePhyBlock(&vpuMem, pInMemInfo);
		VPUCom_FreeMem_Wrapper(&vpuMem,pMemOp);
		VPU_ENC_COMP_ERR_LOG("%s:add hw buffer failure \r\n",__FUNCTION__);
		return (OMX_PTR)NULL;
	}

	//return virtual address
	return (OMX_PTR)vpuMem.nVirtAddr;//OMX_ErrorNone;
}

OMX_S32 SetDefaultEncParam(VpuEncInputParam* pEncParam)
{
	//pEncParam->eFormat=VPU_V_AVC;
	pEncParam->eFormat=VPU_V_MPEG4;
	//pEncParam->eFormat=VPU_V_H263;
	pEncParam->nPicWidth=DEFAULT_ENC_FRM_WIDTH;
	pEncParam->nPicHeight=DEFAULT_ENC_FRM_HEIGHT;
	pEncParam->nWidthStride=pEncParam->nPicWidth;
	pEncParam->nHeightStride=pEncParam->nPicHeight;
	pEncParam->nRotAngle=0;//90;
	pEncParam->nFrameRate=30;
	ASSERT(0!=pEncParam->nFrameRate);
	pEncParam->nBitRate=0;
	pEncParam->nGOPSize=15;
	pEncParam->nChromaInterleave=0;
	pEncParam->sMirror=VPU_ENC_MIRDIR_NONE;

	/* "nQuantParam" is used for all quantization parameters in case of VBR (no rate control).
	The range of value is 1-31 for MPEG-4 and 0-51 for H.264. When rate control is
	enabled, this field is ignored*/
	pEncParam->nQuantParam=10;

	pEncParam->nEnableAutoSkip=1;

	pEncParam->nIDRPeriod=pEncParam->nGOPSize;
	pEncParam->nRefreshIntra=0;
	pEncParam->nIntraFreshNum=0;
	pEncParam->bEnabledSPSIDR=OMX_FALSE;
	pEncParam->nRcIntraQP=0;
	return 1;
}

OMX_ERRORTYPE FreeBuffer(OMX_PTR pBuffer,VpuEncoderMemInfo* pInMemInfo,OMX_PARAM_MEM_OPERATOR* pMemOp)
{
	VpuEncRetCode ret;
	VpuMemDesc vpuMem;

	VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//query related mem info for release
	if(0==MemQueryPhyBlock(pBuffer,&vpuMem,pInMemInfo))
	{
		VPU_ENC_COMP_ERR_LOG("%s: query phy block failure \r\n",__FUNCTION__);
		return OMX_ErrorResourcesLost;
	}

	//release physical memory through vpu
	ret=VPUCom_FreeMem_Wrapper(&vpuMem,pMemOp);
	if(ret!=VPU_ENC_RET_SUCCESS)
	{
		VPU_ENC_COMP_ERR_LOG("%s: free vpu memory failure : ret=0x%X \r\n",__FUNCTION__,ret);
		return OMX_ErrorHardware;
	}

	//remove mem info
	if(0==MemRemovePhyBlock(&vpuMem, pInMemInfo))
	{
		VPU_ENC_COMP_ERR_LOG("%s: remove phy block failure \r\n",__FUNCTION__);
		return OMX_ErrorResourcesLost;
	}

	//unregister memory info from resource manager
	if(OMX_ErrorNone!=RemoveHwBuffer(pBuffer))
	{
		VPU_ENC_COMP_ERR_LOG("%s: remove hw buffer failure \r\n",__FUNCTION__);
		return OMX_ErrorResourcesLost;
	}

	return OMX_ErrorNone;
}

#ifdef MX6X
OMX_S32 PreProcessetSetStrategy(OMX_COLOR_FORMATTYPE colorfmt,OMX_BOOL* pOutEnabled)
{
	OMX_BOOL bEnable=OMX_FALSE;
	switch((int)colorfmt){
		case OMX_COLOR_Format16bitRGB565:
		case OMX_COLOR_Format24bitRGB888:
		case OMX_COLOR_Format24bitBGR888:
		//case OMX_COLOR_Format32bitARGB8888:
		case OMX_COLOR_Format32bitBGRA8888:
		case OMX_COLOR_Format32bitRGBA8888:
			bEnable=OMX_TRUE;
			VPU_ENC_COMP_LOG("enable pre-process \r\n");
			break;
#ifdef PRE_PROCESS_BY_PASS_TEST	//for by pipeline test
		case OMX_COLOR_FormatYUV420Planar:
		case OMX_COLOR_FormatYUV420SemiPlanar:
			bEnable=OMX_TRUE;
			VPU_ENC_COMP_LOG("pre-process by pass test: color fmt: %d \r\n",colorfmt);
			break;
#endif
		default:
			break;
	}
	*pOutEnabled=bEnable;
	return 1;
}
OMX_S32 PreProcessInit(VpuEncPPHandle* preProcessHandle)
{
    VpuEncoderIpuHandle *pIpuHandle;

    pIpuHandle = (VpuEncoderIpuHandle*)fsl_osal_malloc_new(sizeof(VpuEncoderIpuHandle));
    if (NULL == pIpuHandle)
        return 0;

	pIpuHandle->nIpuFd=open("/dev/mxc_ipu", O_RDWR, 0);
	if(pIpuHandle->nIpuFd<=0){
		VPU_ENC_COMP_ERR_LOG("Open /dev/mxc_ipu failed.\n");
		return 0;
	}
	fsl_osal_memset(&pIpuHandle->sIpuTask, 0, sizeof(IpuTask));
    *preProcessHandle = (VpuEncPPHandle)pIpuHandle;

	TIMER_INIT;
	return 1;
}
OMX_S32 PreProcessSetDefault(VpuEncPPHandle preProcessHandle,OMX_VIDEO_PORTDEFINITIONTYPE* pInFmt,OMX_COLOR_FORMATTYPE eOutColor)
{
    VpuEncoderIpuHandle *pIpuHandle = (VpuEncoderIpuHandle*)preProcessHandle;
	struct ipu_input* pIn=&pIpuHandle->sIpuTask.input;
	struct ipu_output* pOut=&pIpuHandle->sIpuTask.output;
	OMX_U32 outcolor=IPU_FOURCC('N', 'V', '1', '2');;
	OMX_U32 incolor=IPU_FOURCC('R', 'G', 'B', 'A');
	OMX_U32 width=pInFmt->nFrameWidth;
	OMX_U32 height=pInFmt->nFrameHeight;

	pIpuHandle->sIpuTask.overlay_en=0;
	pIn->deinterlace.enable=0;
	switch ((int)pInFmt->eColorFormat){
		case OMX_COLOR_Format16bitRGB565:
			incolor=IPU_FOURCC('R', 'G', 'B', 'P');
			break;
		case OMX_COLOR_Format24bitRGB888:
			incolor=IPU_FOURCC('R', 'G', 'B', '3');
			break;
		case OMX_COLOR_Format24bitBGR888:
			incolor=IPU_FOURCC('B', 'G', 'R', '3');
			break;
		case OMX_COLOR_Format32bitBGRA8888:
			incolor=IPU_FOURCC('B', 'G', 'R', 'A');
			break;
		case OMX_COLOR_Format32bitRGBA8888:
			incolor=IPU_FOURCC('R', 'G', 'B', 'A');
			break;
#ifdef PRE_PROCESS_BY_PASS_TEST
		case OMX_COLOR_FormatYUV420Planar:
			incolor=IPU_FOURCC('I', '4', '2', '0');
			break;
		case OMX_COLOR_FormatYUV420SemiPlanar:
			incolor=IPU_FOURCC('N', 'V', '1', '2');
			break;
#endif
		default:
			VPU_ENC_COMP_ERR_LOG("unsupported input color format(%d) for pre-process ! \r\n",pInFmt->eColorFormat);
			break;
	}
	switch(eOutColor){
		case OMX_COLOR_FormatYUV420Planar:
			outcolor=IPU_FOURCC('I', '4', '2', '0');
			VPU_ENC_COMP_LOG("Set pre-process out format to YUVI420.\n");
			break;
		case OMX_COLOR_FormatYUV420SemiPlanar:
			outcolor=IPU_FOURCC('N', 'V', '1', '2');
			VPU_ENC_COMP_LOG("Set pre-process in format to NV12.\n");
			break;
		default:
			VPU_ENC_COMP_ERR_LOG("unsupported output color format(%d) for pre-process ! \r\n",eOutColor);
			break;
	}

	if(width%8!=0){
		VPU_ENC_COMP_ERR_LOG("warning: width(%d) isn't aligned with 8 bytes !!! \r\n",(INT32)width);
	}
	pIn->width = width;
	pIn->height = height;
	pIn->crop.pos.x = 0;
	pIn->crop.pos.y = 0;
	pIn->crop.w = width;
	pIn->crop.h = height;
	pIn->format = incolor;
	VPU_ENC_COMP_LOG("IpulibRender sInParam width %d, height %d,crop x %d, y %d, w %d, h %d, color %d\n",
		pIn->width, pIn->height,pIn->crop.pos.x, pIn->crop.pos.y,pIn->crop.w, pIn->crop.h,pIn->format);

	pOut->width = pIn->width;
	pOut->height = pIn->height;
	pOut->crop.pos.x = pIn->crop.pos.x;
	pOut->crop.pos.y = pIn->crop.pos.y;
	pOut->crop.w = pIn->crop.w;
	pOut->crop.h = pIn->crop.h;
	pOut->format = outcolor;
	pOut->rotate = ROTATE_NONE;
	VPU_ENC_COMP_LOG("IpulibRender sOutParam width %d, height %d,crop x %d, y %d, rot: %d, color %d\n",
		pOut->crop.w, pOut->crop.h ,pOut->crop.pos.x, pOut->crop.pos.y,pOut->rotate, pOut->format);
	return 1;
}
OMX_S32 PreProcessTask(VpuEncPPHandle preProcessHandle,OMX_PTR pInPhy, OMX_PTR pOutPhy)
{
	OMX_S32 ret=0;
    VpuEncoderIpuHandle *pIpuHandle = (VpuEncoderIpuHandle*)preProcessHandle;
	pIpuHandle->sIpuTask.input.paddr=(int)pInPhy;
	pIpuHandle->sIpuTask.output.paddr=(int)pOutPhy;

	ret = IPU_CHECK_ERR_INPUT_CROP;
	while(ret != IPU_CHECK_OK && ret > IPU_CHECK_ERR_MIN) {
		ret = ioctl(pIpuHandle->nIpuFd, IPU_CHECK_TASK, &pIpuHandle->sIpuTask);
		switch(ret) {
			case IPU_CHECK_OK:
				break;
			case IPU_CHECK_ERR_SPLIT_INPUTW_OVER:
				pIpuHandle->sIpuTask.input.crop.w -= 8;
				break;
			case IPU_CHECK_ERR_SPLIT_INPUTH_OVER:
				pIpuHandle->sIpuTask.input.crop.h -= 8;
				break;
			case IPU_CHECK_ERR_SPLIT_OUTPUTW_OVER:
				pIpuHandle->sIpuTask.output.crop.w -= 8;
				break;
			case IPU_CHECK_ERR_SPLIT_OUTPUTH_OVER:
				pIpuHandle->sIpuTask.output.crop.h -= 8;
				break;
			default:
				return 0;
		}
	}

	ret = ioctl(pIpuHandle->nIpuFd, IPU_QUEUE_TASK, &pIpuHandle->sIpuTask);
	if (ret < 0){
		VPU_ENC_COMP_ERR_LOG("ioct IPU_QUEUE_TASK fail\n");
		return 0;
	}
	return 1;
}
OMX_S32 PreProcessDeinit(VpuEncPPHandle preProcessHandle)
{
    VpuEncoderIpuHandle *pIpuHandle = (VpuEncoderIpuHandle*)preProcessHandle;
	if(pIpuHandle->nIpuFd>0){
		close(pIpuHandle->nIpuFd);
		pIpuHandle->nIpuFd=0;
	}
    free(pIpuHandle);
	TIMER_REPORT;
	return 1;
}

#else
g2d_format VPUCom_ConvertColorFmtOmx2G2D(OMX_COLOR_FORMATTYPE omxColorFmt)
{
  g2d_format g2dColorFmt = G2D_NV12;

  switch((int)omxColorFmt) {
    case OMX_COLOR_FormatYUV420SemiPlanar:
        g2dColorFmt = G2D_NV12;
        break;
    case OMX_COLOR_FormatYUV420Planar:
        g2dColorFmt = G2D_I420;
        break;
    case OMX_COLOR_FormatYCbYCr:
        g2dColorFmt = G2D_YUYV;
        break;
    case OMX_COLOR_Format16bitRGB565:
        g2dColorFmt = G2D_RGB565;
        break;
    case OMX_COLOR_Format16bitBGR565:
        g2dColorFmt = G2D_BGR565;
        break;
    case OMX_COLOR_Format32bitRGBA8888:
        g2dColorFmt = G2D_RGBA8888;
        break;
    case OMX_COLOR_Format32bitARGB8888:
        g2dColorFmt = G2D_ARGB8888;
        break;
    case OMX_COLOR_Format32bitBGRA8888:
        g2dColorFmt = G2D_BGRA8888;
        break;
    default:
      VPU_ENC_COMP_LOG("unknown omx color format %d\n", omxColorFmt);
      break;
  }

  return g2dColorFmt;
}

OMX_S32 PreProcessetSetStrategy(OMX_COLOR_FORMATTYPE fmt,OMX_BOOL* pOutEnabled)
{
	OMX_BOOL bEnable=OMX_FALSE;
	switch((int)fmt){
		case OMX_COLOR_Format16bitRGB565:
        case OMX_COLOR_Format16bitBGR565:
		case OMX_COLOR_Format32bitBGRA8888:
		case OMX_COLOR_Format32bitRGBA8888:
        case OMX_COLOR_Format32bitARGB8888:
			bEnable = OMX_TRUE;
			VPU_ENC_COMP_LOG("enable pre-process \r\n");
			break;
		default:
			break;
	}
	*pOutEnabled = bEnable;
	return 1;
}
OMX_S32 PreProcessInit(VpuEncPPHandle* preProcessHandle)
{
    VpuEncoderGpuHandle *pGpuHandle;

    pGpuHandle = (VpuEncoderGpuHandle*)fsl_osal_malloc_new(sizeof(VpuEncoderGpuHandle));
    if (NULL == pGpuHandle)
        return 0;

    fsl_osal_memset(&pGpuHandle->sInput, 0, sizeof(G2DSurface));
    fsl_osal_memset(&pGpuHandle->sOutput, 0, sizeof(G2DSurface));
    *preProcessHandle = (VpuEncPPHandle*)pGpuHandle;
    TIMER_INIT;
	return 1;
}
OMX_S32 PreProcessSetDefault(VpuEncPPHandle preProcessHandle,OMX_VIDEO_PORTDEFINITIONTYPE* pInFmt,OMX_COLOR_FORMATTYPE eOutColor)
{
    VpuEncoderGpuHandle *pGpuHandle = (VpuEncoderGpuHandle*)preProcessHandle;

    pGpuHandle->sInput.format = VPUCom_ConvertColorFmtOmx2G2D(pInFmt->eColorFormat);
    pGpuHandle->sInput.left = 0;
    pGpuHandle->sInput.top = 0;
    pGpuHandle->sInput.right = pInFmt->nFrameWidth;
    pGpuHandle->sInput.bottom = pInFmt->nFrameHeight;
    pGpuHandle->sInput.width = pInFmt->nFrameWidth;
    pGpuHandle->sInput.height = pInFmt->nFrameHeight;
    pGpuHandle->sInput.stride = pInFmt->nFrameWidth;

    pGpuHandle->sOutput.format = VPUCom_ConvertColorFmtOmx2G2D(eOutColor);
    pGpuHandle->sOutput.left = 0;
    pGpuHandle->sOutput.top = 0;
    pGpuHandle->sOutput.right = pInFmt->nFrameWidth;
    pGpuHandle->sOutput.bottom = pInFmt->nFrameHeight;
    pGpuHandle->sOutput.width = pInFmt->nFrameWidth;
    pGpuHandle->sOutput.height = pInFmt->nFrameHeight;
    pGpuHandle->sOutput.stride = pInFmt->nFrameWidth;

	return 1;
}

OMX_S32 PreProcessTask(VpuEncPPHandle preProcessHandle,OMX_PTR pInPhy, OMX_PTR pOutPhy)
{
    VpuEncoderGpuHandle *pGpuHandle = (VpuEncoderGpuHandle*)preProcessHandle;

    if (g2d_open(&pGpuHandle->g2dHandle) == -1 || pGpuHandle->g2dHandle == NULL) {
        VPU_ENC_COMP_ERR_LOG("%s, g2d_open failed\n", __FUNCTION__);
        return 0;
    }

    pGpuHandle->sInput.planes[0] = (int)pInPhy;
    pGpuHandle->sOutput.planes[0] = (int)pOutPhy;

    if (g2d_blit(pGpuHandle->g2dHandle, &pGpuHandle->sInput, &pGpuHandle->sOutput) != 0) {
        VPU_ENC_COMP_ERR_LOG("%s, g2d_blit failed\n", __FUNCTION__);
        return 0;
    }
    if (g2d_finish(pGpuHandle->g2dHandle) != 0) {
        VPU_ENC_COMP_ERR_LOG("%s, g2d_finish failed\n", __FUNCTION__);
        return 0;
    }
    if (g2d_close(pGpuHandle->g2dHandle) != 0) {
        VPU_ENC_COMP_ERR_LOG("%s, g2d_close failed\n", __FUNCTION__);
        return 0;
    }

    pGpuHandle->g2dHandle = NULL;

	return 1;
}

OMX_S32 PreProcessDeinit(VpuEncPPHandle preProcessHandle)
{
    VpuEncoderGpuHandle *pGpuHandle = (VpuEncoderGpuHandle*)preProcessHandle;

    free(pGpuHandle);
	TIMER_REPORT;
	return 1;
}
#endif

OMX_S32 PreFrameSet(VpuEncoderPreProcessMemInfo* pPreMem,OMX_U32 nPhy,OMX_U32 nVir,OMX_U32 nSize,OMX_U32 nFlag, OMX_S32* pIndex)
{
	OMX_S32 i;
	for(i=0;i<VPU_ENC_MAX_NUM_MEM;i++){
		if(pPreMem->phyAddr[i]==NULL){
			pPreMem->phyAddr[i]=nPhy;
			pPreMem->virtAddr[i]=nVir;
			pPreMem->size[i]=nSize;
			pPreMem->flag[i]=nFlag;
			*pIndex=i;
			break;
		}
	}
	if(i==VPU_ENC_MAX_NUM_MEM){
		VPU_ENC_COMP_ERR_LOG("frame buffer is full ! \n");
		*pIndex=-1;
		return 0;
	}
	return 1;
}

OMX_S32 PreFrameClear(VpuEncoderPreProcessMemInfo* pPreMem,OMX_U32 nIndex)
{
    if(nIndex >= VPU_ENC_MAX_NUM_MEM)
        return 0;

	pPreMem->phyAddr[nIndex]=NULL;
	pPreMem->virtAddr[nIndex]=NULL;
	pPreMem->size[nIndex]=0;
	pPreMem->flag[nIndex]=0;
	return 1;
}

OMX_S32 PreFrameGetNode(VpuEncoderPreProcessMemInfo* pPreMem,OMX_U32 nIndex,OMX_U32* pPhy, OMX_U32* pVirt,OMX_S32* pSize,OMX_U32* pFlag)
{
    if(nIndex >= VPU_ENC_MAX_NUM_MEM)
        return 0;

	*pPhy=pPreMem->phyAddr[nIndex];
	*pVirt=pPreMem->virtAddr[nIndex];
	*pSize=(OMX_S32)pPreMem->size[nIndex];
	*pFlag=pPreMem->flag[nIndex];
	return 1;
}

OMX_U32 PreFrameGetVirt(VpuEncoderPreProcessMemInfo* pPreMem,OMX_U32 nIndex)
{
    if(nIndex < VPU_ENC_MAX_NUM_MEM)
        return pPreMem->virtAddr[nIndex];
    else
        return 0;
}

OMX_S32 PreFrameValid(VpuEncoderPreProcessMemInfo* pPreMem,OMX_U32 nIndex)
{
	if(pPreMem->phyAddr[nIndex]==NULL){
		return 0;
	}
	else{
		return 1;
	}
}

OMX_S32 PreCreateOutFrame(VpuEncoderMemInfo* pMem,OMX_PARAM_MEM_OPERATOR* pMemOp,
  VpuEncoderPreProcessMemInfo* pPreMem,Queue*pQueue,OMX_S32 nSize,OMX_S32 nNum)
{
	OMX_S32 index;
	OMX_PTR pVirt;
	OMX_PTR pPhy;
	OMX_S32 i;
	for (i=0;i<nNum;i++){
		pVirt=AllocateBuffer(nSize, pMem, pMemOp);
		if(pVirt==NULL){
			VPU_ENC_COMP_ERR_LOG("allocate buffer(%d) failure ! \r\n",nSize);
		}
		GetHwBuffer(pVirt, &pPhy);
		PreFrameSet(pPreMem, (OMX_U32)pPhy, (OMX_U32)pVirt, (OMX_U32)nSize, 0, &index);
		pQueue->Add(&index);
	}
	return 1;
}

OMX_S32 PreTransferQueue(Queue* pFromQueue,Queue* pToQueue,VpuEncoderPreProcessMemInfo* pPreMem)
{
	//get all buffer from pFromQueue and re-filled into pToQueue
	OMX_S32 index;
	OMX_S32 nCnt=0;
	while(pFromQueue->Size()>0){
		pFromQueue->Get(&index);
		if(PreFrameValid(pPreMem,index)){
			pToQueue->Add(&index);
			nCnt++;
		}
	}
	return nCnt;
}

OMX_S32 PreWaitState(volatile PreProcessState * pState,PreProcessState eTargetState, fsl_osal_sem pSem)
{
	//encoder thread wait pre thread complete command
	fsl_osal_sem_wait(pSem);
	while(*pState!=eTargetState)
	{
		VPU_ENC_COMP_ERR_LOG("incorrect state: %d !!!, expected state: %d \r\n", *pState,eTargetState);
	}
	return 1;
}

OMX_S32 PreWakeUp(fsl_osal_mutex pMutex, pthread_cond_t* pCond, volatile OMX_BOOL * pWaiting)
{
	//encode thread notify pre thread some tasks are received
	fsl_osal_mutex_lock(pMutex);
	if(*pWaiting==OMX_TRUE)
	{
		VPU_ENC_COMP_PRE_LOG("%s: send tasks to pre thread \r\n",__FUNCTION__);
		pthread_cond_signal(pCond);
	}
	else
	{
		VPU_ENC_COMP_PRE_LOG("%s: pre thread is already in work state, needn't wakeup it \r\n",__FUNCTION__);
	}
	fsl_osal_mutex_unlock(pMutex);
	return 1;
}


void *PreThread(void *arg)
{
	/*
	for invalid eos: (in such case, buffer is invalid)
		pPreInQueue: 	   	index>=0, phy=virt=size=0;
		pPreInReturnQueue:	index>=0, phy=virt=size=0;
		pPreOutQueue:		index>=0, phy=virt=valid, size=0
		pPreOutReturnQueue:	index>=0, phy=virt=valid, size=0
	*/
#define PRE_THREAD_WAITTIMEOUT_US	(100000)
	VpuEncoder *base = (VpuEncoder*)arg;
	volatile PreProcessCmd * pCmd=&base->ePreCmd;
	volatile PreProcessState* pState=&base->ePreState;
	volatile OMX_BOOL* pPreWaitingTasks =&base->bPreWaitingTasks;
	fsl_osal_sem pSem=base->pPreCmdSem;
	VpuEncoderPreProcessMemInfo * pPreInMem=&base->sEncPreProcessInFrameInfo;
	VpuEncoderPreProcessMemInfo * pPreOutMem=&base->sEncPreProcessOutFrameInfo;
	VPU_ENC_COMP_PRE_LOG("%s: Pre Thread starting.\n",__FUNCTION__);

	//set waiting firstly when pre thread is actived.
	*pPreWaitingTasks=OMX_TRUE;
	//main loop
	while(1){
		OMX_S32 nPreInIndex,nPreOutIndex;
		OMX_U32 nFlag;
		struct timespec to;
		struct timeval from;
		OMX_S32 ret=0;
		//wait encode thread send tasks(commands,data,...)
		fsl_osal_mutex_lock(base->pPreMutex);
		while(1){
			if(*pCmd!=PRE_PROCESS_CMD_NONE){
				break;
			}
			if((base->pPreInQueue->Size()>0) && (base->pPreOutReturnQueue->Size()>0)){
				break;
			}
			VPU_ENC_COMP_PRE_LOG("%s: will wait tasks from encoder thread \r\n",__FUNCTION__);
#if 0		//debug: using sleep method instead of wait
			#define PRE_THREAD_SLEEP_US (1000)
			fsl_osal_mutex_unlock(base->pPreMutex);
			fsl_osal_sleep(PRE_THREAD_SLEEP_US);
			fsl_osal_mutex_lock(base->pPreMutex);
#else
			*pPreWaitingTasks=OMX_TRUE; //no any tasks, begin waiting
			/*
			gettimeofday(&from, NULL);
			to.tv_sec=from.tv_sec;
			to.tv_nsec=from.tv_usec * 1000;
			to.tv_nsec+=(PRE_THREAD_WAITTIMEOUT_US)*1000;
			//here, we use timeout wait to avoid deadlock as possible
			ret=pthread_cond_timedwait(&base->sPreCond, (pthread_mutex_t *)(base->pPreMutex), &to);
			*/
			ret=pthread_cond_wait(&base->sPreCond, (pthread_mutex_t *)base->pPreMutex);
			if(0!=ret) {
				VPU_ENC_COMP_PRE_LOG("return from pthread_cond_*wait : %d \r\n",ret);
			}
#endif
		}
		if(ret!=0){//if(ret==ETIMEDOUT){
			//in theory, if the sync logic is correct, shouldn't enter here
			VPU_ENC_COMP_ERR_LOG("warning: the pre thread isn't actived immediately ! error: %d \r\n",ret);
		}
		VPU_ENC_COMP_PRE_LOG("%s: ready to work \r\n",__FUNCTION__);
		*pPreWaitingTasks=OMX_FALSE;  //clear waiting
		fsl_osal_mutex_unlock(base->pPreMutex);

		//process command
		if(*pCmd!=PRE_PROCESS_CMD_NONE){
			switch(*pCmd){
				case PRE_PROCESS_CMD_RUN:
					*pState=PRE_PROCESS_STATE_RUN;
					break;
				case PRE_PROCESS_CMD_STOP:
					*pState=PRE_PROCESS_STATE_IDLE;
					break;
				case PRE_PROCESS_CMD_FLUSH_OUT:
					//get all buffers from pPreOutReturnQueue(except invalid eos), and fill into pPreOutQueue directly
					PreTransferQueue(base->pPreOutReturnQueue, base->pPreOutQueue,pPreInMem);
					break;
				case PRE_PROCESS_CMD_FLUSH_IN:
					//get all buffers from pPreInQueue(except invalid eos) and insert them into pPreInReturnQueue
					PreTransferQueue(base->pPreInQueue, base->pPreInReturnQueue,pPreInMem);
					break;
				default:
					VPU_ENC_COMP_PRE_LOG("unknow command: %d, ignore it \r\n",*pCmd);
					break;
			}
			VPU_ENC_COMP_PRE_LOG("%s: command(%d) finished, will notify encoder thread \r\n",__FUNCTION__,*pCmd);
			//clear command
			*pCmd=PRE_PROCESS_CMD_NONE;
			//notify host command finished
			fsl_osal_sem_post(pSem);
		}

		//check state
		if((*pState)==PRE_PROCESS_STATE_IDLE){
			break; //exit main loop
		}
		else if((*pState)!=PRE_PROCESS_STATE_RUN){
			continue;
		}

		//check whether data is enough to do pre-process
		if((base->pPreInQueue->Size()<=0) || ((base->pPreOutReturnQueue->Size()<=0))){
			continue;
		}

		base->pPreInQueue->Get(&nPreInIndex);
		base->pPreOutReturnQueue->Get(&nPreOutIndex);

		//check flag and other parameters
		nFlag=pPreInMem->flag[nPreInIndex];
		if((pPreInMem->phyAddr[nPreInIndex]==NULL)&&(nFlag!=PRE_FLAG_EOS)){
			VPU_ENC_COMP_ERR_LOG("find one invalid input buffer \n");
			continue;
		}

		if((pPreInMem->phyAddr[nPreInIndex]==NULL)&&(nFlag==PRE_FLAG_EOS)){
			//invalid eos: skip pre-process
			pPreOutMem->size[nPreOutIndex]=0;
		}
		else{
			//pre-process
#if 0		//debug: copy directly
			{
				OMX_S32 nOutSize;
				nOutSize=base->sInFmt.nFrameWidth * base->sInFmt.nFrameHeight*pxlfmt2bpp(base->sInFmt.eColorFormat)/8;
				fsl_osal_memcpy((OMX_PTR)pPreOutMem->virtAddr[nPreOutIndex],(OMX_PTR)pPreInMem->virtAddr[nPreInIndex], nOutSize);
			}
#else
			TIMER_START;
			PreProcessTask(base->preProcessHandle,(OMX_PTR)pPreInMem->phyAddr[nPreInIndex],(OMX_PTR)pPreOutMem->phyAddr[nPreOutIndex]);
			TIMER_STOP;
#endif
		}
		VPU_ENC_COMP_PRE_LOG("%s: output index: %d, input Indx: %d, flag: %d \r\n",__FUNCTION__,nPreOutIndex,nPreInIndex,nFlag);
		base->pPreInReturnQueue->Add(&nPreInIndex);
		pPreOutMem->flag[nPreOutIndex]=nFlag;
		base->pPreOutQueue->Add(&nPreOutIndex);
	}
	VPU_ENC_COMP_PRE_LOG("%s: Pre Thread Stopped.\n",__FUNCTION__);
	return NULL;
}

VpuEncoder::VpuEncoder()
{
	VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	// version info
	ComponentVersion.s.nVersionMajor = 0x0;
	ComponentVersion.s.nVersionMinor = 0x1;
	ComponentVersion.s.nRevision = 0x0;
	ComponentVersion.s.nStep = 0x0;

	//set default
	fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_encoder.mpeg4.hw-based");

	fsl_osal_memset(&sInFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
	sInFmt.nFrameWidth = DEFAULT_ENC_FRM_WIDTH;
	sInFmt.nFrameHeight = DEFAULT_ENC_FRM_HEIGHT;
	sInFmt.eColorFormat = OMX_COLOR_FormatYUV420Planar;
	sInFmt.xFramerate = DEFAULT_ENC_FRM_RATE;
	sInFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

	nInPortFormatCnt = 3;
	eInPortPormat[0] = OMX_COLOR_FormatYUV420Planar;
	eInPortPormat[1] = OMX_COLOR_FormatYUV420SemiPlanar;
	eInPortPormat[2] = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatAndroidOpaque;
	nOutPortFormatCnt = 0;

	fsl_osal_memset(&sOutFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
	sOutFmt.nFrameWidth = DEFAULT_ENC_FRM_WIDTH;
	sOutFmt.nFrameHeight = DEFAULT_ENC_FRM_HEIGHT;
	sOutFmt.xFramerate = DEFAULT_ENC_FRM_RATE;
	sOutFmt.nBitrate=DEFAULT_ENC_FRM_BITRATE;
#if 0 //1 eagle : for component unit test
	sOutFmt.eColorFormat = OMX_COLOR_FormatRawBayer8bitcompressed;
#else
	sOutFmt.eColorFormat = OMX_COLOR_FormatUnused;
#endif
	sOutFmt.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

	bFilterSupportPartilInput =OMX_TRUE;
	nInBufferCnt = DEFAULT_ENC_BUF_IN_CNT;
	nInBufferSize = DEFAULT_ENC_BUF_IN_SIZE;
	nOutBufferCnt = DEFAULT_ENC_BUF_OUT_CNT;
	nOutBufferSize = DEFAULT_ENC_BUF_OUT_SIZE;

	//clear internal variable 0
	fsl_osal_memset(&sEncInitInfo,0,sizeof(VpuEncInitInfo));
	fsl_osal_memset(&sVpuEncMemInfo,0,sizeof(VpuEncoderMemInfo));
	fsl_osal_memset(&sEncAllocInMemInfo,0,sizeof(VpuEncoderMemInfo));
	fsl_osal_memset(&sEncAllocOutMemInfo,0,sizeof(VpuEncoderMemInfo));
	fsl_osal_memset(&sEncPreProcessInFrameInfo,0,sizeof(VpuEncoderPreProcessMemInfo));
	fsl_osal_memset(&sEncPreProcessOutFrameInfo,0,sizeof(VpuEncoderPreProcessMemInfo));
	fsl_osal_memset(&sEncOutFrameInfo,0,sizeof(VpuEncoderMemInfo));
	fsl_osal_memset(&sMemOperator,0,sizeof(OMX_PARAM_MEM_OPERATOR));
	fsl_osal_memset(&sVpuEncInputPara,0,sizeof(VpuEncInputParam));
    fsl_osal_memset(&sColorDesc, 0, sizeof(VpuColourDesc));

	nHandle=0;

	//eFormat = VPU_V_MPEG4;
	//nPadWidth=DEFAULT_ENC_FRM_WIDTH;
	//nPadHeight=DEFAULT_ENC_FRM_HEIGHT;

	pInBufferPhy=NULL;
	pInBufferVirt=NULL;
	nInSize=0;
	bInEos=OMX_FALSE;
	pOutBufferPhy=NULL;
	nOutSize=0;
	nOutGOPFrameCnt=1;
    bStoreMetaData = OMX_FALSE;

	bEnabledPreProcess=OMX_FALSE;
    preProcessHandle=NULL;
	pPreThreadId=NULL;
	pPreInQueue=NULL;
	pPreOutQueue=NULL;
	pPreInReturnQueue=NULL;
	pPreOutReturnQueue=NULL;
	pPreCmdSem=NULL;
	pPreMutex=NULL;
	pPreReturnedInput=NULL;
	nPreOutIndex=-1;
	bPreEos=OMX_FALSE;
	eVpuEncoderState=VPU_ENC_COM_STATE_NONE;

	SetDefaultEncParam(&sVpuEncInputPara);

	return;
}
OMX_ERRORTYPE VpuEncoder::SetRoleFormat(OMX_STRING role)
{
	OMX_VIDEO_CODINGTYPE CodingType;

	VPU_ENC_COMP_API_LOG("%s: role: %s \r\n",__FUNCTION__,role);

	if(fsl_osal_strcmp(role, "video_encoder.mpeg4") == 0)
	{
		CodingType = OMX_VIDEO_CodingMPEG4;
		sVpuEncInputPara.eFormat=VPU_V_MPEG4;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_ENC_COMP_NAME_MPEG4ENC);
	}
	else if(fsl_osal_strcmp(role, "video_encoder.avc") == 0)
	{
		CodingType = OMX_VIDEO_CodingAVC;
		sVpuEncInputPara.eFormat=VPU_V_AVC;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_ENC_COMP_NAME_AVCENC);
	}
	else if(fsl_osal_strcmp(role, "video_encoder.h263") == 0)
	{
		CodingType = OMX_VIDEO_CodingH263;
		sVpuEncInputPara.eFormat=VPU_V_H263;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_ENC_COMP_NAME_H263ENC);
	}
    else if(fsl_osal_strcmp(role, "video_encoder.vp8") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP8;
		sVpuEncInputPara.eFormat=VPU_V_VP8;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_ENC_COMP_NAME_VP8ENC);
	}
	else
	{
		CodingType = OMX_VIDEO_CodingUnused;
		//eFormat=;
		VPU_ENC_COMP_ERR_LOG("%s: failure: unknow role: %s \r\n",__FUNCTION__,role);
		return OMX_ErrorUndefined;
	}

	//check output format change
	if(sOutFmt.eCompressionFormat!=CodingType)
	{
		sOutFmt.eCompressionFormat=CodingType;
		OutputFmtChanged();
		//FmtChanged(OUT_PORT,&sOutFmt);
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::GetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
	VPU_ENC_COMP_API_LOG("%s: nParamIndex=0x%X, \r\n",__FUNCTION__,nParamIndex);

	//check state
	switch(eVpuEncoderState)
	{
		default:
			break;
	}

	if(NULL==pComponentParameterStructure)
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: param is null \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

	if(nParamIndex==OMX_IndexParamStandardComponentRole)
	{
		fsl_osal_strcpy((OMX_STRING)((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole,(OMX_STRING)cRole);
		return OMX_ErrorNone;
	}
	else if (nParamIndex==OMX_IndexParamVideoBitrate)
	{
		OMX_VIDEO_PARAM_BITRATETYPE * pPara;
		pPara=(OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
		ASSERT(pPara->nPortIndex==IN_PORT);
		//get bit rate
		if(0==sVpuEncInputPara.nBitRate)
		{
			//in this mode the encoder will ignore nTargetBitrate setting
			//and use the appropriate Qp (nQpI, nQpP, nQpB) values for encoding
			pPara->eControlRate=OMX_Video_ControlRateDisable;
			pPara->nTargetBitrate=0;
		}
		else
		{
			pPara->eControlRate=OMX_Video_ControlRateConstant;
			pPara->nTargetBitrate=sVpuEncInputPara.nBitRate;
		}
	}
	else if (nParamIndex==OMX_IndexParamVideoMpeg4)
	{
		OMX_VIDEO_PARAM_MPEG4TYPE* pPara;
		pPara=(OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
		pPara->eProfile=OMX_VIDEO_MPEG4ProfileSimple;
		pPara->eLevel=OMX_VIDEO_MPEG4Level0;
		pPara->nPFrames=sVpuEncInputPara.nGOPSize-1;
		pPara->nBFrames=0;
		//...
	}
	else if (nParamIndex==OMX_IndexParamVideoAvc)
	{
		OMX_VIDEO_PARAM_AVCTYPE* pPara;
		pPara=(OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
		pPara->eProfile=OMX_VIDEO_AVCProfileBaseline;
		pPara->eLevel=OMX_VIDEO_AVCLevel1;
		pPara->nPFrames=sVpuEncInputPara.nGOPSize-1;
		pPara->nBFrames=0;
		//...
	}
	else if (nParamIndex==OMX_IndexParamVideoH263)
	{
		OMX_VIDEO_PARAM_H263TYPE* pPara;
		pPara=(OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
		pPara->eProfile=OMX_VIDEO_H263ProfileBaseline;
		pPara->eLevel=OMX_VIDEO_H263Level10;
		pPara->nPFrames=sVpuEncInputPara.nGOPSize-1;
		pPara->nBFrames=0;
		//...
	}
    else if (nParamIndex == OMX_IndexParamVideoAndroidVp8)
    {
        OMX_VIDEO_PARAM_ANDROID_VP8TYPE * pPara;
        pPara = (OMX_VIDEO_PARAM_ANDROID_VP8TYPE *)pComponentParameterStructure;
        pPara->eProfile = OMX_VIDEO_VP8ProfileMain;
        pPara->eLevel = OMX_VIDEO_VP8Level_Version0;
        pPara->bErrorResilientMode = OMX_TRUE;
        pPara->nDCTPartitions = 0;
    }
    else if (nParamIndex == OMX_IndexParamVideoVp8Encoder)
    {
        OMX_VIDEO_PARAM_VP8ENCODERTYPE* pPara;
        pPara = (OMX_VIDEO_PARAM_VP8ENCODERTYPE*)pComponentParameterStructure;
        //...
    }
       else if(nParamIndex ==  OMX_IndexParamVideoProfileLevelQuerySupported)
       {
               struct CodecProfileLevel {
                       OMX_U32 mProfile;
                       OMX_U32 mLevel;
               };

               static const CodecProfileLevel kH263ProfileLevels[] = {
                       { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level10 },
                       { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level20 },
                       { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level30 },
                       { OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level45 },
                       { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level10 },
                       { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level20 },
                       { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level30 },
                       { OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level45 },
               };

               static const CodecProfileLevel kProfileLevels[] = {
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1  },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1b },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel11 },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel12 },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel13 },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel2  },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel21 },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel22 },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel3  },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel31 },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel32 },
                       { OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel4  },
                       /* For vpu encoder, the maxium bitrate set by driver is 32Mbps */
               };

               static const CodecProfileLevel kM4VProfileLevels[] = {
                       { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0 },
                       { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0b },
                       { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level1 },
                       { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level2 },
                       { OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level3 },
               };

               OMX_VIDEO_PARAM_PROFILELEVELTYPE  *pPara;
               OMX_S32 index;
               OMX_S32 nProfileLevels;

               pPara = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
               switch(sVpuEncInputPara.eFormat)
               {
                       case VPU_V_H263:
                               index = pPara->nProfileIndex;

                               nProfileLevels =
                                       sizeof(kH263ProfileLevels) / sizeof(kH263ProfileLevels[0]);
                               if (index >= nProfileLevels) {
                                       return OMX_ErrorNoMore;
                               }

                               pPara->eProfile = kH263ProfileLevels[index].mProfile;
                               pPara->eLevel = kH263ProfileLevels[index].mLevel;
                               break;
                       case VPU_V_AVC:
                               index = pPara->nProfileIndex;

                               nProfileLevels =
                                       sizeof(kProfileLevels) / sizeof(kProfileLevels[0]);
                               if (index >= nProfileLevels) {
                                       return OMX_ErrorNoMore;
                               }

                               pPara->eProfile = kProfileLevels[index].mProfile;
                               pPara->eLevel = kProfileLevels[index].mLevel;
                               break;
                       case VPU_V_MPEG4:
                               index = pPara->nProfileIndex;

                               nProfileLevels =
                                       sizeof(kM4VProfileLevels) / sizeof(kM4VProfileLevels[0]);
                               if (index >= nProfileLevels) {
                                       return OMX_ErrorNoMore;
                               }

                               pPara->eProfile = kM4VProfileLevels[index].mProfile;
                               pPara->eLevel = kM4VProfileLevels[index].mLevel;
                               break;
                       default:
                               break;
               }
               return OMX_ErrorNone;
       }
	else
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: unsupported index: 0x%X \r\n",__FUNCTION__,nParamIndex);
		return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::SetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	VPU_ENC_COMP_API_LOG("%s: nParamIndex=0x%X \r\n",__FUNCTION__,nParamIndex);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_LOADED:
			break;
		default:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d, nParamIndex=0x%X, role=%s \r\n",__FUNCTION__,eVpuEncoderState,nParamIndex,(OMX_STRING)cRole);
			return OMX_ErrorIncorrectStateTransition;
	}


	if(NULL==pComponentParameterStructure)
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: param is null \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

	if(nParamIndex==OMX_IndexParamStandardComponentRole)
	{
		fsl_osal_strcpy( (fsl_osal_char *)cRole,(fsl_osal_char *)((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole);
		if(OMX_ErrorNone!=SetRoleFormat((OMX_STRING)cRole))
		{
			VPU_ENC_COMP_ERR_LOG("%s: set role format failure \r\n",__FUNCTION__);
			return OMX_ErrorUndefined;
		}
	}
	else if (nParamIndex==OMX_IndexParamMemOperator)
	{
		//should be set before open vpu(eg. malloc bitstream/frame buffers)
		if(VPU_ENC_COM_STATE_LOADED!=eVpuEncoderState)
		{
			ret=OMX_ErrorInvalidState;
		}
		else
		{
			OMX_PARAM_MEM_OPERATOR * pPara;
			pPara=(OMX_PARAM_MEM_OPERATOR *)pComponentParameterStructure;
			VPU_ENC_COMP_LOG("%s: set OMX_IndexParamMemOperator \r\n",__FUNCTION__);
			sMemOperator=*pPara;
		}
	}
    else if (nParamIndex == OMX_IndexParamStoreMetaDataInBuffers)
    {
        OMX_CONFIG_BOOLEANTYPE *pParams = (OMX_CONFIG_BOOLEANTYPE*)pComponentParameterStructure;
        bStoreMetaData = pParams->bEnabled;
    }
#if 1 //TODO : add related setting, including resolution/framerate/bitrate/... ???
 	else if (nParamIndex==OMX_IndexParamVideoPortFormat)
	{
			OMX_VIDEO_PARAM_PORTFORMATTYPE * pPara;
			pPara=(OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			//set port

			if((sInFmt.eCompressionFormat!=pPara->eCompressionFormat)
				||(sInFmt.eColorFormat!=pPara->eColorFormat))
			{
				sInFmt.eCompressionFormat=pPara->eCompressionFormat;
				sInFmt.eColorFormat=pPara->eColorFormat;
				sVpuEncInputPara.nFrameRate=pPara->xFramerate/Q16_SHIFT;
				//FmtChanged(IN_PORT, &sInFmt);
				InputFmtChanged();
			}
	}
	else if (nParamIndex==OMX_IndexParamVideoAvc)
	{
			OMX_VIDEO_PARAM_AVCTYPE * pPara;
			pPara=(OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			//set AVC GOP size
			sVpuEncInputPara.nGOPSize=pPara->nPFrames+pPara->nBFrames+1;
	}
	else if (nParamIndex==OMX_IndexParamVideoH263)
	{
			OMX_VIDEO_PARAM_H263TYPE * pPara;
			pPara=(OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			//set H263 GOP size
			sVpuEncInputPara.nGOPSize=pPara->nPFrames+pPara->nBFrames+1;
	}
	else if (nParamIndex==OMX_IndexParamVideoMpeg4)
	{
			OMX_VIDEO_PARAM_MPEG4TYPE * pPara;
			pPara=(OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			//set MPEG4 GOP size
			sVpuEncInputPara.nGOPSize=pPara->nPFrames+pPara->nBFrames+1;
	}
    else if (nParamIndex == OMX_IndexParamVideoAndroidVp8)
    {
        OMX_VIDEO_PARAM_ANDROID_VP8TYPE * pPara;
        pPara = (OMX_VIDEO_PARAM_ANDROID_VP8TYPE *)pComponentParameterStructure;
        ASSERT(pPara->nPortIndex == IN_PORT);
    }
    else if (nParamIndex == OMX_IndexParamVideoVp8Encoder)
    {
        OMX_VIDEO_PARAM_VP8ENCODERTYPE* pPara;
        pPara = (OMX_VIDEO_PARAM_VP8ENCODERTYPE*)pComponentParameterStructure;
        ASSERT(pPara->nPortIndex == OUT_PORT);
        //set VP8 GOP size
        if (pPara->nKeyFrameInterval > 0)
            sVpuEncInputPara.nGOPSize = pPara->nKeyFrameInterval;
    }
	else if(nParamIndex==OMX_IndexParamCommonSensorMode)
	{
			OMX_PARAM_SENSORMODETYPE * pPara;
			pPara=(OMX_PARAM_SENSORMODETYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			ASSERT(pPara->sFrameSize.nPortIndex==IN_PORT);
			ASSERT(pPara->bOneShot==1);
			//set frame rate
			if(0!=pPara->nFrameRate)
			{
				sVpuEncInputPara.nFrameRate=pPara->nFrameRate/Q16_SHIFT;
			}
			//set input width/height
			sVpuEncInputPara.nPicWidth=pPara->sFrameSize.nWidth;
			sVpuEncInputPara.nPicHeight=pPara->sFrameSize.nHeight;
			sVpuEncInputPara.nWidthStride=pPara->sFrameSize.nWidth;
			sVpuEncInputPara.nHeightStride=pPara->sFrameSize.nHeight;
			//need not call buffer change since user always allocate the buffer with correct size
			//BufferChanged(IN_PORT, &sInFmt, nInBufferCnt);
	}
	else if (nParamIndex==OMX_IndexParamVideoIntraRefresh)
	{
			OMX_VIDEO_PARAM_INTRAREFRESHTYPE * pPara;
			pPara=(OMX_VIDEO_PARAM_INTRAREFRESHTYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			VPU_ENC_COMP_LOG("%s: eRefreshMode: %d, nAirMBs: %d, nAirRef: %d, nCirMBs: %d \r\n",__FUNCTION__,pPara->eRefreshMode,pPara->nAirMBs,pPara->nAirRef,pPara->nCirMBs);
			switch(pPara->eRefreshMode){
				case OMX_VIDEO_IntraRefreshCyclic: /*it is used by wifi display*/
					//in fact, vpu don't support this mode, we just map it to normal mode
					sVpuEncInputPara.nIntraFreshNum=pPara->nCirMBs;
					break;
				case OMX_VIDEO_IntraRefreshAdaptive:
					sVpuEncInputPara.nIntraFreshNum=pPara->nAirMBs;
					break;
				case OMX_VIDEO_IntraRefreshBoth:
				case OMX_VIDEO_IntraRefreshMax:
				default:
					//do nothing, ignore it
					VPU_ENC_COMP_ERR_LOG("unsupport refresh mode: %d \r\n",pPara->eRefreshMode);
					break;
			}
	}
	else if (nParamIndex==OMX_IndexParamUseAndroidPrependSPSPPStoIDRFrames){
		OMX_PARAM_PREPEND_SPSPPS_TO_IDR * pPara;
		pPara=(OMX_PARAM_PREPEND_SPSPPS_TO_IDR*)pComponentParameterStructure;
		ASSERT(pPara->nPortIndex==IN_PORT);
		VPU_ENC_COMP_LOG("%s: bEnableSPSToIDR: %d \r\n",__FUNCTION__,pPara->bEnableSPSToIDR);
		sVpuEncInputPara.bEnabledSPSIDR=pPara->bEnableSPSToIDR;
	}
	else if (nParamIndex==OMX_IndexParamVideoBitrate)
	{
			OMX_VIDEO_PARAM_BITRATETYPE * pPara;
			pPara=(OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			//set bit rate
			switch (pPara->eControlRate)
			{
				case OMX_Video_ControlRateDisable:
					//in this mode the encoder will ignore nTargetBitrate setting
					//and use the appropriate Qp (nQpI, nQpP, nQpB) values for encoding
					break;
				case OMX_Video_ControlRateVariable:
					sVpuEncInputPara.nBitRate=pPara->nTargetBitrate;
					//Variable bit rate
					break;
				case OMX_Video_ControlRateConstant:
					//the encoder can modify the Qp values to meet the nTargetBitrate target
					sVpuEncInputPara.nBitRate=pPara->nTargetBitrate;
					break;
				case OMX_Video_ControlRateVariableSkipFrames:
					//Variable bit rate with frame skipping
					sVpuEncInputPara.nEnableAutoSkip=1;
					break;
				case OMX_Video_ControlRateConstantSkipFrames:
					//Constant bit rate with frame skipping
					//the encoder cannot modify the Qp values to meet the nTargetBitrate target.
					//Instead, the encoder can drop frames to achieve nTargetBitrate
					sVpuEncInputPara.nEnableAutoSkip=1;
					break;
				case OMX_Video_ControlRateMax:
					//Maximum value
					if(sVpuEncInputPara.nBitRate>(OMX_S32)pPara->nTargetBitrate)
					{
						sVpuEncInputPara.nBitRate=pPara->nTargetBitrate;
					}
					break;
				default:
					//unknown
					ret = OMX_ErrorUnsupportedIndex;
					break;
			}
            // workaround for cts MediaCodecCapabilitiesTest#testGetMaxSupportedInstances
            // increase nOutBufferSize for some vp8 video if its bitrate exceed 20Mbps
            if (sVpuEncInputPara.nBitRate > VPU_ENC_VP8_BITRATE_THRESHOLD && sOutFmt.eCompressionFormat == OMX_VIDEO_CodingVP8) {
                if (nOutBufferSize < (DEFAULT_ENC_BUF_OUT_SIZE << 2)) {
                    nOutBufferSize = (DEFAULT_ENC_BUF_OUT_SIZE << 2);
                    OutputFmtChanged();
                }
            }

	}
#endif
	else
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: unsupported index: 0x%X \r\n",__FUNCTION__,nParamIndex);
		ret = OMX_ErrorUnsupportedIndex;
	}

	return ret;
}

OMX_ERRORTYPE VpuEncoder::GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
	VPU_ENC_COMP_API_LOG("%s: nParamIndex=0x%X, state %d\r\n",__FUNCTION__,nParamIndex, eVpuEncoderState);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
		//case VPU_ENC_COM_STATE_LOADED:
		//case VPU_ENC_COM_STATE_OPENED:	//allow user get wrong value before opened ???
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	if(NULL==pComponentParameterStructure)
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: param is null  \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}
/*
	if(nParamIndex == OMX_IndexConfigCommonOutputCrop)
	{
		OMX_CONFIG_RECTTYPE *pRecConf = (OMX_CONFIG_RECTTYPE*)pComponentParameterStructure;
		if(pRecConf->nPortIndex == VPUDEC_OUT_PORT)
		{
			pRecConf->nTop = sOutCrop.nTop;
			pRecConf->nLeft = sOutCrop.nLeft;
			pRecConf->nWidth = sOutCrop.nWidth;
			pRecConf->nHeight = sOutCrop.nHeight;
			VPU_ENC_COMP_LOG("%s: [top,left,width,height]=[%d,%d,%d,%d], \r\n",__FUNCTION__,(INT32)pRecConf->nTop,(INT32)pRecConf->nLeft,(INT32)pRecConf->nWidth,(INT32)pRecConf->nHeight);
		}
		return OMX_ErrorNone;
	}
	if (nParamIndex == OMX_IndexConfigCommonScale)
	{
		OMX_CONFIG_SCALEFACTORTYPE *pDispRatio = (OMX_CONFIG_SCALEFACTORTYPE *)pComponentParameterStructure;
		if(pDispRatio->nPortIndex == VPUDEC_OUT_PORT)
		{
			pDispRatio->xWidth = sDispRatio.xWidth;
			pDispRatio->xHeight = sDispRatio.xHeight;
		}
		return OMX_ErrorNone;
	}
*/
    if (nParamIndex == OMX_IndexConfigIntraRefresh) {
        OMX_VIDEO_CONFIG_INTRAREFRESHTYPE *pIntraRefresh = (OMX_VIDEO_CONFIG_INTRAREFRESHTYPE*)pComponentParameterStructure;
        if (pIntraRefresh->nPortIndex == OUT_PORT) {
            pIntraRefresh->nRefreshPeriod = sVpuEncInputPara.nIntraFreshNum;
        }
        return OMX_ErrorNone;
    } else {
		VPU_ENC_COMP_ERR_LOG("%s: failure: unsupported index: 0x%X \r\n",__FUNCTION__,nParamIndex);
		return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
	VPU_ENC_COMP_API_LOG("%s: nParamIndex=0x%X, \r\n",__FUNCTION__,nParamIndex);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
		//case VPU_ENC_COM_STATE_LOADED:
		//case VPU_ENC_COM_STATE_OPENED:	//allow user get wrong value before opened ???
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	if(NULL==pComponentParameterStructure)
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: param is null  \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

#if 1 //TODO : add related setting, including resolution/framerate/bitrate/... ???
	if (nParamIndex==OMX_IndexConfigVideoFramerate)
	{
			OMX_CONFIG_FRAMERATETYPE * pPara;
			pPara=(OMX_CONFIG_FRAMERATETYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			//set frame rate
			sVpuEncInputPara.nFrameRate=pPara->xEncodeFramerate/Q16_SHIFT;
	}
	else if (nParamIndex==OMX_IndexConfigVideoBitrate)
	{
			OMX_VIDEO_CONFIG_BITRATETYPE * pPara;
			VpuEncRetCode ret;
			OMX_S32 kbps;
			pPara=(OMX_VIDEO_CONFIG_BITRATETYPE *)pComponentParameterStructure;
			//set bit rate
			ASSERT(pPara->nPortIndex==IN_PORT);
			sVpuEncInputPara.nBitRate=pPara->nEncodeBitrate;
			kbps=sVpuEncInputPara.nBitRate/1000;
			ret=VPU_EncConfig(nHandle, VPU_ENC_CONF_BIT_RATE, &kbps);
			if(VPU_ENC_RET_SUCCESS!=ret){
				VPU_ENC_COMP_ERR_LOG("%s: vpu config failure: config=0x%X, ret=%d \r\n",__FUNCTION__,(UINT32)VPU_ENC_CONF_BIT_RATE,ret);
				return OMX_ErrorNotImplemented;
			}
	}
	else if (nParamIndex==OMX_IndexConfigVideoAVCIntraPeriod)
	{
			OMX_VIDEO_CONFIG_AVCINTRAPERIOD * pPara;
			pPara=(OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			//set GOP/IDRPeriod
			sVpuEncInputPara.nGOPSize=pPara->nPFrames+1;	//???
			sVpuEncInputPara.nIDRPeriod=pPara->nIDRPeriod*(pPara->nPFrames+1);	//???
	}
	else if (nParamIndex==OMX_IndexConfigVideoIntraVOPRefresh)
	{
			OMX_CONFIG_INTRAREFRESHVOPTYPE * pPara;
			pPara=(OMX_CONFIG_INTRAREFRESHVOPTYPE *)pComponentParameterStructure;
			//set IDR refresh manually
			ASSERT(pPara->nPortIndex==IN_PORT);
			sVpuEncInputPara.nRefreshIntra=1;
	}
	else if(nParamIndex==OMX_IndexConfigCommonRotate)
	{
			OMX_CONFIG_ROTATIONTYPE * pPara;
			pPara=(OMX_CONFIG_ROTATIONTYPE *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			//set rotation
			sVpuEncInputPara.nRotAngle=pPara->nRotation;	//0, 90, 180 and 270
			if (pPara->nRotation == 90)
				sVpuEncInputPara.nRotAngle=270;
			if (pPara->nRotation == 270)
				sVpuEncInputPara.nRotAngle=90;
			fsl_osal_memcpy(&Rotation, pPara, sizeof(OMX_CONFIG_ROTATIONTYPE));
	}
	else if(nParamIndex==OMX_IndexConfigCommonInputCrop)
	{
			OMX_CONFIG_RECTTYPE * pPara;
			pPara=(OMX_CONFIG_RECTTYPE *)pComponentParameterStructure;
			//set crop info
			ASSERT(pPara->nPortIndex==IN_PORT);
			return OMX_ErrorUnsupportedIndex;
	}
	else if(nParamIndex==OMX_IndexConfigGrallocBufferParameter)
	{
			GRALLOC_BUFFER_PARAMETER * pPara;
			pPara=(GRALLOC_BUFFER_PARAMETER *)pComponentParameterStructure;
			ASSERT(pPara->nPortIndex==IN_PORT);
			if(sInFmt.eColorFormat!=pPara->eColorFormat){
				sInFmt.eColorFormat=pPara->eColorFormat;
				InputFmtChanged();
			}
	}
#endif
    else if (nParamIndex == OMX_IndexConfigIntraRefresh)
    {
        OMX_VIDEO_CONFIG_INTRAREFRESHTYPE *pPara;
        pPara = (OMX_VIDEO_CONFIG_INTRAREFRESHTYPE*)pComponentParameterStructure;
        ASSERT(pPara->nPortIndex == IN_PORT);
        sVpuEncInputPara.nIntraFreshNum = pPara->nRefreshPeriod;
        return OMX_ErrorNone;
    }
	else
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: unsupported index: 0x%X \r\n",__FUNCTION__,nParamIndex);
		return OMX_ErrorUnsupportedIndex;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::InitFilterComponent()
{
	VpuEncRetCode ret;
	VpuVersionInfo ver;

	VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
			break;
		default:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;

	}

	//load vpu
	ret=VPU_EncLoad();
	if (ret!=VPU_ENC_RET_SUCCESS)
	{
		VPU_ENC_COMP_ERR_LOG("%s: vpu load failure: ret=0x%X \r\n",__FUNCTION__,ret);
		return OMX_ErrorHardware;
	}

	//version info
	ret=VPU_EncGetVersionInfo(&ver);
	if (ret!=VPU_ENC_RET_SUCCESS)
	{
		VPU_ENC_COMP_ERR_LOG("%s: vpu get version failure: ret=0x%X \r\n",__FUNCTION__,ret);
		VPU_EncUnLoad();
		return OMX_ErrorHardware;
	}
	VPU_ENC_COMP_LOG("vpu lib version : rel.major.minor=%d.%d.%d \r\n",ver.nLibRelease,ver.nLibMajor,ver.nLibMinor);
	VPU_ENC_COMP_LOG("vpu fw version : rel.major.minor=%d.%d.%d \r\n",ver.nFwRelease,ver.nFwMajor,ver.nFwMinor);

	//update state
	eVpuEncoderState=VPU_ENC_COM_STATE_LOADED;
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::DeInitFilterComponent()
{
	VpuEncRetCode ret;
	OMX_ERRORTYPE omx_ret=OMX_ErrorNone;
	VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	//close vpu
	ret=VPU_EncClose(nHandle);
	if (ret!=VPU_ENC_RET_SUCCESS)
	{
		VPU_ENC_COMP_ERR_LOG("%s: vpu close failure: ret=0x%X \r\n",__FUNCTION__,ret);
		omx_ret=OMX_ErrorHardware;
	}

	//stop/release pre-process thread
	if(pPreThreadId != NULL) {
		PreThreadStop();
		fsl_osal_thread_destroy(pPreThreadId);
	}
	if(pPreInQueue){
		pPreInQueue->Free();
		FSL_DELETE(pPreInQueue);
	}
	if(pPreOutQueue){
		pPreOutQueue->Free();
		FSL_DELETE(pPreOutQueue);
	}
	if(pPreInReturnQueue){
		pPreInReturnQueue->Free();
		FSL_DELETE(pPreInReturnQueue);
	}
	if(pPreOutReturnQueue){
		pPreOutReturnQueue->Free();
		FSL_DELETE(pPreOutReturnQueue);
	}
	if(pPreCmdSem){
		fsl_osal_sem_destroy(pPreCmdSem);
	}
	if(pPreMutex!= NULL){
		fsl_osal_mutex_destroy(pPreMutex);
		//fsl_osal_cond_destroy(sPreCond);
		pthread_cond_destroy(&sPreCond);
	}
	if(bEnabledPreProcess)	{
		PreProcessDeinit(preProcessHandle);
	}

	// release mem
	if(0==MemFreeBlock(&sVpuEncMemInfo,&sMemOperator,OMX_FALSE))
	{
		VPU_ENC_COMP_ERR_LOG("%s: free memory failure !  \r\n",__FUNCTION__);
		omx_ret=OMX_ErrorHardware;
	}

	//avoid memory leak !!!(user miss releasing them)
	if(0==MemFreeBlock(&sEncAllocInMemInfo,&sMemOperator,OMX_FALSE))
	{
		VPU_ENC_COMP_ERR_LOG("%s: free memory failure !!!  \r\n",__FUNCTION__);
		omx_ret=OMX_ErrorHardware;
	}
	if(0==MemFreeBlock(&sEncAllocOutMemInfo,&sMemOperator,OMX_TRUE))
	{
		VPU_ENC_COMP_ERR_LOG("%s: free memory failure !!!  \r\n",__FUNCTION__);
		omx_ret=OMX_ErrorHardware;
	}

	//clear handle
	nHandle=0;

	//unload
	ret=VPU_EncUnLoad();
	if (ret!=VPU_ENC_RET_SUCCESS)
	{
		VPU_ENC_COMP_ERR_LOG("%s: vpu unload failure: ret=0x%X \r\n",__FUNCTION__,ret);
		omx_ret=OMX_ErrorHardware;
	}

	//update state
	eVpuEncoderState=VPU_ENC_COM_STATE_NONE;

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::SetInputBuffer(OMX_PTR pBufferVirt, OMX_S32 nSize, OMX_BOOL bLast)
{
	VPU_ENC_COMP_API_LOG("%s: state: %d, bufvirt: 0x%X, size: %d, last: %d \r\n",__FUNCTION__,eVpuEncoderState,(UINT32)pBufferVirt,(UINT32)nSize,(UINT32)bLast);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
		//case VPU_ENC_COM_STATE_EOS:
			//if user want to repeat play, user should call the last getoutput (to make state change from eos to decode)
			//forbidden
		//	VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
		//	return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

    if (bStoreMetaData == OMX_TRUE) {
        pInBufferPhy = ((METADATA_BUFFER *)pBufferVirt)->pPhysicAddress;
    } else {
        if(OMX_ErrorNone!=GetHwBuffer(pBufferVirt,&pInBufferPhy))
        {
            VPU_ENC_COMP_ERR_LOG("%s: unvalid virtual buffer: 0x%X \r\n",__FUNCTION__,(UINT32)pBufferVirt);
                return OMX_ErrorBadParameter;
        }
    }
	pInBufferVirt=pBufferVirt;
	ASSERT((OMX_U32)nSize==(sInFmt.nFrameWidth*sInFmt.nFrameHeight*3/2));
	nInSize=nSize;
	bInEos=bLast;

	//check data length, we don't allow zero-length-buf
	if(0>=nInSize)
	{
		pInBufferPhy=NULL;
		pInBufferVirt=NULL;
	}
	else{

	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::SetOutputBuffer(OMX_PTR pBuffer)
{
	OMX_S32 nPhyFrameNum;
	OMX_S32 nIsPhy=0;

	VPU_ENC_COMP_API_LOG("%s: state: %d, buffer: 0x%X \r\n",__FUNCTION__,eVpuEncoderState,(UINT32)pBuffer);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
		//case VPU_ENC_COM_STATE_EOS:
			//if user want to repeat play, user should call the last getoutput (to make state change from eos to decode)
			//forbidden
		//	VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
		//	return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	if(0==OutFrameBufExist(pBuffer,&sEncOutFrameInfo,&nIsPhy))
	{
		//register output frame buffer
		nPhyFrameNum=OutFrameBufRegister(pBuffer,&sEncOutFrameInfo,&nIsPhy);
		if(-1==nPhyFrameNum)
		{
			VPU_ENC_COMP_ERR_LOG("%s: failure: unvalid buffer ! \r\n",__FUNCTION__);
			//sFrameMemInfo is full
			return OMX_ErrorInsufficientResources;
		}
	}
	else
	{
		if(nIsPhy)
		{
			//set repeated !!!
			VPU_ENC_COMP_ERR_LOG("%s: failure: output buffer is set repeatedly ! \r\n",__FUNCTION__);
			return OMX_ErrorIncorrectStateOperation;
		}
		else
		{
			//clear frame node ...
			//...
		}
	}

	if(0==nIsPhy)
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: virtual address is not supported ! \r\n",__FUNCTION__);
		//TODO: Now, virtual address is not supported !!!!!, it will introduce more complex logic.
		return OMX_ErrorUnsupportedSetting;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::InitFilter()
{
	VpuEncRetCode ret;
	VpuFrameBuffer frameBuf[ENC_MAX_FRAME_NUM];
	OMX_S32 BufNum;
	OMX_S32 nSrcStride;

	VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_DO_INIT:
			break;
		default:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
	}

    if (sEncInitInfo.eType == VPU_TYPE_CHIPSMEDIA) {
        //set mini buffer cnt
        BufNum=sEncInitInfo.nMinFrameBufferCount;//+ENC_FRAME_SURPLUS;
        ASSERT(BufNum==sEncInitInfo.nMinFrameBufferCount);

        //fill frameBuf[]
        BufNum=OutFrameBufCreateRegisterFrame(frameBuf, BufNum,sVpuEncInputPara.nWidthStride, sVpuEncInputPara.nHeightStride, &sVpuEncMemInfo,&sMemOperator,sVpuEncInputPara.nRotAngle,&nSrcStride,sEncInitInfo.nAddressAlignment);
        if(-1==BufNum)
        {
            VPU_ENC_COMP_ERR_LOG("%s: create register frame failure \r\n",__FUNCTION__);
            return OMX_ErrorInsufficientResources;
        }

        //register frame buffs
        ret=VPU_EncRegisterFrameBuffer(nHandle, frameBuf, BufNum,nSrcStride);
        if(VPU_ENC_RET_SUCCESS!=ret)
        {
            VPU_ENC_COMP_ERR_LOG("%s: vpu register frame failure: ret=0x%X \r\n",__FUNCTION__,ret);
            return OMX_ErrorHardware;
        }
    }

	//update state
	eVpuEncoderState=VPU_ENC_COM_STATE_DO_DEC;

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::DeInitFilter()
{
	OMX_ERRORTYPE omx_ret=OMX_ErrorNone;
	VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
		case VPU_ENC_COM_STATE_LOADED:
		case VPU_ENC_COM_STATE_OPENED:
		//case VPU_ENC_COM_STATE_WAIT_FRM:
		case VPU_ENC_COM_STATE_DO_INIT:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	//update state
	//eVpuEncoderState=VPU_ENC_COM_STATE_OPENED; //???

	return omx_ret;
}

FilterBufRetCode VpuEncoder::FilterOneBuffer()
{
	VpuEncRetCode ret;
	FilterBufRetCode bufRet=FILTER_OK;
	OMX_S32 index;
	OMX_U32 nOutPhy;
	OMX_U32 nOutVirt;
	OMX_U32 nOutLength;
	VpuEncEncParam sEncEncParam;
	OMX_S32 nValidInput=1;
	OMX_U32 nInputPhy,nInputVirt;
	OMX_S32 nInputSize;
	OMX_COLOR_FORMATTYPE ePreColorFormat=OMX_COLOR_FormatYUV420Planar;

	VPU_ENC_COMP_API_LOG("%s:, state: %d \r\n",__FUNCTION__,eVpuEncoderState);

//RepeatPlay:
	//check state
	switch(eVpuEncoderState)
	{
		//forbidden state
		case VPU_ENC_COM_STATE_NONE:
		case VPU_ENC_COM_STATE_DO_INIT:
		case VPU_ENC_COM_STATE_DO_OUT:
		//case VPU_ENC_COM_STATE_EOS:
			bufRet=FILTER_ERROR;
			VPU_ENC_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return bufRet;
		//allowed state
		//case VPU_ENC_COM_STATE_WAIT_FRM:
		//	break;
		case VPU_ENC_COM_STATE_LOADED:
			if(((NULL==pInBufferPhy)||(0>=nInSize))
			    && (bInEos == OMX_FALSE)){
			    return FILTER_NO_INPUT_BUFFER;
			}
			PreProcessetSetStrategy(sInFmt.eColorFormat,&bEnabledPreProcess);
			if(bEnabledPreProcess){
				OMX_S32 nPreOutSize;
				if(PreProcessInit(&preProcessHandle)==0){
					VPU_ENC_COMP_ERR_LOG("init pre-process failure ! \r\n");
					bEnabledPreProcess=OMX_FALSE; //avoid PreProcessDeinit() is called in DeInitFilterComponent()
					return FILTER_ERROR;
				}
				else	{
					//set ipu task in/out parameter
					ePreColorFormat=OMX_COLOR_FormatYUV420SemiPlanar; //enable NV12
#ifndef MX6X
                    ePreColorFormat = OMX_COLOR_FormatYCbYCr;
#elif defined PRE_PROCESS_BY_PASS_TEST
					ePreColorFormat=OMX_COLOR_FormatYUV420Planar;
#endif
					PreProcessSetDefault(preProcessHandle,&sInFmt,ePreColorFormat);

					//create pre-process thread
					pPreInQueue = FSL_NEW(Queue, ());
					if(pPreInQueue == NULL) {
						VPU_ENC_COMP_ERR_LOG("New pre queue failed.\n");
					}
					if(pPreInQueue->Create(PRE_MAX_QUEUE_SIZE, sizeof(OMX_BUFFERHEADERTYPE*), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
						VPU_ENC_COMP_ERR_LOG("Init pre queue failed.\n");
					}
					pPreOutQueue = FSL_NEW(Queue, ());
					if(pPreOutQueue == NULL) {
						VPU_ENC_COMP_ERR_LOG("New pre queue failed.\n");
					}
					if(pPreOutQueue->Create(PRE_MAX_QUEUE_SIZE, sizeof(OMX_BUFFERHEADERTYPE*), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
						VPU_ENC_COMP_ERR_LOG("Init pre queue failed.\n");
					}
					pPreInReturnQueue = FSL_NEW(Queue, ());
					if(pPreInReturnQueue == NULL) {
						VPU_ENC_COMP_ERR_LOG("New pre queue failed.\n");
					}
					if(pPreInReturnQueue->Create(PRE_MAX_QUEUE_SIZE, sizeof(OMX_BUFFERHEADERTYPE*), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
						VPU_ENC_COMP_ERR_LOG("Init pre queue failed.\n");
					}
					pPreOutReturnQueue = FSL_NEW(Queue, ());
					if(pPreOutReturnQueue == NULL) {
						VPU_ENC_COMP_ERR_LOG("New pre queue failed.\n");
					}
					if(pPreOutReturnQueue->Create(PRE_MAX_QUEUE_SIZE, sizeof(OMX_BUFFERHEADERTYPE*), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
						VPU_ENC_COMP_ERR_LOG("Init pre queue failed.\n");
					}
					if(E_FSL_OSAL_SUCCESS != fsl_osal_sem_init(&pPreCmdSem, 0, 0)){
						VPU_ENC_COMP_ERR_LOG("Create pPreCmdSem Semphore failed.\n");
					}
					if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&pPreMutex, fsl_osal_mutex_normal)) {
						VPU_ENC_COMP_ERR_LOG("Create mutext for pre process failed.\n");
					}
					//if(E_FSL_OSAL_SUCCESS != fsl_osal_cond_create(&sPreCond)) {
					if(0!= pthread_cond_init(&sPreCond, NULL)) {
						VPU_ENC_COMP_ERR_LOG("Create condition variable for pre process failed.\n");
					}
					ePreState=PRE_PROCESS_STATE_NONE;
					ePreCmd=PRE_PROCESS_CMD_NONE;
					bPreWaitingTasks=OMX_TRUE;
					if(E_FSL_OSAL_SUCCESS != fsl_osal_thread_create(&pPreThreadId, NULL, PreThread, this)) {
						VPU_ENC_COMP_ERR_LOG("Create pre-process thread failed.\n");
						//PreProcessDeinit(&preProcessHandle); //will be called in DeInitFilterComponent()
						return FILTER_ERROR;
					}
					nPreOutSize=sInFmt.nFrameWidth * sInFmt.nFrameHeight*pxlfmt2bpp(ePreColorFormat)/8;
					PreCreateOutFrame(&sEncAllocOutMemInfo, &sMemOperator, &sEncPreProcessOutFrameInfo, pPreOutReturnQueue, nPreOutSize, PRE_OUT_BUF_NUM);
					PreThreadRun();
					bThreadedPreProcess = OMX_TRUE;
				}
			}

			//1 we set width/height/stride here ???
			sVpuEncInputPara.nPicWidth=sInFmt.nFrameWidth;
			sVpuEncInputPara.nPicHeight=sInFmt.nFrameHeight;
			sVpuEncInputPara.nWidthStride=sInFmt.nFrameWidth;
			sVpuEncInputPara.nHeightStride=sInFmt.nFrameHeight;
			sVpuEncInputPara.nFrameRate=sInFmt.xFramerate/Q16_SHIFT;
			if ((sVpuEncInputPara.nWidthStride%16!=0)||(sVpuEncInputPara.nHeightStride%16!=0))
			{
				VPU_ENC_COMP_ERR_LOG("%s: warning: stride is not 16 aligned \r\n",__FUNCTION__);
				//return FILTER_ERROR;
			}
#if 0		//1 set nBitRate through OMX_IndexParamVideoBitrate ???
			sVpuEncInputPara.nBitRate=sOutFmt.nBitrate; //sOutFmt.nBitrate is not updated ??
#endif
            sVpuEncInputPara.eColorFormat = sInFmt.eColorFormat;
			if(bEnabledPreProcess){
                sVpuEncInputPara.eColorFormat = ePreColorFormat;
				if(ePreColorFormat==OMX_COLOR_FormatYUV420SemiPlanar){
					sVpuEncInputPara.nChromaInterleave=1;
				}
#ifdef VPU_ENC_WORKAROUND_WIFI_INTRA_QUALITY
				sVpuEncInputPara.nRcIntraQP=15;
#endif
			}
			else if(sInFmt.eColorFormat==OMX_COLOR_FormatYUV420SemiPlanar)
			{
				sVpuEncInputPara.nChromaInterleave=1;
			}
			ret=VPUCom_Init(&sVpuEncInputPara, &sVpuEncMemInfo,&sMemOperator, &nHandle, &sEncInitInfo);
			if (ret!=VPU_ENC_RET_SUCCESS)
			{
				VPU_ENC_COMP_ERR_LOG("%s: vpu init failure: ret=0x%X \r\n",__FUNCTION__,ret);
				//PreProcessDeinit(&preProcessHandle); //will be called in DeInitFilterComponent()
				return FILTER_ERROR;
			}

			//update state
			eVpuEncoderState=VPU_ENC_COM_STATE_DO_INIT;
			return FILTER_DO_INIT;
		//case VPU_ENC_COM_STATE_OPENED:
		case VPU_ENC_COM_STATE_DO_DEC:
			break;
		//case VPU_ENC_COM_STATE_RE_WAIT_FRM:
		//	break;
		//unknow state
		default:
			VPU_ENC_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return FILTER_ERROR;
	}

	if((NULL==pInBufferPhy)||(0>=nInSize)){
		nValidInput=0;
		nInputPhy=0;
		nInputVirt=0;
		nInputSize=0;
	}
	else{
		nInputPhy=(OMX_U32)pInBufferPhy;
		nInputVirt=(OMX_U32)pInBufferVirt;
		nInputSize=nInSize;
	}

	if(bEnabledPreProcess){
		OMX_U32 flag=0;
		if(bInEos==OMX_TRUE){
			flag|=PRE_FLAG_EOS;
			bPreEos=OMX_TRUE;
			bInEos=OMX_FALSE;	//we will re-enalbe eos when receive eos_flag from pre-process output
			if(0==nValidInput){
				VPU_ENC_COMP_LOG("warning: the last eos input is null \r\n");
			}
		}
		//check input queue
		if(nValidInput ||(flag&PRE_FLAG_EOS) ){
			PreFrameSet(&sEncPreProcessInFrameInfo, nInputPhy, nInputVirt,nInputSize,flag, &index);
			pPreInQueue->Add(&index);
			bufRet=(FilterBufRetCode)(bufRet|FILTER_INPUT_CONSUMED_EXT_READ);
			PreWakeUp(pPreMutex, &sPreCond, (volatile OMX_BOOL*) &bPreWaitingTasks);
			pInBufferPhy=NULL;  //clear input
			pInBufferVirt=NULL;
			nInSize=0;
		}

		//check input return queue
		if(pPreReturnedInput==NULL){
			if(pPreInReturnQueue->Size()>0){
				pPreInReturnQueue->Get(&index);
				//if(PreFrameValid(&sEncPreProcessInFrameInfo, index)){
				if(1){ //for invalid eos input, we still need to notify user to get it. in such case, pPreReturnedInput will be set NULL
					pPreReturnedInput=(OMX_PTR)PreFrameGetVirt(&sEncPreProcessInFrameInfo,index);
					PreFrameClear(&sEncPreProcessInFrameInfo, index);
					bufRet=(FilterBufRetCode)(bufRet|FILTER_INPUT_CONSUMED_EXT_RETURN);
				}
			}
		}
#if 0 //disable it to decrease latency for wifi display
		//check input buffer count to improve pipeline efficiency: avoid pre-process is idel as possible when vpu is decoding
		if((pPreInQueue->Size()<PRE_THRESHOLD_MIN_INPUT)&&(pInBufferPhy==NULL)&&(OMX_FALSE==bPreEos)){
			bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_INPUT_BUFFER);
			return bufRet;
		}
#endif
	}

	index=OutFrameBufPhyFindValid(&sEncOutFrameInfo,&nOutPhy,&nOutVirt,&nOutLength);
	if(-1==index)
	{
		//not  output frame
		bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_OUTPUT_BUFFER);
		return bufRet;
	}

    if (nOutBufferSize > nOutLength)
        nOutLength = nOutBufferSize;

	if(bEnabledPreProcess){
		//check out queue
		OMX_U32 flag=0;
		if(nPreOutIndex!=-1){
			PreFrameGetNode(&sEncPreProcessOutFrameInfo,nPreOutIndex, &nInputPhy, &nInputVirt, &nInputSize, &flag);
		}
		else if(pPreOutQueue->Size()>0){
			pPreOutQueue->Get(&nPreOutIndex);
			PreFrameGetNode(&sEncPreProcessOutFrameInfo,nPreOutIndex, &nInputPhy, &nInputVirt, &nInputSize, &flag);
		}
		else{
			//no output from pre-process, now check whether need more input
			if((NULL==pInBufferPhy) && (pPreInQueue->Size()==0)&&(OMX_FALSE==bPreEos)){
				OMX_U32 cnt=pPreInReturnQueue->Size();
				if(cnt<nInBufferCnt-1){
					bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_INPUT_BUFFER);
				}
			}
			return bufRet;
		}
		if(flag&PRE_FLAG_EOS){
			bInEos=OMX_TRUE;
			if(nInputSize==0){
				//no valid data for eos
				eVpuEncoderState=VPU_ENC_COM_STATE_DO_OUT;
				pOutBufferPhy=NULL;
				bufRet=(FilterBufRetCode)(bufRet|FILTER_LAST_OUTPUT);
				return bufRet;
			}
		}
		if((NULL==nInputPhy)||(nInputSize==0)){
			//shouldn't hit here !!
			VPU_ENC_COMP_ERR_LOG("error: invalid data \r\n");
			return FILTER_ERROR;
		}
	}
	else{
		if(0==nValidInput)	{
			if(bInEos==OMX_TRUE)	{
				//eos: and no output frame
				eVpuEncoderState=VPU_ENC_COM_STATE_DO_OUT;
				pOutBufferPhy=NULL;//no real output frame
				return (FilterBufRetCode)(FILTER_INPUT_CONSUMED|FILTER_LAST_OUTPUT);
			}
			else	{
				//no input buffer
				return FILTER_NO_INPUT_BUFFER;
			}
		}
	}

	//clear 0 firstly
	fsl_osal_memset(&sEncEncParam,0,sizeof(VpuEncEncParam));
	sEncEncParam.eFormat=sVpuEncInputPara.eFormat;
	sEncEncParam.nPicWidth=sVpuEncInputPara.nWidthStride;//sVpuEncInputPara.nPicWidth;
	sEncEncParam.nPicHeight=sVpuEncInputPara.nHeightStride;//sVpuEncInputPara.nPicHeight;
	sEncEncParam.nFrameRate=sVpuEncInputPara.nFrameRate;
	sEncEncParam.nQuantParam=sVpuEncInputPara.nQuantParam;
	sEncEncParam.nInPhyInput=nInputPhy;
	sEncEncParam.nInVirtInput=nInputVirt;
	sEncEncParam.nInInputSize=nInputSize;
	sEncEncParam.nInPhyOutput=nOutPhy;
	sEncEncParam.nInVirtOutput=nOutVirt;
	sEncEncParam.nInOutputBufLen=nOutLength;

	//(1)check the frame count, for H.264
	//In current design, we will set IDR frame for every I frame
	//(2)check I refresh command by user
	if((1==sVpuEncInputPara.nRefreshIntra) ||
        ((VPU_V_AVC == sEncEncParam.eFormat || VPU_V_VP8 == sEncEncParam.eFormat) && (1 == nOutGOPFrameCnt)))
	{
		sEncEncParam.nForceIPicture=1;
		sVpuEncInputPara.nRefreshIntra=0; 	//clear it every time
		nOutGOPFrameCnt=1;
	}
	else
	{
		sEncEncParam.nForceIPicture=0;
	}
	sEncEncParam.nSkipPicture=0;
	sEncEncParam.nEnableAutoSkip=sVpuEncInputPara.nEnableAutoSkip;

	//encode frame
	ret=VPU_EncEncodeFrame(nHandle, &sEncEncParam);

	if(VPU_ENC_RET_SUCCESS!=ret)
	{
		if(VPU_ENC_RET_FAILURE_TIMEOUT==ret)
		{
			VPU_ENC_COMP_ERR_LOG("%s: encode frame timeout \r\n",__FUNCTION__);
			VPU_EncReset(nHandle);
			//SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
		}
		return FILTER_ERROR;
	}

	//check input
	if(sEncEncParam.eOutRetCode & VPU_ENC_INPUT_USED)
	{
		if(bEnabledPreProcess){
			//buffer has been consumed by vpu , return back to pre-process.
			pPreOutReturnQueue->Add(&nPreOutIndex);
			nPreOutIndex=-1;
			PreWakeUp(pPreMutex, &sPreCond, (volatile OMX_BOOL*) &bPreWaitingTasks);
		}
		else{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_INPUT_CONSUMED);
			pInBufferPhy=NULL;  //clear input
			pInBufferVirt=NULL;
			nInSize=0;
		}
	}
	else
	{
		//not used
	}

	//check output
	if(sEncEncParam.eOutRetCode & VPU_ENC_OUTPUT_DIS)
	{
		//has output
		if(bInEos==OMX_TRUE)
		{
			//last output
			bufRet=(FilterBufRetCode)(bufRet|FILTER_LAST_OUTPUT);
		}
		else
		{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_HAS_OUTPUT);
		}
		//set flags
		if(1==nOutGOPFrameCnt)
		{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_FLAG_KEY_FRAME);
		}
		else
		{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_FLAG_NONKEY_FRAME);
		}

		eVpuEncoderState=VPU_ENC_COM_STATE_DO_OUT;
		//record output info
		pOutBufferPhy=(OMX_PTR)nOutPhy;
		nOutSize=sEncEncParam.nOutOutputSize;
		ASSERT(nOutSize<=(OMX_S32)nOutBufferSize);
		VPU_ENC_COMP_LOG("[%d]frame data: %d \r\n",nOutGOPFrameCnt,nOutSize);
		//update count
		nOutGOPFrameCnt++;
		//if(nOutGOPFrameCnt>sVpuEncInputPara.nIDRPeriod)
		if(nOutGOPFrameCnt>sVpuEncInputPara.nGOPSize)
		{
			nOutGOPFrameCnt=1;
		}
	}
	else if(sEncEncParam.eOutRetCode & VPU_ENC_OUTPUT_SEQHEADER)
	{
		//has sequence header output
		bufRet=(FilterBufRetCode)(bufRet|FILTER_HAS_OUTPUT|FILTER_FLAG_CODEC_DATA);

		eVpuEncoderState=VPU_ENC_COM_STATE_DO_OUT;
		//record output info
		pOutBufferPhy=(OMX_PTR)nOutPhy;
		nOutSize=sEncEncParam.nOutOutputSize;
		ASSERT(nOutSize<=(OMX_S32)nOutBufferSize);
		VPU_ENC_COMP_LOG("sequence header: %d \r\n",nOutSize);
	}
	else
	{
		//no output
	}

	return bufRet;
}

OMX_ERRORTYPE VpuEncoder::GetReturnedInputDataPtr(OMX_PTR* ppInput)
{
	//if(pPreReturnedInput==NULL){
	if(0){ //for invalid eos input, we need to return NULL pointer
		VPU_ENC_COMP_ERR_LOG("%s: no input ptr is avaliable now \r\n",__FUNCTION__);
		return OMX_ErrorIncorrectStateOperation;
	}
	*ppInput=pPreReturnedInput;
	pPreReturnedInput=NULL;
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::GetOutputBuffer(OMX_PTR *ppOutVirtBuf)
{
	OMX_PTR pOutBufferVirt;

	VPU_ENC_COMP_API_LOG("%s: state: %d  pOutBufferPhy: 0x%X \r\n",__FUNCTION__,eVpuEncoderState,(UINT32)pOutBufferPhy);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_DO_OUT:
			//update state
			eVpuEncoderState=VPU_ENC_COM_STATE_DO_DEC;
			break;
		default:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
	}

	if(ppOutVirtBuf==NULL)
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: ppOutVirtBuf==NULL !!! \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

	//get output frame virtual address
	if(NULL==pOutBufferPhy)
	{
		//no real output frame: for eos case
		OMX_U32 virtAddr;
		if(-1==OutFrameBufVirtFindValidAndClear(&sEncOutFrameInfo,&virtAddr))
		{
			VPU_ENC_COMP_ERR_LOG("%s: failure: can not find one valid virtual address !!! \r\n",__FUNCTION__);
			return OMX_ErrorBadParameter;
		}
		*ppOutVirtBuf=(OMX_PTR)virtAddr;	//return one fake output frame
		VPU_ENC_COMP_LOG("return last output frame: 0x%X \r\n",(UINT32)virtAddr);
		//*pSize=0;
	}
	else
	{
		if(-1==OutFrameBufPhyClear(&sEncOutFrameInfo,pOutBufferPhy,&pOutBufferVirt))
		{
			VPU_ENC_COMP_ERR_LOG("%s: failure: unvalid output physical address !!! \r\n",__FUNCTION__);
			return OMX_ErrorBadParameter;
		}
		*ppOutVirtBuf=pOutBufferVirt;
		//*pSize=nOutSize;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::GetOutputBuffer(OMX_PTR *ppOutVirtBuf,OMX_S32* pOutSize)
{
	OMX_PTR pOutBufferVirt;

	VPU_ENC_COMP_API_LOG("%s: state: %d  pOutBufferPhy: 0x%X \r\n",__FUNCTION__,eVpuEncoderState,(UINT32)pOutBufferPhy);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_DO_OUT:
			//update state
			eVpuEncoderState=VPU_ENC_COM_STATE_DO_DEC;
			break;
		default:
			//forbidden
			VPU_ENC_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
	}

	if(ppOutVirtBuf==NULL)
	{
		VPU_ENC_COMP_ERR_LOG("%s: failure: ppOutVirtBuf==NULL !!! \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

	//get output frame virtual address
	if(NULL==pOutBufferPhy)
	{
		//no real output frame: for eos case
		OMX_U32 virtAddr;
		if(-1==OutFrameBufVirtFindValidAndClear(&sEncOutFrameInfo,&virtAddr))
		{
			VPU_ENC_COMP_ERR_LOG("%s: failure: can not find one valid virtual address !!! \r\n",__FUNCTION__);
			return OMX_ErrorBadParameter;
		}
		*ppOutVirtBuf=(OMX_PTR)virtAddr;	//return one fake output frame
		VPU_ENC_COMP_LOG("return last output frame: 0x%X \r\n",(UINT32)virtAddr);
		*pOutSize=0;
	}
	else
	{
		if(-1==OutFrameBufPhyClear(&sEncOutFrameInfo,pOutBufferPhy,&pOutBufferVirt))
		{
			VPU_ENC_COMP_ERR_LOG("%s: failure: unvalid output physical address !!! \r\n",__FUNCTION__);
			return OMX_ErrorBadParameter;
		}
		*ppOutVirtBuf=pOutBufferVirt;
		*pOutSize=(OMX_U32)nOutSize;
	}

	return OMX_ErrorNone;
}

/*
OMX_ERRORTYPE VpuEncoder::GetOutputBufferSize(OMX_S32* pOutBufSize)
{
	VPU_ENC_COMP_API_LOG("%s: state: %d  \r\n",__FUNCTION__,eVpuEncoderState);

	if(NULL==pOutBufferPhy)
	{
		*pOutBufSize=0;
	}
	else
	{
		*pOutBufSize=nOutSize;
	}
	return OMX_ErrorNone;
}
*/

OMX_ERRORTYPE VpuEncoder::FlushInputBuffer()
{
	VPU_ENC_COMP_API_LOG("%s: state: %d \r\n",__FUNCTION__,eVpuEncoderState);

	//clear input buffer
	pInBufferPhy=NULL;
	pInBufferVirt=NULL;
	nInSize=0;
	bInEos=OMX_FALSE;

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
		case VPU_ENC_COM_STATE_LOADED:
			VPU_ENC_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	if(bEnabledPreProcess){
		PreThreadFlushInput();
		PreThreadFlushOutput();
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::FlushOutputBuffer()
{
	VPU_ENC_COMP_API_LOG("%s: state: %d  \r\n",__FUNCTION__,eVpuEncoderState);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_OPENED:
		case VPU_ENC_COM_STATE_DO_DEC:
			break;
		default:
			//forbidden !!!
			VPU_ENC_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuEncoderState);
			return OMX_ErrorIncorrectStateTransition;
	}

#if 0 //needn't
	if(bEnabledPreProcess){
		PreThreadFlushOutput();
	}
#endif
	//clear out frame info
	OutFrameBufClear(&sEncOutFrameInfo);
	return OMX_ErrorNone;
}

OMX_PTR VpuEncoder::AllocateInputBuffer(OMX_U32 nSize)
{
	OMX_PTR ptr;

	VPU_ENC_COMP_API_LOG("%s: state: %d, size: %d \r\n",__FUNCTION__,eVpuEncoderState, (UINT32)nSize);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
		// case VPU_ENC_COM_STATE_LOADED:
		//case VPU_ENC_COM_STATE_OPENED:
			//1 how to avoid conflict memory operators
			VPU_ENC_COMP_ERR_LOG("%s: error state: %d \r\n",__FUNCTION__,eVpuEncoderState);
			return (OMX_PTR)NULL;
		default:
			break;
	}

	ptr=AllocateBuffer(nSize, &sEncAllocInMemInfo, &sMemOperator);
	return ptr;  //virtual address

}

OMX_ERRORTYPE VpuEncoder::FreeInputBuffer(OMX_PTR pBuffer)
{
	OMX_ERRORTYPE ret;
	VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	ret=FreeBuffer(pBuffer,&sEncAllocInMemInfo, &sMemOperator);
	return ret;
}

OMX_PTR VpuEncoder::AllocateOutputBuffer(OMX_U32 nSize)
{
	OMX_PTR ptr;

	VPU_ENC_COMP_API_LOG("%s: state: %d , size: %d \r\n",__FUNCTION__,eVpuEncoderState,(UINT32)nSize);

	//check state
	switch(eVpuEncoderState)
	{
		case VPU_ENC_COM_STATE_NONE:
		// case VPU_ENC_COM_STATE_LOADED:
		//case VPU_ENC_COM_STATE_OPENED:
			//1 how to avoid conflict memory operators
			VPU_ENC_COMP_ERR_LOG("%s: error state: %d \r\n",__FUNCTION__,eVpuEncoderState);
			return (OMX_PTR)NULL;
		default:
			break;
	}

	ptr=AllocateBuffer(nSize, &sEncAllocOutMemInfo, &sMemOperator);
	return ptr;  //virtual address
}

OMX_ERRORTYPE VpuEncoder::FreeOutputBuffer(OMX_PTR pBuffer)
{
	OMX_ERRORTYPE ret;
	VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	ret=FreeBuffer(pBuffer,&sEncAllocOutMemInfo, &sMemOperator);
	return ret;
}

OMX_ERRORTYPE VpuEncoder::PreThreadRun()
{
	// non-run state => run state
	if(ePreState==PRE_PROCESS_STATE_RUN){
		VPU_ENC_COMP_PRE_LOG("%s: already in run state: %d \r\n",__FUNCTION__,ePreState);
		return OMX_ErrorNone;
	}

	//send command
	VPU_ENC_COMP_PRE_LOG("encoder thread will send run command \r\n");
	ePreCmd=PRE_PROCESS_CMD_RUN;
	PreWakeUp(pPreMutex, &sPreCond, (volatile OMX_BOOL*) &bPreWaitingTasks);

	//wait run state
	PreWaitState((volatile PreProcessState*)&ePreState,PRE_PROCESS_STATE_RUN,pPreCmdSem);
	VPU_ENC_COMP_PRE_LOG("encoder thread receive run command finished\r\n");
	return OMX_ErrorNone;
}
OMX_ERRORTYPE VpuEncoder::PreThreadStop()
{
	// run state => idle state
	if(ePreState!=PRE_PROCESS_STATE_RUN){
		VPU_ENC_COMP_PRE_LOG("%s: not run state: %d \r\n",__FUNCTION__,ePreState);
		return OMX_ErrorNone;
	}
	//send command
	VPU_ENC_COMP_PRE_LOG("encoder thread will send stop command \r\n");
	ePreCmd=PRE_PROCESS_CMD_STOP;
	PreWakeUp(pPreMutex, &sPreCond, (volatile OMX_BOOL*) &bPreWaitingTasks);

	//wait idle state
	PreWaitState((volatile PreProcessState*)&ePreState,PRE_PROCESS_STATE_IDLE,pPreCmdSem);
	VPU_ENC_COMP_PRE_LOG("encoder thread receive stop command finished\r\n");
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::PreThreadFlushInput()
{
	// run -> (flush in) -> run state
	OMX_S32 index;

	if(ePreState!=PRE_PROCESS_STATE_RUN){
		VPU_ENC_COMP_PRE_LOG("%s: not run state: %d \r\n",__FUNCTION__,ePreState);
		return OMX_ErrorNone;
	}

	//send command
	VPU_ENC_COMP_PRE_LOG("encoder thread will send flushinput command \r\n");
	ePreCmd=PRE_PROCESS_CMD_FLUSH_IN;
	PreWakeUp(pPreMutex, &sPreCond, (volatile OMX_BOOL*) &bPreWaitingTasks);

	//wait finished
	PreWaitState((volatile PreProcessState*)&ePreState,ePreState,pPreCmdSem);
	VPU_ENC_COMP_PRE_LOG("encoder thread receive flushinput command finished\r\n");

	ASSERT(0==pPreInQueue->Size());
	//clear all input buffer
	while(pPreInReturnQueue->Size()>0){
		pPreInReturnQueue->Get(&index);
		//VPU_ENC_COMP_PRE_LOG("pre-process: clear input index %d \r\n",index);
		PreFrameClear(&sEncPreProcessInFrameInfo,index);
	}
	pPreReturnedInput=NULL;
	bPreEos=OMX_FALSE; //clear eos flag
	//finally, pPreInQueue/pPreInReturnQueue are empty
#if 0
	//popup all output frame, and refill into pPreOutReturnQueue
	while(pPreOutQueue->Size()>0){
		pPreOutQueue->Get(&index);
		index=index;
		pPreOutReturnQueue->Add(&index);
	}
	//finally, pPreOutQueue are empty; pPreOutReturnQueue isn't empty
	VPU_ENC_COMP_PRE_LOG("%s: pre flush input finised, pPreOutReturnQueue->Size(): %d \r\n",__FUNCTION__,pPreOutReturnQueue->Size());
#else
	VPU_ENC_COMP_PRE_LOG("%s: pre flush input finised \r\n",__FUNCTION__);
#endif

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuEncoder::PreThreadFlushOutput()
{
	// run -> (flush out) -> run state
	OMX_S32 index;

	if(ePreState!=PRE_PROCESS_STATE_RUN){
		VPU_ENC_COMP_PRE_LOG("%s: not run state: %d \r\n",__FUNCTION__,ePreState);
		return OMX_ErrorNone;
	}

	//send command
	VPU_ENC_COMP_PRE_LOG("encoder thread will send flushoutput command \r\n");
	ePreCmd=PRE_PROCESS_CMD_FLUSH_OUT;
	PreWakeUp(pPreMutex, &sPreCond, (volatile OMX_BOOL*) &bPreWaitingTasks);

	//wait finished
	PreWaitState((volatile PreProcessState*)&ePreState,ePreState,pPreCmdSem);
	VPU_ENC_COMP_PRE_LOG("encoder thread receive flushoutput command finished\r\n");

#if 0
	ASSERT(0==pPreInQueue->Size());
	//clear all input buffer
	while(pPreInReturnQueue->Size()>0){
		pPreInReturnQueue->Get(&index);
		//VPU_ENC_COMP_PRE_LOG("pre-process: clear input index %d \r\n",index);
		PreFrameClear(&sEncPreProcessInFrameInfo,index);
	}
	pPreReturnedInput=NULL;
	// finally, pPreInQueue/pPreInReturnQueue are empty
#endif

	//popup all output frame, and refill into pPreOutReturnQueue
	while(pPreOutQueue->Size()>0){
		pPreOutQueue->Get(&index);
		pPreOutReturnQueue->Add(&index);
	}
	nPreOutIndex=-1;
	//finally, pPreOutQueue are empty; pPreOutReturnQueue isn't empty

	VPU_ENC_COMP_PRE_LOG("%s: pre flush output finised  \r\n",__FUNCTION__);
	return OMX_ErrorNone;
}

/**< C style functions to expose entry point for the shared library */
extern "C"
{
	OMX_ERRORTYPE VpuEncoderInit(OMX_IN OMX_HANDLETYPE pHandle)
	{
		OMX_ERRORTYPE ret = OMX_ErrorNone;
		VpuEncoder *obj = NULL;
		ComponentBase *base = NULL;
		VPU_ENC_COMP_API_LOG("%s: \r\n",__FUNCTION__);

		obj = FSL_NEW(VpuEncoder, ());
		if(obj == NULL)
		{
			VPU_ENC_COMP_ERR_LOG("%s: vpu encoder new failure: ret=0x%X \r\n",__FUNCTION__,ret);
			return OMX_ErrorInsufficientResources;
		}

		base = (ComponentBase*)obj;
		ret = base->ConstructComponent(pHandle);
		if(ret != OMX_ErrorNone)
		{
			VPU_ENC_COMP_ERR_LOG("%s: vpu encoder construct failure: ret=0x%X \r\n",__FUNCTION__,ret);
			return ret;
		}
		return ret;
	}
}

/* File EOF */
