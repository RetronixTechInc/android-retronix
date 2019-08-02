LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := Receive_Tools
LOCAL_CERTIFICATE := platform

include $(BUILD_PACKAGE)

include $(CLEAR_VARS)
LOCAL_MODULE := pidalive
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := pidalive
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := rtxupdate.sh
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := rtxupdate.sh
include $(BUILD_PREBUILT)
