ifeq ($(HAVE_FSL_IMX_CODEC),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(BOARD_VPU_TYPE), hantro)
LOCAL_SRC_FILES := vpu_wrapper_hantro.c \
	utils.c

ifeq ($(BOARD_SOC_TYPE), IMX8MM)
LOCAL_SRC_FILES += vpu_wrapper_hantro_encoder.c
endif

LOCAL_CFLAGS += -DSET_OUTPUT_CROP_RECT -DUSE_EXTERNAL_BUFFER -DUSE_OUTPUT_RELEASE -DVSI_API -DIS_G2_DECODER -DENABLE_CODEC_HEVC -DGET_FREE_BUFFER_NON_BLOCK -DDOWN_SCALER -DENABLE_CODEC_VP8

LOCAL_CFLAGS += -DENCH1
endif

ifeq ($(BOARD_VPU_TYPE), malone)

LOCAL_SRC_FILES := vpu_wrapper_amphion.c \
    utils.c

LOCAL_CFLAGS += -D DTV_GATHER_PERF_METRICS \
                    -D MVD_DTV_USERDATA \
                    -D MVD_WAIT_BOB_INACTIVE \
                    -D DECLIB_FORCE_HW_STOP \
                    -D MVD_NO_BSDMA_SAFETY_MARGIN \
                    -D MVD_CQ_ENABLE_REFILL \
                    -D MVD_SPP_HW_GOULOMB \
                    -D SVC_SFA_ADD_ERROR_CHECKING \
                    -D MVC_SFA_ADD_ERROR_CHECKING \
                    -D SVC_SPP_SAVE_CTX_PER_VCL_NAL \
                    -D MVD_CQ_CQSR \
                    -D AVC_SUPPORT_THRU_MVC \
                    -D MVC_ERROR_CONTROL_INSERT_SKIP_START_CONTROLS \
                    -D DECLIB_CTX_FLUSH_AFTER_SAVE \
                    -D DECLIB_SERVICE_EOS \
                    -D MVD_PERF_MEASURE \
                    -D VC1_ENABLED \
                    -D HEVC_ENABLED \
                    -D HEVC_CM_WORKAROUND \
                    -D HEVC_NEW_OUTPUT_TRIGGER \
                    -D HEVC_ALL_PICS_REF \
                    -D HEVC_SCAL_LIST_USE_YCRCB_XREF \
                    -D HEVC_SFA_ADD_ERROR_CHECKING \
                    -D MVD_DFE_DBG \
                    -D HEVC_JVT_MODEL=100 \
                    -D PAL_CLOCK_API \
                    -D SVC_ADDITIONAL_DEBUG \
                    -D DIAG_SUPPORT_ENABLED \
                    -D FW_API_VERSION=19 \
                    -D GLOBAL_USE_RUN_TIME_CFG \
                    -D ENABLE_TRACE_IN_RELEASE=0 \
                    -D YES=1 \
                    -D NO=0 \
                    -D NONE=0 \
                    -D NUP=1 \
                    -D UCOS=2 \
                    -D UCOS3=3 \
                    -D RTOS=0 \
                    -D USE_DECODER \
                    -D ARM=0 \
                    -D MIPS=1 \
                    -D X86=2 \
                    -D OR1K=3 \
                    -D CPU=0 \
                    -D NO_AL=0 \
                    -D CNXT_KAL=1 \
                    -D NXP_OSAL=2 \
                    -D OSAL=0 \
                    -D ARM926=0 \
                    -D ARMR5=1 \
                    -D ARMA53=2 \
                    -D ARM_CPU_TYPE=2 \
                    -D ADS=0 \
                    -D RVDS=1 \
                    -D GNU_MIPS=2 \
                    -D GNU_MIPS_LNX=3 \
                    -D GNU_ARM=4 \
                    -D GNU_ARM_SOURCERY=5 \
                    -D GNU_X86=6 \
                    -D WIN_X86=7 \
                    -D DS5=8 \
                    -D GNU_OR32=9 \
                    -D GNU_ARM_LINARO=10 \
                    -D GNU_OR1K=11 \
                    -D TOOLSET=10 \
                    -D NO_DEBUG=0 \
                    -D BUILD_DEBUG=1 \
                    -D ARRAY_DEBUG=2 \
                    -D FULL_DEBUG=3 \
                    -D DEBUG_CAPS=0 \
                    -D GENTB_PLATFORM=0 \
                    -D WIN_LIB=1 \
                    -D GEN_TB_ENC=2 \
                    -D TARGET_PLATFORM=0 \
                    -D VIDEO_TRANS=0 \
                    -D GTB_TRANS=1 \
                    -D GTB_DEC=2 \
                    -D WINDSOR_LIB=3 \
                    -D GTB_ENC=4 \
                    -D MEDIA_DEC=5 \
                    -D MEDIA_LIB=6 \
                    -D PAL_CLOCK_API \
                    -D SVC_ADDITIONAL_DEBUG \
                    -D DIAG_SUPPORT_ENABLED \
                    -D FW_API_VERSION=19 \
                    -D GLOBAL_USE_RUN_TIME_CFG \
                    -D ENABLE_TRACE_IN_RELEASE=0 \
                    -D YES=1 \
                    -D NO=0 \
                    -D NONE=0 \
                    -D NUP=1 \
                    -D UCOS=2 \
                    -D UCOS3=3 \
                    -D RTOS=0 \
                    -D USE_DECODER \
                    -D CHIP=0 \
                    -D EMULATION=1 \
                    -D HAPS=2 \
                    -D SIMULATION=3 \
                    -D CMODEL=4 \
                    -D TARGET_LEVEL=0 \
                    -D SVC_DISABLED=0 \
                    -D SVC_ENABLED=1 \
                    -D SVC_SUPPORT=0 \
                    -D MVC_DISABLED=0 \
                    -D MVC_ENABLED=1 \
                    -D MVC_SUPPORT=1 \
                    -D SFA_DISABLED=0 \
                    -D SFA_ENABLED=1 \
                    -D SFA_SUPPORT=1 \
                    -D CNXT_HW=0 \
                    -D NXP_HW=1 \
                    -D HWLIB=1 \
                    -D DTV=0 \
                    -D STB=1 \
                    -D PLAYMODE=0 \
                    -D STANDARD=0 \
                    -D REBOOT=1 \
                    -D BOOT_ARCH=0 \
                    -D TBPLAYER_FLOW_CHANGE_ON_REF_FRMS \
                    -D PULSAR_MERGE \
                    -D FSLCACHE_ENABLED \
                    -D DECLIB_ENABLE_DFE -D DECLIB_ENABLE_DBE \
                    -D DECLIB_ENABLE_DCP -D MVD_DCP_DYNAMIC_CONFIG \
                    -D DECLIB_4K_SUPPORTED -D HEVC_LEVEL_5PT0_SUPPORT \
                    -D DECLIB_ISR_IN_THREAD_CTX \
                    -D JPG_ENABLED \
                    -D JPGD_AUTO_DOWN_SCALE \
                    -D SPARK_ENABLED \
                    -D RV_ENABLED \
                    -D VP6_ENABLED \
                    -D VP8_ENABLED \
                    -D JPG_DPV_ENABLED \
                    -D SVN_VERSION=0 \
                    -D DECLIB_USER_SPACE \
                    -D DECLIB_USE_DMA_MEM_REGION \
                    -D USE_ION \
                    #-D ENABLE_PERF_TIMER \
                    #-D DISABLE_TRACE

