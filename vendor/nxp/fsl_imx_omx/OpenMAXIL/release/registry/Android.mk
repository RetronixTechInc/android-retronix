ifeq ($(HAVE_FSL_IMX_CODEC),true)


LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := core_register
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := core_register
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := component_register

ifeq ($(BOARD_SOC_TYPE), IMX8Q)
LOCAL_SRC_FILES := component_register_mek_8q
endif
ifeq ($(BOARD_SOC_TYPE), IMX8MQ)
LOCAL_SRC_FILES := component_register_evk_8mq
endif
ifeq ($(BOARD_SOC_TYPE), IMX8MM)
LOCAL_SRC_FILES := component_register_evk_8mm
endif

LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)

HAVE_AC3 = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ac3_dec && echo yes)
ifeq ($(HAVE_AC3), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_ac3
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ac3_dec/component_register_ac3
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif

HAVE_DDP = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ddp_dec && echo yes)
ifeq ($(HAVE_DDP), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_ddp
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ddp_dec/component_register_ddp
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif

HAVE_MS = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec && echo yes)
ifeq ($(HAVE_MS), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_ms
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec/component_register_ms
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif

HAVE_RA = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec && echo yes)
ifeq ($(HAVE_RA), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_ra
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec/component_register_ra
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif

ifeq ($(BOARD_SOC_TYPE), $(filter $(BOARD_SOC_TYPE), IMX8MQ IMX6DQ))

HAVE_RV = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec && echo yes)
ifeq ($(HAVE_RV), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_rv
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_real_dec/component_register_rv
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif

HAVE_WMV9 = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec && echo yes)
ifeq ($(HAVE_WMV9), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_wmv9
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec/component_register_wmv9
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif
endif

ifeq ($(BOARD_SOC_TYPE), IMX8Q)

HAVE_DSP_AACP = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/imx_dsp_aacp_dec && echo yes)
ifeq ($(HAVE_DSP_AACP), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_dsp_aacp
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/imx_dsp_aacp_dec/component_register_dsp_aacp
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif

HAVE_DSP = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/imx_dsp_codec && echo yes)
ifeq ($(HAVE_DSP), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_dsp
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/imx_dsp_codec/component_register_dsp
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif

HAVE_WMV9 = $(shell test -d $(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec && echo yes)
ifeq ($(HAVE_WMV9), yes)
include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := component_register_wmv9
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := ../../../../../../$(FSL_RESTRICTED_CODEC_PATH)/fsl-restricted-codec/fsl_ms_codec/component_register_wmv9_v4l2
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)
endif

endif


include $(CLEAR_VARS)
LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)
LOCAL_MODULE := contentpipe_register
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := contentpipe_register
LOCAL_MODULE_TAGS := eng
include $(BUILD_PREBUILT)


endif
