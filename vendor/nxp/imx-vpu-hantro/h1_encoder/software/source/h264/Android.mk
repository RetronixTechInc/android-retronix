LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    H264Cabac.c \
    H264CodeFrame.c \
    H264Denoise.c \
    H264EncApi.c \
    h264encapi_ext.c \
    H264Init.c \
    H264Mad.c \
    H264NalUnit.c \
    H264PictureBuffer.c \
    H264PictureParameterSet.c \
    H264PutBits.c \
    H264RateControl.c \
    H264Sei.c \
    H264SequenceParameterSet.c \
    H264Slice.c \
    H264TestId.c \
    ../common/encasiccontroller.c \
    ../common/encasiccontroller_v2.c \
    ../common/encpreprocess.c \
    ../common/encInputLineBuffer.c \
    ../common/encswhwregisters.c


LOCAL_CFLAGS += $(IMX_VPU_CFLAGS)
LOCAL_CFLAGS += $(IMX_VPU_G1_CFLAGS)



LOCAL_CFLAGS += -DGET_FREE_BUFFER_NON_BLOCK -DGET_OUTPUT_BUFFER_NON_BLOCK -DGET_OUTPUT_BUFFER_NON_BLOCK
LOCAL_CFLAGS +=  -DFIFO_DATATYPE=addr_t
LOCAL_LDFLAGS += $(IMX_VPU_LDFLAGS)

LOCAL_CFLAGS_arm64 += -DUSE_64BIT_ENV

LOCAL_C_INCLUDES += $(IMX_VPU_ENC_INCLUDES)

LOCAL_VENDOR_MODULE := true
LOCAL_MODULE:= lib_imx_vsi_h264_enc
LOCAL_MODULE_TAGS := eng
include $(BUILD_STATIC_LIBRARY)

