LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := eng
LOCAL_C_INCLUDES += bootable/recovery
LOCAL_SRC_FILES := recovery_ui.cpp

# should match TARGET_RECOVERY_UI_LIB set in BoardConfigCommon.mk
LOCAL_MODULE := librecovery_ui_imx

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
