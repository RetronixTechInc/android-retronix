ifeq ($(HAVE_FSL_IMX_CODEC),true)


LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	SoftHevcDec.cpp

LOCAL_CFLAGS += $(FSL_OMX_CFLAGS)

LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS) -Wl,-Bsymbolic

LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES) \
		$(LOCAL_PATH)/../../../../../../external/libhevc/decoder \
		$(LOCAL_PATH)/../../../../../../external/libhevc/common

LOCAL_STATIC_LIBRARIES  := libhevcdec

LOCAL_SHARED_LIBRARIES := lib_omx_common_v2_arm11_elinux \
                          lib_omx_osal_v2_arm11_elinux \
			  lib_omx_utils_v2_arm11_elinux

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= lib_omx_soft_hevc_dec_arm11_elinux
LOCAL_MODULE_TAGS := eng
include $(BUILD_SHARED_LIBRARY)

endif
