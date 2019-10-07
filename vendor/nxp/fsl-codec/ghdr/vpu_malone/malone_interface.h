/***********************************************
 * Copyright (c) 2017 Amphion Semiconductor Ltd *
 ***********************************************
*
* Filename		: 	 malone_interface.h
* Description : 	 API interface
* Author			: 	 Media IP FW team (Belfast)
*
************************************************/

#ifndef _MALONE_INTERFACE_H_
#define _MALONE_INTERFACE_H_

#include <stdint.h>
#include <stdio.h>

typedef uint32_t       u_int32;
typedef uint16_t       u_int16;
typedef uint8_t        u_int8;
typedef unsigned int        bool;

typedef int32_t         int32;
typedef int16_t         int16;
typedef int8_t          int8;
typedef unsigned char       BYTE;

#ifdef MALONE_64BIT_ADDR
typedef uint64_t       uint_addr;
#else
typedef uint32_t       uint_addr;
#endif

#ifndef false
  #define false 0
#endif  
#ifndef true
  #define true  1
#endif  

#ifndef FALSE
  #define FALSE 0
#endif  
#ifndef TRUE
  #define TRUE  1
#endif  

#ifndef NULL
#define NULL 0
#endif

#ifndef OKAY
#define	OKAY 0
#endif

#ifndef ZERO
#define	ZERO 0
#endif

#ifdef _MSC_VER
#define BOOL               int
#else
#define BOOL               bool
#endif

#define PAL_NO_WAIT               0
#define PAL_WAIT_FOREVER          ((u_int32)-1)

#define MEDIA_PLAYER_SKIPPED_FRAME_ID       0x555

#define VID_API_MAX_NUM_MVC_VIEWS 4

#define  PAL_QUEUE_ID   u_int32
#define VPU_MAX_SUPPORTED_STREAMS 2
#define VPU_EVENT_MAX_DATA_LENGTH 32 /* MEDIA_PLAYER_EVENT_PIC_DECODED  data*/

#define MEDIAIP_TRACE_LEVEL_NEVER        0x00000000
#define MEDIAIP_TRACE_LEVEL_1            0x10000000
#define MEDIAIP_TRACE_LEVEL_2            0x20000000
#define MEDIAIP_TRACE_LEVEL_3            0x30000000
#define MEDIAIP_TRACE_LEVEL_4            0x40000000
#define MEDIAIP_TRACE_LEVEL_5            0x50000000
#define MEDIAIP_TRACE_LEVEL_6            0x60000000
#define MEDIAIP_TRACE_LEVEL_ALWAYS       0x70000000

typedef enum
{
  MEDIA_IP_FMT_NULL        = 0x0,
  MEDIA_IP_FMT_AVC         = 0x1,
  MEDIA_IP_FMT_VC1         = 0x2,
  MEDIA_IP_FMT_MP2         = 0x3,
  MEDIA_IP_FMT_AVS         = 0x4,
  MEDIA_IP_FMT_ASP         = 0x5,
  MEDIA_IP_FMT_JPG         = 0x6,
  MEDIA_IP_FMT_RV          = 0x7,
  MEDIA_IP_FMT_VP6         = 0x8,
  MEDIA_IP_FMT_SPK         = 0x9,
  MEDIA_IP_FMT_VP8         = 0xA,
  MEDIA_IP_FMT_MVC         = 0xB,
  MEDIA_IP_FMT_VP3         = 0xC,
  MEDIA_IP_FMT_HEVC        = 0xD,
  MEDIA_IP_FMT_AUTO_DETECT = 0xAD00,
  MEDIA_IP_FMT_ALL         = (int)0xAAAAAAAA,
  MEDIA_IP_FMT_UNSUPPORTED = (int)0xFFFFFFFF,
  MEDIA_IP_FMT_LAST = MEDIA_IP_FMT_UNSUPPORTED

} MEDIA_IP_FORMAT;

