ifeq ($(HAVE_FSL_IMX_CODEC),true)


LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        GMPlayerWrapper.cpp \
	GMPlayer.cpp \
	OMX_MetadataExtractor.cpp \
	OMX_Recorder.cpp \
	GMRecorder.cpp \
	GMComponent.cpp \
	MediaInspector.cpp \
	PMemory.cpp \
	GMSubtitleSource.cpp \
	GMSubtitlePlayer.cpp
		
LOCAL_CFLAGS += $(FSL_OMX_CFLAGS) 

LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)
 
LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES) \
		    device/fsl-proprietary/include

LOCAL_SHARED_LIBRARIES := lib_omx_osal_v2_arm11_elinux \
                          lib_omx_utils_v2_arm11_elinux \
			  lib_omx_core_mgr_v2_arm11_elinux \
			  lib_omx_res_mgr_v2_arm11_elinux \
			  libutils

ifeq ($(HAVE_FSL_IMX_GPU2D), true)
LOCAL_SRC_FILES += GPUMemory.cpp
LOCAL_CFLAGS += -DUSE_GPU
LOCAL_SHARED_LIBRARIES += libg2d
endif

LOCAL_PRELINK_MODULE := false
	
LOCAL_MODULE:= lib_omx_client_arm11_elinux
LOCAL_MODULE_TAGS := eng
include $(BUILD_SHARED_LIBRARY)

endif
