LOCAL_PATH := $(call my-dir)
ENCODER_RELEASE := ../../../h1_encoder/software

############################################################
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libh1enc
LOCAL_MODULE_TAGS := optional

INCLUDES := $(LOCAL_PATH)/$(ENCODER_RELEASE)

LOCAL_SRC_FILES := \
    $(ENCODER_RELEASE)/source/common/encasiccontroller_v2.c \
    $(ENCODER_RELEASE)/source/common/encasiccontroller.c \
    $(ENCODER_RELEASE)/source/common/encpreprocess.c \
    $(ENCODER_RELEASE)/source/h264/H264CodeFrame.c\
    $(ENCODER_RELEASE)/source/h264/H264Init.c\
    $(ENCODER_RELEASE)/source/h264/H264NalUnit.c\
    $(ENCODER_RELEASE)/source/h264/H264PictureParameterSet.c\
    $(ENCODER_RELEASE)/source/h264/H264PutBits.c\
    $(ENCODER_RELEASE)/source/h264/H264RateControl.c\
    $(ENCODER_RELEASE)/source/h264/H264SequenceParameterSet.c\
    $(ENCODER_RELEASE)/source/h264/H264Slice.c\
    $(ENCODER_RELEASE)/source/h264/H264EncApi.c\
    $(ENCODER_RELEASE)/source/h264/H264Cabac.c\
    $(ENCODER_RELEASE)/source/h264/H264Mad.c\
    $(ENCODER_RELEASE)/source/h264/H264Sei.c\
    $(ENCODER_RELEASE)/source/vp8/vp8codeframe.c\
    $(ENCODER_RELEASE)/source/vp8/vp8init.c\
    $(ENCODER_RELEASE)/source/vp8/vp8putbits.c\
    $(ENCODER_RELEASE)/source/vp8/vp8header.c\
    $(ENCODER_RELEASE)/source/vp8/vp8picturebuffer.c\
    $(ENCODER_RELEASE)/source/vp8/vp8picparameterset.c\
    $(ENCODER_RELEASE)/source/vp8/vp8entropy.c\
    $(ENCODER_RELEASE)/source/vp8/vp8macroblocktools.c\
    $(ENCODER_RELEASE)/source/vp8/vp8ratecontrol.c\
    $(ENCODER_RELEASE)/source/vp8/vp8encapi.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpeg.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegInit.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegCodeFrame.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegPutBits.c\
    $(ENCODER_RELEASE)/source/jpeg/JpegEncApi.c\
    $(ENCODER_RELEASE)/source/camstab/vidstabcommon.c\
    $(ENCODER_RELEASE)/source/camstab/vidstabalg.c\
    $(ENCODER_RELEASE)/source/camstab/vidstabapi.c\
    $(ENCODER_RELEASE)/source/camstab/vidstabinternal.c\
    $(ENCODER_RELEASE)/linux_reference/debug_trace/enctrace.c\
    $(ENCODER_RELEASE)/linux_reference/debug_trace/enctracestream.c\
    $(ENCODER_RELEASE)/source/common/encswhwregisters.c\
    $(ENCODER_RELEASE)/source/h264/H264PictureBuffer.c

ifeq ($(INCLUDE_TESTING),y)
	LOCAL_SRC_FILES += \
    $(ENCODER_RELEASE)/source/h264/H264TestId.c\
    $(ENCODER_RELEASE)/source/h264/h264encapi_ext.c\
    $(ENCODER_RELEASE)/source/vp8/vp8testid.c
    LOCAL_CFLAGS += -DINTERNAL_TEST
endif

LOCAL_C_INCLUDES := \
    $(INCLUDES)/inc \
    $(INCLUDES)/source/h264 \
    $(INCLUDES)/source/vp8 \
    $(INCLUDES)/source/jpeg \
    $(INCLUDES)/source/common \
    $(INCLUDES)/source/camstab \
    $(INCLUDES)/linux_reference/ewl \
    $(INCLUDES)/linux_reference/debug_trace \
    $(INCLUDES)/linux_reference/kernel_module \
    $(INCLUDES)/linux_reference/memalloc


LOCAL_CFLAGS += \
    -DVIDEOSTAB_ENABLED

include $(BUILD_STATIC_LIBRARY)

############################################################
