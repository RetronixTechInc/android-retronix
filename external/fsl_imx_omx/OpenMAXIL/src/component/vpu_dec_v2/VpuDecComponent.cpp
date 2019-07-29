/**
 *  Copyright (c) 2010-2016, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "VpuDecComponent.h"
#include "Mem.h"
#include "sys/ioctl.h"
#include "fcntl.h"

#define Align(ptr,align)	(((OMX_U32)(ptr)+(align)-1)/(align)*(align))
#define IPU_FOURCC(a,b,c,d) (((OMX_U32)(a)<<0)|((OMX_U32)(b)<<8)|((OMX_U32)(c)<<16)|((OMX_U32)(d)<<24))
#define Max(a,b)	((OMX_S32)(a)>=(OMX_S32)(b)?(a):(b))

//#define VPU_COMP_DIS_POST			//disable post-process
#define DEFAULT_BUF_IN_CNT			0x3
#define DEFAULT_BUF_IN_SIZE		(1048576)//1*1024*1024
#define DEFAULT_BUF_OUT_POST_ZEROCNT (0)
#define DEFAULT_BUF_OUT_POST_CNT	(5)	//5 will bring better performance than 4
#define DEFAULT_BUF_OUT_DEC_CNT	(3)
#define DEFAULT_BUF_OUT_CNT		((DEFAULT_BUF_OUT_POST_ZEROCNT)+(DEFAULT_BUF_OUT_DEC_CNT))
#define MAX_POST_QUEUE_SIZE		(VPU_DEC_MAX_NUM_MEM)

#define DEFAULT_FRM_WIDTH			(176)
#define DEFAULT_FRM_HEIGHT		(144)
#define DEFAULT_FRM_RATE			(30 * Q16_SHIFT)

#define DEFAULT_BUF_DELAY			(0)

#define MAX_FRAME_NUM	(VPU_DEC_MAX_NUM_MEM)

#if (ANDROID_VERSION >= ICS) && defined(MX5X)
#define FRAME_ALIGN		(64) //set 64 for gpu y/u/v alignment in iMX5 ICS
#define CHROMA_ALIGN            (4096)
#elif (ANDROID_VERSION == GINGER_BREAD)
#define FRAME_ALIGN		(16) //set 16 for gpu y/u/v alignment in iMX5 GingerBread
#define CHROMA_ALIGN            (1)
#else
#define FRAME_ALIGN		(32) //set 32 for gpu y/u/v alignment
#define CHROMA_ALIGN            (1)  //default, needn't align
#endif

//nOutBufferCntDec=nMinFrameBufferCount + FRAME_SURPLUS
#if 0
#define FRAME_SURPLUS	(0)
#define FRAME_MIN_FREE_THD		(((nOutBufferCntDec-2)>2)?(nOutBufferCntDec-2):2)//(nOutBufferCntDec-1)
#else
#define FRAME_SURPLUS	(3)
//#define FRAME_MIN_FREE_THD	((nOutBufferCntDec-FRAME_SURPLUS)-1)	// eg => FrameBuffers must be > (nMinFrameBufferCount - 1)
/*
to improve performance with limited frame buffers, we may set smaller threshold for FRAME_MIN_FREE_THD
for smaller FRAME_MIN_FREE_THD, vpu wrapper may return VPU_DEC_NO_ENOUGH_BUF_***
*/
#define FRAME_MIN_FREE_THD ((nOutBufferCntDec-FRAME_SURPLUS)-3) //adjust performance: for clip: Divx5_1920x1080_30fps_19411kbps_MP3_44.1khz_112kbps_JS.avi
#endif
#define FRAME_POST_MIN_FREE_THD	(2)//(nOutBufferCntPost-2)
#define POST_INDEX_EOS			(0x1)
#define POST_INDEX_MOSAIC		(0x2)

//#define VPUDEC_IN_PORT 		IN_PORT
#define VPUDEC_OUT_PORT 	OUT_PORT

#undef NULL
#define NULL (0)
#define INVALID (0xFFFFFFFF)		// make sure the match between "return FILTER_INPUT_CONSUMED" and SetInputBuffer()
typedef int INT32;
typedef unsigned int UINT32;

#define VPU_DEC_COMP_DROP_B	//drop B frames automatically
#ifdef VPU_DEC_COMP_DROP_B
#define DROP_B_THRESHOLD	30000
#endif

//#define VPU_COMP_DBGLOG
#ifdef VPU_COMP_DBGLOG
#define VPU_COMP_LOG	printf
#else
#define VPU_COMP_LOG(...)
#endif

//#define VPU_COMP_API_DBGLOG
#ifdef VPU_COMP_API_DBGLOG
#define VPU_COMP_API_LOG	printf
#else
#define VPU_COMP_API_LOG(...)
#endif

//#define VPU_COMP_ERR_DBGLOG
#ifdef VPU_COMP_ERR_DBGLOG
#define VPU_COMP_ERR_LOG	printf
#define ASSERT(exp)	if(!(exp)) {printf("%s: %d : assert condition !!!\r\n",__FUNCTION__,__LINE__);}
#else
#define VPU_COMP_ERR_LOG LOG_ERROR //(...)
#define ASSERT(...)
#endif

//#define VPU_COMP_DEBUG
#ifdef VPU_COMP_DEBUG
#define MAX_NULL_LOOP	(0xFFFFFFF)
#define MAX_DEC_FRAME  (0xFFFFFFF)
#define MAX_YUV_FRAME  (200)
#endif

#define VPU_COMP_NAME_H263DEC	"OMX.Freescale.std.video_decoder.h263.hw-based"
#define VPU_COMP_NAME_SORENSONDEC	"OMX.Freescale.std.video_decoder.sorenson.hw-based"
#define VPU_COMP_NAME_AVCDEC		"OMX.Freescale.std.video_decoder.avc.hw-based"
#define VPU_COMP_NAME_MPEG2DEC	"OMX.Freescale.std.video_decoder.mpeg2.hw-based"
#define VPU_COMP_NAME_MPEG4DEC	"OMX.Freescale.std.video_decoder.mpeg4.hw-based"
#define VPU_COMP_NAME_WMVDEC		"OMX.Freescale.std.video_decoder.wmv.hw-based"
#define VPU_COMP_NAME_DIV3DEC	"OMX.Freescale.std.video_decoder.div3.hw-based"
#define VPU_COMP_NAME_DIV4DEC	"OMX.Freescale.std.video_decoder.div4.hw-based"
#define VPU_COMP_NAME_DIVXDEC	"OMX.Freescale.std.video_decoder.divx.hw-based"
#define VPU_COMP_NAME_XVIDDEC	"OMX.Freescale.std.video_decoder.xvid.hw-based"
#define VPU_COMP_NAME_RVDEC		"OMX.Freescale.std.video_decoder.rv.hw-based"
#define VPU_COMP_NAME_MJPEGDEC	"OMX.Freescale.std.video_decoder.mjpeg.hw-based"
#define VPU_COMP_NAME_AVS	"OMX.Freescale.std.video_decoder.avs.hw-based"
#define VPU_COMP_NAME_VP8	"OMX.Freescale.std.video_decoder.vp8.hw-based"
// Add the VPX component for StageFright.
#define VPU_COMP_NAME_VPX	"OMX.Freescale.std.video_decoder.vpx.hw-based"
#define VPU_COMP_NAME_MVC	"OMX.Freescale.std.video_decoder.mvc.hw-based"

//#define VPU_COMP_POST_DEBUG
#ifdef VPU_COMP_POST_DEBUG
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
	printf("post-process time: %d (us) \r\n",(tm_2-tm_1));
	return;
}
static void time_report()
{
	double avg;
	if(total_cnt)
	{
		avg=total_time/total_cnt;
		printf(" average post-process time: %f (ms)\r\n",avg/1000);
	}
	return;
}
#define TIMER_INIT	time_init()
#define TIMER_START time_start()
#define TIMER_STOP	time_stop()
#define TIMER_REPORT	time_report()
#define VPU_COMP_POST_LOG printf
#else
#define TIMER_INIT
#define TIMER_START
#define TIMER_STOP
#define TIMER_REPORT
#define VPU_COMP_POST_LOG(...)
#endif

//#define USE_PROCESS_SEM		//for debug : use semaphore between process
#ifdef USE_PROCESS_SEM
#include <semaphore.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
sem_t* sharedmem_sem_open(fsl_osal_s32 pshared, fsl_osal_u32 value);
efsl_osal_return_type_t fsl_osal_sem_init_process(fsl_osal_sem *sem_obj,fsl_osal_s32 pshared,fsl_osal_u32 value);
sem_t* sharedmem_sem_open(fsl_osal_s32 pshared, fsl_osal_u32 value)
{
	int fd;
	int ret;
	sem_t *pSem = NULL;
	char *shm_path, shm_file[256];
	shm_path = getenv("CODEC_SHM_PATH");      /*the CODEC_SHM_PATH is on a memory map the fs */

	if (shm_path == NULL)
		strcpy(shm_file, "/dev/shm");   /* default path */
	else
		strcpy(shm_file, shm_path);

	strcat(shm_file, "/");
	//strcat(shm_file, "codec.shm");
	strcat(shm_file, "vpudec.shm");
	fd = open(shm_file, O_RDWR, 0777);
	if (fd < 0)
	{
		/* first thread/process need codec protection come here */
		fd = open(shm_file, O_RDWR | O_CREAT | O_EXCL, 0777);
		if(fd < 0)
		{
			return NULL;
		}
		ftruncate(fd, sizeof(sem_t));
		/* map the semaphore variant in the file */
		pSem = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if((void *)(-1) == pSem)
		{
			return NULL;
		}
		/* do the semaphore initialization */
		ret = sem_init(pSem, pshared, value);
		if(-1 == ret)
		{
			return NULL;
		}
	}
	else
		pSem = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	return pSem;
}
efsl_osal_return_type_t fsl_osal_sem_init_process(fsl_osal_sem *psem_obj,fsl_osal_s32 pshared,fsl_osal_u32 value)
{
	ASSERT(1==pshared);
	*psem_obj = (sem_t *)sharedmem_sem_open(pshared,value);
	if(*psem_obj == NULL)
	{
		VPU_COMP_ERR_LOG("\n Creation of semaphore failed.");
		return E_FSL_OSAL_UNAVAILABLE;
	}
	return E_FSL_OSAL_SUCCESS;
}
efsl_osal_return_type_t fsl_osal_sem_destroy_process(fsl_osal_sem sem_obj)
{
#ifdef ANDROID_BUILD
	/* workaround for android semaphore usage */
	fsl_osal_sem_post(sem_obj);
#endif
	if (sem_destroy((sem_t *)sem_obj) != 0)
	{
	VPU_COMP_ERR_LOG("\n Error in destroying semaphore.");
	return E_FSL_OSAL_INVALIDPARAM ;
	}
	munmap((void *)sem_obj, sizeof(sem_t));
	return E_FSL_OSAL_SUCCESS;
}
#endif

#ifdef VPU_COMP_DEBUG
void printf_memory(OMX_U8* addr, OMX_S32 width, OMX_S32 height, OMX_S32 stride)
{
	OMX_S32 i,j;
	OMX_U8* ptr;

	ptr=addr;
	VPU_COMP_LOG("addr: 0x%X \r\n",(UINT32)addr);
	for(i=0;i<height;i++)
	{
		for(j=0;j<width;j++)
		{
			VPU_COMP_LOG("%2X ",ptr[j]);
		}
		VPU_COMP_LOG("\r\n");
		ptr+=stride;
	}
	VPU_COMP_LOG("\r\n");
	return;
}

void FileDumpBitstrem(FILE** ppFp, OMX_U8* pBits, OMX_U32 nSize)
{
	if(nSize==0)
	{
		return;
	}

	if(*ppFp==NULL)
	{
		*ppFp=fopen("temp.bit","wb");
		if(*ppFp==NULL)
		{
			VPU_COMP_LOG("open temp.bit failure \r\n");
			return;
		}
		VPU_COMP_LOG("open temp.bit OK \r\n");
	}

	fwrite(pBits,1,nSize,*ppFp);
	return;
}

void FileDumpYUV(FILE** ppFp, OMX_U8*  pY,OMX_U8*  pU,OMX_U8*  pV, OMX_U32 nYSize,OMX_U32 nCSize,OMX_COLOR_FORMATTYPE colorFormat)
{
	static OMX_U32 cnt=0;
	OMX_S32 nCScale=1;

	switch (colorFormat)
	{
		case OMX_COLOR_FormatYUV420SemiPlanar:
			VPU_COMP_ERR_LOG("interleave 420 color format : %d \r\n",colorFormat);
		case OMX_COLOR_FormatYUV420Planar:
			nCScale=1;
			break;
		case OMX_COLOR_FormatYUV422SemiPlanar:
			VPU_COMP_ERR_LOG("interleave 422 color format : %d \r\n",colorFormat);
		case OMX_COLOR_FormatYUV422Planar:
			//hor ???
			nCScale=2;
			break;
		//FIXME: add 4:0:0/4:4:4/...
		default:
			VPU_COMP_ERR_LOG("unsupported color format : %d \r\n",colorFormat);
			break;
	}

	if(*ppFp==NULL)
	{
		*ppFp=fopen("temp.yuv","wb");
		if(*ppFp==NULL)
		{
			VPU_COMP_LOG("open temp.yuv failure \r\n");
			return;
		}
		VPU_COMP_LOG("open temp.yuv OK \r\n");
	}

	if(cnt<=MAX_YUV_FRAME)
	{
		fwrite(pY,1,nYSize,*ppFp);
		fwrite(pU,1,nCSize*nCScale,*ppFp);
		fwrite(pV,1,nCSize*nCScale,*ppFp);
		cnt++;
	}

	return;
}
#endif

VpuDecRetCode VPU_DecGetMem_Wrapper(VpuMemDesc* pInOutMem,OMX_PARAM_MEM_OPERATOR* pMemOp)
{
	VpuDecRetCode ret;
	if((pMemOp->pfMalloc==NULL) || (pMemOp->pfFree==NULL))
	{
		//use default method
		ret=VPU_DecGetMem(pInOutMem);
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
			ret=VPU_DEC_RET_SUCCESS;
		}
		else
		{
			ret=VPU_DEC_RET_FAILURE;
		}
	}
	return ret;
}

VpuDecRetCode VPU_DecFreeMem_Wrapper(VpuMemDesc* pInMem,OMX_PARAM_MEM_OPERATOR* pMemOp)
{
	VpuDecRetCode ret;
	if((pMemOp->pfMalloc==NULL) || (pMemOp->pfFree==NULL))
	{
		//use default method
		ret=VPU_DecFreeMem(pInMem);
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
			ret=VPU_DEC_RET_SUCCESS;
		}
		else
		{
			ret=VPU_DEC_RET_FAILURE;
		}
	}
	return ret;
}

OMX_S32 MemFreeBlock(VpuDecoderMemInfo* pDecMem,OMX_PARAM_MEM_OPERATOR* pMemOp)
{
	OMX_S32 i;
	VpuMemDesc vpuMem;
	VpuDecRetCode vpuRet;
	OMX_S32 retOk=1;

	//free virtual mem
	//for(i=0;i<pDecMem->nVirtNum;i++)
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if(pDecMem->virtMem[i])
		{
			fsl_osal_ptr ptr=(fsl_osal_ptr)pDecMem->virtMem[i];
			FSL_FREE(ptr);
			pDecMem->virtMem[i]=NULL;
		}
	}

	pDecMem->nVirtNum=0;

	//free physical mem
	//for(i=0;i<pDecMem->nPhyNum;i++)
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if(pDecMem->phyMem_phyAddr[i])
		{
			vpuMem.nPhyAddr=pDecMem->phyMem_phyAddr[i];
			vpuMem.nVirtAddr=pDecMem->phyMem_virtAddr[i];
			vpuMem.nCpuAddr=pDecMem->phyMem_cpuAddr[i];
			vpuMem.nSize=pDecMem->phyMem_size[i];
			vpuRet=VPU_DecFreeMem_Wrapper(&vpuMem,pMemOp);
			if(vpuRet!=VPU_DEC_RET_SUCCESS)
			{
				VPU_COMP_LOG("%s: free vpu memory failure : ret=0x%X \r\n",__FUNCTION__,vpuRet);
				retOk=0;
			}
			pDecMem->phyMem_phyAddr[i]=NULL;
			pDecMem->phyMem_virtAddr[i]=NULL;
			pDecMem->phyMem_cpuAddr[i]=NULL;
			pDecMem->phyMem_size[i]=0;
		}
	}
	pDecMem->nPhyNum=0;

	return retOk;
}


OMX_S32 MemAddPhyBlock(VpuMemDesc* pInMemDesc,VpuDecoderMemInfo* pOutDecMem)
{
	OMX_S32 i;

	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		// insert value into empty node
		if(NULL==pOutDecMem->phyMem_phyAddr[i])
		{
			pOutDecMem->phyMem_phyAddr[i]=pInMemDesc->nPhyAddr;
			pOutDecMem->phyMem_virtAddr[i]=pInMemDesc->nVirtAddr;
			pOutDecMem->phyMem_cpuAddr[i]=pInMemDesc->nCpuAddr;
			pOutDecMem->phyMem_size[i]=pInMemDesc->nSize;
			pOutDecMem->nPhyNum++;
			return 1;
		}
	}

	return 0;
}

OMX_S32 MemRemovePhyBlock(VpuMemDesc* pInMemDesc,VpuDecoderMemInfo* pOutDecMem)
{
	OMX_S32 i;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		// clear matched node
		if(pInMemDesc->nPhyAddr==pOutDecMem->phyMem_phyAddr[i])
		{
			pOutDecMem->phyMem_phyAddr[i]=NULL;
			pOutDecMem->phyMem_virtAddr[i]=NULL;
			pOutDecMem->phyMem_cpuAddr[i]=NULL;
			pOutDecMem->phyMem_size[i]=0;
			pOutDecMem->nPhyNum--;
			return 1;
		}
	}

	return 0;
}

OMX_S32 MemQueryPhyBlock(OMX_PTR pInVirtAddr,VpuMemDesc* pOutMemDesc,VpuDecoderMemInfo* pInDecMem)
{
	OMX_S32 i;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		// find matched node
		if((OMX_U32)pInVirtAddr==pInDecMem->phyMem_virtAddr[i])
		{
			pOutMemDesc->nPhyAddr=pInDecMem->phyMem_phyAddr[i];
			pOutMemDesc->nVirtAddr=pInDecMem->phyMem_virtAddr[i];
			pOutMemDesc->nCpuAddr=pInDecMem->phyMem_cpuAddr[i];
			pOutMemDesc->nSize=pInDecMem->phyMem_size[i];
			return 1;
		}
	}

	return 0;
}


OMX_S32 MemMallocVpuBlock(VpuMemInfo* pMemBlock,VpuDecoderMemInfo* pVpuMem,OMX_PARAM_MEM_OPERATOR* pMemOp)
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
				VPU_COMP_LOG("%s: get virtual memory failure, size=%d \r\n",__FUNCTION__,(INT32)size);
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
			VpuDecRetCode ret;
			vpuMem.nSize=size;
			ret=VPU_DecGetMem_Wrapper(&vpuMem,pMemOp);
			if(ret!=VPU_DEC_RET_SUCCESS)
			{
				VPU_COMP_LOG("%s: get vpu memory failure, size=%d, ret=0x%X \r\n",__FUNCTION__,(INT32)size,ret);
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
	MemFreeBlock(pVpuMem,pMemOp);
	return 0;

}

OMX_S32 FramePoolRegisterBuf(OMX_PTR pInVirtAddr,OMX_PTR pInPhyAddr,VpuDecoderFrmPoolInfo* pOutFrmPool)
{
	OMX_S32 i;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		//insert into empty node
		if(NULL==pOutFrmPool->nFrm_phyAddr[i])
		{
			pOutFrmPool->eFrmOwner[i]=VPU_COM_FRM_OWNER_NULL;
			pOutFrmPool->eFrmState[i]=VPU_COM_FRM_STATE_FREE;
			pOutFrmPool->nFrm_phyAddr[i]=(OMX_U32)pInPhyAddr;
			pOutFrmPool->nFrm_virtAddr[i]=(OMX_U32)pInVirtAddr;
			pOutFrmPool->nFrmNum++;
			return pOutFrmPool->nFrmNum;
		}
	}
	return -1;
}

OMX_S32 FramePoolBufExist(OMX_PTR pInVirtAddr,VpuDecoderFrmPoolInfo* pInFrmPool,OMX_PTR* ppOutPhyAddr,OMX_S32* pOutIndx)
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
		VPU_COMP_ERR_LOG("%s: physical addr is null !!!\r\n",__FUNCTION__);
		return -1;
	}
	*ppOutPhyAddr=pPhyAddr;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		//search matched node
		if((OMX_U32)pPhyAddr==pInFrmPool->nFrm_phyAddr[i])
		{
			*pOutIndx=i;
			return 1;
		}
	}
	return 0;
}

OMX_S32 FramePoolGetBufVirt(VpuDecoderFrmPoolInfo* pInFrmPool, OMX_S32 Index,OMX_PTR *ppOutVirtBuf)
{
	*ppOutVirtBuf=(OMX_PTR)pInFrmPool->nFrm_virtAddr[Index];
	return 1;
}

OMX_S32 FramePoolGetBufProperty(VpuDecoderFrmPoolInfo* pInFrmPool, OMX_S32 Index,VpuDecoderFrmOwner* pOutOwner,VpuDecoderFrmState* pOutState,VpuDecOutFrameInfo** ppOutFrame)
{
	*pOutOwner=pInFrmPool->eFrmOwner[Index];
	*pOutState=pInFrmPool->eFrmState[Index];
	*ppOutFrame=&pInFrmPool->outFrameInfo[Index];
	return 1;
}

OMX_S32 FramePoolSetBufState(VpuDecoderFrmPoolInfo* pInFrmPool, OMX_S32 Index,VpuDecoderFrmState eInState)
{
	pInFrmPool->eFrmState[Index]=eInState;
	return 1;
}

