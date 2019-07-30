# Copyright 2006 The Android Open Source Project

ifdef WIFI_DRIVER_MODULE_PATH
LOCAL_CFLAGS += -DWIFI_DRIVER_MODULE_PATH=\"$(WIFI_DRIVER_MODULE_PATH)\"
endif
ifdef WIFI_DRIVER_MODULE_ARG
LOCAL_CFLAGS += -DWIFI_DRIVER_MODULE_ARG=\"$(WIFI_DRIVER_MODULE_ARG)\"
endif
ifdef WIFI_DRIVER_P2P_MODULE_ARG
LOCAL_CFLAGS += -DWIFI_DRIVER_P2P_MODULE_ARG=\"$(WIFI_DRIVER_P2P_MODULE_ARG)\"
endif
ifdef WIFI_DRIVER_MODULE_NAME
LOCAL_CFLAGS += -DWIFI_DRIVER_MODULE_NAME=\"$(WIFI_DRIVER_MODULE_NAME)\"
endif

ifdef WIFI_SDIO_IF_DRIVER_MODULE_PATH
LOCAL_CFLAGS += -DWIFI_SDIO_IF_DRIVER_MODULE_PATH=\"$(WIFI_SDIO_IF_DRIVER_MODULE_PATH)\"
endif
ifdef WIFI_SDIO_IF_DRIVER_MODULE_ARG
LOCAL_CFLAGS += -DWIFI_SDIO_IF_DRIVER_MODULE_ARG=\"$(WIFI_SDIO_IF_DRIVER_MODULE_ARG)\"
endif
ifdef WIFI_SDIO_IF_DRIVER_MODULE_NAME
LOCAL_CFLAGS += -DWIFI_SDIO_IF_DRIVER_MODULE_NAME=\"$(WIFI_SDIO_IF_DRIVER_MODULE_NAME)\"
endif

ifdef WIFI_COMPAT_MODULE_PATH
LOCAL_CFLAGS += -DWIFI_COMPAT_MODULE_PATH=\"$(WIFI_COMPAT_MODULE_PATH)\"
endif
ifdef WIFI_COMPAT_MODULE_ARG
LOCAL_CFLAGS += -DWIFI_COMPAT_MODULE_ARG=\"$(WIFI_COMPAT_MODULE_ARG)\"
endif
ifdef WIFI_COMPAT_MODULE_NAME
LOCAL_CFLAGS += -DWIFI_COMPAT_MODULE_NAME=\"$(WIFI_COMPAT_MODULE_NAME)\"
endif

ifdef WIFI_FIRMWARE_LOADER
LOCAL_CFLAGS += -DWIFI_FIRMWARE_LOADER=\"$(WIFI_FIRMWARE_LOADER)\"
endif
ifdef WIFI_DRIVER_FW_PATH_STA
LOCAL_CFLAGS += -DWIFI_DRIVER_FW_PATH_STA=\"$(WIFI_DRIVER_FW_PATH_STA)\"
endif
ifdef WIFI_DRIVER_FW_PATH_AP
LOCAL_CFLAGS += -DWIFI_DRIVER_FW_PATH_AP=\"$(WIFI_DRIVER_FW_PATH_AP)\"
endif
ifdef WIFI_DRIVER_FW_PATH_P2P
LOCAL_CFLAGS += -DWIFI_DRIVER_FW_PATH_P2P=\"$(WIFI_DRIVER_FW_PATH_P2P)\"
endif
ifdef WIFI_DRIVER_FW_PATH_PARAM
LOCAL_CFLAGS += -DWIFI_DRIVER_FW_PATH_PARAM=\"$(WIFI_DRIVER_FW_PATH_PARAM)\"
endif

ifdef WIFI_DRIVER_STATE_CTRL_PARAM
LOCAL_CFLAGS += -DWIFI_DRIVER_STATE_CTRL_PARAM=\"$(WIFI_DRIVER_STATE_CTRL_PARAM)\"
endif
ifdef WIFI_DRIVER_STATE_ON
LOCAL_CFLAGS += -DWIFI_DRIVER_STATE_ON=\"$(WIFI_DRIVER_STATE_ON)\"
endif
ifdef WIFI_DRIVER_STATE_OFF
LOCAL_CFLAGS += -DWIFI_DRIVER_STATE_OFF=\"$(WIFI_DRIVER_STATE_OFF)\"
endif

ifeq ($(BOARD_WLAN_DEVICE),UNITE)
  LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../external/wpa_supplicant_ath/wpa_supplicant/src/common
ifeq ($(TARGET_PRODUCT),sabresd_7d)
  LOCAL_CFLAGS += -DSABRESD_7D
endif
  LOCAL_SRC_FILES += wifi/wifi_unite.c
else
  LOCAL_SRC_FILES += wifi/wifi.c
  LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../external/wpa_supplicant_8/src/common
endif

ifdef WPA_SUPPLICANT_VERSION
LOCAL_CFLAGS += -DLIBWPA_CLIENT_EXISTS
LOCAL_SHARED_LIBRARIES += libwpa_client
endif
LOCAL_SHARED_LIBRARIES += libnetutils