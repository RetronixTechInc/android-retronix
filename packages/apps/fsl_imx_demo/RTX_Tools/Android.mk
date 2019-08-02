LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := RTX_Tools
LOCAL_CERTIFICATE := platform

include $(BUILD_PACKAGE)

# Use the folloing include to make our test apk.
include $(call all-makefiles-under,$(LOCAL_PATH))

#rtx_setenv
include $(CLEAR_VARS)
LOCAL_MODULE := rtx_setenv
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := rtx_setenv
include $(BUILD_PREBUILT)

#rtx_setting
include $(CLEAR_VARS)
LOCAL_MODULE := rtx_setting
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := rtx_setting
include $(BUILD_PREBUILT)

#efm32fn
include $(CLEAR_VARS)
RTX_TOOLS_PATH := $(CURDIR)/$(LOCAL_PATH)
efm32fn_cleanup:=dummy
$(efm32fn_cleanup) :
	rm -f $(RTX_TOOLS_PATH)/efm32fn

efm32fn_file :=iMX6-$(BOARD_RTX_VENDOR)
$(RTX_TOOLS_PATH)/$(efm32fn_file):$(efm32fn_cleanup) $(ACP)
	$(ACP) $(RTX_TOOLS_PATH)/iMX6-$(BOARD_RTX_VENDOR) $(RTX_TOOLS_PATH)/efm32fn

LOCAL_MODULE := efm32fn
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT)/bin
LOCAL_SRC_FILES := $(efm32fn_file)
include $(BUILD_PREBUILT)

