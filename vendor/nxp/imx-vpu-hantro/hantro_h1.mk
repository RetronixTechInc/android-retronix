LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    h1_encoder/software/linux_reference/ewl/ewl_linux_lock.c \
    h1_encoder/software/linux_reference/ewl/ewl_x280_common.c \
    h1_encoder/software/linux_reference/ewl/ewl_x280_irq.c

LOCAL_CFLAGS += $(IMX_VPU_CFLAGS) -DEWL_NO_HW_TIMEOUT


LOCAL_LDFLAGS += $(IMX_VPU_LDFLAGS)

LOCAL_LDFLAGS += -shared -nostartfiles -Wl,-Bsymbolic -Wl,-z -Wl,muldefs
LOCAL_LDFLAGS += $(IMX_VPU_G1_LDFLAGS)

LOCAL_CFLAGS_arm64 += -DUSE_64BIT_ENV

LOCAL_C_INCLUDES += $(IMX_VPU_ENC_INCLUDES) \
    device/fsl/common/kernel-headers

LOCAL_C_INCLUDES += $(LOCAL_PATH)/legacy \
	$(LOCAL_PATH)/h1_encoder/software/source/vp8 \
    $(LOCAL_PATH)/h1_encoder/software/source/h264 \
	$(LOCAL_PATH)/h1_encoder/software/linux_reference/ewl

LOCAL_WHOLE_STATIC_LIBRARIES := lib_imx_vsi_vp8_enc lib_imx_vsi_h264_enc


ifeq ($(ENABLE_HANTRO_DEBUG_LOG), true)
LOCAL_SHARED_LIBRARIES := liblog libcutils
endif

LOCAL_MODULE:= libhantro_h1
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_SHARED_LIBRARY)


