ifeq ($(HAVE_FSL_IMX_CODEC),true)


LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	OMXPlayer.cpp \
	OMXMetadataRetriever.cpp \
	OMXAndroidUtils.cpp
		
ifneq ($(findstring x2.2,x$(PLATFORM_VERSION)), x2.2)
    LOCAL_SRC_FILES += \
		OMXRecorder.cpp 
endif
ifeq ($(findstring x2.3,x$(PLATFORM_VERSION)), x2.3)
	LOCAL_SRC_FILES += \
	 OMXFastPlayer.cpp \
	 OMXMediaScanner.cpp 
endif
ifeq ($(findstring x3,x$(PLATFORM_VERSION)), x3)
	LOCAL_SRC_FILES += \
	 OMXFastPlayer.cpp \
	 OMXMediaScanner.cpp 
endif
ifeq ($(findstring x4.0,x$(PLATFORM_VERSION)), x4.0)
	LOCAL_SRC_FILES += \
	 OMXFastPlayer.cpp \
	 OMXMediaScanner.cpp 
endif


LOCAL_CFLAGS += $(FSL_OMX_CFLAGS)
 
LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)

LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES) 

LOCAL_SHARED_LIBRARIES := lib_omx_osal_v2_arm11_elinux \
	lib_omx_client_arm11_elinux \
	libmedia          \
	libbinder         \
	libutils  \
	libcutils \
	libsurfaceflinger \
	libgui \
	libsonivox \
	lib_omx_utils_v2_arm11_elinux

ifeq ($(findstring x2,x$(PLATFORM_VERSION)), x2)
	LOCAL_SHARED_LIBRARIES += \
		libsurfaceflinger_client 
endif
ifeq ($(findstring x3,x$(PLATFORM_VERSION)), x3)
	LOCAL_SHARED_LIBRARIES += \
		libsurfaceflinger_client 
endif
ifeq ($(findstring x4.0,x$(PLATFORM_VERSION)), x4.0)
	LOCAL_SHARED_LIBRARIES += \
		libsurfaceflinger_client 
endif

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= lib_omx_player_arm11_elinux
LOCAL_MODULE_TAGS := eng
include $(BUILD_SHARED_LIBRARY)

endif
