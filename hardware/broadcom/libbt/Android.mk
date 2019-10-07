LOCAL_PATH := $(call my-dir)

ifneq ($(BOARD_HAVE_BLUETOOTH_BCM),)
include $(CLEAR_VARS)

ifneq ($(BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR),)
  bdroid_C_INCLUDES := $(BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR)
  bdroid_CFLAGS += -DHAS_BDROID_BUILDCFG
else
  bdroid_C_INCLUDES :=
  bdroid_CFLAGS += -DHAS_NO_BDROID_BUILDCFG
endif

BDROID_DIR := $(TOP_DIR)system/bt

#ifeq ($(strip $(USE_BLUETOOTH_BCM4343)),true)
LOCAL_CFLAGS += -DUSE_BLUETOOTH_BCM4343
#endif

LOCAL_CFLAGS += \
        -Wall \
        -Werror \
        -Wno-switch \
        -Wno-unused-function \
        -Wno-unused-parameter \
        -Wno-unused-variable \

LOCAL_SRC_FILES := \
        src/bt_vendor_brcm.c \
        src/hardware.c \
        src/userial_vendor.c \
        src/upio.c \
        src/conf.c

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include \
        $(BDROID_DIR)/hci/include \
        $(BDROID_DIR)/include \
        $(BDROID_DIR)/device/include \
        $(BDROID_DIR)

LOCAL_C_INCLUDES += $(bdroid_C_INCLUDES)
LOCAL_CFLAGS += $(bdroid_CFLAGS)

LOCAL_HEADER_LIBRARIES := libutils_headers

ifneq ($(BOARD_HAVE_BLUETOOTH_BCM_A2DP_OFFLOAD),)
  LOCAL_STATIC_LIBRARIES := libbt-brcm_a2dp
endif

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        liblog

ifeq ($(BOARD_WLAN_DEVICE_UNITE),UNITE)
LOCAL_MODULE := libbt-vendor-unite-bcm
else
LOCAL_MODULE := libbt-vendor
endif
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_OWNER := broadcom
LOCAL_PROPRIETARY_MODULE := true

include $(LOCAL_PATH)/vnd_buildcfg.mk

include $(BUILD_SHARED_LIBRARY)
ifeq ($(TARGET_PRODUCT), sabresd_6sx)
	include $(LOCAL_PATH)/conf/fsl/sabresd_6sx/Android.mk
endif
ifeq ($(TARGET_PRODUCT), sabresd_6dq)
	include $(LOCAL_PATH)/conf/fsl/sabresd_6dq/Android.mk
endif
ifeq ($(TARGET_PRODUCT), sabresd_7d)
    include $(LOCAL_PATH)/conf/fsl/sabresd_7d/Android.mk
endif
ifeq ($(TARGET_PRODUCT), evk_7ulp)
    include $(LOCAL_PATH)/conf/fsl/evk_7ulp/Android.mk
endif
ifeq ($(TARGET_PRODUCT), evk_8mm)
    include $(LOCAL_PATH)/conf/fsl/evk_8mm/Android.mk
endif
ifeq ($(TARGET_PRODUCT), evk_8mn)
    include $(LOCAL_PATH)/conf/fsl/evk_8mn/Android.mk
endif
ifeq ($(TARGET_PRODUCT), mek_8q)
    include $(LOCAL_PATH)/conf/fsl/mek_8q/Android.mk
endif
ifeq ($(TARGET_PRODUCT), mek_8q_car)
    include $(LOCAL_PATH)/conf/fsl/mek_8q_car/Android.mk
endif
ifeq ($(TARGET_PRODUCT), mek_8q_car2)
    include $(LOCAL_PATH)/conf/fsl/mek_8q_car/Android.mk
endif
ifeq ($(TARGET_PRODUCT), sabresd_6dq_car)
    include $(LOCAL_PATH)/conf/fsl/sabresd_6dq/Android.mk
endif
ifeq ($(TARGET_PRODUCT), arm2_8q_car)
    include $(LOCAL_PATH)/conf/fsl/arm2_8q/Android.mk
endif
ifeq ($(TARGET_PRODUCT), evk_8mq)
    include $(LOCAL_PATH)/conf/fsl/evk_8mq/Android.mk
endif
ifeq ($(TARGET_PRODUCT), full_maguro)
    include $(LOCAL_PATH)/conf/samsung/maguro/Android.mk
endif
ifeq ($(TARGET_PRODUCT), full_crespo)
    include $(LOCAL_PATH)/conf/samsung/crespo/Android.mk
endif
ifeq ($(TARGET_PRODUCT), full_crespo4g)
    include $(LOCAL_PATH)/conf/samsung/crespo4g/Android.mk
endif
ifeq ($(TARGET_PRODUCT), full_wingray)
    include $(LOCAL_PATH)/conf/moto/wingray/Android.mk
endif

endif # BOARD_HAVE_BLUETOOTH_BCM
