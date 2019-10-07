LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += $(IMX_VPU_INCLUDES)

LOCAL_SRC_FILES := h1_encoder/software/linux_reference/test/h264/H264TestBench.c \
           h1_encoder/software/linux_reference/test/h264/EncGetOption.c \
           h1_encoder/software/linux_reference/test/h264/ssim.c
		   


LOCAL_CFLAGS += $(IMX_VPU_CFLAGS)
LOCAL_LDFLAGS += $(IMX_VPU_LDFLAGS)

LOCAL_CFLAGS += $(IMX_VPU_G1_CFLAGS)
LOCAL_LDFLAGS += $(IMX_VPU_G1_LDFLAGS)

LOCAL_C_INCLUDES += $(IMX_VPU_INCLUDES)


LOCAL_CFLAGS += -D_SW_DEBUG_PRINT -DTB_PP -DEWL_NO_HW_TIMEOUT
LOCAL_CFLAGS_arm64 += -DUSE_64BIT_ENV
LOCAL_CFLAGS += -DGET_FREE_BUFFER_NON_BLOCK -DGET_OUTPUT_BUFFER_NON_BLOCK -DGET_OUTPUT_BUFFER_NON_BLOCK \
	-DSKIP_OPENB_FRAME -DENABLE_DPB_RECOVER -D_DISABLE_PIC_FREEZE -DUSE_WORDACCESS

LOCAL_C_INCLUDES += device/fsl/common/kernel-headers
LOCAL_C_INCLUDES += $(LOCAL_PATH)/h1_encoder/software/inc \
    $(LOCAL_PATH)/h1_encoder/software/source/h264 \
    $(LOCAL_PATH)/h1_encoder/software/source/common \
    $(LOCAL_PATH)/h1_encoder/software/camstab \
    $(LOCAL_PATH)/h1_encoder/software/linux_reference/debug_trace

LOCAL_SHARED_LIBRARIES  += libhantro_h1

LOCAL_32_BIT_ONLY := true
LOCAL_MODULE:= h264_testenc
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_EXECUTABLE)