typedef enum VPU_EVENTTYPE
{
  VPU_EventCmdProcessed,         
  VPU_EventError,               
  VPU_EventStarted,                
  VPU_EventStopped, 
  VPU_EventStreamBufferFeedDone,          
  VPU_EventStreamBufferLow,   
  VPU_EventStreamBufferHigh,     
  VPU_EventStreamBufferOverflow,
  VPU_EventStreamBufferStarved,
  VPU_EventSequenceInfo, 
  VPU_EventPictureHdrFound, /* a */
  VPU_EventPictureDecoded,
  VPU_EventChunkDecoded,
  VPU_EventFrameResourceRequest,      
  VPU_EventFrameResourceReady,
  VPU_EventFrameResourceRelease,
  VPU_EventDebugNotify,
  VPU_EventCodecFormatChange,
  VPU_EventDebugPing,
  VPU_EventStreamDecodeError,
  VPU_EventEndofStreamScodeFound,
  VPU_EventFlushDone,
  VPU_EventFrameResourceCheck,
  VPU_EventStreamBufferResetDone

} VPU_EVENTTYPE;


typedef enum
{
   /*   0  0x00  */   MEDIAIP_FW_STATUS_OK = 0,
   /*   1  0x01  */   MEDIAIP_FW_STATUS_ALREADY_INIT,
   /*   2  0x02  */   MEDIAIP_FW_STATUS_NOT_INIT,
   /*   3  0x03  */   MEDIAIP_FW_STATUS_INTERNAL_ERROR,
   /*   4  0x04  */   MEDIAIP_FW_STATUS_BAD_HANDLE,
   /*   5  0x05  */   MEDIAIP_FW_STATUS_BAD_PARAMETER,
   /*   6  0x06  */   MEDIAIP_FW_STATUS_BAD_LENGTH,
   /*   7  0x07  */   MEDIAIP_FW_STATUS_BAD_UNIT,
   /*   8  0x08  */   MEDIAIP_FW_STATUS_RESOURCE_ERROR,
   /*   9  0x09  */   MEDIAIP_FW_STATUS_CLOSED_HANDLE,
   /*  10  0x0A  */   MEDIAIP_FW_STATUS_TIMEOUT,
   /*  11  0x0B  */   MEDIAIP_FW_STATUS_NOT_ATTACHED,
   /*  12  0x0C  */   MEDIAIP_FW_STATUS_NOT_SUPPORTED,
   /*  13  0x0D  */   MEDIAIP_FW_STATUS_REOPENED_HANDLE,
   /*  14  0x0E  */   MEDIAIP_FW_STATUS_INVALID,
   /*  15  0x0F  */   MEDIAIP_FW_STATUS_DESTROYED,
   /*  16  0x10  */   MEDIAIP_FW_STATUS_DISCONNECTED,
   /*  17  0x11  */   MEDIAIP_FW_STATUS_BUSY,
   /*  18  0x12  */   MEDIAIP_FW_STATUS_IN_USE,
   /*  19  0x13  */   MEDIAIP_FW_STATUS_CANCELLED,
   /*  20  0x14  */   MEDIAIP_FW_STATUS_UNDEFINED,
   /*  21  0x15  */   MEDIAIP_FW_STATUS_UNKNOWN,
   /*  22  0x16  */   MEDIAIP_FW_STATUS_NOT_FOUND,
   /*  23  0x17  */   MEDIAIP_FW_STATUS_NOT_AVAILABLE,
   /*  24  0x18  */   MEDIAIP_FW_STATUS_NOT_COMPATIBLE,
   /*  25  0x19  */   MEDIAIP_FW_STATUS_NOT_IMPLEMENTED,
   /*  26  0x1A  */   MEDIAIP_FW_STATUS_EMPTY,
   /*  27  0x1B  */   MEDIAIP_FW_STATUS_FULL,
   /*  28  0x1C  */   MEDIAIP_FW_STATUS_FAILURE,
   /*  29  0x1D  */   MEDIAIP_FW_STATUS_ALREADY_ATTACHED,
   /*  30  0x1E  */   MEDIAIP_FW_STATUS_ALREADY_DONE,
   /*  31  0x1F  */   MEDIAIP_FW_STATUS_ASLEEP,
   /*  32  0x20  */   MEDIAIP_FW_STATUS_BAD_ATTACHMENT,
   /*  33  0x21  */   MEDIAIP_FW_STATUS_BAD_COMMAND,
   /*  34  0x22  */   MEDIAIP_FW_STATUS_INT_HANDLED,
   /*  35  0x23  */   MEDIAIP_FW_STATUS_INT_NOT_HANDLED,
   /*  36  0x24  */   MEDIAIP_FW_STATUS_NOT_SET,
   /*  37  0x25  */   MEDIAIP_FW_STATUS_NOT_HOOKED,
   /*  38  0x26  */   MEDIAIP_FW_STATUS_COMPLETE,
   /*  39  0x27  */   MEDIAIP_FW_STATUS_INVALID_NODE,
   /*  40  0x28  */   MEDIAIP_FW_STATUS_DUPLICATE_NODE,
   /*  41  0x29  */   MEDIAIP_FW_STATUS_HARDWARE_NOT_FOUND,
   /*  42  0x2A  */   MEDIAIP_FW_STATUS_ILLEGAL_OPERATION,
   /*  43  0x2B  */   MEDIAIP_FW_STATUS_INCOMPATIBLE_FORMATS,
   /*  44  0x2C  */   MEDIAIP_FW_STATUS_INVALID_DEVICE,
   /*  45  0x2D  */   MEDIAIP_FW_STATUS_INVALID_EDGE,
   /*  46  0x2E  */   MEDIAIP_FW_STATUS_INVALID_NUMBER,
   /*  47  0x2F  */   MEDIAIP_FW_STATUS_INVALID_STATE,
   /*  48  0x30  */   MEDIAIP_FW_STATUS_INVALID_TYPE,
   /*  49  0x31  */   MEDIAIP_FW_STATUS_STOPPED,
   /*  50  0x32  */   MEDIAIP_FW_STATUS_SUSPENDED,
   /*  51  0x33  */   MEDIAIP_FW_STATUS_TERMINATED,
   /*  52  0x34  */   MEDIAIP_FW_STATUS_FRAMESTORE_NOT_HANDLED,
   /* Last Entry */   MEDIAIP_FW_STATUS_CODE_LAST = MEDIAIP_FW_STATUS_FRAMESTORE_NOT_HANDLED
} MEDIAIP_FW_STATUS;

