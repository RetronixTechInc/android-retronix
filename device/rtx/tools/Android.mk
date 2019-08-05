LOCAL_PATH := $(call my-dir)

########################################################################
# busybox
include $(CLEAR_VARS)
LOCAL_SRC_FILES := bin/busybox 
LOCAL_MODULE := busybox
LOCAL_MODULE_PATH := $(TARGET_OUT)/xbin
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
include $(BUILD_PREBUILT)

########################################################################
# efm32cmd
#include $(CLEAR_VARS)
#LOCAL_SRC_FILES := bin/efm32cmd 
#LOCAL_MODULE := efm32cmd
#LOCAL_MODULE_PATH := $(TARGET_OUT)/xbin
#LOCAL_MODULE_CLASS := EXECUTABLES
#LOCAL_MODULE_TAGS := optional
#include $(BUILD_PREBUILT)

########################################################################
# rtx.init
include $(CLEAR_VARS)
LOCAL_SRC_FILES := bin/rtx.init
LOCAL_MODULE := rtx.init
LOCAL_MODULE_PATH := $(TARGET_OUT)/xbin
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_TAGS := optional
include $(BUILD_PREBUILT)

########################################################################
# iwconfig
#include $(CLEAR_VARS)

#LOCAL_SRC_FILES := bin/iwconfig
#LOCAL_MODULE := iwconfig
#LOCAL_MODULE_PATH := $(TARGET_OUT)/xbin
#LOCAL_MODULE_CLASS := EXECUTABLES
#LOCAL_MODULE_TAGS := optional
#include $(BUILD_PREBUILT)
