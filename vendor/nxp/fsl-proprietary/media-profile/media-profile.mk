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

ifeq ($(BOARD_SOC_TYPE),IMX8MQ)
LOCAL_SRC_FILES :=  media_profiles_8mq.xml
endif

ifeq ($(BOARD_SOC_TYPE),IMX8Q)
LOCAL_SRC_FILES :=  media_profiles_720p.xml
endif

ifeq ($(BOARD_SOC_TYPE),IMX8MM)
LOCAL_SRC_FILES :=  media_profiles_8mm.xml
endif

else
ifeq ($(BOARD_SOC_TYPE),IMX8Q)
LOCAL_SRC_FILES :=  media_profiles_720p.xml
else
ifeq ($(PRODUCT_MODEL),SABREAUTO-MX6SX)
LOCAL_SRC_FILES := media_profiles_sxTVin.xml
else
LOCAL_SRC_FILES := media_profiles_qvga.xml
endif
endif
endif

LOCAL_MODULE := media_profiles_V1_0.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

# for Multimedia codec profile setting
include $(CLEAR_VARS)
LOCAL_SRC_FILES := fsl-restricted-codec/media_codecs_template.xml
HAVE_AC3 = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ac3_dec && echo yes)
ifeq ($(HAVE_AC3), yes)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ac3_dec/media_codecs_ac3.xml
endif
LOCAL_MODULE := media_codecs_ac3.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := fsl-restricted-codec/media_codecs_template.xml
HAVE_DDP = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ddp_dec && echo yes)
ifeq ($(HAVE_DDP), yes)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ddp_dec/media_codecs_ddp.xml
endif
LOCAL_MODULE := media_codecs_ddp.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := fsl-restricted-codec/media_codecs_template.xml
HAVE_MS = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec && echo yes)
ifeq ($(HAVE_MS), yes)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec/media_codecs_ms.xml
endif
LOCAL_MODULE := media_codecs_ms.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := fsl-restricted-codec/media_codecs_template.xml
HAVE_RA = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec && echo yes)
ifeq ($(HAVE_RA), yes)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec/media_codecs_ra.xml
endif
LOCAL_MODULE := media_codecs_ra.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

ifeq ($(BOARD_SOC_TYPE),IMX8Q)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := fsl-restricted-codec/media_codecs_template.xml
HAVE_DSP_AACP = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/imx_dsp_aacp_dec && echo yes)
ifeq ($(HAVE_DSP_AACP), yes)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/imx_dsp_aacp_dec/media_codecs_dsp_aacp.xml
endif
LOCAL_MODULE := media_codecs_dsp_aacp.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := fsl-restricted-codec/media_codecs_template.xml
HAVE_DSP = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/imx_dsp_codec && echo yes)
ifeq ($(HAVE_DSP), yes)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/imx_dsp_codec/media_codecs_dsp.xml
endif
LOCAL_MODULE := media_codecs_dsp.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

endif

include $(CLEAR_VARS)
LOCAL_SRC_FILES := fsl-restricted-codec/media_codecs_template.xml
HAVE_RV = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec && echo yes)
ifeq ($(HAVE_RV), yes)
ifeq ($(BOARD_SOC_TYPE),IMX8MQ)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec/media_codecs_rv.xml
else ifeq ($(BOARD_SOC_TYPE),IMX6DQ)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec/media_codecs_rv_adaptive.xml
endif
endif
LOCAL_MODULE := media_codecs_rv.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := fsl-restricted-codec/media_codecs_template.xml
HAVE_WMV9 = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec && echo yes)
ifeq ($(HAVE_WMV9), yes)
ifeq ($(BOARD_SOC_TYPE),IMX8MQ)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec/media_codecs_wmv9.xml
else ifeq ($(BOARD_SOC_TYPE),IMX8Q)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec/media_codecs_wmv9_v4l2.xml
else ifeq ($(BOARD_SOC_TYPE),IMX6DQ)
    LOCAL_SRC_FILES = ../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec/media_codecs_wmv9_adaptive.xml
endif
endif
LOCAL_MODULE := media_codecs_wmv9.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)


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
ifeq ($(BOARD_SOC_TYPE),IMX8MQ)
LOCAL_SRC_FILES := imx8mq/media_codecs.xml
endif
ifeq ($(BOARD_SOC_TYPE),IMX8Q)
LOCAL_SRC_FILES := imx8q/media_codecs.xml
endif
ifeq ($(BOARD_SOC_TYPE),IMX8MM)
LOCAL_SRC_FILES := imx8mm/media_codecs.xml
endif
LOCAL_MODULE := media_codecs.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)

#for media codec performance setting
include $(CLEAR_VARS)
ifeq ($(BOARD_HAVE_VPU), true)
LOCAL_SRC_FILES := media_codecs_performance.xml
else
LOCAL_SRC_FILES := media_codecs_performance_no_vpu.xml
endif
ifeq ($(BOARD_SOC_TYPE),IMX8MQ)
LOCAL_SRC_FILES := imx8mq/media_codecs_performance.xml
endif
ifeq ($(BOARD_SOC_TYPE),IMX8Q)
LOCAL_SRC_FILES := imx8q/media_codecs_performance.xml
endif
ifeq ($(BOARD_SOC_TYPE),IMX8MM)
LOCAL_SRC_FILES := imx8mm/media_codecs_performance.xml
endif
LOCAL_MODULE := media_codecs_performance.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := eng
LOCAL_VENDOR_MODULE := true
include $(BUILD_PREBUILT)
