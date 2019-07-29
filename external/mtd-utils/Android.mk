# Copyright 2009 The Android Open Source Project
#

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(CLEAR_VARS)

LOCAL_MULTILIB = 32
LOCAL_CFLAGS += -O2 -Wall
LOCAL_LDLIBS += -lz -lm 
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_STATIC_LIBRARIES := libselinux 
LOCAL_SRC_FILES := \
       ubi-utils/src/libubi.c \
       mkfs.ubifs/mkfs.ubifs.c \
       mkfs.ubifs/crc16.c \
       mkfs.ubifs/crc32.c \
       mkfs.ubifs/lpt.c \
       mkfs.ubifs/compr.c \
       mkfs.ubifs/devtable.c \
       mkfs.ubifs/hashtable/hashtable.c \
       mkfs.ubifs/hashtable/hashtable_itr.c \
       lzo-2.03/src/lzo1x_9x.c \
       uuid/gen_uuid.c \
       uuid/unpack.c \
       uuid/pack.c \
       uuid/unparse.c



LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/include \
        $(LOCAL_PATH)/ubi-utils/include \
        $(LOCAL_PATH)/ubi-utils/src \
        $(LOCAL_PATH)/lzo-2.03/include \
        $(LOCAL_PATH)/uuid/  \
	external/libselinux/include 

LOCAL_MODULE := mkfs_ubifs

include $(BUILD_HOST_EXECUTABLE)

#==================
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        ubi-utils/src/ubinize.c \
        ubi-utils/src/common.c \
        ubi-utils/src/crc32.c \
        ubi-utils/src/libiniparser.c \
        ubi-utils/src/dictionary.c \
        ubi-utils/src/libubigen.c

LOCAL_C_INCLUDES+= \
        $(LOCAL_PATH)/include \
        $(LOCAL_PATH)/ubi-utils/include \
        $(LOCAL_PATH)/ubi-utils/src

LOCAL_MODULE:=ubinize

LOCAL_CFLAGS+=-O3

include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ubi-utils/src/libubi.c \
	ubi-utils/src/libmtd_legacy.c \
	ubi-utils/src/libmtd.c \
	ubi-utils/src/libscan.c \
	ubi-utils/src/libubigen.c \
	ubi-utils/src/crc32.c \
	ubi-utils/src/common.c

LOCAL_CFLAGS = -O2 -Wall
LOCAL_CFLAGS += -Wall -Wextra -Wwrite-strings -Wno-sign-compare -D_FILE_OFFSET_BITS=64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/src

LOCAL_MODULE := libubi
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

# libubi static lib.
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ubi-utils/src/libubi.c \
	ubi-utils/src/libmtd_legacy.c \
	ubi-utils/src/libmtd.c \
	ubi-utils/src/libscan.c \
	ubi-utils/src/libubigen.c \
	ubi-utils/src/crc32.c \
	ubi-utils/src/common.c

LOCAL_CFLAGS = -O2 -Wall
LOCAL_CFLAGS += -Wall -Wextra -Wwrite-strings -Wno-sign-compare -D_FILE_OFFSET_BITS=64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/src

LOCAL_MODULE := libubi
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)

# ubinfo util
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ubi-utils/src/ubinfo.c

LOCAL_CFLAGS = -O2 -Wall
LOCAL_CFLAGS += -Wall -Wextra -Wwrite-strings -Wno-sign-compare -D_FILE_OFFSET_BITS=64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/src

LOCAL_SHARED_LIBRARIES := libubi

LOCAL_MODULE := ubinfo
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

# ubiformat util
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ubi-utils/src/ubiformat.c

LOCAL_CFLAGS = -O2 -Wall
LOCAL_CFLAGS += -Wall -Wextra -Wwrite-strings -Wno-sign-compare -D_FILE_OFFSET_BITS=64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/src

LOCAL_SHARED_LIBRARIES := libubi

LOCAL_MODULE := ubiformat
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

# ubimkvol util
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ubi-utils/src/ubimkvol.c

LOCAL_CFLAGS = -O2 -Wall
LOCAL_CFLAGS += -Wall -Wextra -Wwrite-strings -Wno-sign-compare -D_FILE_OFFSET_BITS=64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/src

LOCAL_SHARED_LIBRARIES := libubi

LOCAL_MODULE := ubimkvol
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

# ubiattach util
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ubi-utils/src/ubiattach.c

LOCAL_CFLAGS = -O2 -Wall
LOCAL_CFLAGS += -Wall -Wextra -Wwrite-strings -Wno-sign-compare -D_FILE_OFFSET_BITS=64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/src

LOCAL_SHARED_LIBRARIES := libubi

LOCAL_MODULE := ubiattach
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

# ubidetach util
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ubi-utils/src/ubidetach.c

LOCAL_CFLAGS = -O2 -Wall
LOCAL_CFLAGS += -Wall -Wextra -Wwrite-strings -Wno-sign-compare -D_FILE_OFFSET_BITS=64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/src

LOCAL_SHARED_LIBRARIES := libubi

LOCAL_MODULE := ubidetach
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

# ubiupdatevol util
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := ubi-utils/src/ubiupdatevol.c

LOCAL_CFLAGS = -O2 -Wall
LOCAL_CFLAGS += -Wall -Wextra -Wwrite-strings -Wno-sign-compare -D_FILE_OFFSET_BITS=64

LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ubi-utils/src

LOCAL_STATIC_LIBRARIES := libubi libc

LOCAL_MODULE := ubiupdatevol
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_SBIN)
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

