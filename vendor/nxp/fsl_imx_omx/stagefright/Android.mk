LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)

LOCAL_MODULE := libstagefrighthw

LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += $(FSL_OMX_CFLAGS)

LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)

LOCAL_C_INCLUDES := $(FSL_OMX_INCLUDES) \
                    frameworks/native/include

LOCAL_SRC_FILES := src/OMXFSLPlugin_new.cpp

LOCAL_SHARED_LIBRARIES := \
       lib_omx_core_v2_arm11_elinux \
       lib_omx_osal_v2_arm11_elinux \
       lib_omx_res_mgr_v2_arm11_elinux \
       lib_omx_tunneled_decoder_arm11_elinux \
       libui libutils libcutils liblog

ifeq ($(findstring x2.3,x$(PLATFORM_VERSION)), x2.3)
LOCAL_SRC_FILES := src/OMXFSLPlugin.cpp \
    		   src/FSLRenderer.cpp

LOCAL_SHARED_LIBRARIES := \
        libbinder         \
        libmedia          \
        libutils          \
        libcutils         \
        libsurfaceflinger_client \
	libdl \
	libipu lib_omx_res_mgr_v2_arm11_elinux
endif

ifeq ($(findstring x3.,x$(PLATFORM_VERSION)), x3.)
LOCAL_SRC_FILES := src/OMXFSLPlugin_new.cpp

LOCAL_SHARED_LIBRARIES := \
    	lib_omx_core_v2_arm11_elinux \
	lib_omx_res_mgr_v2_arm11_elinux \
	libui libutils libcutils
endif

include $(BUILD_SHARED_LIBRARY)

