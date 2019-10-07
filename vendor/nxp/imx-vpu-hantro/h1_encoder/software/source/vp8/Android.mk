LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    vp8encapi.c \
    vp8codeframe.c \
    vp8entropy.c \
    vp8header.c \
    vp8init.c \
    vp8macroblocktools.c \
    vp8picparameterset.c \
    vp8picturebuffer.c \
    vp8putbits.c \
    vp8ratecontrol.c \
    vp8testid.c \
    ../common/encasiccontroller.c \
    ../common/encasiccontroller_v2.c \
    ../common/encpreprocess.c \
    ../common/encswhwregisters.c


LOCAL_CFLAGS += $(IMX_VPU_CFLAGS)
LOCAL_CFLAGS += $(IMX_VPU_G1_CFLAGS)



LOCAL_CFLAGS += -DGET_FREE_BUFFER_NON_BLOCK -DGET_OUTPUT_BUFFER_NON_BLOCK -DGET_OUTPUT_BUFFER_NON_BLOCK
LOCAL_CFLAGS +=  -DFIFO_DATATYPE=addr_t
LOCAL_LDFLAGS += $(IMX_VPU_LDFLAGS)

LOCAL_CFLAGS_arm64 += -DUSE_64BIT_ENV

LOCAL_C_INCLUDES += $(IMX_VPU_ENC_INCLUDES)

LOCAL_VENDOR_MODULE := true
LOCAL_MODULE:= lib_imx_vsi_vp8_enc
LOCAL_MODULE_TAGS := eng
include $(BUILD_STATIC_LIBRARY)