OMX_S32 FramePoolBufNum(VpuDecoderFrmPoolInfo* pInFrmPool)
{
	return pInFrmPool->nFrmNum;
}

OMX_S32 FramePoolStolenDecoderBufNum(VpuDecoderFrmPoolInfo* pInFrmPool)
{
	OMX_S32 cnt=0;
	OMX_S32 i;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if((pInFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_DEC)
			&&(pInFrmPool->eFrmState[i]==VPU_COM_FRM_STATE_OUT))
		{
			if(NULL==pInFrmPool->outFrameInfo[i].pDisplayFrameBuf)
			{
				cnt++;
			}
		}
	}
	return cnt;
}

OMX_S32 FramePoolCreateDecoderRegisterFrame(
	VpuFrameBuffer* pOutRegisterFrame,VpuDecoderFrmPoolInfo* pInOutFrmPool,
	OMX_S32 nInRequiredCnt,OMX_S32 nPadW,OMX_S32 nPadH,
	VpuDecoderMemInfo* pOutDecMemInfo,
	OMX_PARAM_MEM_OPERATOR* pMemOp,OMX_COLOR_FORMATTYPE colorFormat,OMX_U32 nInChromaAlign)
{
	VpuDecRetCode ret;
	VpuMemDesc vpuMem;
	OMX_S32 i;
	OMX_S32 nCnt=0;
	OMX_S32 yStride;
	OMX_S32 uvStride;
	OMX_S32 ySize;
	OMX_S32 uvSize;
	OMX_S32 mvSize;
	OMX_S32 mvTotalSize;
	OMX_U8* ptr;
	OMX_U8* ptrVirt;

	yStride=nPadW;
	ySize=yStride*nPadH;

	switch (colorFormat)
	{
		case OMX_COLOR_FormatYUV420Planar:
		case OMX_COLOR_FormatYUV420SemiPlanar:
			uvStride=yStride/2;
			uvSize=ySize/4;
			mvSize=ySize/4;
			break;
		case OMX_COLOR_FormatYUV422Planar:
		case OMX_COLOR_FormatYUV422SemiPlanar:
			//hor ???
			uvStride=yStride/2;
			uvSize=ySize/2;
			mvSize=ySize/2;	//In fact, for MJPG, mv is useless
			break;
		//FIXME: add 4:0:0/4:4:4/...
		default:
			VPU_COMP_ERR_LOG("unsupported color format : %d \r\n",colorFormat);
			uvStride=yStride/2;
			uvSize=ySize/4;
			mvSize=ySize/4;
			break;
	}

	//fill StrideY/StrideC/Y/Cb/Cr info
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		//search valid phy buff
		if((pInOutFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_NULL)&&(pInOutFrmPool->nFrm_phyAddr[i]!=NULL))
		{
			ptr=(OMX_U8*)pInOutFrmPool->nFrm_phyAddr[i];
			ptrVirt=(OMX_U8*)pInOutFrmPool->nFrm_virtAddr[i];

			/* fill stride info */
			pOutRegisterFrame[nCnt].nStrideY=yStride;
			pOutRegisterFrame[nCnt].nStrideC=uvStride;

			/* fill phy addr*/
			pOutRegisterFrame[nCnt].pbufY=(OMX_U8*)Align(ptr,nInChromaAlign);
			pOutRegisterFrame[nCnt].pbufCb=(OMX_U8*)Align(pOutRegisterFrame[nCnt].pbufY+ySize,nInChromaAlign);
			pOutRegisterFrame[nCnt].pbufCr=(OMX_U8*)Align(pOutRegisterFrame[nCnt].pbufCb+uvSize,nInChromaAlign);
			//pOutRegisterFrame[nCnt].pbufMvCol=ptr+ySize+uvSize*2;

			/* fill virt addr */
			pOutRegisterFrame[nCnt].pbufVirtY=(OMX_U8*)Align(ptrVirt,nInChromaAlign);
			pOutRegisterFrame[nCnt].pbufVirtCb=(OMX_U8*)Align(pOutRegisterFrame[nCnt].pbufVirtY+ySize,nInChromaAlign);
			pOutRegisterFrame[nCnt].pbufVirtCr=(OMX_U8*)Align(pOutRegisterFrame[nCnt].pbufVirtCb+uvSize,nInChromaAlign);
			//pOutRegisterFrame[nCnt].pbufVirtMvCol=ptrVirt+ySize+uvSize*2;

			if(pOutRegisterFrame[nCnt].pbufVirtY!=ptrVirt)
			{
				//FIXME: In this case, memory management will be out of order !!!!
				VPU_COMP_ERR_LOG("%s: warning: buffer base address isn't aligned(%d bytes) correctly: 0x%X \r\n",__FUNCTION__,(UINT32)nInChromaAlign,(UINT32)ptrVirt);
			}
			//update frame owner/state
			pInOutFrmPool->eFrmOwner[i]=VPU_COM_FRM_OWNER_DEC;
			pInOutFrmPool->eFrmState[i]=VPU_COM_FRM_STATE_FREE;
			nCnt++;
			if(nCnt>=nInRequiredCnt)
			{
				break;
			}
		}
		else
		{
			VPU_COMP_ERR_LOG("%s: warning: frame pool is not clean before register frame for vpu !!",__FUNCTION__);
			//return -1;
		}
	}

	ASSERT(nCnt==nInRequiredCnt);
	//malloc phy memory for mv
	mvTotalSize=mvSize*nInRequiredCnt;

	vpuMem.nSize=mvTotalSize;
	ret=VPU_DecGetMem_Wrapper(&vpuMem,pMemOp);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu malloc frame buf failure: ret=0x%X \r\n",__FUNCTION__,ret);
		return -1;//OMX_ErrorInsufficientResources;
	}

	//record memory info for release
	pOutDecMemInfo->phyMem_phyAddr[pOutDecMemInfo->nPhyNum]=vpuMem.nPhyAddr;
	pOutDecMemInfo->phyMem_virtAddr[pOutDecMemInfo->nPhyNum]=vpuMem.nVirtAddr;
	pOutDecMemInfo->phyMem_cpuAddr[pOutDecMemInfo->nPhyNum]=vpuMem.nCpuAddr;
	pOutDecMemInfo->phyMem_size[pOutDecMemInfo->nPhyNum]=vpuMem.nSize;
	pOutDecMemInfo->nPhyNum++;

	//fill mv info
	ptr=(OMX_U8*)vpuMem.nPhyAddr;
	ptrVirt=(OMX_U8*)vpuMem.nVirtAddr;
	for(i=0;i<nInRequiredCnt;i++)
	{
		pOutRegisterFrame[i].pbufMvCol=ptr;
		pOutRegisterFrame[i].pbufVirtMvCol=ptrVirt;
		ptr+=mvSize;
		ptrVirt+=mvSize;
	}

	return nInRequiredCnt;
}


OMX_S32 FramePoolRegisterPostFrame(VpuDecoderFrmPoolInfo* pInOutFrmPool, OMX_S32 nInRequiredCnt,Queue* pQueue)
{
	OMX_S32 i;
	OMX_S32 nCnt=0;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if((pInOutFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_NULL)&&(pInOutFrmPool->nFrm_phyAddr[i]!=NULL))
		{
			//update frame owner/state
			pInOutFrmPool->eFrmOwner[i]=VPU_COM_FRM_OWNER_POST;
			pInOutFrmPool->eFrmState[i]=VPU_COM_FRM_STATE_FREE;
			//pInOutFrmPool->outFrameInfo[i].pDisplayFrameBuf->pbufVirtY=pInOutFrmPool->nFrm_virtAddr[i];
			pQueue->Add(&i);
			nCnt++;
			if(nCnt>=nInRequiredCnt)
			{
				break;
			}
		}
	}
	ASSERT(nCnt==nInRequiredCnt);
	return nInRequiredCnt;
}

OMX_S32 FramePoolFindOneDecoderUnOutputed(OMX_PTR * ppOutVirtBuf,VpuDecoderFrmPoolInfo* pInFrmPool,VpuDecOutFrameInfo** ppOutFrame)
{
	OMX_S32 i;
	//find one un-outputed frame
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if(pInFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_DEC)
		{
			if(pInFrmPool->eFrmState[i]!=VPU_COM_FRM_STATE_OUT)
			{
				*ppOutVirtBuf=(OMX_PTR)pInFrmPool->nFrm_virtAddr[i];
				*ppOutFrame=&pInFrmPool->outFrameInfo[i];
				return i;
			}
		}
	}
	*ppOutVirtBuf=NULL;
	return -1;
}

OMX_S32 FramePoolFindOnePostUnOutputed(OMX_PTR * ppOutPhyBuf,OMX_PTR * ppOutVirtBuf,VpuDecoderFrmPoolInfo* pInFrmPool)
{
	OMX_S32 i;
	//find one un-outputed frame
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if(pInFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_POST)
		{
			if(pInFrmPool->eFrmState[i]!=VPU_COM_FRM_STATE_OUT)
			{
				*ppOutPhyBuf=(OMX_PTR)pInFrmPool->nFrm_phyAddr[i];
				*ppOutVirtBuf=(OMX_PTR)pInFrmPool->nFrm_virtAddr[i];
				return i;
			}
		}
	}
	*ppOutPhyBuf=NULL;
	*ppOutVirtBuf=NULL;
	return -1;
}

OMX_S32 FramePoolRecordOutFrame(OMX_PTR pInVirtBuf,VpuDecoderFrmPoolInfo* pInFrmPool,VpuDecOutFrameInfo* pInFrameInfo,VpuDecoderFrmOwner eInOwner)
{
	OMX_S32 i;
	//search matched node and restore output frame info
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if(pInFrmPool->eFrmOwner[i]==eInOwner)
		{
			if(pInFrmPool->nFrm_virtAddr[i]==(OMX_U32)pInVirtBuf)
			{
				pInFrmPool->outFrameInfo[i]=*pInFrameInfo;
				return i;
			}
		}
	}
	return -1;
}

OMX_S32 FramePoolDecoderOutReset(VpuDecoderFrmPoolInfo* pInFrmPool,VpuFrameBuffer** pInFrameBufInfo,OMX_S32 nInFrameBufNum,OMX_S32 nInFlag)
{
	/*
	nInFlag
		0: iMX5: It should be OK whatever reset or clear to all buffers. since all buffers have been cleared automatically in internal wrapper.
		1: iMX6: only clear NULL for those stolen buffers
	*/
	OMX_S32 i;
	OMX_S32 nDecCnt=0;
	OMX_S32 nClearedCnt=0;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if(pInFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_DEC)
		{
			if(nInFlag==1)
			{
				if(pInFrmPool->eFrmState[i]!=VPU_COM_FRM_STATE_OUT)
				{
					pInFrmPool->eFrmState[i]=VPU_COM_FRM_STATE_OUT;
					pInFrmPool->outFrameInfo[i].pDisplayFrameBuf=NULL;
					nClearedCnt++;
				}
			}
			else
			{
				if(pInFrmPool->eFrmState[i]!=VPU_COM_FRM_STATE_OUT)
				{
					pInFrmPool->eFrmState[i]=VPU_COM_FRM_STATE_OUT;
					nClearedCnt++;
				}
				pInFrmPool->outFrameInfo[i].pDisplayFrameBuf=*pInFrameBufInfo++;
				if((OMX_U32)(pInFrmPool->outFrameInfo[i].pDisplayFrameBuf->pbufVirtY)!=pInFrmPool->nFrm_virtAddr[i])
				{
					//FIXME: Now, we always reserve the first nOutBufferCntDec frames for vpu decoder
					//So it should be match, otherwise, we need to search correct node based on virtual address.
					VPU_COMP_ERR_LOG("%s: error: frame pool out of order !!!",__FUNCTION__);
				}
			}
			nDecCnt++;
		}
	}
	ASSERT(nInFrameBufNum==nDecCnt);
	return nClearedCnt;
}

OMX_S32 FramePoolPostOutReset(VpuDecoderFrmPoolInfo* pInFrmPool,OMX_S32 nInFrameBufNum)
{
	OMX_S32 i;
	OMX_S32 nDecCnt=0;
	OMX_S32 nClearedCnt=0;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++)
	{
		if(pInFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_POST)
		{
			if(pInFrmPool->eFrmState[i]!=VPU_COM_FRM_STATE_OUT)
			{
				pInFrmPool->eFrmState[i]=VPU_COM_FRM_STATE_OUT;
				nClearedCnt++;
			}
			//pInFrmPool->outFrameInfo[i].pDisplayFrameBuf=pInFrmPool->nFrm_virtAddr[i];
			nDecCnt++;
		}
	}
	ASSERT(nInFrameBufNum==nDecCnt);
	return nClearedCnt;
}

OMX_S32 FramePoolClear(VpuDecoderFrmPoolInfo* pOutFrmPool)
{
	fsl_osal_memset(pOutFrmPool,0, sizeof(VpuDecoderFrmPoolInfo));
	return 1;
}

VpuDecRetCode FramePoolReturnPostFrameToVpu(VpuDecoderFrmPoolInfo* pInFrmPool,VpuDecHandle InHandle,OMX_S32 index,VPUCompSemaphor psem)
{
	VpuDecRetCode ret=VPU_DEC_RET_SUCCESS;
	if(index>=0)
	{
		VPU_COMP_SEM_LOCK(psem);
		ret=VPU_DecOutFrameDisplayed(InHandle,pInFrmPool->outFrameInfo[index].pDisplayFrameBuf);
		VPU_COMP_SEM_UNLOCK(psem);
	}
	return ret;
}

OMX_S32 FramePoolClearPostIndexMapping(VpuDecoderFrmPoolInfo* pInFrmPool,OMX_S32 nPostIndx)
{
	pInFrmPool->nPostIndxMapping[nPostIndx]=0; // 0 mean invalid, align with memset(0).
	return 1;
}

OMX_S32 FramePoolSetPostIndexMapping(VpuDecoderFrmPoolInfo* pInFrmPool,OMX_S32 nPostIndx,OMX_S32 nDecIndex)
{
	VPU_COMP_LOG("post thread: mapping: post index: %d,  dec index: %d \r\n",nPostIndx,nDecIndex);
	pInFrmPool->nPostIndxMapping[nPostIndx]=nDecIndex+1;
	pInFrmPool->outFrameInfo[nPostIndx]=pInFrmPool->outFrameInfo[nDecIndex]; // copy frame info
	return 1;
}

OMX_S32 FramePoolSearchMappedDecBuffer(VpuDecoderFrmPoolInfo* pInFrmPool,OMX_PTR pPostBuf, OMX_PTR* ppDecBuf)
{
	OMX_S32 i,decIndx=-1;
	for(i=0;i<VPU_DEC_MAX_NUM_MEM;i++){
		if((OMX_U32)pPostBuf==pInFrmPool->nFrm_virtAddr[i]){
			break;
		}
	}
	if(i>=VPU_DEC_MAX_NUM_MEM){
		VPU_COMP_ERR_LOG("%s: address is invalid : 0x%X \r\n",__FUNCTION__,pPostBuf);
		return 0;
	}
	if(pInFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_POST){
		decIndx=pInFrmPool->nPostIndxMapping[i]-1;
		if(decIndx>=0){
			*ppDecBuf=(OMX_PTR)pInFrmPool->nFrm_virtAddr[decIndx];
			VPU_COMP_POST_LOG("%s: post: %d (0x%X), dec: %d (0x%X) \r\n",__FUNCTION__,i,pPostBuf,decIndx,*ppDecBuf);
			return 1;
		}
		VPU_COMP_ERR_LOG("unmapped post buffer: 0x%X index: %d \r\n",pPostBuf,i);
	}
	else if(pInFrmPool->eFrmOwner[i]==VPU_COM_FRM_OWNER_DEC){
		*ppDecBuf=pPostBuf;
		VPU_COMP_POST_LOG("%s: no mapped: %d (0x%X), dec: %d (0x%X) \r\n",__FUNCTION__,i,pPostBuf,decIndx,*ppDecBuf);
		return 1;
	}
	else{
		VPU_COMP_ERR_LOG("invalid owner: buffer index: %d \r\n",i);
	}
	*ppDecBuf=NULL;
	return 0;
}

#ifdef MX6X
OMX_S32 PostProcessSetStrategy(VpuDecInitInfo* pInInitInfo,OMX_BOOL* pOutEnabled, OMX_U32* pOutBufNum,VpuCodStd InFormat,ipu_motion_sel* pOutMotion)
{
#define POST_PROCESS_MAX_FRM_PIXELS (1920*1088*12)
	ipu_motion_sel motion=MED_MOTION;  //default
	OMX_BOOL bClipEnable=OMX_FALSE;
	OMX_BOOL bUserEnable=OMX_FALSE;
#ifdef VPU_COMP_DIS_POST
	//disable post-process since risk exist when CHROMA_ALIGN !=1 (gpu render limitation)
#else
	if(pInInitInfo->nInterlace!=0)
	{
		if(pInInitInfo->nPicWidth*pInInitInfo->nPicHeight*pInInitInfo->nMinFrameBufferCount < POST_PROCESS_MAX_FRM_PIXELS)
		{
			switch(InFormat)
			{
				case VPU_V_AVC:
				case VPU_V_MPEG2:
				case VPU_V_VC1:
				case VPU_V_VC1_AP:
					bClipEnable=OMX_TRUE;
					break;
				default:
					break;
			}
		}
	}

#if 1  //dynamic config
	{
#ifdef ANDROID_BUILD
		#define POST_CONFIGFILE "/data/omx_post_process"
#else
		#define POST_CONFIGFILE "/etc/omx_post_process"
#endif
		efsl_osal_return_type_t ret;
		FILE* fpPost;
		ret = fsl_osal_fopen(POST_CONFIGFILE, "r", (fsl_osal_file *)&fpPost);
		if (ret != E_FSL_OSAL_SUCCESS)
		{
		       //printf("read post process config file: %s failed !!!!!!\r\n",POST_CONFIGFILE);
			//bEnable=OMX_FALSE;
		}
		else
		{
			fsl_osal_char symbol;
			fsl_osal_s32 ReadLen = 0;
			int level=0;
			ret = fsl_osal_fread(&symbol, 1, fpPost, &ReadLen);
			if (ret != E_FSL_OSAL_SUCCESS && ret != E_FSL_OSAL_EOF)
			{
				//bEnable=OMX_FALSE;
			}
			else
			{
				level=fsl_osal_atoi(&symbol);
				if(level>0) bUserEnable=OMX_TRUE;
				if(level==0) bUserEnable=OMX_FALSE;
				else if(level==1) motion=LOW_MOTION;
				else if (level==2) motion=MED_MOTION;
				else if (level==3) motion=HIGH_MOTION;
				else motion=MED_MOTION;

				//printf("post process: enable: %d, mode: %d  \r\n",bEnable,motion);
			}
			fsl_osal_fclose(fpPost);
		}
	}
#endif
#endif

	if((bUserEnable==OMX_FALSE) || (bClipEnable==OMX_FALSE))
	{
		*pOutEnabled=OMX_FALSE;
		*pOutBufNum=DEFAULT_BUF_OUT_POST_ZEROCNT;
		*pOutMotion=motion;
	}
	else
	{
		*pOutEnabled=OMX_TRUE;
		*pOutBufNum=DEFAULT_BUF_OUT_POST_CNT;
		*pOutMotion=motion;
		//may crash if new post cnt is smaller than default cnt !
		ASSERT(DEFAULT_BUF_OUT_POST_CNT>=DEFAULT_BUF_OUT_POST_ZEROCNT);
	}
	return 1;
}

OMX_S32 PostProcessIPUInit(VpuDecoderIpuHandle* pIpuHandle)
{
	pIpuHandle->nIpuFd=open("/dev/mxc_ipu", O_RDWR, 0);
	if(pIpuHandle->nIpuFd<=0)
	{
		VPU_COMP_ERR_LOG("Open /dev/mxc_ipu failed.\n");
		return 0;
	}
	fsl_osal_memset(&pIpuHandle->sIpuTask, 0, sizeof(IpuTask));
	TIMER_INIT;
	return 1;
}

OMX_S32 PostProcessIPUSetDefault(VpuDecoderIpuHandle* pIpuHandle,OMX_VIDEO_PORTDEFINITIONTYPE* pOutFmt,OMX_CONFIG_RECTTYPE* pOutCrop,ipu_motion_sel InMotion)
{
	struct ipu_input* pIn=&pIpuHandle->sIpuTask.input;
	struct ipu_output* pOut=&pIpuHandle->sIpuTask.output;
	OMX_U32 color=IPU_FOURCC('I', '4', '2', '0');;
	pIpuHandle->sIpuTask.overlay_en=0;

	if(pOutFmt->eColorFormat == OMX_COLOR_FormatYUV420Planar)
	{
		color=IPU_FOURCC('I', '4', '2', '0');
		VPU_COMP_LOG("Set post-process in format to YUVI420.\n");
	}
	else if(pOutFmt->eColorFormat == OMX_COLOR_FormatYUV420SemiPlanar)
	{
		color=IPU_FOURCC('N', 'V', '1', '2');
		VPU_COMP_LOG("Set post-process in format to NV12.\n");
	}
	else
	{
		VPU_COMP_ERR_LOG("unsupported color format(%d) for deinterlace ! \r\n",pOutFmt->eColorFormat);
	}

	pIn->width = pOutFmt->nFrameWidth;
	pIn->height = pOutFmt->nFrameHeight;
	pIn->crop.pos.x = Align(pOutCrop->nLeft,8);
	if(pIn->crop.pos.x!=(OMX_U32)pOutCrop->nLeft)
	{
		VPU_COMP_ERR_LOG("warning: crop left(%d) isn't aligned with 8 bytes !!! \r\n",(INT32)pOutCrop->nLeft);
	}
	pIn->crop.pos.y = pOutCrop->nTop;
	pIn->crop.w = pOutCrop->nWidth/8*8;
	if(pIn->crop.w!=pOutCrop->nWidth)
	{
		VPU_COMP_ERR_LOG("warning: crop width(%d) isn't aligned with 8 bytes !!! \r\n",(INT32)pOutCrop->nWidth);
	}
	pIn->crop.h = pOutCrop->nHeight;
	pIn->format = color;
	VPU_COMP_LOG("IpulibRender sInParam width %d, height %d,crop x %d, y %d, w %d, h %d, color %d\n",
		pIn->width, pIn->height,pIn->crop.pos.x, pIn->crop.pos.y,pIn->crop.w, pIn->crop.h,pIn->format);

	pIn->deinterlace.enable=1;
	pIn->deinterlace.motion=InMotion;
	pIn->deinterlace.field_fmt=IPU_DEINTERLACE_FIELD_TOP;

	pOut->width = pIn->width;
	pOut->height = pIn->height;
	pOut->crop.pos.x = pIn->crop.pos.x;
	pOut->crop.pos.y = pIn->crop.pos.y;
	pOut->crop.w = pIn->crop.w;
	pOut->crop.h = pIn->crop.h;
	pOut->format = color;
	pOut->rotate = ROTATE_NONE;
	VPU_COMP_LOG("IpulibRender sOutParam width %d, height %d,crop x %d, y %d, rot: %d, color %d\n",
		pOut->crop.w, pOut->crop.h ,pOut->crop.pos.x, pOut->crop.pos.y,pOut->rotate, pOut->format);

	return 1;
}