typedef enum VPU_PARAMID_TYPE
{
  VPU_ParamId_Format,
  VPU_ParamId_PESEnable,
  VPU_ParamId_uSharedStreamBufLoc, /*mark the address of the shared stream buffer with the calling app */
  VPU_ParamId_DCPEnable,
  VPU_ParamId_uNumDBEToUse,
  VPU_ParamId_DeBlockEn, /* Only formats with out-of-loop deblocking */
  VPU_ParamId_DeRingEn,  /* Only formats with out-of-loop deringing */
  VPU_ParamId_StreamLwm,
  
} VPU_VPU_PARAMID_TYPE;

typedef enum
{
  VPU_FS_FRAME_REQ = 0,
  VPU_FS_MBI_REQ,
  VPU_FS_DCP_REQ,
  VPU_FS_REQ_LAST = VPU_FS_DCP_REQ
} VPU_FS_REQ_TYPE;

typedef enum VPU_STATUS_TYPE
{
  VPU_STATUS_TYPE_DECODE_COUNTS = 0,
  VPU_STATUS_TYPE_MEM_STATUS,
  VPU_STATUS_TYPE_ACTIVE_SEQ_ID,
  VPU_STATUS_TYPE_ACTIVE_GOP_ID,
  VPU_STATUS_TYPE_SEQ_INFO,
  VPU_STATUS_TYPE_GOP_INFO,
  VPU_STATUS_TYPE_PIC_DECODE_INFO,
  // VPU_STATUS_TYPE_PIC_DISP_INFO, we don't need this as contained in VPU_STATUS_TYPE_PIC_DECODE_INFO 
  VPU_STATUS_TYPE_PIC_PERF_INFO,
  VPU_STATUS_TYPE_PIC_PERFDCP_INFO,
  VPU_STATUS_TYPE_LAST
} VPU_STATUS_TYPE;


