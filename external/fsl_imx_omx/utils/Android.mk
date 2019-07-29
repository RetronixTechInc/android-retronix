ifeq ($(HAVE_FSL_IMX_CODEC),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
SOURCE_FILES = \
	       Mem.cpp \
	       Queue.cpp \
	       RegistryAnalyser.cpp \
	       RingBuffer.cpp \
	       AudioTSManager.cpp \
	       FadeInFadeOut.cpp \
	       ShareLibarayMgr.cpp \
	       mfw_gst_ts.c \
	       Tsm_wrapper.c \
	       image_convert.c \
			colorconvert/src/cczoomrotation16.cpp \
			colorconvert/src/cczoomrotationbase.cpp \
			audio_frame_parser/AudioFrameParser.c \
			audio_frame_parser/AacFrameParser.c \
			audio_frame_parser/Mp3FrameParser.c \
			audio_frame_parser/Ac3FrameParser.c

LOCAL_CFLAGS += $(FSL_OMX_CFLAGS)

LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)
 
LOCAL_C_INCLUDES += $(FSL_OMX_INCLUDES) \
		    $(LOCAL_PATH)/colorconvert/include \
			$(LOCAL_PATH)/audio_frame_parser

LOCAL_SHARED_LIBRARIES := libdl lib_omx_osal_v2_arm11_elinux

ifeq ($(HAVE_FSL_IMX_CODEC), true)
            LOCAL_CFLAGS += -DUSE_FSL_JPEG_ENCODER
            LOCAL_SHARED_LIBRARIES += libfsl_jpeg_enc_arm11_elinux
endif

LOCAL_PRELINK_MODULE := false
	
LOCAL_MODULE:= lib_omx_utils_v2_arm11_elinux
LOCAL_MODULE_TAGS := eng
include $(BUILD_SHARED_LIBRARY)

endif
