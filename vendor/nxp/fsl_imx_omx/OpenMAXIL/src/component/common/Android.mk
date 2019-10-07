ifeq ($(HAVE_FSL_IMX_CODEC),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_SRC_FILES := \
	ComponentBase.cpp \
        ExecutingState.cpp \
        IdleState.cpp \
        InvalidState.cpp \
        LoadedState.cpp \
        PauseState.cpp \
        Port.cpp \
        State.cpp \
        WaitForResourcesState.cpp \
		VideoFilter.cpp \
		VideoRender.cpp

ifneq ($(FSL_VPU_OMX_ONLY), true)
LOCAL_SRC_FILES += \
		AudioFilter.cpp \
		UniaDecoder.cpp
endif

ifeq ($(FSL_BUILD_OMX_PLAYER), true)
LOCAL_SRC_FILES += \
		AudioParserBase.cpp \
		Parser.cpp \
		Muxer.cpp \
		AudioRender.cpp \
		VideoSource.cpp \
		AudioSource.cpp
endif

LOCAL_CFLAGS += $(FSL_OMX_CFLAGS)

LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)
 
LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES) 

LOCAL_SHARED_LIBRARIES := lib_omx_osal_v2_arm11_elinux \
    			  lib_omx_utils_v2_arm11_elinux \
				  lib_omx_res_mgr_v2_arm11_elinux \
				  lib_omx_core_v2_arm11_elinux \
				  libutils 

ifeq ($(FSL_BUILD_OMX_PLAYER), true)
LOCAL_SHARED_LIBRARIES += lib_id3_parser_arm11_elinux
endif

LOCAL_PRELINK_MODULE := false
	
LOCAL_MODULE:= lib_omx_common_v2_arm11_elinux
LOCAL_MODULE_TAGS := eng
include $(BUILD_SHARED_LIBRARY)

endif