// Define the magic cookie
#define PAL_CONFIG_MAGIC        0x434C4150      // "PALC", little endian


#if ( TARGET_PLATFORM == GENTB_PLATFORM ) || ( TARGET_PLATFORM == WIN_LIB ) || ( TARGET_PLATFORM == GEN_TB_ENC )

#define PAL_CONFIG_MAX_IRQS          0x12
#define PAL_CONFIG_MAX_MALONES       0x2
#define PAL_CONFIG_MAX_WINDSORS      0x1
#define PAL_CONFIG_MAX_TIMER_IRQS    0x4
#define PAL_CONFIG_MAX_TIMER_SLOTS   0x4

/* Define the entry locations in the irq vector */
#define PAL_IRQ_MALONE0_LOW      0x0
#define PAL_IRQ_MALONE0_HI       0x1
#define PAL_IRQ_MALONE1_LOW      0x2
#define PAL_IRQ_MALONE1_HI       0x3
#define PAL_IRQ_WINDSOR_LOW      0x4
#define PAL_IRQ_WINDSOR_HI       0x5
#define PAL_IRQ_HOST_CMD_LO      0x6
#define PAL_IRQ_HOST_CMD_HI      0x7
#define PAL_IRQ_HOST_MSG         0x9
#define PAL_IRQ_DPV              0xA
#define PAL_IRQ_TIMER_0          0xE
#define PAL_IRQ_TIMER_1          0xF
#define PAL_IRQ_TIMER_2          0x10
#define PAL_IRQ_TIMER_3          0x11

#else

#define PAL_CONFIG_MAX_INITS    4               // Number of init slots
#define PAL_CONFIG_MAX_IRQS     2               // Number of incoming irq lines supported

#endif /* TARGET_PLATFORM == TB_PLATFORM */


//////////////////////////////////////////////////////////////
// Player api action type enum

typedef enum
{
  /* Non-Stream Specific messages   */
  MEDIA_PLAYER_API_MODE_INVALID     = 0x00,
  MEDIA_PLAYER_API_MODE_PARSE_STEP  = 0x01,
  MEDIA_PLAYER_API_MODE_DECODE_STEP = 0x02,
  MEDIA_PLAYER_API_MODE_CONTINUOUS  = 0x03
} MEDIA_PLAYER_API_MODE;

//////////////////////////////////////////////////////////////
// Player fs control mode

typedef enum
{
  /* Non-Stream Specific messages   */
  MEDIA_PLAYER_FS_CTRL_MODE_INTERNAL    = 0x00,
  MEDIA_PLAYER_FS_CTRL_MODE_EXTERNAL    = 0x01
} MEDIA_PLAYER_FS_CTRL_MODE;



#if ( TARGET_APP == VPU_TEST_APP )
/* sPALMemDesc Added by NXP for their PAL implementation */
typedef struct {
	u_int32 size;
	u_int32 phy_addr;
	uint_addr virt_addr;
#ifdef USE_ION
	int32 ion_buf_fd;
#endif
} sPALMemDesc, *psPALMemDesc;


#if ( TARGET_PLATFORM == GENTB_PLATFORM ) || ( TARGET_PLATFORM == WIN_LIB ) || ( TARGET_PLATFORM == GEN_TB_ENC )

