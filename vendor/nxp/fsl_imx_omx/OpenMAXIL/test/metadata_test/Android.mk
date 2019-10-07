ifeq ($(HAVE_FSL_IMX_CODEC),true)


LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	    metadata_test.cpp

ifeq ($(FSL_CODEC_PATH),)
     FSL_CODEC_PATH := device
endif

LOCAL_CFLAGS += $(FSL_OMX_CFLAGS) -DDIVXINT_USE_STDINT

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../ghdr \
					$(FSL_CODEC_PATH)/fsl-codec/ghdr \
                    $(LOCAL_PATH)/../../../OSAL/ghdr \
                    $(LOCAL_PATH)/../../../utils

LOCAL_SHARED_LIBRARIES := lib_omx_osal_v2_arm11_elinux \
	                      lib_omx_core_v2_arm11_elinux \
						  lib_omx_utils_v2_arm11_elinux \
						  lib_omx_common_v2_arm11_elinux \
						  lib_omx_client_arm11_elinux

LOCAL_MODULE:= omx_metadata_test_arm11_elinux

include $(BUILD_EXECUTABLE)

endif
