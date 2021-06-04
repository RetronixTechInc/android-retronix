ifeq ($(HAVE_FSL_IMX_CODEC),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)

LOCAL_SRC_FILES := \
	V4l2Dec.cpp


LOCAL_CFLAGS += $(FSL_OMX_CFLAGS) -DENABLE_TS_MANAGER

LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)
 
LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../v4l2_common

LOCAL_SHARED_LIBRARIES := lib_omx_common_v2_arm11_elinux \
                          lib_omx_osal_v2_arm11_elinux \
                          lib_omx_utils_v2_arm11_elinux \
						  lib_omx_v4l2_common_arm11_elinux

LOCAL_PRELINK_MODULE := false
	
LOCAL_CLANG := true
LOCAL_MODULE:= lib_omx_v4l2_dec_arm11_elinux

include $(BUILD_SHARED_LIBRARY)

endif