typedef struct _PALConfig
{
  u_int32             uPalConfigMagicCookie;

  u_int32             uGICBaseAddr;
  u_int32             uIrqLines[PAL_CONFIG_MAX_IRQS];
  u_int32             uIrqTarget[PAL_CONFIG_MAX_IRQS];

  u_int32             uUartBaseAddr;

  u_int32             uSysClkFreq;
  u_int32             uNumTimers;
  u_int32             uTimerBaseAddr;
  u_int32             uTimerSlots[PAL_CONFIG_MAX_TIMER_SLOTS];

  /* Do we need this in the PAL config? Only for checking mmu setup  */
  /* perhaps - otherwise its more naturtal home is in the DECLIB_CFG */
  /* structure                                                       */
  u_int32             uNumMalones;
  u_int32             uMaloneBaseAddr[PAL_CONFIG_MAX_MALONES];
  u_int32             uHifOffset[PAL_CONFIG_MAX_MALONES];

  u_int32             uNumWindsors;
  u_int32             uWindsorBaseAddr[PAL_CONFIG_MAX_WINDSORS];

  u_int32             uDPVBaseAddr;
  u_int32             uPixIfAddr;

  u_int32             pal_trace_level;
//  u_int32             pal_trace_destination;
//  u_int32             pal_trace_CBDescAddr[3];		// 3 separate circular buffers for PAL_TRACE_TO_CIRCULARBUF
   						                                    // 0: normal  1: irq  2: fiq
  u_int32             uHeapBase;
  u_int32             uHeapSize;

  u_int32             uFSLCacheBaseAddr;

} sPALConfig, *psPALConfig;

#else

typedef struct _PALConfig
{
  u_int32             pal_config_magic_cookie;
  u_int32             cmd_irq_line[PAL_CONFIG_MAX_IRQS];
  u_int32             cmd_irq_clear_addr[PAL_CONFIG_MAX_INITS];
  u_int32             cmd_irq_clear_mask[PAL_CONFIG_MAX_INITS];
  u_int32             cmd_irq_clear_val[PAL_CONFIG_MAX_INITS];
  u_int32             msg_irq_init_addr[PAL_CONFIG_MAX_INITS];
  u_int32             msg_irq_init_mask[PAL_CONFIG_MAX_INITS];
  u_int32             msg_irq_init_val[PAL_CONFIG_MAX_INITS];
  u_int32             msg_irq_raise_addr;
  u_int32             msg_irq_raise_mask;
  u_int32             msg_irq_raise_val;
  u_int32             uart_init_addr[PAL_CONFIG_MAX_INITS];
  u_int32             uart_init_mask[PAL_CONFIG_MAX_INITS];
  u_int32             uart_init_val[PAL_CONFIG_MAX_INITS];
  u_int32             uart_check_addr;
  u_int32             uart_check_mask;
  u_int32             uart_check_val;
  u_int32             uart_put_addr;
  u_int32             pal_trace_level;
  u_int32             pal_trace_destination;
  MEDIAIP_TRACE_FLAGS pal_trace_flags;         // Currently 5 words
  u_int32             pal_trace_CBDescAddr[3];		// 3 separate circular buffers for PAL_TRACE_TO_CIRCULARBUF
   						                                    // 0: normal  1: irq  2: fiq
} sPALConfig, *psPALConfig;

#endif /*  TARGET_PLATFORM == TB_PLATFORM */

typedef struct
{
  u_int32 uMaloneBase;
  u_int32 uMaloneIrqID[0x2];

  u_int32 uDPVBaseAddr;
  u_int32 uDPVIrqPin;
  u_int32 uPixIfBaseAddr;
  u_int32 uFSLCacheBaseAddr;

  bool bFrameBufferCompressionEnabled;
  
  /* Note that uMvdDMAMemAddr should be a physical address */
  u_int32 uMvdDMAMemSize;
  u_int32 uMvdDMAMemPhysAddr;
  uint_addr uMvdDMAMemVirtAddr;
  
  MEDIA_PLAYER_API_MODE     eApiMode;
  MEDIA_PLAYER_FS_CTRL_MODE eFsCtrlMode;

} VPU_DRIVER_CFG, *pVPU_DRIVER_CFG;


MEDIAIP_FW_STATUS pal_get_phy_buf(psPALMemDesc pbuf);
MEDIAIP_FW_STATUS pal_free_phy_buf(psPALMemDesc pbuf);
#endif

