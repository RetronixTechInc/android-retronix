ifeq ($(HAVE_FSL_IMX_CODEC),true)


LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_SRC_FILES := \
	vpu_test.cpp
		
LOCAL_CFLAGS += $(FSL_OMX_CFLAGS) -DDIVXINT_USE_STDINT
 
LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES)

LOCAL_SHARED_LIBRARIES := lib_omx_osal_v2_arm11_elinux \
                          lib_omx_core_v2_arm11_elinux \
			  lib_omx_utils_v2_arm11_elinux
	
LOCAL_MODULE:= omx_vpu_test_arm11_elinux
LOCAL_32_BIT_ONLY := true

include $(BUILD_EXECUTABLE)

endif