OMX_S32 PostProcessIPUDeInterlace(OMX_PTR pInPhyBuf,VpuDecOutFrameInfo* pInFrmInfo,VpuDecOutFrameInfo* pInFutureFrmInfo,VpuDecoderIpuHandle* pIpuHandle)
{
	/*
	(1) HIGH MOTION:
		input.paddr : current frame
	(2) LOW/MED MOTION:
		input.paddr: current frame
		input.paddr_n: next frame
	*/
	OMX_S32 ret=0;
	OMX_U8 motion_bak=(OMX_U8)LOW_MOTION;
	OMX_BOOL bNeedChangeMotion=OMX_FALSE;
	VPU_COMP_LOG("in De-interlaced: field type: %d \r\n",pInFrmInfo->eFieldType);
	//FIXME: field_fmt should be same for previous and current frame ????
	pIpuHandle->sIpuTask.input.deinterlace.field_fmt=(pInFrmInfo->eFieldType==VPU_FIELD_TB)?IPU_DEINTERLACE_FIELD_TOP:IPU_DEINTERLACE_FIELD_BOTTOM;
	pIpuHandle->sIpuTask.input.paddr=(int)pInFrmInfo->pDisplayFrameBuf->pbufY;
	pIpuHandle->sIpuTask.output.paddr=(int)pInPhyBuf;
	if(pInFrmInfo->eFieldType!=pInFutureFrmInfo->eFieldType){
		//use the same frame if fieldtype are different
		pIpuHandle->sIpuTask.input.paddr_n=(int)pInFrmInfo->pDisplayFrameBuf->pbufY;
	}
	else{
		pIpuHandle->sIpuTask.input.paddr_n=(int)pInFutureFrmInfo->pDisplayFrameBuf->pbufY;	//needed for LOW_MOTION/MED_MOTION
	}

	//For LOW_MOTION/MED_MOTION case: if paddr and paddr_n point to the same frame, the effect isn't perfect. so we make temporary swith to high motion
	if((pIpuHandle->sIpuTask.input.deinterlace.motion!=HIGH_MOTION)
		&&(pIpuHandle->sIpuTask.input.paddr==pIpuHandle->sIpuTask.input.paddr_n)){
		motion_bak=pIpuHandle->sIpuTask.input.deinterlace.motion;
		VPU_COMP_POST_LOG("%s: temporary swith to high motion: from %d to %d \r\n",__FUNCTION__,(int)motion_bak,(int)HIGH_MOTION);
		pIpuHandle->sIpuTask.input.deinterlace.motion=HIGH_MOTION;
		bNeedChangeMotion=OMX_TRUE;
	}

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
				pIpuHandle->sIpuTask.output.crop.h -= 8;;
				break;
			default:
				return 0;
		}
	}

	ret = ioctl(pIpuHandle->nIpuFd, IPU_QUEUE_TASK, &pIpuHandle->sIpuTask);
	if(bNeedChangeMotion==OMX_TRUE){
		pIpuHandle->sIpuTask.input.deinterlace.motion=motion_bak;
	}
	if (ret < 0)
	{
		VPU_COMP_ERR_LOG("ioct IPU_QUEUE_TASK fail\n");
		return 0;
	}
	return 1;
}

OMX_S32 PostProcessIPUDeinit(VpuDecoderIpuHandle* pIpuHandle)
{
	if(pIpuHandle->nIpuFd>0)
	{
		close(pIpuHandle->nIpuFd);
		pIpuHandle->nIpuFd=0;
	}
	TIMER_REPORT;
	return 1;
}

#else
OMX_S32 PostProcessSetStrategy(VpuDecInitInfo* pInInitInfo,OMX_BOOL* pOutEnabled, OMX_U32* pOutBufNum,VpuCodStd InFormat,ipu_motion_sel* pOutMotion)
{
	VPU_COMP_LOG("%s: not implemented \r\n",__FUNCTION__);
	*pOutEnabled=OMX_FALSE;
	*pOutBufNum=DEFAULT_BUF_OUT_POST_ZEROCNT;
	*pOutMotion=MED_MOTION;
	return 1;
}
OMX_S32 PostProcessIPUInit(VpuDecoderIpuHandle* pIpuHandle)
{
	VPU_COMP_LOG("%s: not implemented \r\n",__FUNCTION__);
	return 1;
}
OMX_S32 PostProcessIPUSetDefault(VpuDecoderIpuHandle* pIpuHandle,OMX_VIDEO_PORTDEFINITIONTYPE* pOutFmt,OMX_CONFIG_RECTTYPE* pOutCrop,ipu_motion_sel InMotion)
{
	VPU_COMP_LOG("%s: not implemented \r\n",__FUNCTION__);
	return 1;
}
OMX_S32 PostProcessIPUDeInterlace(OMX_PTR pInPhyBuf,VpuDecOutFrameInfo* pInFrmInfo,VpuDecOutFrameInfo* pInFutureFrmInfo,VpuDecoderIpuHandle* pIpuHandle)
{
	VPU_COMP_LOG("%s: not implemented \r\n",__FUNCTION__);
	return 1;
}
OMX_S32 PostProcessIPUDeinit(VpuDecoderIpuHandle* pIpuHandle)
{
	VPU_COMP_LOG("%s: not implemented \r\n",__FUNCTION__);
	return 1;
}
#endif

OMX_ERRORTYPE OpenVpu(VpuDecHandle* pOutHandle, VpuMemInfo* pInMemInfo,
	VpuCodStd eCodeFormat, OMX_S32 nPicWidth,OMX_S32 nPicHeight,OMX_COLOR_FORMATTYPE eColorFormat,OMX_S32 enableFileMode,VPUCompSemaphor psem,OMX_BOOL bReorderDis)
{
	VpuDecOpenParam decOpenParam;
	VpuDecRetCode ret;
	OMX_S32 para;

	VPU_COMP_LOG("%s: codec format: %d \r\n",__FUNCTION__,eCodeFormat);
	//clear 0
	fsl_osal_memset(&decOpenParam, 0, sizeof(VpuDecOpenParam));
	//set open params
	decOpenParam.CodecFormat=eCodeFormat;

	//FIXME: for MJPG, we need to add check for 4:4:4/4:2:2 ver/4:0:0  !!!
	if((OMX_COLOR_FormatYUV420SemiPlanar==eColorFormat)
		||(OMX_COLOR_FormatYUV422SemiPlanar==eColorFormat))
	{
		decOpenParam.nChromaInterleave=1;
	}
	else
	{
		decOpenParam.nChromaInterleave=0;
	}
	decOpenParam.nReorderEnable=(bReorderDis==OMX_TRUE)?0:1;	//for H264
	switch(eCodeFormat)
	{
		case VPU_V_MPEG4:
		case VPU_V_DIVX4:
		case VPU_V_DIVX56:
		case VPU_V_XVID:
		case VPU_V_H263:
			decOpenParam.nEnableFileMode=enableFileMode;	//FIXME: set 1 in future
			break;
		case VPU_V_AVC:
			decOpenParam.nEnableFileMode=enableFileMode; 	//FIXME: set 1 in future
			break;
		case VPU_V_MPEG2:
			decOpenParam.nEnableFileMode=enableFileMode; 	//FIXME: set 1 in future
			break;
		case VPU_V_VC1:
		case VPU_V_VC1_AP:
		case VPU_V_DIVX3:
		case VPU_V_RV:
			//for VC1/RV/DivX3: we should use filemode, otherwise, some operations are unstable(such as seek...)
			decOpenParam.nEnableFileMode=enableFileMode;
			break;
		case VPU_V_MJPG:
			//must file mode ???
			decOpenParam.nEnableFileMode=enableFileMode;
			break;
		default:
			decOpenParam.nEnableFileMode=enableFileMode;
			break;
	}

	//for special formats, such as package VC1 header,...
	decOpenParam.nPicWidth=nPicWidth;
	decOpenParam.nPicHeight=nPicHeight;

	//open vpu
	VPU_COMP_SEM_LOCK(psem);
	ret=VPU_DecOpen(pOutHandle, &decOpenParam, pInMemInfo);
	VPU_COMP_SEM_UNLOCK(psem);
	if (ret!=VPU_DEC_RET_SUCCESS)
	{
		VPU_COMP_ERR_LOG("%s: vpu open failure: ret=0x%X \r\n",__FUNCTION__,ret);
		return OMX_ErrorHardware;
	}

	//set default config
	VPU_COMP_SEM_LOCK(psem);
	para=VPU_DEC_SKIPNONE;
	ret=VPU_DecConfig(*pOutHandle, VPU_DEC_CONF_SKIPMODE, &para);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu config failure: config=0x%X, ret=%d \r\n",__FUNCTION__,(UINT32)VPU_DEC_CONF_SKIPMODE,ret);
		VPU_DecClose(*pOutHandle);
		VPU_COMP_SEM_UNLOCK(psem);
		return OMX_ErrorHardware;
	}
	para=DEFAULT_BUF_DELAY;
	ret=VPU_DecConfig(*pOutHandle, VPU_DEC_CONF_BUFDELAY, &para);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu config failure: config=0x%X, ret=%d \r\n",__FUNCTION__,(UINT32)VPU_DEC_CONF_SKIPMODE,ret);
		VPU_DecClose(*pOutHandle);
		VPU_COMP_SEM_UNLOCK(psem);
		return OMX_ErrorHardware;
	}
	VPU_COMP_SEM_UNLOCK(psem);
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::CheckDropB(VpuDecHandle InHandle,OMX_TICKS nTimeStamp,OMX_PTR pClock,VPUCompSemaphor psem)
{
    OMX_ERRORTYPE ret;
    VpuDecConfig config;
    OMX_S32 param;
    OMX_S64 media_ts = 0;
    if(bDropPB)
        return OMX_ErrorNone;

    if(pClock!=NULL)
    {
        OMX_TIME_CONFIG_TIMESTAMPTYPE sCur;
        OMX_TIME_CONFIG_PLAYBACKTYPE sPlayback;
        OMX_INIT_STRUCT(&sPlayback, OMX_TIME_CONFIG_PLAYBACKTYPE);
        OMX_GetConfig(pClock, OMX_IndexConfigPlaybackRate, &sPlayback);
        if(sPlayback.ePlayMode != NORMAL_MODE){
            VPU_COMP_LOG("*** not normal playback for drop B, return");
            return OMX_ErrorNone;
        }
        OMX_INIT_STRUCT(&sCur, OMX_TIME_CONFIG_TIMESTAMPTYPE);
        OMX_GetConfig(pClock, OMX_IndexConfigTimeCurrentMediaTime, &sCur);
        media_ts = sCur.nTimestamp;
    }else if(OMX_ErrorNone != GetMediaTime(&media_ts))
        return OMX_ErrorNone;

    if(media_ts <= 0)
        return OMX_ErrorNone;

    if(media_ts > (nTimeStamp - DROP_B_THRESHOLD))
    {
        VPU_COMP_LOG("drop B frame \r\n");
        config=VPU_DEC_CONF_SKIPMODE;
        param=VPU_DEC_SKIPB;
    }
    else
    {
        config=VPU_DEC_CONF_SKIPMODE;
        param=VPU_DEC_SKIPNONE;
    }

    ret = ConfigVpu(InHandle,config,param,psem);

    return ret;
}
OMX_ERRORTYPE VpuDecoder::ConfigVpu(VpuDecHandle InHandle,VpuDecConfig config,OMX_S32 param,VPUCompSemaphor psem)
{
    VpuDecRetCode ret;
    if(InHandle == NULL)
        return OMX_ErrorBadParameter;

    VPU_COMP_SEM_LOCK(psem);
    ret=VPU_DecConfig(InHandle, config, (void*)(&param));
    VPU_COMP_SEM_UNLOCK(psem);
    if(VPU_DEC_RET_SUCCESS!=ret)
    {
        VPU_COMP_ERR_LOG("%s: vpu config failure: config=0x%X, ret=%d \r\n",__FUNCTION__,config,ret);
        return OMX_ErrorHardware;
    }

    return OMX_ErrorNone;
}

OMX_COLOR_FORMATTYPE ConvertMjpgColorFormat(OMX_S32 sourceFormat,OMX_COLOR_FORMATTYPE oriColorFmt)
{
	OMX_COLOR_FORMATTYPE	colorformat;
	OMX_S32 interleave=0;

	switch(oriColorFmt)
	{
		case OMX_COLOR_FormatYUV420SemiPlanar:
		case OMX_COLOR_FormatYUV422SemiPlanar:
		//FIXME: add 4:0:0/4:4:4/...
			interleave=1;
			break;
		default:
			break;
	}
	switch(sourceFormat)
	{
		case 0:	//4:2:0
			colorformat=(1==interleave)?OMX_COLOR_FormatYUV420SemiPlanar:OMX_COLOR_FormatYUV420Planar;
			break;
		case 1:	//4:2:2 hor
			colorformat=(1==interleave)?OMX_COLOR_FormatYUV422SemiPlanar:OMX_COLOR_FormatYUV422Planar;
			break;
#if 0	//FIXME
		case 2:	//4:2:2 ver
			VPU_COMP_ERR_LOG("unsupport mjpg 4:2:2 ver color format: %d \r\n",sourceFormat);
			colorformat=OMX_COLOR_FormatYUV422Planar;
			break;
		case 3:	//4:4:4
			VPU_COMP_ERR_LOG("unsupport mjpg 4:4:4 color format: %d \r\n",sourceFormat);
			colorformat=OMX_COLOR_FormatYUV444Planar;
			break;
		case 4:	//4:0:0
			VPU_COMP_ERR_LOG("unsupport mjpg 4:0:0 color format: %d \r\n",sourceFormat);
			colorformat=OMX_COLOR_FormatYUV400Planar;
			break;
#endif
		default:	//unknow
			VPU_COMP_ERR_LOG("unknown mjpg color format: %d \r\n",(INT32)sourceFormat);
			colorformat=OMX_COLOR_FormatUnused;
			break;
	}
	return colorformat;
}

OMX_S32 PostTransferQueue(Queue* pFromQueue,Queue* pToQueue,OMX_U32 nIndexFlag,OMX_U32 nInvalidExtFlag)
{
	//get all buffer from pFromQueue(except invalid index) and re-filled into pToQueue
	OMX_S32 index;
	OMX_S32 nCnt=0;
	while(pFromQueue->Size()>0){
		pFromQueue->Get(&index);
		if(0==((index>>16)&nInvalidExtFlag)){
			index=index&nIndexFlag;
			pToQueue->Add(&index);
			nCnt++;
		}
	}
	return nCnt;
}

OMX_S32 PostWaitState(volatile PostProcessState * pState,PostProcessState eTargetState, fsl_osal_sem pSem)
{
	//decode thread wait post thread complete command
	fsl_osal_sem_wait(pSem);
	while(*pState!=eTargetState)
	{
		VPU_COMP_ERR_LOG("incorrect state: %d !!!, expected state: %d \r\n", *pState,eTargetState);
	}
	return 1;
}

OMX_S32 PostWakeUp(fsl_osal_mutex pMutex, pthread_cond_t* pCond, volatile OMX_BOOL * pWaiting)
{
	//decode thread notify post thread some tasks are received
	fsl_osal_mutex_lock(pMutex);
	if(*pWaiting==OMX_TRUE)
	{
		VPU_COMP_POST_LOG("%s: send tasks to post thread \r\n",__FUNCTION__);
		pthread_cond_signal(pCond);
	}
	else
	{
		VPU_COMP_POST_LOG("%s: post thread is already in work state, needn't wakeup it \r\n",__FUNCTION__);
	}
	fsl_osal_mutex_unlock(pMutex);
	return 1;
}

