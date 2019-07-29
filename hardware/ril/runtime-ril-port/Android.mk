# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    runtime_port.c

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils

LOCAL_CFLAGS :=

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libruntime-ril-port


include $(BUILD_SHARED_LIBRARY)
