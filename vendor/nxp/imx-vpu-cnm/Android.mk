ifneq ($(PREBUILT_FSL_IMX_VPU),true)
ifeq ($(BOARD_HAVE_VPU),true)
ifeq ($(BOARD_VPU_TYPE), chipsmedia)

LOCAL_PATH := $(call my-dir)

ifeq ($(FSL_PROPRIETARY_PATH),)
     FSL_PROPRIETARY_PATH := device
endif
ifeq ($(IMX_PATH),)
     IMX_PATH := hardware
endif

# Share library
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
	vpu_io.c \
	vpu_util.c \
	vpu_lib.c \
	vpu_gdi.c \
	vpu_debug.c
ifeq ($(BOARD_SOC_CLASS),IMX6)
LOCAL_CFLAGS += -DBUILD_FOR_ANDROID -DIMX6Q
else
LOCAL_CFLAGS += -DBUILD_FOR_ANDROID -D$(BOARD_SOC_TYPE)
endif
ifeq ($(USE_GPU_ALLOCATOR), true)
LOCAL_CFLAGS += -DUSE_GPU=1
ifneq ($(BUILD_IN_ANDROIDTHINGS),true)
LOCAL_C_INCLUDES += $(FSL_PROPRIETARY_PATH)/fsl-proprietary/include
else
LOCAL_C_INCLUDES += device/nxp/$(TARGET_BOOTLOADER_BOARD_NAME)/include
endif
LOCAL_SHARED_LIBRARIES := libutils libc liblog libg2d
else
ifeq ($(USE_ION_ALLOCATOR), true)
LOCAL_CFLAGS += -DUSE_ION
LOCAL_SHARED_LIBRARIES := libutils libc liblog libion
ifneq ($(BUILD_IN_ANDROIDTHINGS),true)
LOCAL_C_INCLUDES += device/fsl/common/kernel-headers \
                    $(IMX_PATH)/imx/include \
                    system/core/libion
else
LOCAL_C_INCLUDES += device/nxp/$(TARGET_BOOTLOADER_BOARD_NAME)/kernel-headers \
                    device/nxp/$(TARGET_BOOTLOADER_BOARD_NAME)/include
endif
else
LOCAL_SHARED_LIBRARIES := libutils libc liblog
endif
endif
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_MODULE := libvpu
LOCAL_LD_FLAGS += -nostartfiles
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := eng
include $(BUILD_SHARED_LIBRARY)

endif
endif
endif
