ifeq ($(HAVE_FSL_IMX_CODEC),false)
ifeq ($(BOARD_HAVE_VPU),true)

LOCAL_PATH := $(call my-dir)

# LOCAL_PATH will be changed by each Android.mk under this. So save it firstly
FSL_OMX_PATH := $(LOCAL_PATH)

include $(CLEAR_VARS)
FSL_OMX_CFLAGS := -DANDROID_BUILD -D_POSIX_SOURCE -UDOMX_MEM_CHECK -Wno-unused-parameter -Werror

FSL_OMX_LDFLAGS := -Wl,--fatal-warnings

FSL_OMX_INCLUDES := \
	$(LOCAL_PATH)/OSAL/ghdr \
	$(LOCAL_PATH)/utils \
	$(LOCAL_PATH)/OpenMAXIL/ghdr \
	$(LOCAL_PATH)/OpenMAXIL/src/component/common \
	$(LOCAL_PATH)/../linux-lib/vpu \
	$(LOCAL_PATH)/../../frameworks/native/include/media/hardware \
	$(LOCAL_PATH)/../../device/fsl-codec/ghdr \
	$(LOCAL_PATH)/../../device/fsl-codec/ghdr/common

ANDROID_VERSION_LIST = -DFROYO=220 -DGINGER_BREAD=230 -DHONEY_COMB=300 \
		       -DICS=400 -DJELLY_BEAN_42=420 -DJELLY_BEAN_43=430 -DKITKAT_44=440 -DLOLLIPOP_50=500 \
			   -DMARSH_MALLOW_600=600

ifeq ($(findstring x2.2,x$(PLATFORM_VERSION)), x2.2)
    ANDROID_VERSION_MACRO = 220
endif
ifeq ($(findstring x2.3,x$(PLATFORM_VERSION)), x2.3)
    ANDROID_VERSION_MACRO = 230
endif
ifeq ($(findstring x3.,x$(PLATFORM_VERSION)), x3.)
    ANDROID_VERSION_MACRO = 300
endif
ifeq ($(findstring x4.0,x$(PLATFORM_VERSION)), x4.0)
    ANDROID_VERSION_MACRO = 400
endif
ifeq ($(findstring x4.2,x$(PLATFORM_VERSION)), x4.2)
    ANDROID_VERSION_MACRO = 420
endif
ifeq ($(findstring x4.3,x$(PLATFORM_VERSION)), x4.3)
    ANDROID_VERSION_MACRO = 430
endif
ifeq ($(findstring x4.4,x$(PLATFORM_VERSION)), x4.4)
    ANDROID_VERSION_MACRO = 440
endif
ifeq ($(findstring x5.0,x$(PLATFORM_VERSION)), x5.0)
    ANDROID_VERSION_MACRO = 500
endif
ifeq ($(findstring x5.1,x$(PLATFORM_VERSION)), x5.1)
    ANDROID_VERSION_MACRO = 510
endif
ifeq ($(findstring x6.0,x$(PLATFORM_VERSION)), x6.0)
    ANDROID_VERSION_MACRO = 600
endif



FSL_OMX_CFLAGS += $(ANDROID_VERSION_LIST) -DANDROID_VERSION=$(ANDROID_VERSION_MACRO)

ifeq ($(TARGET_BOARD_PLATFORM), imx5x)
    FSL_OMX_INCLUDES += $(LOCAL_PATH)/../../hardware/imx/mx5x/libgralloc
    FSL_OMX_CFLAGS += -DMX5X
endif
ifeq ($(TARGET_BOARD_PLATFORM), imx6)
    FSL_OMX_INCLUDES += $(LOCAL_PATH)/../../hardware/imx/mx6/libgralloc_wrapper
    FSL_OMX_CFLAGS += -DMX6X
endif

include $(FSL_OMX_PATH)/utils/id3_parser/Android.mk

include $(FSL_OMX_PATH)/OSAL/linux/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/common/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/resource_mgr/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/core/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/release/registry/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/vpu_wrapper/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/vpu_dec_v2/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/vpu_enc/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/test/vpu_test/Android.mk
include $(FSL_OMX_PATH)/stagefright/Android.mk

include $(FSL_OMX_PATH)/utils/Android.mk

endif
endif