/* Common type used for external FS Req and Rel */
typedef struct
{
  u_int32          uFSIdx;
  u_int32          uFSNum;
  VPU_FS_REQ_TYPE  eType;
  bool             bNotDisplayed;
} VPU_FS_TYPE, *pVPU_FS_TYPE;

typedef struct
{
  u_int32 ulFsId;            /* External handle for managing frame resource */
  u_int32 ulFsLumaBase[2];   /* Physical base address */
  u_int32 ulFsChromaBase[2]; /* Physical base address */
  u_int32 ulFsHeight;
  u_int32 ulFsWidth;
  u_int32 ulFsStride;
  u_int32 ulFsType;
} VPU_FS_DESC, *pVPU_FS_DESC;


typedef struct
{
  u_int32 uActiveSpsID;
  u_int32 uNumRefFrms;
  u_int32 uNumDPBFrms;
  u_int32 uNumDFEAreas;
  u_int32 uColorDesc;
  u_int32 uProgressive;
  u_int32 uVerRes;
  u_int32 uHorRes;
  u_int32 uParWidth;
  u_int32 uParHeight;
  u_int32 FrameRate;
  u_int32 UDispAspRatio;
  u_int32 uLevelIDC;
  u_int32 uVerDecodeRes;
  u_int32 uHorDecodeRes;
  u_int32 uOverScan;
  u_int32 uChromaFmt;
  u_int32 uPAFF;
  u_int32 uMBAFF;
  u_int32 uBitDepthLuma;
  u_int32 uBitDepthChroma;
  u_int32 uMVCNumViews;
  u_int32 uMVCViewList[VID_API_MAX_NUM_MVC_VIEWS];
  u_int32 uFBCInUse;
    
} MediaIPFW_Video_SeqInfo;

typedef struct
{
  u_int32 bTopFldFirst;
  u_int32 bRptFstField;
  u_int32 uDispVerRes;
  u_int32	uDispHorRes;
  u_int32	uCentreVerOffset;
  u_int32	uCentreHorOffset;
  u_int32 uCropLeftRightOffset;
  u_int32 uCropTopBotOffset;

} MediaIPFW_Video_PicDispInfo;

/* Use the metrics info shared to Coreplay */
#if 0
typedef struct MediaIPFW_PicPerfInfo
{
  u_int32 uMemCRC;
  u_int32 uBSCRC;
  u_int32 uSlcActiveCnt;
  u_int32 uIBEmptyCnt; 
  u_int32 uBaseMemCRC;

  u_int32 uBaseCRCSkip;
  u_int32 uBaseCRCDrop;
  bool    bBaseCRCValid;

  u_int32 uCRC0;
  u_int32 uCRC1;
  u_int32 uCRC2;
  u_int32 uCRC3;
  u_int32 uCRC4;
  u_int32 uCRC5;
  
  u_int32 uFrameActCount;
  u_int32 uRbspBytesCount;
  u_int32 uDpbReadCount;
  u_int32 uMprWaitCount;
  u_int32 uAccQP;
  u_int32 uCacheStat;
  u_int32 mbq_full;
  u_int32 mbq_empty;
  u_int32 slice_cnt;
  u_int32 mb_count;
  
  u_int32 uTotalTime_us;
  u_int32 uTotalFwTime_us;

  u_int32 uProcIaccTotRdCnt;
  u_int32 uProcDaccTotRdCnt;
  u_int32 uProcDaccTotWrCnt;
  u_int32 uProcDaccRegRdCnt;
  u_int32 uProcDaccRegWrCnt;
  u_int32 uProcDaccRngRdCnt;
  u_int32 uProcDaccRngWrCnt;

}MediaIPFW_Video_PicPerfInfo;
#else
typedef struct
{
  u_int32 uDpbmcCrc;
  u_int32 uFrameActiveCount;
  u_int32 uSliceActiveCount;
  u_int32 uRbspBytesCount;
  u_int32 uSibWaitCount;
  u_int32 uDpbReadCount;
  u_int32 uMprWaitCount;
  u_int32 uBBBCrc;
  u_int32 uAccQP;
  u_int32 uCacheStat;
  u_int32 uCRCSkip;
  u_int32 uCRCDrop;
  BOOL    bCRCValid;

  u_int32 uBaseDpbMcCrc;
  u_int32 uByteStreamCrc;

  u_int32 uCRC0;
  u_int32 uCRC1;
  u_int32 uCRC2;
  u_int32 uCRC3;
  u_int32 uCRC4;
  u_int32 uCRC5;
  u_int32 uCRC6;
  u_int32 uCRC7;
  u_int32 uCRC8;
  u_int32 uCRC9;
  u_int32 uCRC10;
  u_int32 uCRC11;
  u_int32 uCRC12;
  u_int32 uCRC13;
  u_int32 uCRC14;

  u_int32 mbq_full;
  u_int32 mbq_empty;
  u_int32 slice_cnt;
  u_int32 mb_count;

  u_int32 uTotalTime_us;
  u_int32 uTotalFwTime_us;

  u_int32 uSTCAtFrameDone;

  u_int32 uProcIaccTotRdCnt;
  u_int32 uProcDaccTotRdCnt;
  u_int32 uProcDaccTotWrCnt;
  u_int32 uProcDaccRegRdCnt;
  u_int32 uProcDaccRegWrCnt;
  u_int32 uProcDaccRngRdCnt;
  u_int32 uProcDaccRngWrCnt;

} MediaIPFW_Video_PicPerfInfo;

