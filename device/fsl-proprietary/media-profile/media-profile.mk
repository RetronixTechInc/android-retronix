LOCAL_PATH := $(call my-dir)

#for video recoder profile setting
include $(CLEAR_VARS)

ifeq ($(BOARD_HAVE_VPU), true)

ifeq ($(BOARD_SOC_TYPE),IMX51)
LOCAL_SRC_FILES := media_profiles_d1.xml
endif

ifeq ($(BOARD_SOC_TYPE),IMX53)
LOCAL_SRC_FILES := media_profiles_720p.xml
endif

#for mx6x, it should be up to 1080p profile
ifeq ($(BOARD_SOC_TYPE),IMX6DQ)
LOCAL_SRC_FILES := media_profiles_1080p.xml
endif

ifeq ($(BOARD_HAVE_USB_CAMERA),true)
LOCAL_SRC_FILES := media_profiles_480p.xml
endif

else
LOCAL_SRC_FILES := media_profiles_qvga.xml
endif

LOCAL_MODULE := media_profiles.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)

# for Multimedia codec profile setting
include $(CLEAR_VARS)
ifeq ($(BOARD_HAVE_VPU), true)
ifeq ($(BOARD_SOC_CLASS),IMX5X)
LOCAL_SRC_FILES := media_codecs_imx5x_vpu.xml
else
LOCAL_SRC_FILES := media_codecs_vpu.xml
endif
else
LOCAL_SRC_FILES := media_codecs.xml
endif
LOCAL_MODULE := media_codecs.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)

#for media codec performance setting
include $(CLEAR_VARS)
LOCAL_SRC_FILES := media_codecs_performance.xml
LOCAL_MODULE := media_codecs_performance.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)