LOCAL_CFLAGS +=        -D NXP_MX_REAL_TARGET
LOCAL_CFLAGS +=        -D VPU_TEST_APP=7
LOCAL_CFLAGS +=        -D TARGET_APP=7

LOCAL_CFLAGS_64 += -D MALONE_64BIT_ADDR
endif

ifeq ($(BOARD_VPU_TYPE), chipsmedia)
LOCAL_SRC_FILES := vpu_wrapper.c
endif

LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)

LOCAL_CFLAGS += $(FSL_OMX_CFLAGS) -Wno-error=date-time


LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)

LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES) \

LOCAL_MODULE:= lib_vpu_wrapper


LOCAL_SHARED_LIBRARIES := libutils libc libm \
    			  lib_omx_osal_v2_arm11_elinux

ifeq ($(BOARD_SOC_TYPE), IMX8MM)
LOCAL_SHARED_LIBRARIES += libcodec_enc libhantro_h1
endif

ifeq ($(BOARD_VPU_TYPE), hantro)
LOCAL_SHARED_LIBRARIES +=  libcodec libhantro
endif

ifeq ($(BOARD_VPU_TYPE), malone)
LOCAL_SHARED_LIBRARIES += libvpu-malone
endif

ifeq ($(BOARD_VPU_TYPE), chipsmedia)
LOCAL_SHARED_LIBRARIES +=  libvpu
endif

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := eng
include $(BUILD_SHARED_LIBRARY)

endif
