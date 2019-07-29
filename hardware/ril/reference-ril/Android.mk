# Copyright 2006 The Android Open Source Project

# XXX using libutils for simulator build only...
#
ifneq ($(PREBUILT_3G_MODEM_RIL),true)
ifneq ($(BOARD_NOT_HAVE_MODEM),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    reference-ril.c \
    atchannel.c \
    misc.c \
    fcp_parser.c \
    at_tok.c

LOCAL_SHARED_LIBRARIES := \
    liblog libcutils libutils libril librilutils libruntime-ril-port

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE

LOCAL_C_INCLUDES :=

ifeq ($(BOARD_MODEM_HAVE_DATA_DEVICE),true)
  LOCAL_CFLAGS += -DHAVE_DATA_DEVICE
endif

ifeq ($(TARGET_DEVICE),sooner)
  LOCAL_CFLAGS += -DUSE_TI_COMMANDS
endif

ifeq ($(TARGET_DEVICE),surf)
  LOCAL_CFLAGS += -DPOLL_CALL_STATE -DUSE_QMI
endif

ifeq ($(TARGET_DEVICE),dream)
  LOCAL_CFLAGS += -DPOLL_CALL_STATE -DUSE_QMI
endif

ifeq (foo,foo)
  #build shared library
  LOCAL_SHARED_LIBRARIES += \
      libcutils libutils
  LOCAL_CFLAGS += -DRIL_SHLIB
  LOCAL_MODULE:= libreference-ril
  include $(BUILD_SHARED_LIBRARY)
else
  #build executable
  LOCAL_SHARED_LIBRARIES += \
      libril
  LOCAL_MODULE:= reference-ril
  include $(BUILD_EXECUTABLE)
endif

endif
endif