void *PostThread(void *arg)
{
#define POST_THREAD_WAITTIMEOUT_US	(100000)
	VpuDecoder *base = (VpuDecoder*)arg;
	volatile PostProcessCmd * pCmd=&base->ePostCmd;
	volatile PostProcessState* pState=&base->ePostState;
	volatile OMX_BOOL* pPostWaitingTasks =&base->bPostWaitingTasks;
	fsl_osal_sem pSem=base->pPostCmdSem;
	VPU_COMP_POST_LOG("%s: Post Thread starting.\n",__FUNCTION__);

	//set waiting firstly when post thread is actived.
	*pPostWaitingTasks=OMX_TRUE;
	//main loop
	while(1){
		VpuDecOutFrameInfo * pFrameInfo;
		VpuDecOutFrameInfo * pPreFrameInfo;
		OMX_S32 nVpuInIndex,nPostOutIndex;
		static OMX_S32 nPreVpuInIndex=-1;
		OMX_S32 nExtInfo=0;
		struct timespec to;
		struct timeval from;
		int ret=0;
		//wait decode thread send tasks(commands,data,...)
		fsl_osal_mutex_lock(base->pPostMutex);
		while(1){
			if(*pCmd!=POST_PROCESS_CMD_NONE){
				break;
			}
			if((base->pPostInQueue->Size()>0) && (base->pPostOutReturnQueue->Size()>0)){
				break;
			}
			VPU_COMP_POST_LOG("%s: will wait tasks from decoder thread \r\n",__FUNCTION__);
#if 0		//debug: using sleep method instead of wait
			#define POST_THREAD_SLEEP_US (1000)
			fsl_osal_mutex_unlock(base->pPostMutex);
			fsl_osal_sleep(POST_THREAD_SLEEP_US);
			fsl_osal_mutex_lock(base->pPostMutex);
#else
			*pPostWaitingTasks=OMX_TRUE; //no any tasks, begin waiting
			/*
			gettimeofday(&from, NULL);
			to.tv_sec=from.tv_sec;
			to.tv_nsec=from.tv_usec * 1000;
			to.tv_nsec+=(POST_THREAD_WAITTIMEOUT_US)*1000;
			//here, we use timeout wait to avoid deadlock as possible
			ret=pthread_cond_timedwait(&base->sPostCond, (pthread_mutex_t *)(base->pPostMutex), &to);
			*/
			ret=pthread_cond_wait(&base->sPostCond, (pthread_mutex_t *)base->pPostMutex);
			if(0!=ret) {
				VPU_COMP_POST_LOG("return from pthread_cond_*wait : %d \r\n",ret);
			}
#endif
		}
		if(ret!=0){//if(ret==ETIMEDOUT){
			//in theory, if the sync logic is correct, shouldn't enter here
			VPU_COMP_ERR_LOG("warning: the post thread isn't actived immediately ! error: %d \r\n",ret);
		}
		VPU_COMP_POST_LOG("%s: ready to work \r\n",__FUNCTION__);
		*pPostWaitingTasks=OMX_FALSE;  //clear waiting
		fsl_osal_mutex_unlock(base->pPostMutex);

		//process command
		if(*pCmd!=POST_PROCESS_CMD_NONE){
			switch(*pCmd){
				case POST_PROCESS_CMD_RUN:
					*pState=POST_PROCESS_STATE_RUN;
					break;
				case POST_PROCESS_CMD_STOP:
					*pState=POST_PROCESS_STATE_IDLE;
					break;
				case POST_PROCESS_CMD_FLUSH_OUT:
					//get all buffer from pPostOutReturnQueue, and fill into pPostOutQueue directly
					PostTransferQueue(base->pPostOutReturnQueue, base->pPostOutQueue, 0xFFFFFFFF,0x0);
#if 0				//now, flush output cover flush input
					break;
#endif
				case POST_PROCESS_CMD_FLUSH_IN:
					//get all buffer from pPostInQueue except eos(invalid index), and fill into pPostInReturnQueue
					PostTransferQueue(base->pPostInQueue, base->pPostInReturnQueue, 0xFFFF,POST_INDEX_EOS);
					if(nPreVpuInIndex>=0){
						//VPU_COMP_POST_LOG("%s: return vpu index: %d, ext info: %d \r\n",__FUNCTION__,nPreVpuInIndex&0xFFFF,nPreVpuInIndex>>16);
						nPreVpuInIndex=nPreVpuInIndex&0xFFFF;
						base->pPostInReturnQueue->Add(&nPreVpuInIndex);
						nPreVpuInIndex=-1;
					}
					break;
				default:
					VPU_COMP_POST_LOG("unknow command: %d, ignore it \r\n",*pCmd);
					break;
			}
			VPU_COMP_POST_LOG("%s: command(%d) finished, will notify decoder thread \r\n",__FUNCTION__,*pCmd);
			//clear command
			*pCmd=POST_PROCESS_CMD_NONE;
			//notify host command finished
			fsl_osal_sem_post(pSem);
		}

		//check state
		if((*pState)==POST_PROCESS_STATE_IDLE){
			break; //exit main loop
		}
		else if((*pState)!=POST_PROCESS_STATE_RUN){
			continue;
		}

		//check whether data is enough to do post-process
		if((base->pPostInQueue->Size()<=0) || ((base->pPostOutReturnQueue->Size()<=0))){
			continue;
		}

		base->pPostInQueue->Get(&nVpuInIndex);
		//base->pPostOutReturnQueue->Get(&nPostOutIndex);
		//VPU_COMP_POST_LOG("%s: get vpu index: %d(ext: %d), post output index: %d(ext: %d) \n",__FUNCTION__,nVpuInIndex&0xFFFF,nVpuInIndex>>16,nPostOutIndex&0xFFFF,nPostOutIndex>>16);

		//check EOS event
		if((nVpuInIndex>>16)&POST_INDEX_EOS){
			//ASSERT(0==base->pPostInQueue->Size());
			//clear previous index
			if(nPreVpuInIndex>=0){
				//VPU_COMP_POST_LOG("%s: return vpu index: %d, ext info: %d \r\n",__FUNCTION__,nPreVpuInIndex&0xFFFF,nPreVpuInIndex>>16);
				nPreVpuInIndex=nPreVpuInIndex&0xFFFF;
				base->pPostInReturnQueue->Add(&nPreVpuInIndex);
				nPreVpuInIndex=-1;
			}
			//in this case, nVpuInIndex is unmeaningful, drop it.
			//send eos output
			base->pPostOutReturnQueue->Get(&nPostOutIndex);
			nPostOutIndex=(POST_INDEX_EOS<<16)|nPostOutIndex;
			VPU_COMP_POST_LOG("%s: output EOS index: %d, ext info: %d \r\n",__FUNCTION__,nPostOutIndex&0xFFFF,nPostOutIndex>>16);
			base->pPostOutQueue->Add(&nPostOutIndex);
			FramePoolClearPostIndexMapping(&base->sFramePoolInfo, nPostOutIndex&0xFFFF); //clear it since no valid map index for EOS
			continue;
		}

		//get frame info
		pFrameInfo=&base->sFramePoolInfo.outFrameInfo[nVpuInIndex&0xFFFF];

		//equivalent: VPU_FIELD_TOP / VPU_FIELD_BOTTOM => VPU_FIELD_BT and VPU_FIELD_TB
		if(pFrameInfo->eFieldType==VPU_FIELD_TOP){
			pFrameInfo->eFieldType=VPU_FIELD_BT;
		}
		else if(pFrameInfo->eFieldType==VPU_FIELD_BOTTOM){
			pFrameInfo->eFieldType=VPU_FIELD_TB;
		}

		//check frame or field
#if 0	//debug: simulate frame+interlaced clips
		if(1)
#else
		if(pFrameInfo->eFieldType==VPU_FIELD_NONE)	//frame type
#endif
		{
			//clear previous index
			if(nPreVpuInIndex>=0){
				//VPU_COMP_POST_LOG("%s: return vpu index: %d, ext info: %d \r\n",__FUNCTION__,nPreVpuInIndex&0xFFFF,nPreVpuInIndex>>16);
				nPreVpuInIndex=nPreVpuInIndex&0xFFFF;
				base->pPostInReturnQueue->Add(&nPreVpuInIndex);
				nPreVpuInIndex=-1;
			}
			//by pass vpu index to output
			VPU_COMP_POST_LOG("%s: by pass vpu frame index: %d, ext info: %d \r\n",__FUNCTION__,nVpuInIndex&0xFFFF,nVpuInIndex>>16);
			base->pPostOutQueue->Add(&nVpuInIndex);
			FramePoolSetPostIndexMapping(&base->sFramePoolInfo, nVpuInIndex&0xFFFF, nVpuInIndex&0xFFFF); //mapping itself
		}
		else{	// field type

			//do de-interlace
			if(nPreVpuInIndex>=0){
				pPreFrameInfo=&base->sFramePoolInfo.outFrameInfo[nPreVpuInIndex&0xFFFF];
			}
			else{
				pPreFrameInfo=pFrameInfo;
			}
			base->pPostOutReturnQueue->Get(&nPostOutIndex);
#if 0		//debug: copy directly
			{
				OMX_S32 nOutSize;
				nOutSize=base->sOutFmt.nFrameWidth * base->sOutFmt.nFrameHeight*pxlfmt2bpp(base->sOutFmt.eColorFormat)/8;
				fsl_osal_memcpy((OMX_PTR)(base->sFramePoolInfo.nFrm_virtAddr[nPostOutIndex]), pFrameInfo->pDisplayFrameBuf->pbufVirtY, nOutSize);
			}
#else
			TIMER_START;
			if(base->nPostMotion==HIGH_MOTION){
				//only use current frame, pPreFrameInfo will be ignored by ipu
				PostProcessIPUDeInterlace((OMX_PTR)(base->sFramePoolInfo.nFrm_phyAddr[nPostOutIndex]),pFrameInfo,pPreFrameInfo,&base->sIpuHandle);
			}
			else{
				//IPU need to use current frame and future frame
				//FIXME: Now, for simply implementation, we won't wait future frame. just make simple trick. e.g. using previous/current frame pair to simulate current/next pair
				//as result, there will be one frame offset, like this:  1' 1 2 3 4.... N-1
				PostProcessIPUDeInterlace((OMX_PTR)(base->sFramePoolInfo.nFrm_phyAddr[nPostOutIndex]),pPreFrameInfo,pFrameInfo,&base->sIpuHandle);
			}
			TIMER_STOP;
#endif
			//return previous index to vpu
			if(nPreVpuInIndex>=0){
				//VPU_COMP_POST_LOG("%s: return vpu index: %d, ext info: %d \r\n",__FUNCTION__,nPreVpuInIndex&0xFFFF,nPreVpuInIndex>>16);
				nExtInfo=nPreVpuInIndex>>16;
				nPreVpuInIndex=nPreVpuInIndex&0xFFFF;
				base->pPostInReturnQueue->Add(&nPreVpuInIndex);
			}

			nExtInfo=nExtInfo|(nVpuInIndex>>16); //the extension is decided by two frames

#if 0		//FIXME: Now, we don't need these frame info
			//copy frame info
			base->sFramePoolInfo.outFrameInfo[nPostOutIndex]=base->sFramePoolInfo.outFrameInfo[nVpuInIndex&0xFFFF];
			base->sFramePoolInfo.outFrameInfo[nPostOutIndex].pDisplayFrameBuf=NULL; //no meaning for post owner
#endif
			//send output
			nPostOutIndex=(nExtInfo<<16)|nPostOutIndex;  //set extension flag
			VPU_COMP_POST_LOG("%s: output index: %d, ext info: %d, (vpu index: %d, ext info: %d, preIndx: %d) \r\n",__FUNCTION__,nPostOutIndex&0xFFFF,nPostOutIndex>>16,nVpuInIndex&0xFFFF,nVpuInIndex>>16,nPreVpuInIndex&0xFFFF);
			base->pPostOutQueue->Add(&nPostOutIndex);
			FramePoolSetPostIndexMapping(&base->sFramePoolInfo, nPostOutIndex&0xFFFF, nVpuInIndex&0xFFFF); //mapping decode index
			nPreVpuInIndex=nVpuInIndex;		//update previous index
		}
	}
	VPU_COMP_POST_LOG("%s: Post Thread Stopped.\n",__FUNCTION__);
	return NULL;
}

OMX_ERRORTYPE VpuDecoder::ReleaseVpuSource()
{
	VpuDecRetCode ret;
	OMX_ERRORTYPE omx_ret=OMX_ErrorNone;
	//close vpu
	if(NULL!=nHandle)
	{
		VPU_COMP_SEM_LOCK(psemaphore);
		ret=VPU_DecClose(nHandle);
		VPU_COMP_SEM_UNLOCK(psemaphore);
		if (ret!=VPU_DEC_RET_SUCCESS)
		{
			VPU_COMP_ERR_LOG("%s: vpu close failure: ret=0x%X \r\n",__FUNCTION__,ret);
			omx_ret=OMX_ErrorHardware;
		}
	}

	//stop/release post-process thread
	if(pPostThreadId != NULL) {
		PostThreadStop();
		fsl_osal_thread_destroy(pPostThreadId);
	}
	if(pPostInQueue){
		pPostInQueue->Free();
		FSL_DELETE(pPostInQueue);
	}
	if(pPostOutQueue){
		pPostOutQueue->Free();
		FSL_DELETE(pPostOutQueue);
	}
	if(pPostInReturnQueue){
		pPostInReturnQueue->Free();
		FSL_DELETE(pPostInReturnQueue);
	}
	if(pPostOutReturnQueue){
		pPostOutReturnQueue->Free();
		FSL_DELETE(pPostOutReturnQueue);
	}
	if(pPostCmdSem){
		fsl_osal_sem_destroy(pPostCmdSem);
	}
	if(pPostMutex!= NULL){
		fsl_osal_mutex_destroy(pPostMutex);
		//fsl_osal_cond_destroy(sPostCond);
		pthread_cond_destroy(&sPostCond);
	}
	if(bEnabledPostProcess)	{
		PostProcessIPUDeinit(&sIpuHandle);
	}

	//release mem
	if(0==MemFreeBlock(&sVpuMemInfo,&sMemOperator))
	{
		VPU_COMP_ERR_LOG("%s: free memory failure !  \r\n",__FUNCTION__);
		omx_ret=OMX_ErrorHardware;
	}

	//avoid memory leak !!!
	if(0==MemFreeBlock(&sAllocMemInfo,&sMemOperator))
	{
		VPU_COMP_ERR_LOG("%s: free memory failure !!!  \r\n",__FUNCTION__);
		omx_ret=OMX_ErrorHardware;
	}

	//unload
	VPU_COMP_SEM_LOCK(psemaphore);
	ret=VPU_DecUnLoad();
	VPU_COMP_SEM_UNLOCK(psemaphore);
	if (ret!=VPU_DEC_RET_SUCCESS)
	{
		VPU_COMP_ERR_LOG("%s: vpu unload failure: ret=0x%X \r\n",__FUNCTION__,ret);
		omx_ret=OMX_ErrorHardware;
	}
	return omx_ret;
}

OMX_ERRORTYPE VpuDecoder::SetDefaultSetting()
{
	fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_decoder.mpeg4.hw-based");

	fsl_osal_memset(&sInFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
	sInFmt.nFrameWidth = DEFAULT_FRM_WIDTH;
	sInFmt.nFrameHeight = DEFAULT_FRM_HEIGHT;
	sInFmt.xFramerate = DEFAULT_FRM_RATE;
	sInFmt.eColorFormat = OMX_COLOR_FormatUnused;
	sInFmt.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

	nInPortFormatCnt = 0;
	nOutPortFormatCnt = 2;
	eOutPortPormat[0] = OMX_COLOR_FormatYUV420Planar;
	eOutPortPormat[1] = OMX_COLOR_FormatYUV420SemiPlanar;

	fsl_osal_memset(&sOutFmt, 0, sizeof(OMX_VIDEO_PORTDEFINITIONTYPE));
	sOutFmt.nFrameWidth = DEFAULT_FRM_WIDTH;
	sOutFmt.nFrameHeight = DEFAULT_FRM_HEIGHT;
	sOutFmt.eColorFormat = OMX_COLOR_FormatYUV420Planar;
	sOutFmt.eCompressionFormat = OMX_VIDEO_CodingUnused;

	bFilterSupportPartilInput = OMX_TRUE;
	nInBufferCnt = DEFAULT_BUF_IN_CNT;
	nInBufferSize = DEFAULT_BUF_IN_SIZE;
	nOutBufferCnt = DEFAULT_BUF_OUT_CNT;
	nOutBufferSize = sOutFmt.nFrameWidth * sOutFmt.nFrameHeight * pxlfmt2bpp(sOutFmt.eColorFormat) / 8;

	//clear internal variable 0
	fsl_osal_memset(&sMemInfo,0,sizeof(VpuMemInfo));
	fsl_osal_memset(&sVpuMemInfo,0,sizeof(VpuDecoderMemInfo));
	fsl_osal_memset(&sAllocMemInfo,0,sizeof(VpuDecoderMemInfo));
	fsl_osal_memset(&sInitInfo,0,sizeof(VpuDecInitInfo));
	fsl_osal_memset(&sFramePoolInfo,0,sizeof(VpuDecoderFrmPoolInfo));

	nHandle=0;

#if 0
	// role info
	role_cnt = 10;	//Mpeg2;Mpeg4;DivX3;DivX4;Divx56;XviD;H263;H264;RV;VC1
	role_cnt = 8;
	role[0] = "video_decoder.avc";
	role[1] = "video_decoder.wmv9";
	role[2] = "video_decoder.mpeg2";
	role[3] = "video_decoder.mpeg4";
	role[4] = "video_decoder.divx3";
	role[5] = "video_decoder.divx4";
	role[6] = "video_decoder.divx";
	role[7] = "video_decoder.xvid";
	role[8] = "video_decoder.h263";
	role[9] = "video_decoder.rv";
#endif

	eFormat = VPU_V_MPEG2;//VPU_V_MPEG4;
	nPadWidth=DEFAULT_FRM_WIDTH;
	nPadHeight=DEFAULT_FRM_HEIGHT;
	OMX_INIT_STRUCT(&sOutCrop, OMX_CONFIG_RECTTYPE);
	//sOutCrop.nPortIndex = VPUDEC_OUT_PORT;
	sOutCrop.nLeft = sOutCrop.nTop = 0;
	sOutCrop.nWidth = sInFmt.nFrameWidth;
	sOutCrop.nHeight = sInFmt.nFrameHeight;

	nFrameWidthStride=-1;	//default: invalid
	nFrameHeightStride=-1;
	nFrameMaxCnt=-1;

	pInBuffer=(OMX_PTR)INVALID;
	nInSize=0;
	bInEos=OMX_FALSE;

	//pOutBuffer=NULL;
	//bOutLast=OMX_FALSE;

	eVpuDecoderState=VPU_COM_STATE_NONE;

	pLastOutVirtBuf=NULL;

	nFreeOutBufCntDec=0;
	nFreeOutBufCntPost=0;

	nNeedFlush=0;
	nNeedSkip=0;

	fsl_osal_memset(&sMemOperator,0,sizeof(OMX_PARAM_MEM_OPERATOR));

	ePlayMode=DEC_FILE_MODE;
	nChromaAddrAlign=CHROMA_ALIGN;
	pClock=NULL;
	nCapability=0;

	nMaxDurationMsThr=-1;
	nMaxBufCntThr=-1;

	nOutBufferCntPost=DEFAULT_BUF_OUT_POST_ZEROCNT;
	nOutBufferCntDec=DEFAULT_BUF_OUT_DEC_CNT;
	bEnabledPostProcess=OMX_FALSE;
	nPostMotion=MED_MOTION;  //default
	pPostThreadId=NULL;
	pPostInQueue=NULL;
	pPostOutQueue=NULL;
	pPostInReturnQueue=NULL;
	pPostOutReturnQueue=NULL;
	pPostCmdSem=NULL;
	pPostMutex=NULL;

	bReorderDisabled=OMX_FALSE;
    bDropPB = OMX_FALSE;
	return OMX_ErrorNone;
}

VpuDecoder::VpuDecoder()
{
	VPU_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	// version info
	//fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.video_decoder.vpu.v2");
	ComponentVersion.s.nVersionMajor = 0x0;
	ComponentVersion.s.nVersionMinor = 0x1;
	ComponentVersion.s.nRevision = 0x0;
	ComponentVersion.s.nStep = 0x0;

	//set default
	SetDefaultSetting();

	//return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::SetRoleFormat(OMX_STRING role)
{
	OMX_VIDEO_CODINGTYPE CodingType = OMX_VIDEO_CodingUnused;
	//OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

	VPU_COMP_API_LOG("%s: role: %s \r\n",__FUNCTION__,role);

	if(fsl_osal_strcmp(role, "video_decoder.mpeg2") == 0)
	{
		CodingType = OMX_VIDEO_CodingMPEG2;
		eFormat=VPU_V_MPEG2;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_MPEG2DEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.h263") == 0)
	{
		CodingType = OMX_VIDEO_CodingH263;
		eFormat=VPU_V_H263;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_H263DEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.sorenson") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_SORENSON263;//OMX_VIDEO_CodingH263;
		eFormat=VPU_V_H263;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_SORENSONDEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.mpeg4") == 0)
	{
		CodingType = OMX_VIDEO_CodingMPEG4;
		eFormat=VPU_V_MPEG4;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_MPEG4DEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.wmv9") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE )OMX_VIDEO_CodingWMV9;
		//CodingType = OMX_VIDEO_CodingWMV; ???
		//1 we need to use SetParameter() to set eFormat  !!!
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_WMVDEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.rv") == 0)
	{
		CodingType = OMX_VIDEO_CodingRV;
		eFormat=VPU_V_RV;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_RVDEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.avc") == 0)
	{
		CodingType = OMX_VIDEO_CodingAVC;
		eFormat=VPU_V_AVC;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_AVCDEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.divx") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingDIVX;
		eFormat=VPU_V_DIVX56;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_DIVXDEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.div3") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingDIV3;
		eFormat=VPU_V_DIVX3;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_DIV3DEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.div4") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingDIV4;
		eFormat=VPU_V_DIVX4;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_DIV4DEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.xvid") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingXVID;
		eFormat=VPU_V_XVID;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_XVIDDEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.mjpeg") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingMJPEG;
		eFormat=VPU_V_MJPG;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_MJPEGDEC);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.avs") == 0)
	{
		//1 CodingType = (OMX_VIDEO_CODINGTYPE);
		VPU_COMP_ERR_LOG("%s: failure: avs unsupported: %s \r\n",__FUNCTION__,role);
		eFormat=VPU_V_AVS;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_AVS);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.vp8") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP8;
		eFormat=VPU_V_VP8;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_VP8);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.vpx") == 0)
	{
		CodingType = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVPX;
		eFormat=VPU_V_VP8;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_VPX);
	}
	else if(fsl_osal_strcmp(role, "video_decoder.mvc") == 0)
	{
		//1 CodingType = (OMX_VIDEO_CODINGTYPE);
		VPU_COMP_ERR_LOG("%s: failure: mvc unsupported: %s \r\n",__FUNCTION__,role);
		eFormat=VPU_V_AVC_MVC;
		fsl_osal_strcpy((fsl_osal_char*)name, VPU_COMP_NAME_MVC);
	}
	else
	{
		CodingType = OMX_VIDEO_CodingUnused;
		//eFormat=;
		VPU_COMP_ERR_LOG("%s: failure: unknow role: %s \r\n",__FUNCTION__,role);
		return OMX_ErrorUndefined;
	}

	//check input change
	if(sInFmt.eCompressionFormat!=CodingType)
	{
		sInFmt.eCompressionFormat=CodingType;
		InputFmtChanged();
	}
#if 0
	OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
	sPortDef.nPortIndex = VPUDEC_IN_PORT;
	ports[VPUDEC_IN_PORT]->GetPortDefinition(&sPortDef);
	sPortDef.format.video.eCompressionFormat = CodingType;
	ports[VPUDEC_IN_PORT]->SetPortDefinition(&sPortDef);
#endif

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::GetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
	VPU_COMP_API_LOG("%s: nParamIndex=0x%X, \r\n",__FUNCTION__,nParamIndex);

	//check state
	switch(eVpuDecoderState)
	{
		default:
			break;
	}

	if(NULL==pComponentParameterStructure)
	{
		VPU_COMP_ERR_LOG("%s: failure: param is null \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

	if(nParamIndex==OMX_IndexParamStandardComponentRole)
	{
		fsl_osal_strcpy((OMX_STRING)((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole,(OMX_STRING)cRole);
		return OMX_ErrorNone;
	}
	else if(nParamIndex ==  OMX_IndexParamVideoWmv)
	{
		OMX_VIDEO_PARAM_WMVTYPE  *pPara;
		pPara = (OMX_VIDEO_PARAM_WMVTYPE *)pComponentParameterStructure;
		switch(eFormat)
		{
			case VPU_V_VC1:
				pPara->eFormat = OMX_VIDEO_WMVFormat9;
				break;
			default:
				pPara->eFormat = OMX_VIDEO_WMVFormatUnused;
				break;
		}
		return OMX_ErrorNone;
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
			{ OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level50 },
			{ OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level60 },
			{ OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level70 },
			{ OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level10 },
			{ OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level20 },
			{ OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level30 },
			{ OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level45 },
			{ OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level50 },
			{ OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level60 },
			{ OMX_VIDEO_H263ProfileISWV2,    OMX_VIDEO_H263Level70 },
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
			{ OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel41 },
			{ OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel42 },
			{ OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel5  },
			{ OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel51 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel1  },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel1b },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel11 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel12 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel13 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel2  },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel21 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel22 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel3  },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel31 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel32 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel4  },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel41 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel42 },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel5  },
			{ OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel51 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel1  },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel1b },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel11 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel12 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel13 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel2  },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel21 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel22 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel3  },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel31 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel32 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel4  },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel41 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel42 },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel5  },
			{ OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel51 },
		};

		static const CodecProfileLevel kM4VProfileLevels[] = {
			{ OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0 },
			{ OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0b },
			{ OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level1 },
			{ OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level2 },
			{ OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level3 },
			{ OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level0 },
			{ OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level0b },
			{ OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level1 },
			{ OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level2 },
			{ OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level3 },
			{ OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level4 },
			{ OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level4a },
			{ OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level5 },
		};

		static const CodecProfileLevel kMpeg2ProfileLevels[] = {
			{ OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelLL },
			{ OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelML },
			{ OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelH14},
			{ OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelHL},
			{ OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelLL },
			{ OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelML },
			{ OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelH14},
			{ OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelHL},
		};

		OMX_VIDEO_PARAM_PROFILELEVELTYPE  *pPara;
		OMX_S32 index;
		OMX_S32 nProfileLevels;

		pPara = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
		switch(eFormat)
		{
			case VPU_V_H263:
				index = pPara->nProfileIndex;

				nProfileLevels =sizeof(kH263ProfileLevels) / sizeof(kH263ProfileLevels[0]);
				if (index >= nProfileLevels) {
					return OMX_ErrorNoMore;
				}

				pPara->eProfile = kH263ProfileLevels[index].mProfile;
				pPara->eLevel = kH263ProfileLevels[index].mLevel;
				break;
			case VPU_V_AVC:
				index = pPara->nProfileIndex;

				nProfileLevels =sizeof(kProfileLevels) / sizeof(kProfileLevels[0]);
				if (index >= nProfileLevels) {
					return OMX_ErrorNoMore;
				}

				pPara->eProfile = kProfileLevels[index].mProfile;
				pPara->eLevel = kProfileLevels[index].mLevel;
				break;
			case VPU_V_MPEG4:
				index = pPara->nProfileIndex;

				nProfileLevels =sizeof(kM4VProfileLevels) / sizeof(kM4VProfileLevels[0]);
				if (index >= nProfileLevels) {
					return OMX_ErrorNoMore;
				}

				pPara->eProfile = kM4VProfileLevels[index].mProfile;
				pPara->eLevel = kM4VProfileLevels[index].mLevel;
				break;
			case VPU_V_MPEG2:
				index = pPara->nProfileIndex;

				nProfileLevels =sizeof(kMpeg2ProfileLevels) / sizeof(kMpeg2ProfileLevels[0]);
				if (index >= nProfileLevels) {
					return OMX_ErrorNoMore;
				}

				pPara->eProfile = kMpeg2ProfileLevels[index].mProfile;
				pPara->eLevel = kMpeg2ProfileLevels[index].mLevel;
				break;
			default:
				return OMX_ErrorUnsupportedIndex;
				//break;
		}
		return OMX_ErrorNone;
	}
	else
	{
		VPU_COMP_ERR_LOG("%s: failure: unsupported index: 0x%X \r\n",__FUNCTION__,nParamIndex);
		return OMX_ErrorUnsupportedIndex;
	}
}