#endif

//////////////////////////////////////////////////////////////
// Decoder decoupled operation metrics event data structure

typedef struct
{ 
  u_int32 mb_count;
  u_int32 slice_cnt;
  
  /* Front End Metrics */
  u_int32 uDFEBinsUsed;
  u_int32 uDFECycleCount;
  u_int32 uDFESliceCycleCount;
  u_int32 uDFEIBWaitCount;
  u_int32 uDFENumBytes;

  u_int32 uProcIaccTotRdCnt;
  u_int32 uProcDaccTotRdCnt;
  u_int32 uProcDaccTotWrCnt;
  u_int32 uProcDaccRegRdCnt;
  u_int32 uProcDaccRegWrCnt;
  u_int32 uProcDaccRngRdCnt;
  u_int32 uProcDaccRngWrCnt;
  
  /* Back End metrics */
  u_int32 uNumBEUsed;
  u_int32 uTotalTime_us;
  u_int32 uTotalFwTime_us;
  u_int32 uDBECycleCount[0x2];
  u_int32 uDBESliceCycleCount[0x2];
  u_int32 uDBEMprWaitCount[0x2];
  u_int32 uDBEWaitCount[0x2];
  u_int32 uDBECRC[0x2];
  u_int32 uDBETotalTime_us[0x2];

  u_int32 uDBEMPRPRXWaitCount[0x2];
  u_int32 uDBEPXDPRXWaitCount[0x2];
  u_int32 uDBEFCHPLQWaitCount[0x2];
  u_int32 uDBEPXDPLQWaitCount[0x2];
  
  u_int32 uDBEFchWordsCount[0x2];  
  u_int32 uDBEDpbCRC[0x2];
  u_int32 uDBEDpbReadCount[0x2];
  u_int32 uDBECacheStats[0x2];
 
} MediaIPFW_Video_PicPerfDcpInfo, *pMediaIPFW_Video_PicPerfDcpInfo;

typedef struct
{
  u_int32 uPicType;                                      
  u_int32 uPicStruct;
  u_int32 bLastPicNPF;
  u_int32 uPicStAddr;
  u_int32 uFrameStoreID;
  MediaIPFW_Video_PicDispInfo    DispInfo;
  /*  Removed to upper level for just current frame
  MediaIPFW_Video_PicPerfInfo    PerfInfo;
  MediaIPFW_Video_PicPerfDcpInfo PerfDcpInfo;
  */
  
  u_int32 bUserDataAvail;
  u_int32 uPercentInErr;

  u_int32 uBbdHorActive;
  u_int32 uBbdVerActive;
  u_int32 uBbdLogoActive;
  u_int32 uBbdBotPrev;
  u_int32 uBbdMinColPrj;
  u_int32 uBbdMinRowPrj;
  u_int32 uFSBaseAddr;

  /* Only for RealVideo RPR */
  u_int32 uRprPicWidth;
  u_int32 uRprPicHeight;

  /*only for divx3*/
  u_int32 uFrameRate;
  
}MediaIPFW_Video_PicInfo;

