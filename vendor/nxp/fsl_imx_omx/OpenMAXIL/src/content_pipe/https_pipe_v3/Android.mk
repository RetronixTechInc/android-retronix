ifeq ($(HAVE_FSL_IMX_CODEC),true)


LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_SRC_FILES := \
	HttpsPipe.cpp \
	HttpsProtocol.cpp \
	../streaming_common/StreamingCache.cpp \
	../streaming_common/StreamingProtocol.cpp \
	../streaming_common/StreamingPipe.cpp 
		
LOCAL_CFLAGS += $(FSL_OMX_CFLAGS)

LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)
 
LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES)

LOCAL_SHARED_LIBRARIES := lib_omx_osal_v2_arm11_elinux \
                          lib_omx_utils_v2_arm11_elinux

LOCAL_PRELINK_MODULE := false
	
LOCAL_MODULE:= lib_omx_https_pipe_v3_arm11_elinux
LOCAL_MODULE_TAGS := eng
include $(BUILD_SHARED_LIBRARY)

endif
