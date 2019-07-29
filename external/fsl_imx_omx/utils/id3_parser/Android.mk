ifeq ($(HAVE_FSL_IMX_CODEC),true)


LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ID3.cpp

LOCAL_CFLAGS += $(FSL_OMX_CFLAGS)
LOCAL_LDFLAGS += $(FSL_OMX_LDFLAGS)
LOCAL_C_INCLUDES += $(FSL_OMX_PATH)/../../external/icu/icu4c/source/common \
                    $(FSL_OMX_PATH)/../../external/icu/icu4c/source/i18n \
                    $(FSL_OMX_PATH)/../../frameworks/av/include/media \
                    $(FSL_OMX_PATH)/../../frameworks/av/media/libstagefright \
                    $(FSL_OMX_PATH)/utils \
                    $(FSL_OMX_PATH)/OSAL/ghdr

LOCAL_SHARED_LIBRARIES := libstagefright libstagefright_foundation libutils libc liblog libmedia lib_omx_osal_v2_arm11_elinux

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := eng
LOCAL_MODULE:= lib_id3_parser_arm11_elinux

include $(BUILD_SHARED_LIBRARY)

endif