//////////////////////////////////////////////////////////////
// Decoder Event info structure

typedef void VPU_EVENT_DATA;

//////////////////////////////////////////////////////////////
// Player Event info callback function

typedef void ( * VPU_Event_Callback_t )(
    u_int32          uStrIdx,
    VPU_EVENTTYPE    tEvent,
    VPU_EVENT_DATA  *ptEventData,
    u_int32 uEventDataLength
);

//////////////////////////////////////////////////////////////
// Player FS callback function

typedef void ( * VPU_FS_Callback_t )(
    u_int32          uStrIdx,
    VPU_EVENTTYPE    tEvent,
    VPU_EVENT_DATA  *ptEventData,
    u_int32 uEventDataLength
);


/*********************************************************************************/
/* Function prototypes   */
/*********************************************************************************/
u_int32 VPU_Init              ( pVPU_DRIVER_CFG ptDriverCfg, VPU_Event_Callback_t pfAppCallback, VPU_FS_Callback_t pfAppFsCallback );
u_int32 VPU_Term              ( void );
u_int32 VPU_Get_Version_Info ( u_int32 *uMaloneHWVersion, u_int32 *uMaloneHWFeatures, u_int32 *uMaloneFWVersion );
u_int32 VPU_Start             ( u_int32 uStrIdx );
u_int32 VPU_Stop              ( u_int32 uStrIdx );
u_int32 VPU_Flush             ( u_int32 uStrIdx, bool bClearDPB );
u_int32 VPU_Set_Event_Filter  ( u_int32 uStrIdx, VPU_EVENTTYPE uStrEventFilter );
u_int32 VPU_Get_Event_Filter  ( u_int32 uStrIdx );
u_int32 VPU_Set_Params        ( u_int32 uStrIdx, VPU_VPU_PARAMID_TYPE uStrParamId, u_int32 *uStrParamValue );
u_int32 VPU_Get_Params        ( u_int32 uStrIdx, VPU_VPU_PARAMID_TYPE uStrParamId, u_int32 * puStrParamValue );
u_int32 VPU_Get_Decode_Status ( u_int32 uStrIdx, u_int32 uStatusType, u_int32 uParamSetID, void ** ppStatusData );
u_int32 VPU_Frame_Alloc       ( u_int32 uStrIdx, pVPU_FS_DESC pVPUFsParams );
u_int32 VPU_DQ_Release_Frame  ( u_int32 uStrIdx, u_int32 uFsToRelease );
u_int32 VPU_Stream_FeedBuffer ( u_int32 uStrIdx, u_int32 uFeedBaseAddress, u_int32 uFeedChunkSize, u_int32 *uStreamPriorConsumed );

////////////////////////////////////////////////////////////////////////////////
// Queue functions
////////////////////////////////////////////////////////////////////////////////

MEDIAIP_FW_STATUS pal_qu_create ( unsigned int nMaxElements, 
                                  const char *pszName, 
                                  PAL_QUEUE_ID *pQuId );

MEDIAIP_FW_STATUS pal_qu_destroy ( PAL_QUEUE_ID QuId );


MEDIAIP_FW_STATUS pal_qu_send ( PAL_QUEUE_ID QuId, 
                                void         *pMessage );

MEDIAIP_FW_STATUS pal_qu_receive ( PAL_QUEUE_ID QuId, 
                                   u_int32      uTimeoutMs, 
                                   void         *pMessage );

MEDIAIP_FW_STATUS pal_initialise ( psPALConfig pconfig );
void pal_assert_impl(u_int32 uAssertPC, u_int32 uAssertInfo);
#endif //_MALONE_INTERFACE_H_