OMX_ERRORTYPE VpuDecoder::SetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
	OMX_ERRORTYPE ret = OMX_ErrorNone;
	VPU_COMP_API_LOG("%s: nParamIndex=0x%X \r\n",__FUNCTION__,nParamIndex);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_LOADED:
			break;
		case VPU_COM_STATE_OPENED:
			if(nParamIndex ==  OMX_IndexParamVideoWmv)
			{
				break;
			}
		default:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d, nParamIndex=0x%X, role=%s \r\n",__FUNCTION__,eVpuDecoderState,nParamIndex,(OMX_STRING)cRole);
			return OMX_ErrorIncorrectStateTransition;
	}


	if(NULL==pComponentParameterStructure)
	{
		VPU_COMP_ERR_LOG("%s: failure: param is null \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

	if(nParamIndex==OMX_IndexParamStandardComponentRole)
	{
		fsl_osal_strcpy( (fsl_osal_char *)cRole,(fsl_osal_char *)((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole);
		if(OMX_ErrorNone!=SetRoleFormat((OMX_STRING)cRole))
		{
			VPU_COMP_ERR_LOG("%s: set role format failure \r\n",__FUNCTION__);
			return OMX_ErrorBadParameter;
		}
	}
	else if(nParamIndex ==  OMX_IndexParamVideoWmv)
	{
		OMX_VIDEO_PARAM_WMVTYPE  *pPara;
		pPara = (OMX_VIDEO_PARAM_WMVTYPE *)pComponentParameterStructure;

		switch((int)(pPara->eFormat))
		{
			case OMX_VIDEO_WMVFormat7:
			case OMX_VIDEO_WMVFormat8:
			case OMX_VIDEO_WMVFormat9a:
				ret = OMX_ErrorBadParameter;
				break;
			case OMX_VIDEO_WMVFormat9:
				eFormat = VPU_V_VC1;
				VPU_COMP_LOG("%s: set OMX_VIDEO_WMVFormat9 \r\n",__FUNCTION__);
				break;
			case OMX_VIDEO_WMVFormatWVC1:
				eFormat = VPU_V_VC1_AP;
				VPU_COMP_LOG("%s: set OMX_VIDEO_WMVFormatWVC1 \r\n",__FUNCTION__);
				break;
			default:
				//eFormat = ;
				VPU_COMP_ERR_LOG("%s: failure: unsupported format: 0x%X \r\n",__FUNCTION__,pPara->eFormat);
				ret = OMX_ErrorBadParameter;
				break;
		}
	}
	else if (nParamIndex==OMX_IndexParamMemOperator)
	{
		//should be set before open vpu(eg. malloc bitstream/frame buffers)
		if(VPU_COM_STATE_LOADED!=eVpuDecoderState)
		{
			ret=OMX_ErrorInvalidState;
		}
		else
		{
			OMX_PARAM_MEM_OPERATOR * pPara;
			pPara=(OMX_PARAM_MEM_OPERATOR *)pComponentParameterStructure;
			VPU_COMP_LOG("%s: set OMX_IndexParamMemOperator \r\n",__FUNCTION__);
			sMemOperator=*pPara;
		}
	}
	else if(nParamIndex==OMX_IndexParamDecoderPlayMode)
	{
		OMX_DECODE_MODE* pMode=(OMX_DECODE_MODE*)pComponentParameterStructure;
		ePlayMode=*pMode;
		VPU_COMP_LOG("%s: set OMX_IndexParamDecoderPlayMode: %d \r\n",__FUNCTION__,*pMode);
	}
	else if(nParamIndex==OMX_IndexParamVideoDecChromaAlign)
	{
		OMX_U32* pAlignVal=(OMX_U32*)pComponentParameterStructure;
		nChromaAddrAlign=*pAlignVal;
		VPU_COMP_LOG("%s: set OMX_IndexParamVideoDecChromaAlign: %d \r\n",__FUNCTION__,nChromaAddrAlign);
		if(nChromaAddrAlign==0) nChromaAddrAlign=1;
	}
	else if(nParamIndex==OMX_IndexParamDecoderCachedThreshold)
	{
		OMX_DECODER_CACHED_THR* pDecCachedInfo=(OMX_DECODER_CACHED_THR*)pComponentParameterStructure;
		if(pDecCachedInfo->nPortIndex==IN_PORT)
		{
			nMaxDurationMsThr=pDecCachedInfo->nMaxDurationMsThreshold;
			nMaxBufCntThr=pDecCachedInfo->nMaxBufCntThreshold;
			VPU_COMP_LOG("%s: set OMX_IndexParamDecoderCachedThreshold: max duration(ms) threshold : %d, max buf cnt threshold: %d \r\n",__FUNCTION__,nMaxDurationMsThr,nMaxBufCntThr);
		}
	}
	else if(nParamIndex==OMX_IndexParamVideoRegisterFrameExt){
		OMX_VIDEO_REG_FRM_EXT_INFO* pExtInfo=(OMX_VIDEO_REG_FRM_EXT_INFO*)pComponentParameterStructure;
		if(pExtInfo->nPortIndex==OUT_PORT){
			nFrameWidthStride=Align(pExtInfo->nWidthStride, FRAME_ALIGN);
			nFrameHeightStride=Align(pExtInfo->nHeightStride, FRAME_ALIGN);
			nFrameMaxCnt=(pExtInfo->nMaxBufferCnt<=VPU_DEC_MAX_NUM_MEM)?pExtInfo->nMaxBufferCnt:VPU_DEC_MAX_NUM_MEM;
		VPU_COMP_LOG("%s: set OMX_IndexParamVideoFrameStride: width stride : %d, height stride: %d, max count: %d \r\n",__FUNCTION__,nFrameWidthStride,nFrameHeightStride,nFrameMaxCnt);
		}
	}
	else if(nParamIndex==OMX_IndexParamVideoDecReorderDisable){
		OMX_DECODER_REORDER * pReorderInfo=(OMX_DECODER_REORDER *)pComponentParameterStructure;
		bReorderDisabled=pReorderInfo->bDisable;
		VPU_COMP_LOG("%s: set OMX_IndexParamVideoDecReorderDisable: disabled: %d \r\n",__FUNCTION__,bReorderDisabled);
	}
	else
	{
		VPU_COMP_ERR_LOG("%s: failure: unsupported index: 0x%X \r\n",__FUNCTION__,nParamIndex);
		ret = OMX_ErrorUnsupportedIndex;
	}

	return ret;
}

OMX_ERRORTYPE VpuDecoder::GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure)
{
	VPU_COMP_API_LOG("%s: nParamIndex=0x%X, \r\n",__FUNCTION__,nParamIndex);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
		case VPU_COM_STATE_LOADED:
		case VPU_COM_STATE_OPENED:	//allow user get wrong value before opened ???
			//forbidden
			VPU_COMP_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	if(NULL==pComponentParameterStructure)
	{
		VPU_COMP_ERR_LOG("%s: failure: param is null  \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

	switch ((int)nParamIndex)
	{
		case OMX_IndexConfigCommonOutputCrop:
			{
				OMX_CONFIG_RECTTYPE *pRecConf = (OMX_CONFIG_RECTTYPE*)pComponentParameterStructure;
				if(pRecConf->nPortIndex == VPUDEC_OUT_PORT)
				{
					pRecConf->nTop = sOutCrop.nTop;
					pRecConf->nLeft = sOutCrop.nLeft;
					pRecConf->nWidth = sOutCrop.nWidth;
					pRecConf->nHeight = sOutCrop.nHeight;
					VPU_COMP_LOG("%s: [top,left,width,height]=[%d,%d,%d,%d], \r\n",__FUNCTION__,(INT32)pRecConf->nTop,(INT32)pRecConf->nLeft,(INT32)pRecConf->nWidth,(INT32)pRecConf->nHeight);
				}
			}
			break;
		case OMX_IndexConfigCommonScale:
			{
				OMX_CONFIG_SCALEFACTORTYPE *pDispRatio = (OMX_CONFIG_SCALEFACTORTYPE *)pComponentParameterStructure;
				if(pDispRatio->nPortIndex == VPUDEC_OUT_PORT)
				{
					pDispRatio->xWidth = sDispRatio.xWidth;
					pDispRatio->xHeight = sDispRatio.xHeight;
				}
			}
			break;
		case OMX_IndexConfigVideoOutBufPhyAddr:
			{
				OMX_CONFIG_VIDEO_OUTBUFTYPE * param = (OMX_CONFIG_VIDEO_OUTBUFTYPE *)pComponentParameterStructure;
				if(OMX_ErrorNone!=GetHwBuffer(param->pBufferHdr->pBuffer,&param->nPhysicalAddr))
				{
					return OMX_ErrorBadParameter;
				}
			}
			break;
		default:
			VPU_COMP_ERR_LOG("%s: failure: unsupported index: 0x%X \r\n",__FUNCTION__,nParamIndex);
			return OMX_ErrorUnsupportedIndex;
			//break;
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::SetConfig(OMX_INDEXTYPE nIndex, OMX_PTR pComponentConfigStructure)
{
	OMX_ERRORTYPE eRetVal = OMX_ErrorNone;
	OMX_CONFIG_CLOCK *pC;
	if(pComponentConfigStructure == NULL)
	{
		return OMX_ErrorBadParameter;

	}
	switch((int)nIndex)
	{
		case OMX_IndexConfigClock:
			pC = (OMX_CONFIG_CLOCK*) pComponentConfigStructure;
			pClock = pC->hClock;
			break;
        case OMX_IndexConfigVideoMediaTime:
        {
            OMX_CONFIG_VIDEO_MEDIA_TIME *pInfo = (OMX_CONFIG_VIDEO_MEDIA_TIME*)pComponentConfigStructure;
            SetMediaTime(pInfo->nTime);
            break;
        }
        case OMX_IndexConfigAndroidOperatingRate:
        {
            OMX_PARAM_U32TYPE *pInfo = (OMX_PARAM_U32TYPE*)pComponentConfigStructure;
            float speed = (float)pInfo->nU32 / (float)Q16_SHIFT;
            if(speed > 80.0){
                bDropPB = OMX_TRUE;
            }else
                bDropPB = OMX_FALSE;
            break;
        }
		default:
			return eRetVal;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::GetInputDataDepthThreshold(OMX_S32* pDurationThr, OMX_S32* pBufCntThr)
{
    /*
      for some application, such rtsp/http, we need to set some thresholds to avoid input data is consumed by decoder too fast.
      -1: no threshold
    */
    *pDurationThr=nMaxDurationMsThr;
    *pBufCntThr=nMaxBufCntThr;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::InitFilterComponent()
{
	VpuDecRetCode ret;
	//VpuVersionInfo ver;
	//VpuMemInfo sMemInfo;

	VPU_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
			break;
		default:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;

	}

	// init semaphore
#ifdef USE_PROCESS_SEM
	if(E_FSL_OSAL_SUCCESS!=VPU_COMP_SEM_INIT_PROCESS(&psemaphore)){
		VPU_COMP_ERR_LOG("%s: init semaphore failed ! \r\n",__FUNCTION__);
	}
#else
	if(E_FSL_OSAL_SUCCESS!=VPU_COMP_SEM_INIT(&psemaphore)){
		VPU_COMP_ERR_LOG("%s: init semaphore failed ! \r\n",__FUNCTION__);
	}
#endif

	//update state
	eVpuDecoderState=VPU_COM_STATE_LOADED;

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::DeInitFilterComponent()
{
	OMX_ERRORTYPE omx_ret=OMX_ErrorNone;
	VPU_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
		case VPU_COM_STATE_LOADED:
			break;
		default:
			//invalid state, DeInitFilter() is skipped : we need to close/unload vpu
			VPU_COMP_ERR_LOG("invalid state: %d, close vpu manually \r\n",eVpuDecoderState);
			ReleaseVpuSource();
			break;
	}

#ifdef USE_PROCESS_SEM
	VPU_COMP_SEM_DESTROY_PROCESS(psemaphore);
#else
	VPU_COMP_SEM_DESTROY(psemaphore);
	psemaphore=NULL;
#endif

	//update state
	eVpuDecoderState=VPU_COM_STATE_NONE;

	return omx_ret;
}

OMX_ERRORTYPE VpuDecoder::SetInputBuffer(OMX_PTR pBuffer, OMX_S32 nSize, OMX_BOOL bLast)
{
	pInBuffer=pBuffer;
	nInSize=nSize;
	bInEos=bLast;

	VPU_COMP_API_LOG("%s: state: %d, size: %d, last: %d \r\n",__FUNCTION__,eVpuDecoderState,(UINT32)nInSize,(UINT32)bInEos);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
		case VPU_COM_STATE_EOS:
			//if user want to repeat play, user should call the last getoutput (to make state change from eos to decode)
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	//check data length, we don't allow zero-length-buf
	if(0>=nInSize)
	{
		pInBuffer=NULL;
	}

	if(1==nNeedFlush)
	{
		VPU_COMP_LOG("flush internal !!!! \r\n");
		if(OMX_ErrorNone!=FlushFilter())
		{
			VPU_COMP_ERR_LOG("internal flush filter failure \r\n");
		}
		if(bEnabledPostProcess)	{
			//PostThreadFlushInput();
			PostThreadFlushOutput();  //flush output cover flush input
		}
		//nNeedFlush=0;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::SetOutputBuffer(OMX_PTR pBuffer)
{
	VpuDecRetCode ret;
	VpuDecOutFrameInfo * pFrameInfo;
	OMX_S32 nFrameNum;
	OMX_S32 index;
	OMX_PTR pPhyAddr;
	OMX_S32 nBufExist;

	VPU_COMP_API_LOG("%s: state: %d, buffer: 0x%X \r\n",__FUNCTION__,eVpuDecoderState,(UINT32)pBuffer);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
		case VPU_COM_STATE_EOS:
			//if user want to repeat play, user should call the last getoutput (to make state change from eos to decode)
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	//pOutBuffer=pBuffer;
	nBufExist=FramePoolBufExist(pBuffer,&sFramePoolInfo,&pPhyAddr,&index);
	if(nBufExist<0)
	{
		VPU_COMP_ERR_LOG("%s: failure: unvalid buffer: 0x%X \r\n",__FUNCTION__,(UINT32)pBuffer);
		return OMX_ErrorInsufficientResources;
	}
	else if(nBufExist==0)
	{
		//register frame buffer
		nFrameNum=FramePoolRegisterBuf(pBuffer,pPhyAddr,&sFramePoolInfo);
		if(-1==nFrameNum)
		{
			VPU_COMP_ERR_LOG("%s: register frame failure: frame pool is full ! \r\n",__FUNCTION__);
			return OMX_ErrorInsufficientResources;
		}
	}
	else
	{
		VpuDecoderFrmOwner eOwner;
		VpuDecoderFrmState eState;
		FramePoolGetBufProperty(&sFramePoolInfo,index,&eOwner,&eState,&pFrameInfo);
		switch(eOwner)
		{
			case VPU_COM_FRM_OWNER_DEC:
				if (eState==VPU_COM_FRM_STATE_OUT)
				{
					if(NULL!=pFrameInfo->pDisplayFrameBuf)
					{
						//clear displayed frame
						ret=FramePoolReturnPostFrameToVpu(&sFramePoolInfo, nHandle, index, psemaphore);
						if(VPU_DEC_RET_SUCCESS!=ret)
						{
							VPU_COMP_ERR_LOG("%s: vpu clear frame display failure: ret=0x%X \r\n",__FUNCTION__,ret);
							return OMX_ErrorHardware;
						}
					}
					else
					{
						//this is fake output frame which is stolen at some special steps, include EOS, flush,...
						VPU_COMP_LOG("application return one stolen buffer: 0x%X \r\n",pBuffer);
					}
					//update buffer state
					nFreeOutBufCntDec++;
					VPU_COMP_LOG("set output: 0x%X , nFreeOutBufCntDec: %d \r\n",(UINT32)pBuffer,(UINT32)nFreeOutBufCntDec);
					FramePoolSetBufState(&sFramePoolInfo,index,VPU_COM_FRM_STATE_FREE);
				}
				else
				{
					VPU_COMP_ERR_LOG("%s: failure: repeat setting output buffer: 0x%X \r\n",__FUNCTION__,(UINT32)pBuffer);
					return OMX_ErrorIncorrectStateOperation;
				}
				break;
			case VPU_COM_FRM_OWNER_POST:
				if (eState==VPU_COM_FRM_STATE_OUT)
				{
					nFreeOutBufCntPost++;
					VPU_COMP_LOG("set output: 0x%X , nFreeOutBufCntPost: %d \r\n",(UINT32)pBuffer,(UINT32)nFreeOutBufCntPost);
					FramePoolSetBufState(&sFramePoolInfo,index,VPU_COM_FRM_STATE_FREE);
					pPostOutReturnQueue->Add(&index);
					PostWakeUp(pPostMutex, &sPostCond, (volatile OMX_BOOL*) &bPostWaitingTasks);
				}
				else
				{
					VPU_COMP_ERR_LOG("%s: failure: repeat setting output buffer: 0x%X \r\n",__FUNCTION__,(UINT32)pBuffer);
					return OMX_ErrorIncorrectStateOperation;
				}
				break;
			default:
				VPU_COMP_ERR_LOG("%s: warning: find one isolated buffer: 0x%X, it will be discarded from pipeline !",__FUNCTION__,(UINT32)pBuffer);
				break;
		}
	}

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::InitFilter()
{
	VpuDecRetCode ret;
	VpuFrameBuffer frameBuf[MAX_FRAME_NUM];
	OMX_S32 BufNum;
	OMX_S32 nWidthStride,nHeightStride;

	VPU_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_DO_INIT:
			break;
		default:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
	}

	ASSERT(nOutBufferCnt<=MAX_FRAME_NUM);
	//vpu register step: fill frameBuf[]
	if(nFrameWidthStride<nPadWidth || nFrameHeightStride<nPadHeight){
		nWidthStride=nPadWidth;
		nHeightStride=nPadHeight;
	}
	else{
		/*user will allocate enough frame memory, vpu needn't report resolution change which will lead to much overhead*/
		nWidthStride=nFrameWidthStride;
		nHeightStride=nFrameHeightStride;
		VPU_DecDisCapability(nHandle,VPU_DEC_CAP_RESOLUTION_CHANGE);
	}
	BufNum=FramePoolCreateDecoderRegisterFrame(frameBuf, &sFramePoolInfo,nOutBufferCntDec,nWidthStride, nHeightStride, &sVpuMemInfo,&sMemOperator,sOutFmt.eColorFormat,nChromaAddrAlign);
	if(-1==BufNum)
	{
		VPU_COMP_ERR_LOG("%s: create register frame failure \r\n",__FUNCTION__);
		return OMX_ErrorInsufficientResources;
	}

	//vpu register step: register frame buffs
	VPU_COMP_SEM_LOCK(psemaphore);
	ret=VPU_DecRegisterFrameBuffer(nHandle, frameBuf, BufNum);
	VPU_COMP_SEM_UNLOCK(psemaphore);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu register frame failure: ret=0x%X \r\n",__FUNCTION__,ret);
		return OMX_ErrorHardware;
	}

	if(bEnabledPostProcess)
	{
		//post-process register step:
		BufNum=FramePoolRegisterPostFrame(&sFramePoolInfo,nOutBufferCntPost,pPostOutReturnQueue);
		if(-1==BufNum)
		{
			VPU_COMP_ERR_LOG("%s: register post frame failure \r\n",__FUNCTION__);
			return OMX_ErrorInsufficientResources;
		}
	}

	//update state
	eVpuDecoderState=VPU_COM_STATE_DO_DEC;

	//update buffer state
	nFreeOutBufCntDec=nOutBufferCntDec;
	nFreeOutBufCntPost=nOutBufferCntPost;

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::DeInitFilter()
{
	VpuDecRetCode ret;
	OMX_ERRORTYPE omx_ret=OMX_ErrorNone;
	VPU_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
		case VPU_COM_STATE_LOADED:
		//case VPU_COM_STATE_OPENED:
		//case VPU_COM_STATE_WAIT_FRM:
		//case VPU_COM_STATE_DO_INIT:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
		default:
			break;
	}

	omx_ret=ReleaseVpuSource();

	//clear handle
	nHandle=0;

	//restore default to support following switch from loaded to idle later.
	SetDefaultSetting();

	//update state
	eVpuDecoderState=VPU_COM_STATE_LOADED;

	return omx_ret;
}

FilterBufRetCode VpuDecoder::FilterOneBuffer()
{
	VpuDecRetCode ret;
	VpuBufferNode InData;
	OMX_S32 bufRetCode;
	FilterBufRetCode bufRet=FILTER_OK;

	OMX_U8* pBitstream;
	OMX_S32 readbytes;
	OMX_U8  dummy;
	OMX_S32 enableFileMode=0;
	OMX_S32 capability=0;
#ifdef VPU_COMP_DEBUG
	static OMX_U32 nNotUsedCnt=0;
	static OMX_U32 nOutFrameCnt=0;
	static FILE* fpBitstream=NULL;
#endif

	VPU_COMP_API_LOG("%s: state: %d, InBuf: 0x%X, data size: %d, bInEos: %d \r\n",__FUNCTION__,eVpuDecoderState,(UINT32)pInBuffer,(UINT32)nInSize,bInEos);

	if(1==nNeedSkip)
	{
		VPU_COMP_LOG("skip directly: we need to get one time stamp \r\n");
		nNeedSkip=0;
		return FILTER_SKIP_OUTPUT;
	}

RepeatPlay:
	//check state
	switch(eVpuDecoderState)
	{
		//forbidden state
		case VPU_COM_STATE_NONE:
		case VPU_COM_STATE_DO_INIT:
		case VPU_COM_STATE_DO_OUT:
		case VPU_COM_STATE_EOS:
			bufRet=FILTER_ERROR;
			VPU_COMP_ERR_LOG("%s: failure: error state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return bufRet;
		//allowed state
		case VPU_COM_STATE_WAIT_FRM:
			if(FramePoolBufNum(&sFramePoolInfo)>=(OMX_S32)nOutBufferCnt)
			{
				//ready to call InitFilter()
				eVpuDecoderState=VPU_COM_STATE_DO_INIT;
				bufRet =FILTER_DO_INIT;
			}
			else
			{
				//do nothing, wait for more output buffer
				bufRet=FILTER_NO_OUTPUT_BUFFER;
			}
			VPU_COMP_LOG("%s: waiting frames ready, return and do nothing \r\n",__FUNCTION__);
			return bufRet;//OMX_ErrorNone;
		case VPU_COM_STATE_LOADED:
			//load vpu
			VPU_COMP_SEM_LOCK(psemaphore);
			ret=VPU_DecLoad();
			VPU_COMP_SEM_UNLOCK(psemaphore);
			if (ret!=VPU_DEC_RET_SUCCESS)
			{
				VPU_COMP_ERR_LOG("%s: vpu load failure: ret=0x%X \r\n",__FUNCTION__,ret);
				return FILTER_ERROR;//OMX_ErrorHardware;
			}

			//version info
			VPU_COMP_SEM_LOCK(psemaphore);
			ret=VPU_DecGetVersionInfo(&sVpuVer);
			if (ret!=VPU_DEC_RET_SUCCESS)
			{
				VPU_COMP_ERR_LOG("%s: vpu get version failure: ret=0x%X \r\n",__FUNCTION__,ret);
				VPU_DecUnLoad();
				VPU_COMP_SEM_UNLOCK(psemaphore);
				return FILTER_ERROR;//OMX_ErrorHardware;
			}
			VPU_COMP_SEM_UNLOCK(psemaphore);
			VPU_COMP_LOG("vpu lib version : rel.major.minor=%d.%d.%d \r\n",sVpuVer.nLibRelease,sVpuVer.nLibMajor,sVpuVer.nLibMinor);
			VPU_COMP_LOG("vpu fw version : rel.major.minor=%d.%d.%d \r\n",sVpuVer.nFwRelease,sVpuVer.nFwMajor,sVpuVer.nFwMinor);

			//query memory
			VPU_COMP_SEM_LOCK(psemaphore);
			ret=VPU_DecQueryMem(&sMemInfo);
			VPU_COMP_SEM_UNLOCK(psemaphore);
			if (ret!=VPU_DEC_RET_SUCCESS)
			{
				VPU_COMP_ERR_LOG("%s: vpu query memory failure: ret=0x%X \r\n",__FUNCTION__,ret);
				return FILTER_ERROR;
			}
			//malloc memory for vpu wrapper
			if(0==MemMallocVpuBlock(&sMemInfo,&sVpuMemInfo,&sMemOperator))
			{
				VPU_COMP_ERR_LOG("%s: malloc memory failure: \r\n",__FUNCTION__);
				return FILTER_ERROR;
			}
			//open vpu
			enableFileMode=(ePlayMode==DEC_FILE_MODE)?1:0;
			if(OMX_ErrorNone!=OpenVpu(&nHandle, &sMemInfo, eFormat, sInFmt.nFrameWidth, sInFmt.nFrameHeight,sOutFmt.eColorFormat,enableFileMode,psemaphore,bReorderDisabled)) //1 sInFmt is valid ???
			{
				VPU_COMP_ERR_LOG("%s: open vpu failure \r\n",__FUNCTION__);
				return FILTER_ERROR;
			}
			//check capability
			VPU_COMP_SEM_LOCK(psemaphore);
			ret=VPU_DecGetCapability(nHandle, VPU_DEC_CAP_FRAMESIZE, (INT32*)&capability);
			if((ret==VPU_DEC_RET_SUCCESS)&&capability)
			{
				nCapability|=VPU_COM_CAPABILITY_FRMSIZE;
				bFilterSupportFrmSizeRpt=OMX_TRUE;
			}
			VPU_COMP_SEM_UNLOCK(psemaphore);
			//update state
			eVpuDecoderState=VPU_COM_STATE_OPENED;
		case VPU_COM_STATE_OPENED:
			break;
		case VPU_COM_STATE_DO_DEC:
			if(bEnabledPostProcess)
			{
				OMX_S32 index;
				//return frame buffer to vpu as possible
				while(pPostInReturnQueue->Size()>0){
					pPostInReturnQueue->Get(&index);
					VPU_COMP_LOG("post-process: return vpu index: %d \r\n",index);
					FramePoolReturnPostFrameToVpu(&sFramePoolInfo, nHandle, index, psemaphore);
					nFreeOutBufCntDec++;
				}
				//notify user get output if output ready in post-process output port
				if(pPostOutQueue->Size()>0)
				{
					//printf("notify user get one post out : cnt: %d \r\n",pPostOutQueue->Size());
					eVpuDecoderState=VPU_COM_STATE_DO_OUT;
					return FILTER_HAS_OUTPUT;
				}
				//no enough output frame for post-process
				if(nFreeOutBufCntPost<FRAME_POST_MIN_FREE_THD)
				{
					return FILTER_NO_OUTPUT_BUFFER;
				}
			}
			else
			{
				//if(nFreeOutBufCntDec<=0)	//1 not enough, may hang up ???
				if((OMX_S32)nFreeOutBufCntDec<=(OMX_S32)(Max((OMX_S32)FRAME_MIN_FREE_THD,0)))
				{
					//notify user release outputed buffer
					//printf("%s: no output buffer, do nothing, current free buf numbers: %d \r\n",__FUNCTION__,(INT32)nFreeOutBufCntDec);
					return FILTER_NO_OUTPUT_BUFFER;
				}
			}
			break;
		case VPU_COM_STATE_RE_WAIT_FRM:
			if(bEnabledPostProcess)
			{
				//no enough output frame for post-process
				if(nFreeOutBufCntPost<FRAME_POST_MIN_FREE_THD)
				{
					return FILTER_NO_OUTPUT_BUFFER;
				}
			}
			//FIXME: for iMX6(now its major version==2), we needn't to return all frame buffers.
			if((sVpuVer.nFwMajor!=2)&&(sVpuVer.nFwMajor!=3))
			{
				//need to user SetOutputBuffer() for left buffers, otherwise vpu may return one buffer which is not set by user
				if((OMX_U32)(nFreeOutBufCntDec+nFreeOutBufCntPost)<nOutBufferCnt)
				{
					VPU_COMP_LOG("%s: no output buffer, do nothing, current free buf numbers: %d \r\n",__FUNCTION__,(INT32)nFreeOutBufCntDec);
					return FILTER_NO_OUTPUT_BUFFER;
				}
			}
			else
			{
				//for  iMX6: needn't return all frame buffers except those who are stolen in flush/eos step.
				OMX_S32 stolenNum;
				stolenNum=FramePoolStolenDecoderBufNum(&sFramePoolInfo);
				if(stolenNum>0)
				{
					VPU_COMP_LOG("stolen buffer cnt isn't zero: %d \r\n",stolenNum);
					return FILTER_NO_OUTPUT_BUFFER;
				}
			}
			eVpuDecoderState=VPU_COM_STATE_DO_DEC;
			goto RepeatPlay;
			break;
		case VPU_COM_STATE_WAIT_POST_EOS:
			if(bEnabledPostProcess)
			{
				OMX_S32 index;
				//return frame buffer to vpu as possible
				while(pPostInReturnQueue->Size()>0){
					pPostInReturnQueue->Get(&index);
					VPU_COMP_LOG("post-process: return vpu index: %d \r\n",index);
					FramePoolReturnPostFrameToVpu(&sFramePoolInfo, nHandle, index, psemaphore);
					nFreeOutBufCntDec++;
				}
				//notify user get output if output ready in post-process output port
				if(pPostOutQueue->Size()>0)
				{
					//printf("notify user get one post out : cnt: %d \r\n",pPostOutQueue->Size());
					pPostOutQueue->Access(&index, 0);
					if((index>>16)&POST_INDEX_EOS)
					{
						eVpuDecoderState=VPU_COM_STATE_EOS;
						return FILTER_LAST_OUTPUT;
					}
					return FILTER_HAS_OUTPUT;
				}
				//no enough output frame for post-process
				if(nFreeOutBufCntPost<FRAME_POST_MIN_FREE_THD)
				{
					return FILTER_NO_OUTPUT_BUFFER;
				}
			}
			return FILTER_OK;
		//unknow state
		default:
			VPU_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return FILTER_ERROR;
	}

	//for all codecs
	pBitstream=(pInBuffer==(OMX_PTR)INVALID)?NULL:(OMX_U8*)pInBuffer;
	readbytes=nInSize;

	//check eos or null data
	if(pBitstream==NULL)
	{
		if(bInEos==OMX_TRUE)
		{
			//create and send EOS data (with length=0)
			pBitstream=&dummy;
			readbytes=0;
			//bInEos=OMX_FALSE;
		}
		else
		{
			//no new input setting , return directly
			//VPU_COMP_LOG("%s: pInBuffer==NULL, set consumed and return directly(do nothing)  \r\n",__FUNCTION__);
			//bufRet=(FilterBufRetCode)(bufRet|FILTER_INPUT_CONSUMED);
			//return FILTER_OK;//OMX_ErrorNone;
		}
	}

	//EOS: 		0==readbytesp && Bitstream!=NULL
	//non-EOS: 	0==readbytesp && Bitstream==NULL

	//reset bOutLast for any non-zero data ??
	//bOutLast=OMX_FALSE;

	//seq init
	//decode bitstream buf
	VPU_COMP_LOG("%s: pBitstream=0x%X, readbytes=%d  \r\n",__FUNCTION__,(UINT32)pBitstream,(INT32)readbytes);
	InData.nSize=readbytes;
	InData.pPhyAddr=NULL;
	InData.pVirAddr=pBitstream;
	InData.sCodecData.pData=(OMX_U8*)pCodecData;
	InData.sCodecData.nSize=nCodecDataLen;

#ifdef VPU_DEC_COMP_DROP_B
#if 0  //FIXME: if disable drop B for some tough interlaced clips(1080p), it will impact the performance heavily
        if((bEnabledPostProcess==OMX_FALSE)||(HIGH_MOTION==nPostMotion)) //post thread may detect frame/field type context, so shouldn't drop any fra
#endif
    if(bDropPB){
        if(OMX_ErrorNone != ConfigVpu(nHandle,VPU_DEC_CONF_SKIPMODE,VPU_DEC_SKIPPB,psemaphore))
            return FILTER_ERROR;
    }else
    {
        OMX_TICKS nTimeStamp;
        nTimeStamp=QueryStreamTs();
        if(nTimeStamp >= 0)
        {
            if(OMX_ErrorNone!=CheckDropB(nHandle,nTimeStamp,pClock,psemaphore))
            {
                return FILTER_ERROR;
            }
        }
    }
#endif

	VPU_COMP_SEM_LOCK(psemaphore);
	ret=VPU_DecDecodeBuf(nHandle, &InData,(INT32*)&bufRetCode);
	//VPU_COMP_SEM_UNLOCK(&sSemaphore);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu dec buf failure: ret=0x%X \r\n",__FUNCTION__,ret);
		if(VPU_DEC_RET_FAILURE_TIMEOUT==ret)
		{
			//VPU_COMP_SEM_LOCK(psemaphore);
			VPU_DecReset(nHandle);
			SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
		}
		VPU_COMP_SEM_UNLOCK(psemaphore);
		return FILTER_ERROR; //OMX_ErrorHardware;
	}
	VPU_COMP_SEM_UNLOCK(psemaphore);
	VPU_COMP_LOG("%s: bufRetCode: 0x%X  \r\n",__FUNCTION__,bufRetCode);
	//check input buff
	if(bufRetCode&VPU_DEC_INPUT_USED)
	{
#ifdef VPU_COMP_DEBUG
		FileDumpBitstrem(&fpBitstream,pBitstream,readbytes);
#endif

		if(pInBuffer!=(OMX_PTR)INVALID)
		{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_INPUT_CONSUMED);
			pInBuffer=(OMX_PTR)INVALID;  //clear input
			nInSize=0;
		}
	}
	else
	{
		//not used
		VPU_COMP_LOG("%s: not used  \r\n",__FUNCTION__);
#ifdef VPU_COMP_DEBUG
		nNotUsedCnt++;
		if(nNotUsedCnt>MAX_NULL_LOOP)
		{
			VPU_COMP_ERR_LOG("%s: too many(%d times) null loop: return failure \r\n",__FUNCTION__, (INT32)nNotUsedCnt);
			return FILTER_ERROR;
		}
#endif
	}

	//check init info
	if(bufRetCode&VPU_DEC_INIT_OK)
	{
		FilterBufRetCode ret;
		ret=ProcessVpuInitInfo();
		bufRet=(FilterBufRetCode)((OMX_U32)bufRet|(OMX_U32)ret);
		return bufRet;
	}

	//check resolution change
	if(bufRetCode&VPU_DEC_RESOLUTION_CHANGED)
	{
		/*in such case:
		   (1) the frames which haven't been output (in vpu or post-process) will be discard, So, video may be not complete continuous.
		   (2) user need to re-allocate buffers, so buffer state will be meaningless.
		   (3) the several frames dropped may affect timestamp if user don't use decoded handle to map frames between decoded and display ?
		 */
		FilterBufRetCode ret;
		VPU_COMP_LOG("resolution changed \r\n");
		//release post-process module
		if(bEnabledPostProcess)
		{
			//stop/release post-process thread
			if(pPostThreadId != NULL) {
				PostThreadStop();
				fsl_osal_thread_destroy(pPostThreadId); pPostThreadId=NULL;
			}
			if(pPostInQueue){
				pPostInQueue->Free();
				FSL_DELETE(pPostInQueue);
			}
			if(pPostOutQueue){
				pPostOutQueue->Free();
				FSL_DELETE(pPostOutQueue);
			}
			if(pPostInReturnQueue){
				pPostInReturnQueue->Free();
				FSL_DELETE(pPostInReturnQueue);
			}
			if(pPostOutReturnQueue){
				pPostOutReturnQueue->Free();
				FSL_DELETE(pPostOutReturnQueue);
			}
			if(pPostCmdSem){
				fsl_osal_sem_destroy(pPostCmdSem); pPostCmdSem=NULL;
			}
			if(pPostMutex!= NULL){
				fsl_osal_mutex_destroy(pPostMutex); pPostMutex=NULL;
				//fsl_osal_cond_destroy(sPostCond);
				pthread_cond_destroy(&sPostCond);
			}
			PostProcessIPUDeinit(&sIpuHandle);
		}

		//get new init info to re-set some variables
		ret=ProcessVpuInitInfo();
		bufRet=(FilterBufRetCode)((OMX_U32)bufRet|(OMX_U32)ret);
		ASSERT(eVpuDecoderState==VPU_COM_STATE_WAIT_FRM);  //user need to re-register frames

		//later, user will call port disable -> FlushOutputBuffer -> sFramePoolInfo will be cleared
		return bufRet;
	}

	//check decoded info
	if(nCapability&VPU_COM_CAPABILITY_FRMSIZE)
	{
		if(bufRetCode&VPU_DEC_ONE_FRM_CONSUMED)
		{
			VPU_COMP_LOG("one frame is decoded \r\n");
			bufRet=(FilterBufRetCode)(bufRet|FILTER_ONE_FRM_DECODED);
		}
	}

	//check output buff
	//VPU_COMP_LOG("%s: bufRetCode=0x%X \r\n",__FUNCTION__,bufRetCode);
	if(bufRetCode&VPU_DEC_OUTPUT_DIS)
	{
		bufRet=(FilterBufRetCode)(bufRet|FILTER_HAS_OUTPUT);
		eVpuDecoderState=VPU_COM_STATE_DO_OUT;
#ifdef VPU_COMP_DEBUG
		nOutFrameCnt++;
		if(nOutFrameCnt>MAX_DEC_FRAME)
		{
			VPU_COMP_ERR_LOG("already output %d frames, return failure \r\n",(INT32)nOutFrameCnt);
			return FILTER_ERROR;
		}
#endif
		nOutBufferSize=sOutFmt.nFrameWidth * sOutFmt.nFrameHeight*pxlfmt2bpp(sOutFmt.eColorFormat)/8;
	}
	else if (bufRetCode&VPU_DEC_OUTPUT_MOSAIC_DIS)
	{
		bufRet=(FilterBufRetCode)(bufRet|FILTER_HAS_OUTPUT);
		eVpuDecoderState=VPU_COM_STATE_DO_OUT;
		//send out one frame with length=0
		nOutBufferSize=0;
	}
	else if (bufRetCode&VPU_DEC_OUTPUT_EOS)
	{
		bufRet=(FilterBufRetCode)(bufRet|FILTER_LAST_OUTPUT);
		//bOutLast=OMX_TRUE;
		eVpuDecoderState=VPU_COM_STATE_EOS;
	}
	//else if (bufRetCode&VPU_DEC_OUTPUT_NODIS)
	//{
	//	bufRet=(FilterBufRetCode)(bufRet|FILTER_SKIP_OUTPUT);
	//}
	else if (bufRetCode&VPU_DEC_OUTPUT_REPEAT)
	{
		bufRet=(FilterBufRetCode)(bufRet|FILTER_SKIP_OUTPUT);
	}
	else if (bufRetCode&VPU_DEC_OUTPUT_DROPPED)
	{
		bufRet=(FilterBufRetCode)(bufRet|FILTER_SKIP_OUTPUT);
	}
	else
	{
		//bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_OUTPUT);
	}

	//check "no enough buf"
	if(bufRetCode&VPU_DEC_NO_ENOUGH_BUF)
	{
		//FIXME: if have one output , we ignore this "not enough buf" flag, since video filter may only check one bit among them
		//only consider VPU_DEC_OUTPUT_MOSAIC_DIS/VPU_DEC_OUTPUT_DIS/VPU_DEC_OUTPUT_NODIS
		if(bufRetCode&VPU_DEC_OUTPUT_NODIS)
		{
			if(bEnabledPostProcess)
			{
				//do nothing
#if 0
				OMX_S32 index;
				//printf("vpu no frame !!!!!!!!!!!!post occupy cnt: %d , post out cnt: %d \r\n",pPostInReturnQueue->Size(),pPostOutQueue->Size());
				//return frame buffer to vpu as possible
				while(pPostInReturnQueue->Size()>0){
					pPostInReturnQueue->Get(&index);
					VPU_COMP_LOG("post-process: return vpu index: %d \r\n",index);
					FramePoolReturnPostFrameToVpu(&sFramePoolInfo, nHandle, index, psemaphore);
					nFreeOutBufCntDec++;
				}
				//no enough output frame for post-process
				if(nFreeOutBufCntPost<=FRAME_POST_MIN_FREE_THD)
				{
					bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_OUTPUT_BUFFER);
				}
#endif
				if((0==pPostInReturnQueue->Size())&&(0==pPostInQueue->Size())){
					//for frame+interlaced clips: we still need to notify user return related vpu index
					bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_OUTPUT_BUFFER);
				}
			}
			else
			{
				bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_OUTPUT_BUFFER);
			}
		}
		else if(bufRetCode&VPU_DEC_OUTPUT_DIS)
		{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_HAS_OUTPUT);
			eVpuDecoderState=VPU_COM_STATE_DO_OUT;
			nOutBufferSize=sOutFmt.nFrameWidth * sOutFmt.nFrameHeight*pxlfmt2bpp(sOutFmt.eColorFormat)/8;
		}
		else if(bufRetCode&VPU_DEC_OUTPUT_MOSAIC_DIS)
		{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_HAS_OUTPUT);
			eVpuDecoderState=VPU_COM_STATE_DO_OUT;
			nOutBufferSize=0;
		}
	}

	if(bufRetCode&VPU_DEC_SKIP)
	{
		//notify user to get one time stamp.
		ASSERT(bufRet&FILTER_HAS_OUTPUT);	//only for this case now !!
		//bufRet=(FilterBufRetCode)(bufRet|FILTER_SKIP_TS);
		nNeedSkip=1;
	}

	if((bufRetCode&VPU_DEC_NO_ENOUGH_INBUF)&&(OMX_FALSE==bInEos))
	{
		//in videofilter: these flags are exclusively, so we must be careful !!!
		if(bufRet==(FILTER_OK|FILTER_INPUT_CONSUMED))
		{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_INPUT_BUFFER);
		}
		else if((bufRet==FILTER_OK)&&(pInBuffer==(OMX_PTR)INVALID))  //check this since we added one state for pInBuffer=INVALID
		//else if((bufRet==FILTER_OK)&&(bufRetCode&VPU_DEC_INPUT_USED))  //check this since we added one state for pInBuffer=INVALID
		{
			bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_INPUT_BUFFER);
		}
	}

	if(bufRetCode&VPU_DEC_FLUSH)
	{
		//flush operation is recommended by vpu wrapper
		//we call flush filter at setinput step to avoid missing getoutput
		nNeedFlush=1;
	}

	VPU_COMP_LOG("%s: return OMX_ErrorNone \r\n",__FUNCTION__);

	if(bEnabledPostProcess)
	{
		OMX_S32 index;
		if(eVpuDecoderState==VPU_COM_STATE_DO_OUT)
		{
			VpuDecOutFrameInfo sFrameInfo;
			VPU_COMP_SEM_LOCK(psemaphore);
			ret=VPU_DecGetOutputFrame(nHandle, &sFrameInfo);
			VPU_COMP_SEM_UNLOCK(psemaphore);
			ASSERT(VPU_DEC_RET_SUCCESS==ret);
			nFreeOutBufCntDec--;
			//find the matched node in frame pool based on frame virtual address, and record output frame info
			index=FramePoolRecordOutFrame(sFrameInfo.pDisplayFrameBuf->pbufVirtY, &sFramePoolInfo,&sFrameInfo,VPU_COM_FRM_OWNER_DEC);
			ASSERT(-1!=index);
			if(0==nOutBufferSize)
			{
				index=(POST_INDEX_MOSAIC<<16)|index;  // add extension flag info
			}
			VPU_COMP_LOG("post-process: add vpu index: %d, mosaic: %d \r\n",index&0xFFFF, index>>16);
			pPostInQueue->Add(&index);
			PostWakeUp(pPostMutex, &sPostCond, (volatile OMX_BOOL*) &bPostWaitingTasks);
			//clear output flag and enter decode state
			OMX_U32 tmp=(OMX_U32)FILTER_HAS_OUTPUT;
			bufRet=(FilterBufRetCode)(bufRet&(~tmp));
			eVpuDecoderState=VPU_COM_STATE_DO_DEC;
		}
		if(eVpuDecoderState==VPU_COM_STATE_EOS)
		{
			index=POST_INDEX_EOS<<16;
			VPU_COMP_LOG("post-process: add EOS:  index(invalid): %d, flag: %d \r\n",index&0xFFFF, index>>16);
			pPostInQueue->Add(&index);
			PostWakeUp(pPostMutex, &sPostCond, (volatile OMX_BOOL*) &bPostWaitingTasks);
			//clear last output flag and still enter wait post eos state
			OMX_U32 tmp=(OMX_U32)FILTER_LAST_OUTPUT;
			bufRet=(FilterBufRetCode)(bufRet&(~tmp));
			eVpuDecoderState=VPU_COM_STATE_WAIT_POST_EOS;
		}
	}
	return bufRet;//OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder:: GetDecBuffer(OMX_PTR *ppBuffer,OMX_S32 * pOutStuffSize,OMX_S32* pOutFrmSize)
{
	VpuDecFrameLengthInfo sLengthInfo;
	VpuDecRetCode ret;
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_DO_DEC:
		case VPU_COM_STATE_DO_OUT:
			break;
		default:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			//return OMX_ErrorIncorrectStateTransition;
	}
	VPU_COMP_SEM_LOCK(psemaphore);
	ret=VPU_DecGetConsumedFrameInfo(nHandle, &sLengthInfo);
	VPU_COMP_SEM_UNLOCK(psemaphore);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu get decoded frame info failure: ret=0x%X \r\n",__FUNCTION__,ret);
		return OMX_ErrorHardware;
	}
	*pOutStuffSize=sLengthInfo.nStuffLength;
	*pOutFrmSize=sLengthInfo.nFrameLength;
	if(sLengthInfo.pFrame==NULL)
	{
		*ppBuffer=NULL;	//the frame is skipped
	}
	else
	{
		*ppBuffer=(OMX_PTR)sLengthInfo.pFrame->pbufVirtY;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::GetPostMappedDecBuffer(OMX_PTR pPostBuf, OMX_PTR *ppDecBuffer)
{
	OMX_S32 ret;
	ret=FramePoolSearchMappedDecBuffer(&sFramePoolInfo, pPostBuf,ppDecBuffer);
	if(ret==0){
		return OMX_ErrorBadParameter;
	}
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder:: GetOutputBuffer(OMX_PTR *ppOutVirtBuf,OMX_S32* pOutSize)
{
	VpuDecRetCode ret;
	OMX_S32 index;
	OMX_BOOL bOutLast=OMX_FALSE;
	OMX_PTR pPhyBuf=NULL;
	OMX_BOOL bPostProcess=OMX_FALSE;
	VpuDecoderFrmOwner eOwner=VPU_COM_FRM_OWNER_DEC;

#ifdef VPU_COMP_DEBUG
	static FILE* fpYUV=NULL;
#endif

	VPU_COMP_API_LOG("%s: state: %d \r\n",__FUNCTION__,eVpuDecoderState);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_DO_OUT:
			//update state
			eVpuDecoderState=VPU_COM_STATE_DO_DEC;
			break;
		case VPU_COM_STATE_EOS:
			bOutLast=OMX_TRUE;
			//update to decode state for repeat play ??
			//eVpuDecoderState=VPU_COM_STATE_DO_DEC;
			eVpuDecoderState=VPU_COM_STATE_RE_WAIT_FRM;
			break;
		case VPU_COM_STATE_WAIT_POST_EOS:
			break;
		default:
			//forbidden
			VPU_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorIncorrectStateTransition;
	}

	if(ppOutVirtBuf==NULL)
	{
		VPU_COMP_ERR_LOG("%s: failure: ppOutVirtBuf==NULL !!! \r\n",__FUNCTION__);
		return OMX_ErrorBadParameter;
	}

	//get output frame
	if(OMX_TRUE==bOutLast)
	{
		VpuDecOutFrameInfo* pFrameInfo;
		if(bEnabledPostProcess)
		{
			OMX_S32 nOutSize;
			pPostOutQueue->Get(&index);
			ASSERT((index>>16)&POST_INDEX_EOS);
			index=index&0xFFFF;  // remove flag info
			FramePoolGetBufVirt(&sFramePoolInfo, index, ppOutVirtBuf);
			//copy the last frame
			if(pLastOutVirtBuf){
				nOutSize=sOutFmt.nFrameWidth * sOutFmt.nFrameHeight*pxlfmt2bpp(sOutFmt.eColorFormat)/8;
				fsl_osal_memcpy(*ppOutVirtBuf, pLastOutVirtBuf, nOutSize);
				*pOutSize=nOutSize;
			}
			else{
				*pOutSize=0;
			}

			VPU_COMP_LOG("%s:  reach EOS , repeat the last frame: 0x%X  \r\n",__FUNCTION__,*ppOutVirtBuf);
			bPostProcess=OMX_TRUE;
			//in this case, index owner should be VPU_COM_FRM_OWNER_POST
			eOwner=VPU_COM_FRM_OWNER_POST;
		}
		else
		{
			//need to search one valid frame to output since vpu don't output one valid frame for EOS
			index=FramePoolFindOneDecoderUnOutputed(ppOutVirtBuf,&sFramePoolInfo,&pFrameInfo);
			if(-1==index)
			{
				VPU_COMP_ERR_LOG("%s: find unoutputed frame failure \r\n",__FUNCTION__);
				return OMX_ErrorInsufficientResources;
			}
			//set pDisplayFrameBuf to NULL to avoid clear operation in SetOutputBuffer()
			pFrameInfo->pDisplayFrameBuf=NULL;
			*pOutSize=0;
			VPU_COMP_LOG("%s:  reach EOS , search one valid frame: 0x%X  \r\n",__FUNCTION__,*ppOutVirtBuf);
		}
	}
	else
	{
		VpuDecOutFrameInfo sFrameInfo;
		VpuDecOutFrameInfo * pFrameInfo;
		if(bEnabledPostProcess)
		{
			VpuDecoderFrmState eState;
			ASSERT(pPostOutQueue->Size()>0);
			pPostOutQueue->Get(&index);
			VPU_COMP_LOG("post-process: output index: %d, mosaic: %d \r\n",index&0xFFFF,(index>>16));
			*pOutSize=sOutFmt.nFrameWidth * sOutFmt.nFrameHeight*pxlfmt2bpp(sOutFmt.eColorFormat)/8;
			if((index>>16)&POST_INDEX_MOSAIC)
			{
				*pOutSize=0;
			}
			index=index&0xFFFF;  // remove flag info
			FramePoolGetBufVirt(&sFramePoolInfo, index, ppOutVirtBuf);
			FramePoolGetBufProperty(&sFramePoolInfo,index,&eOwner,&eState,&pFrameInfo);
			bPostProcess=OMX_TRUE;
		}
		else
		{
			pFrameInfo=&sFrameInfo;
			VPU_COMP_SEM_LOCK(psemaphore);
			ret=VPU_DecGetOutputFrame(nHandle, pFrameInfo);
			VPU_COMP_SEM_UNLOCK(psemaphore);
			if(VPU_DEC_RET_SUCCESS!=ret)
			{
				VPU_COMP_ERR_LOG("%s: vpu get output frame failure: ret=0x%X \r\n",__FUNCTION__,ret);
				return OMX_ErrorHardware;
			}
			*ppOutVirtBuf=(OMX_PTR)sFrameInfo.pDisplayFrameBuf->pbufVirtY;
			//find the matched node in frame pool based on frame virtual address, and record output frame info
			index=FramePoolRecordOutFrame(*ppOutVirtBuf, &sFramePoolInfo,&sFrameInfo,VPU_COM_FRM_OWNER_DEC);
			if(-1==index)
			{
				VPU_COMP_ERR_LOG("%s: can't find matched node in frame pool: 0x%X !!! \r\n",__FUNCTION__,(UINT32)(*ppOutVirtBuf));
				return OMX_ErrorInsufficientResources;
			}
			*pOutSize=nOutBufferSize;
			VPU_COMP_LOG("%s: return output: 0x%X, nFreeOutBufCntDec: %d \r\n",__FUNCTION__,(UINT32)(*ppOutVirtBuf),(UINT32)nFreeOutBufCntDec);
		}
		//update crop info for every output frame
		sOutCrop.nLeft=pFrameInfo->pExtInfo->FrmCropRect.nLeft;
		sOutCrop.nTop=pFrameInfo->pExtInfo->FrmCropRect.nTop;
		sOutCrop.nWidth=pFrameInfo->pExtInfo->FrmCropRect.nRight-pFrameInfo->pExtInfo->FrmCropRect.nLeft;
		sOutCrop.nHeight=pFrameInfo->pExtInfo->FrmCropRect.nBottom-pFrameInfo->pExtInfo->FrmCropRect.nTop;
	}

#ifdef VPU_COMP_DEBUG
		FileDumpYUV(&fpYUV,sFramePoolInfo.outFrameInfo[index].pDisplayFrameBuf->pbufVirtY,sFramePoolInfo.outFrameInfo[index].pDisplayFrameBuf->pbufVirtCb,sFramePoolInfo.outFrameInfo[index].pDisplayFrameBuf->pbufVirtCr,
			nPadWidth*nPadHeight,nPadWidth*nPadHeight/4,sOutFmt.eColorFormat);
#endif

	if(bPostProcess)
	{
		if(eOwner==VPU_COM_FRM_OWNER_POST)
		{
			nFreeOutBufCntPost--;
			//FIXME: in future, we need to add one schema to mapping this output buffer(post) with decoded buffer(vpu)
			//otherwise, it is difficulty to get accurate timestamp !!!
		}
		else
		{
			//nFreeOutBufCntDec--;	//it is already done before
		}
	}
	else
	{
		nFreeOutBufCntDec--;
	}
	pLastOutVirtBuf=*ppOutVirtBuf;
	FramePoolSetBufState(&sFramePoolInfo, index, VPU_COM_FRM_STATE_OUT);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::FlushFilter()
{
	VpuDecRetCode ret;
	VPU_COMP_LOG("%s: \r\n",__FUNCTION__);

	VPU_COMP_SEM_LOCK(psemaphore);
	ret=VPU_DecFlushAll(nHandle);
	//VPU_COMP_SEM_UNLOCK(psemaphore);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu flush failure: ret=0x%X \r\n",__FUNCTION__,ret);
		if(VPU_DEC_RET_FAILURE_TIMEOUT==ret)
		{
			VPU_DecReset(nHandle);
			SendEvent(OMX_EventError, OMX_ErrorStreamCorrupt, 0, NULL);
		}
		VPU_COMP_SEM_UNLOCK(psemaphore);
		return OMX_ErrorHardware;
	}
	VPU_COMP_SEM_UNLOCK(psemaphore);
	//since vpu will auto clear all buffers(is equal to setoutput() operation), we need to add additional protection(set VPU_COM_STATE_WAIT_FRM).
	//otherwise, vpu may return one buffer which is still not set by user.
	eVpuDecoderState=VPU_COM_STATE_RE_WAIT_FRM;

	nNeedFlush=0;
	nNeedSkip=0;

	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::FlushInputBuffer()
{
	OMX_ERRORTYPE ret=OMX_ErrorNone;

	VPU_COMP_API_LOG("%s: state: %d \r\n",__FUNCTION__,eVpuDecoderState);

	//clear input buffer
	pInBuffer=(OMX_PTR)INVALID;
	nInSize=0;

	//need to clear bInEos
	//fixed case: SetInputBuffer(,bInEos), and FlushInputBuffer(), then FilterOneBuffer() before SetInputBuffer().
	//As result, user will get one output(EOS) without calling SetInputBuffer().
	bInEos=OMX_FALSE;

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
		case VPU_COM_STATE_LOADED:
		case VPU_COM_STATE_OPENED:
		case VPU_COM_STATE_WAIT_FRM:	// have not registered frames, so can not call flushfilter
		case VPU_COM_STATE_DO_INIT:
		case VPU_COM_STATE_DO_OUT:
			//forbidden !!!
			VPU_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorNone; //for conformance test: don't return OMX_ErrorIncorrectStateTransition;
		case VPU_COM_STATE_DO_DEC:
		case VPU_COM_STATE_EOS:
		case VPU_COM_STATE_RE_WAIT_FRM:
		case VPU_COM_STATE_WAIT_POST_EOS:
			break;
		default:
			VPU_COMP_ERR_LOG("%s: unknown state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorNone; //for conformance test: don't return OMX_ErrorIncorrectStateTransition;
	}

	//flush post-process
	if(bEnabledPostProcess){
		PostThreadFlushInput();
	}

	//flush vpu input/output
	ret=FlushFilter();

	return ret;
}

OMX_ERRORTYPE VpuDecoder::FlushOutputBuffer()
{
	OMX_ERRORTYPE ret=OMX_ErrorNone;
	OMX_S32 num;
	VPU_COMP_API_LOG("%s: state: %d  \r\n",__FUNCTION__,eVpuDecoderState);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
		case VPU_COM_STATE_LOADED:
		case VPU_COM_STATE_OPENED:
		case VPU_COM_STATE_WAIT_FRM:
			break;
#if 1
		case VPU_COM_STATE_DO_DEC:
		case VPU_COM_STATE_EOS:
		case VPU_COM_STATE_RE_WAIT_FRM:
		case VPU_COM_STATE_WAIT_POST_EOS:
			//flush post-process
			if(bEnabledPostProcess)
			{
				OMX_S32 index;
				PostThreadFlushOutput();
				ASSERT(0==pPostInQueue->Size());
				ASSERT(0==pPostInReturnQueue->Size());
				ASSERT(0==pPostOutQueue->Size());
				ASSERT(0==pPostOutReturnQueue->Size());
				//clear all frame state
				num=FramePoolPostOutReset(&sFramePoolInfo,nOutBufferCntPost);
				ASSERT(nFreeOutBufCntPost==num);
			}
			//flush vpu input/output
			ret=FlushFilter();

			//re set out map info: simulate: all frames have been returned from vpu and been recorded into outFrameInfo
#if 0
{
			VpuFrameBuffer* vpuFrameBuffer[VPU_DEC_MAX_NUM_MEM];
			VPU_COMP_SEM_LOCK(psemaphore);
			VPU_DecAllRegFrameInfo(nHandle, vpuFrameBuffer, (INT32*)&num);
			VPU_COMP_SEM_UNLOCK(psemaphore);
			if(num<=0)
			{
				//in theory, shouldn't enter here if all states are protected correctly.
				VPU_COMP_ERR_LOG("%s: no buffers registered \r\n",__FUNCTION__);
				return OMX_ErrorNone;
			}
			num=FramePoolDecoderOutReset(&sFramePoolInfo,vpuFrameBuffer, num,1);
}
#else
			num=FramePoolDecoderOutReset(&sFramePoolInfo,NULL, nOutBufferCntDec,1);
#endif
			ASSERT(nFreeOutBufCntDec==num);

			nFreeOutBufCntDec=0;
			nFreeOutBufCntPost=0;
			return ret;
#endif
		case VPU_COM_STATE_DO_INIT:
		case VPU_COM_STATE_DO_OUT:
			//forbidden !!!
			VPU_COMP_ERR_LOG("%s: failure state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorNone; //for conformance test: don't return OMX_ErrorIncorrectStateTransition;
		default:
			//forbidden !!!
			VPU_COMP_ERR_LOG("%s: unknown state transition, current state=%d \r\n",__FUNCTION__,eVpuDecoderState);
			return OMX_ErrorNone; //for conformance test: don't return OMX_ErrorIncorrectStateTransition;
	}

	//clear frame pool
	FramePoolClear(&sFramePoolInfo);

	return ret;

}

OMX_PTR VpuDecoder::AllocateOutputBuffer(OMX_U32 nSize)
{
	VpuDecRetCode ret;
	VpuMemDesc vpuMem;

	VPU_COMP_API_LOG("%s: state: %d \r\n",__FUNCTION__,eVpuDecoderState);

	//check state
	switch(eVpuDecoderState)
	{
		case VPU_COM_STATE_NONE:
		// case VPU_COM_STATE_LOADED:
		//case VPU_COM_STATE_OPENED:
			//1 how to avoid conflict memory operators
			VPU_COMP_ERR_LOG("%s: error state: %d \r\n",__FUNCTION__,eVpuDecoderState);
			return (OMX_PTR)NULL;
		default:
			break;
	}

	//malloc physical memory through vpu
	vpuMem.nSize=nSize;
	ret=VPU_DecGetMem_Wrapper(&vpuMem,&sMemOperator);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu malloc frame buf failure: size=%d, ret=0x%X \r\n",__FUNCTION__,(INT32)nSize,ret);
		return (OMX_PTR)NULL;//OMX_ErrorInsufficientResources;
	}

	//record memory for release
	if(0==MemAddPhyBlock(&vpuMem, &sAllocMemInfo))
	{
		VPU_DecFreeMem_Wrapper(&vpuMem,&sMemOperator);
		VPU_COMP_ERR_LOG("%s:add phy block failure \r\n",__FUNCTION__);
		return (OMX_PTR)NULL;
	}

	//register memory info into resource manager
	if(OMX_ErrorNone!=AddHwBuffer((OMX_PTR)vpuMem.nPhyAddr, (OMX_PTR)vpuMem.nVirtAddr))
	{
		MemRemovePhyBlock(&vpuMem, &sAllocMemInfo);
		VPU_DecFreeMem_Wrapper(&vpuMem,&sMemOperator);
		VPU_COMP_ERR_LOG("%s:add hw buffer failure \r\n",__FUNCTION__);
		return (OMX_PTR)NULL;
	}

	//return virtual address
	return (OMX_PTR)vpuMem.nVirtAddr;//OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::FreeOutputBuffer(OMX_PTR pBuffer)
{
	VpuDecRetCode ret;
	VpuMemDesc vpuMem;

	VPU_COMP_API_LOG("%s: \r\n",__FUNCTION__);

	//query related mem info for release
	if(0==MemQueryPhyBlock(pBuffer,&vpuMem,&sAllocMemInfo))
	{
		VPU_COMP_ERR_LOG("%s: query phy block failure \r\n",__FUNCTION__);
		return OMX_ErrorResourcesLost;
	}

	//release physical memory through vpu
	ret=VPU_DecFreeMem_Wrapper(&vpuMem,&sMemOperator);
	if(ret!=VPU_DEC_RET_SUCCESS)
	{
		VPU_COMP_ERR_LOG("%s: free vpu memory failure : ret=0x%X \r\n",__FUNCTION__,ret);
		return OMX_ErrorHardware;
	}

	//remove mem info
	if(0==MemRemovePhyBlock(&vpuMem, &sAllocMemInfo))
	{
		VPU_COMP_ERR_LOG("%s: remove phy block failure \r\n",__FUNCTION__);
		return OMX_ErrorResourcesLost;
	}

	//unregister memory info from resource manager
	if(OMX_ErrorNone!=RemoveHwBuffer(pBuffer))
	{
		VPU_COMP_ERR_LOG("%s: remove hw buffer failure \r\n",__FUNCTION__);
		return OMX_ErrorResourcesLost;
	}

	return OMX_ErrorNone;
}

FilterBufRetCode VpuDecoder::ProcessVpuInitInfo()
{
	FilterBufRetCode bufRet=FILTER_OK;
	VpuDecRetCode ret;
	OMX_S32 nChanged=0;
	//process init info
	VPU_COMP_SEM_LOCK(psemaphore);
	ret=VPU_DecGetInitialInfo(nHandle, &sInitInfo);
	VPU_COMP_SEM_UNLOCK(psemaphore);
	if(VPU_DEC_RET_SUCCESS!=ret)
	{
		VPU_COMP_ERR_LOG("%s: vpu get init info failure: ret=0x%X \r\n",__FUNCTION__,ret);
		return FILTER_ERROR;//OMX_ErrorHardware;
	}

	//set resolution info
	sInFmt.nFrameWidth=sInitInfo.nPicWidth;
	sInFmt.nFrameHeight=sInitInfo.nPicHeight;
	nPadWidth = Align(sInFmt.nFrameWidth,FRAME_ALIGN);//(sInFmt.nFrameWidth +15)&(~15);
	if(sInitInfo.nInterlace)
	{
		nPadHeight = Align(sInFmt.nFrameHeight ,2*FRAME_ALIGN);//(sInFmt.nFrameHeight +31)&(~31);
	}
	else
	{
		nPadHeight = Align(sInFmt.nFrameHeight ,FRAME_ALIGN);//(sInFmt.nFrameHeight +15)&(~15);
	}

	if((nFrameWidthStride>=nPadWidth) && (nFrameHeightStride>=nPadHeight)){
		nPadWidth=nFrameWidthStride;
		nPadHeight=nFrameHeightStride;
	}

	//check change for nFrameWidth/nFrameHeight
	if(((OMX_S32)sOutFmt.nFrameWidth !=nPadWidth)||((OMX_S32)sOutFmt.nFrameHeight != nPadHeight))
	{
		sOutFmt.nFrameWidth = nPadWidth;
		sOutFmt.nFrameHeight = nPadHeight;
		sOutFmt.nStride=nPadWidth;
        sOutFmt.nSliceHeight = nPadHeight;
		nChanged=1;
	}

	//check color format, only for mjpg : 4:2:0/4:2:2(ver/hor)/4:4:4/4:0:0
	if(VPU_V_MJPG==eFormat)
	{
		OMX_COLOR_FORMATTYPE colorFormat;
		colorFormat=ConvertMjpgColorFormat(sInitInfo.nMjpgSourceFormat,sOutFmt.eColorFormat);
		if(colorFormat!=sOutFmt.eColorFormat)
		{
			sOutFmt.eColorFormat=colorFormat;
			nChanged=1;
		}
	}

	//TODO: check change for sOutCrop ?
	//...

	//set crop info
	VPU_COMP_LOG("%s: original init info: [top,left,bottom,right]=[%d,%d,%d,%d], \r\n",__FUNCTION__,
		(INT32)sInitInfo.PicCropRect.nTop,(INT32)sInitInfo.PicCropRect.nLeft,(INT32)sInitInfo.PicCropRect.nBottom,(INT32)sInitInfo.PicCropRect.nRight);

	sOutCrop.nLeft = sInitInfo.PicCropRect.nLeft;
	sOutCrop.nTop = sInitInfo.PicCropRect.nTop;
	//here, we am not responsible to 8 pixels limitation at display end. !!!
	sOutCrop.nWidth = sInitInfo.PicCropRect.nRight-sInitInfo.PicCropRect.nLeft;// & (~7);
	sOutCrop.nHeight= sInitInfo.PicCropRect.nBottom-sInitInfo.PicCropRect.nTop;

#if 1	//user need to know the non-pad width/height in portchange process ???
	//it may have potential risk if crop info is not equal to non-pad info !!!
	sInFmt.nFrameWidth=sOutCrop.nWidth;
	sInFmt.nFrameHeight=sOutCrop.nHeight;
	if(((nPadWidth-sOutCrop.nWidth)>=16)||((sInitInfo.nInterlace)&&(nPadHeight-sOutCrop.nHeight)>=32)||((0==sInitInfo.nInterlace)&&(nPadHeight-sOutCrop.nHeight)>=16))
	{
		VPU_COMP_LOG("potential risk for sInFmt [widthxheight]: [%dx%d] \r\n",(INT32)sOutCrop.nWidth,(INT32)sOutCrop.nHeight);
	}
#endif

	//check change for nOutBufferCntDec
	if((sInitInfo.nMinFrameBufferCount+FRAME_SURPLUS)!=(OMX_S32)nOutBufferCntDec)
	{
		nOutBufferCntDec=sInitInfo.nMinFrameBufferCount+FRAME_SURPLUS;
		nChanged=1;
	}
#if 1	//workaround for ENGR00171878: we should make sure porting change event always be triggered
	else
	{
		VPU_COMP_LOG("plus one buffer cnt manually: original cnt: %d, new cnt: %d \r\n",nOutBufferCntDec,nOutBufferCntDec+1);
		nOutBufferCntDec++;
		sInitInfo.nMinFrameBufferCount++;
		nChanged=1;
	}
#endif

	PostProcessSetStrategy(&sInitInfo,&bEnabledPostProcess,&nOutBufferCntPost,eFormat,&nPostMotion);
	if(bEnabledPostProcess)
	{
		ASSERT(CHROMA_ALIGN==1);	//IPU don't support three address (Y/Cb/Cr)
		if(PostProcessIPUInit(&sIpuHandle)==0)
		{
			//init failure, disable it
			nOutBufferCntPost=DEFAULT_BUF_OUT_POST_ZEROCNT;
			bEnabledPostProcess=OMX_FALSE;
		}
		else
		{
			//set ipu task in/out parameter
			if(nFrameWidthStride<(OMX_S32)sOutFmt.nFrameWidth || nFrameHeightStride<(OMX_S32)sOutFmt.nFrameHeight){
				PostProcessIPUSetDefault(&sIpuHandle,&sOutFmt,&sOutCrop,nPostMotion);
			}
			else{
				//use stride value specified by user
				OMX_VIDEO_PORTDEFINITIONTYPE outFmt;
				outFmt=sOutFmt;
				outFmt.nFrameWidth=nFrameWidthStride;
				outFmt.nFrameHeight=nFrameHeightStride;
				PostProcessIPUSetDefault(&sIpuHandle,&outFmt,&sOutCrop,nPostMotion);
			}

			//create post-process thread
			pPostInQueue = FSL_NEW(Queue, ());
			if(pPostInQueue == NULL) {
				VPU_COMP_ERR_LOG("New post queue failed.\n");
			}
			if(pPostInQueue->Create(MAX_POST_QUEUE_SIZE, sizeof(OMX_BUFFERHEADERTYPE*), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
				VPU_COMP_ERR_LOG("Init post queue failed.\n");
			}
			pPostOutQueue = FSL_NEW(Queue, ());
			if(pPostOutQueue == NULL) {
				VPU_COMP_ERR_LOG("New post queue failed.\n");
			}
			if(pPostOutQueue->Create(MAX_POST_QUEUE_SIZE, sizeof(OMX_BUFFERHEADERTYPE*), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
				VPU_COMP_ERR_LOG("Init post queue failed.\n");
			}
			pPostInReturnQueue = FSL_NEW(Queue, ());
			if(pPostInReturnQueue == NULL) {
				VPU_COMP_ERR_LOG("New post queue failed.\n");
			}
			if(pPostInReturnQueue->Create(MAX_POST_QUEUE_SIZE, sizeof(OMX_BUFFERHEADERTYPE*), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
				VPU_COMP_ERR_LOG("Init post queue failed.\n");
			}
			pPostOutReturnQueue = FSL_NEW(Queue, ());
			if(pPostOutReturnQueue == NULL) {
				VPU_COMP_ERR_LOG("New post queue failed.\n");
			}
			if(pPostOutReturnQueue->Create(MAX_POST_QUEUE_SIZE, sizeof(OMX_BUFFERHEADERTYPE*), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS) {
				VPU_COMP_ERR_LOG("Init post queue failed.\n");
			}
			if(E_FSL_OSAL_SUCCESS != fsl_osal_sem_init(&pPostCmdSem, 0, 0)){
				VPU_COMP_ERR_LOG("Create pPostCmdSem Semphore failed.\n");
			}
			if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&pPostMutex, fsl_osal_mutex_normal)) {
				VPU_COMP_ERR_LOG("Create mutext for post process failed.\n");
			}
			//if(E_FSL_OSAL_SUCCESS != fsl_osal_cond_create(&sPostCond)) {
			if(0!= pthread_cond_init(&sPostCond, NULL)) {
				VPU_COMP_ERR_LOG("Create condition variable for post process failed.\n");
			}
			ePostState=POST_PROCESS_STATE_NONE;
			ePostCmd=POST_PROCESS_CMD_NONE;
			bPostWaitingTasks=OMX_TRUE;
			if(E_FSL_OSAL_SUCCESS != fsl_osal_thread_create(&pPostThreadId, NULL, PostThread, this)) {
				VPU_COMP_ERR_LOG("Create post-process thread failed.\n");
				//init failure, disable it
				nOutBufferCntPost=DEFAULT_BUF_OUT_POST_ZEROCNT;
				bEnabledPostProcess=OMX_FALSE;
				pPostThreadId=NULL;
				PostProcessIPUDeinit(&sIpuHandle);
			}
			PostThreadRun();
			bNeedMapDecAndOutput=OMX_TRUE;
		}
	}
	else{
		bNeedMapDecAndOutput=OMX_FALSE;
	}

	nOutBufferCnt=nOutBufferCntDec+nOutBufferCntPost;

	if(nFrameMaxCnt>0){
		/*user specify the max buffer count*/
		if(nFrameMaxCnt>(OMX_S32)nOutBufferCnt){
			nOutBufferCntDec+=nFrameMaxCnt-nOutBufferCnt;
			nOutBufferCnt=nOutBufferCntDec+nOutBufferCntPost;
		}
		else{
			VPU_COMP_ERR_LOG("warning: buffer isn't enough,  nFrameMaxCnt: %d, vpu required cnt: %d, final required cnt: %d \r\n",nFrameMaxCnt,sInitInfo.nMinFrameBufferCount,nOutBufferCnt);
		}
	}

	VPU_COMP_LOG("%s: Init OK, [width x height]=[%d x %d] \r\n",__FUNCTION__,sInitInfo.nPicWidth,sInitInfo.nPicHeight);
	VPU_COMP_LOG("%s: [top,left,width,height]=[%d,%d,%d,%d], \r\n",__FUNCTION__,
		(INT32)sOutCrop.nLeft,(INT32)sOutCrop.nTop,(INT32)sOutCrop.nWidth,(INT32)sOutCrop.nHeight);
	VPU_COMP_LOG("nOutBufferCntDec:%d ,nPadWidth: %d, nPadHeight: %d \r\n",(INT32)nOutBufferCntDec, (INT32)nPadWidth, (INT32)nPadHeight);

	//get aspect ratio info
	sDispRatio.xWidth=sInitInfo.nQ16ShiftWidthDivHeightRatio;
	sDispRatio.xHeight=Q16_SHIFT;
	VPU_COMP_LOG("%s: ratio: width: 0x%X, height: 0x%X \r\n",__FUNCTION__,sDispRatio.xWidth,sDispRatio.xHeight);

	//update state
	eVpuDecoderState=VPU_COM_STATE_WAIT_FRM;

	if(nChanged)
	{
	    VPU_COMP_LOG("out format change width=%d,height=%d",sOutFmt.nFrameWidth,sOutFmt.nFrameHeight);
		nOutBufferSize=sOutFmt.nFrameWidth * sOutFmt.nFrameHeight*pxlfmt2bpp(sOutFmt.eColorFormat)/8;
        sOutFmt.nStride = sOutFmt.nFrameWidth;
        sOutFmt.nSliceHeight = sOutFmt.nFrameHeight;
		OutputFmtChanged();
	}

	//bufRet|=FILTER_DO_INIT;
	bufRet=(FilterBufRetCode)(bufRet|FILTER_NO_OUTPUT_BUFFER); //request enough output buffer before do InitFilter() operation

	VPU_COMP_LOG("%s: enter wait frame state \r\n",__FUNCTION__);
	return bufRet;	//OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::PostThreadRun()
{
	// non-run state => run state
	if(ePostState==POST_PROCESS_STATE_RUN){
		VPU_COMP_POST_LOG("%s: already in run state: %d \r\n",__FUNCTION__,ePostState);
		return OMX_ErrorNone;
	}

	//send command
	VPU_COMP_POST_LOG("decoder thread will send run command \r\n");
	ePostCmd=POST_PROCESS_CMD_RUN;
	PostWakeUp(pPostMutex, &sPostCond, (volatile OMX_BOOL*) &bPostWaitingTasks);

	//wait idle state
	PostWaitState((volatile PostProcessState*)&ePostState,POST_PROCESS_STATE_RUN,pPostCmdSem);
	VPU_COMP_POST_LOG("decoder thread receive run command finished\r\n");
	return OMX_ErrorNone;
}
OMX_ERRORTYPE VpuDecoder::PostThreadStop()
{
	// run state => idle state
	if(ePostState!=POST_PROCESS_STATE_RUN){
		VPU_COMP_POST_LOG("%s: not run state: %d \r\n",__FUNCTION__,ePostState);
		return OMX_ErrorNone;
	}
	//send command
	VPU_COMP_POST_LOG("decoder thread will send stop command \r\n");
	ePostCmd=POST_PROCESS_CMD_STOP;
	PostWakeUp(pPostMutex, &sPostCond, (volatile OMX_BOOL*) &bPostWaitingTasks);

	//wait idle state
	PostWaitState((volatile PostProcessState*)&ePostState,POST_PROCESS_STATE_IDLE,pPostCmdSem);
	VPU_COMP_POST_LOG("decoder thread receive stop command finished\r\n");
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::PostThreadFlushInput()
{
	// run -> (flush in) -> run state
	OMX_S32 index;
	VpuDecoderFrmState eState;
	VpuDecOutFrameInfo * pFrameInfo;
	VpuDecoderFrmOwner eOwner=VPU_COM_FRM_OWNER_NULL;

	if(ePostState!=POST_PROCESS_STATE_RUN){
		VPU_COMP_POST_LOG("%s: not run state: %d \r\n",__FUNCTION__,ePostState);
		return OMX_ErrorNone;
	}

	//send command
	VPU_COMP_POST_LOG("decoder thread will send flushinput command \r\n");
	ePostCmd=POST_PROCESS_CMD_FLUSH_IN;
	PostWakeUp(pPostMutex, &sPostCond, (volatile OMX_BOOL*) &bPostWaitingTasks);

	//wait finished
	PostWaitState((volatile PostProcessState*)&ePostState,ePostState,pPostCmdSem);
	VPU_COMP_POST_LOG("decoder thread receive flushinput command finished\r\n");

	ASSERT(0==pPostInQueue->Size());
	//return frame buffer to vpu as possible
	while(pPostInReturnQueue->Size()>0){
		pPostInReturnQueue->Get(&index);
		//VPU_COMP_POST_LOG("post-process: return vpu index to vpu: %d \r\n",index);
		FramePoolReturnPostFrameToVpu(&sFramePoolInfo, nHandle, index, psemaphore);
		nFreeOutBufCntDec++;
	}

	//popup all output frame, and refill into pPostOutReturnQueue or return to vpu
	while(pPostOutQueue->Size()>0){
		pPostOutQueue->Get(&index);
		index=index&0xFFFF;	// remove extension flag
		FramePoolGetBufProperty(&sFramePoolInfo,index,&eOwner,&eState,&pFrameInfo);
		if(eOwner==VPU_COM_FRM_OWNER_POST){
			//VPU_COMP_POST_LOG("%s: return index: %d to post \r\n",__FUNCTION__,index);
			pPostOutReturnQueue->Add(&index);
			//nFreeOutBufCntPost++;
		}
		else if (eOwner==VPU_COM_FRM_OWNER_DEC){
			//VPU_COMP_POST_LOG("%s: return index: %d to vpu \r\n",__FUNCTION__,index);
			FramePoolReturnPostFrameToVpu(&sFramePoolInfo, nHandle, index, psemaphore);
			nFreeOutBufCntDec++;
		}
		else{
			VPU_COMP_POST_LOG("index: %d in incorrect owner: %d \r\n",index,eOwner);
		}
		ASSERT(VPU_COM_FRM_STATE_FREE==sFramePoolInfo.eFrmState[index]);
	}

	//finally, pPostInQueue/pPostInReturnQueue/pPostOutQueue are empty; pPostOutReturnQueue isn't empty

	VPU_COMP_POST_LOG("%s: post flush input finised, nFreeOutBufCntDec: %d, nFreeOutBufCntPost: %d, pPostOutReturnQueue->Size(): %d \r\n",__FUNCTION__,nFreeOutBufCntDec,nFreeOutBufCntPost,pPostOutReturnQueue->Size());
	return OMX_ErrorNone;
}

OMX_ERRORTYPE VpuDecoder::PostThreadFlushOutput()
{
	// run -> (flush out) -> run state
	OMX_S32 index;
	VpuDecoderFrmState eState;
	VpuDecOutFrameInfo * pFrameInfo;
	VpuDecoderFrmOwner eOwner=VPU_COM_FRM_OWNER_NULL;

	if(ePostState!=POST_PROCESS_STATE_RUN){
		VPU_COMP_POST_LOG("%s: not run state: %d \r\n",__FUNCTION__,ePostState);
		return OMX_ErrorNone;
	}

	//send command
	VPU_COMP_POST_LOG("decoder thread will send flushoutput command \r\n");
	ePostCmd=POST_PROCESS_CMD_FLUSH_OUT;
	PostWakeUp(pPostMutex, &sPostCond, (volatile OMX_BOOL*) &bPostWaitingTasks);

	//wait finished
	PostWaitState((volatile PostProcessState*)&ePostState,ePostState,pPostCmdSem);
	VPU_COMP_POST_LOG("decoder thread receive flushoutput command finished\r\n");

	ASSERT(0==pPostInQueue->Size());
	//return frame buffer to vpu as possible
	while(pPostInReturnQueue->Size()>0){
		pPostInReturnQueue->Get(&index);
		//VPU_COMP_POST_LOG("post-process: return vpu index to vpu: %d \r\n",index);
		FramePoolReturnPostFrameToVpu(&sFramePoolInfo, nHandle, index, psemaphore);
		nFreeOutBufCntDec++;
	}

	ASSERT(0==pPostOutReturnQueue->Size());
	//popup all output frame
	while(pPostOutQueue->Size()>0){
		pPostOutQueue->Get(&index);
		index=index&0xFFFF;	// remove extension flag
		FramePoolGetBufProperty(&sFramePoolInfo,index,&eOwner,&eState,&pFrameInfo);
		if(eOwner==VPU_COM_FRM_OWNER_POST){
			nFreeOutBufCntPost--;
		}
		else if (eOwner==VPU_COM_FRM_OWNER_DEC){
#if 0		//we can also return them to vpu
			FramePoolReturnPostFrameToVpu(&sFramePoolInfo, nHandle, index, psemaphore);
			nFreeOutBufCntDec++;
#else
			//nFreeOutBufCntDec--;
#endif
		}
		else{
			VPU_COMP_POST_LOG("index: %d in incorrect owner: %d \r\n",index,eOwner);
		}
		FramePoolSetBufState(&sFramePoolInfo, index, VPU_COM_FRM_STATE_OUT);
	}

	//finally, pPostInQueue/pPostInReturnQueue/pPostOutQueue/pPostOutReturnQueue are all empty

	VPU_COMP_POST_LOG("%s: post flush output finised, nFreeOutBufCntDec: %d, nFreeOutBufCntPost: %d \r\n",__FUNCTION__,nFreeOutBufCntDec,nFreeOutBufCntPost);
	return OMX_ErrorNone;
}
OMX_ERRORTYPE VpuDecoder::GetCropInfo(OMX_CONFIG_RECTTYPE *sCrop)
{
    if(sCrop == NULL)
        return OMX_ErrorBadParameter;

    if(sCrop->nPortIndex != VPUDEC_OUT_PORT)
        return OMX_ErrorUnsupportedIndex;

    sCrop->nLeft = sOutCrop.nLeft;
    sCrop->nTop = sOutCrop.nTop;
    sCrop->nWidth = sOutCrop.nWidth;
    sCrop->nHeight = sOutCrop.nHeight;

    return OMX_ErrorNone;
}
OMX_ERRORTYPE VpuDecoder::SetCropInfo(OMX_CONFIG_RECTTYPE *sCrop)
{
    if(sCrop == NULL || sCrop->nPortIndex != OUT_PORT)
        return OMX_ErrorBadParameter;

    sOutCrop.nTop = sCrop->nTop;
    sOutCrop.nLeft = sCrop->nLeft;
    sOutCrop.nWidth = sCrop->nWidth;
    sOutCrop.nHeight = sCrop->nHeight;

    return OMX_ErrorNone;
}

/**< C style functions to expose entry point for the shared library */
extern "C"
{
	OMX_ERRORTYPE VpuDecoderInit(OMX_IN OMX_HANDLETYPE pHandle)
	{
		OMX_ERRORTYPE ret = OMX_ErrorNone;
		VpuDecoder *obj = NULL;
		ComponentBase *base = NULL;
		VPU_COMP_API_LOG("%s: \r\n",__FUNCTION__);

		obj = FSL_NEW(VpuDecoder, ());
		if(obj == NULL)
		{
			VPU_COMP_ERR_LOG("%s: vpu decoder new failure: ret=0x%X \r\n",__FUNCTION__,ret);
			return OMX_ErrorInsufficientResources;
		}

		base = (ComponentBase*)obj;
		ret = base->ConstructComponent(pHandle);
		if(ret != OMX_ErrorNone)
		{
			VPU_COMP_ERR_LOG("%s: vpu decoder construct failure: ret=0x%X \r\n",__FUNCTION__,ret);
			return ret;
		}
		return ret;
	}
}

/* File EOF */
