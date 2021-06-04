ifeq ($(FSL_CODEC_PATH),)
	FSL_CODEC_PATH := device
endif
ifeq ($(IMX_VPU_CNM_PATH),)
	IMX_VPU_CNM_PATH := external
endif
ifeq ($(IMX_PATH),)
	IMX_PATH := hardware
endif
ifeq ($(IMX_LIB_PATH),)
	IMX_LIB_PATH := external
endif
ifeq ($(IMX_VPU_HANTRO_PATH),)
	IMX_VPU_HANTRO_PATH := external
endif
ifeq ($(FSL_PROPRIETARY_PATH),)
	FSL_PROPRIETARY_PATH := device
endif
ifeq ($(FSL_IMX_OMX_PATH),)
	FSL_IMX_OMX_PATH := external
endif

ifeq ($(PREBUILT_FSL_IMX_OMX),true)
	HAVE_FSL_IMX_CODEC := false
else
	HAVE_FSL_IMX_CODEC := true
endif

ifeq ($(HAVE_FSL_IMX_CODEC),true)

LOCAL_PATH := $(call my-dir)

# LOCAL_PATH will be changed by each Android.mk under this. So save it firstly
FSL_OMX_PATH := $(LOCAL_PATH)

include $(CLEAR_VARS)

ANDROID_VERSION_LIST = -DFROYO=220 -DGINGER_BREAD=230 -DHONEY_COMB=300 \
		       -DICS=400 -DJELLY_BEAN_42=420 -DJELLY_BEAN_43=430 -DKITKAT_44=440 -DLOLLIPOP_50=500 \
               -DMARSH_MALLOW_600=600 -DNOUGAT=700 -DANDROID_O=800 -DANDROID_P=900

ANDROID_VERSION_MACRO := 900

ifeq ($(findstring x2.2,x$(PLATFORM_VERSION)), x2.2)
	ANDROID_VERSION_MACRO := 220
endif
ifeq ($(findstring x2.3,x$(PLATFORM_VERSION)), x2.3)
	ANDROID_VERSION_MACRO := 230
endif
ifeq ($(findstring x3.,x$(PLATFORM_VERSION)), x3.)
	ANDROID_VERSION_MACRO := 300
endif
ifeq ($(findstring x4.0,x$(PLATFORM_VERSION)), x4.0)
	ANDROID_VERSION_MACRO := 400
endif
ifeq ($(findstring x4.2,x$(PLATFORM_VERSION)), x4.2)
	ANDROID_VERSION_MACRO := 420
endif
ifeq ($(findstring x4.3,x$(PLATFORM_VERSION)), x4.3)
	ANDROID_VERSION_MACRO := 430
endif
ifeq ($(findstring x4.4,x$(PLATFORM_VERSION)), x4.4)
	ANDROID_VERSION_MACRO := 440
endif
ifeq ($(findstring x5.0,x$(PLATFORM_VERSION)), x5.0)
	ANDROID_VERSION_MACRO := 500
endif
ifeq ($(findstring x5.1,x$(PLATFORM_VERSION)), x5.1)
	ANDROID_VERSION_MACRO := 510
endif
ifeq ($(findstring x6.0.0,x$(PLATFORM_VERSION)), x6.0.0)
	ANDROID_VERSION_MACRO := 600
endif
ifeq ($(findstring x6.0.1,x$(PLATFORM_VERSION)), x6.0.1)
	ANDROID_VERSION_MACRO := 601
endif
ifeq ($(findstring x7.0,x$(PLATFORM_VERSION)), x7.0)
	ANDROID_VERSION_MACRO := 700
endif
ifeq ($(findstring x7.1,x$(PLATFORM_VERSION)), x7.1)
	ANDROID_VERSION_MACRO := 710
endif
ifeq ($(findstring x8.0,x$(PLATFORM_VERSION)), x8.0)
	ANDROID_VERSION_MACRO := 800
endif
ifeq ($(findstring x8.1,x$(PLATFORM_VERSION)), x8.1)
	ANDROID_VERSION_MACRO := 810
endif
ifeq ($(findstring x9.0,x$(PLATFORM_VERSION)), x9.0)
	ANDROID_VERSION_MACRO := 900
endif

FSL_OMX_CFLAGS += $(ANDROID_VERSION_LIST) -DANDROID_VERSION=$(ANDROID_VERSION_MACRO)

FSL_OMX_TARGET_OUT_VENDOR := $(shell if [ $(ANDROID_VERSION_MACRO) -ge 800 ];then echo "true";fi)

FSL_BUILD_OMX_PLAYER := $(shell if [ $(ANDROID_VERSION_MACRO) -lt 800 ];then echo "true";fi)

# special case for pico, it's android 7.0 but has feature of android 800
ifneq ($(PREBUILT_FSL_IMX_OMX),"")
ifeq ($(ANDROID_VERSION_MACRO),700)
	FSL_BUILD_OMX_PLAYER := false
	FSL_OMX_CFLAGS += -DNO_FORCE_CONTIGUOUS
endif
endif

#macros for vpu
ifeq ($(BOARD_HAVE_VPU), true)

ifeq ($(BOARD_VPU_TYPE), chipsmedia)
FSL_OMX_CFLAGS += -DCHIPSMEDIA_VPU
endif

ifeq ($(BOARD_VPU_TYPE), hantro)
FSL_OMX_CFLAGS += -DHANTRO_VPU
endif

ifeq ($(BOARD_VPU_TYPE), malone)
FSL_OMX_CFLAGS += -DMALONE_VPU
endif

ifeq ($(BOARD_SOC_TYPE), IMX8MM)
FSL_OMX_CFLAGS += -DHANTRO_VPU_845S
endif
endif

use_gralloc_v3 := $(shell if [ $(ANDROID_VERSION_MACRO) -ge 800 ];then echo "true";fi)
#FSL_VPU_OMX_ONLY is set in device/nxp/[platform]/BoardConfig.mk
#set default value to false for android 7.1
FSL_VPU_OMX_ONLY ?= false


#$(warning FSL_BUILD_OMX_PLAYER=$(FSL_BUILD_OMX_PLAYER))
#$(warning use_gralloc_v3=$(use_gralloc_v3))
#$(warning FSL_VPU_OMX_ONLY=$(FSL_VPU_OMX_ONLY))
#$(warning TARGET_BOOTLOADER_BOARD_NAME=$(TARGET_BOOTLOADER_BOARD_NAME))

ifneq ($(FSL_BUILD_OMX_PLAYER), true)

ifeq ($(FSL_VPU_OMX_ONLY), true)
include $(FSL_OMX_PATH)/Android_vpu.mk
else
include $(FSL_OMX_PATH)/Android_decoders.mk
endif

else

player := $(shell if [ $(ANDROID_VERSION_MACRO) -gt 600 ];then echo "simple_player";fi)
ifeq ($(player), simple_player)
	include $(FSL_OMX_PATH)/Android_simple_player.mk
else
	include $(FSL_OMX_PATH)/Android_full_player.mk
endif

endif

FSLEXTRACTOR_IN_OMX := $(shell if [ $(ANDROID_VERSION_MACRO) -ge 900 ];then echo "true";fi)
ifeq ($(FSLEXTRACTOR_IN_OMX), true)
include $(FSL_OMX_PATH)/fslextractor/Android.mk
include $(FSL_OMX_PATH)/ExtractorPkg/Android.mk
endif

endif
