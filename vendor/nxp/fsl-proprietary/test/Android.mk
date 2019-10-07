
LOCAL_PATH := $(call my-dir)

ifeq ($(PREBUILT_FSL_IMX_GPU),true)
ifeq ($(TARGET_BOARD_PLATFORM),imx6)
include $(CLEAR_VARS)
LOCAL_MODULE :=gmem_info
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SRC_FILES :=  bin/gmem_info
include $(BUILD_PREBUILT)
endif
endif
