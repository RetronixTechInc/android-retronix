ifeq ($(TARGET_BOARD_PLATFORM),imx6)

LOCAL_PATH := $(call my-dir)

# Share library
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
	g2d_test.c

LOCAL_CFLAGS += -DBUILD_FOR_ANDROID -DIMX6Q

LOCAL_SHARED_LIBRARIES := libutils libc liblog libg2d

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include/ device/fsl-proprietary/include/

LOCAL_MODULE := g2d_test
LOCAL_LD_FLAGS += -nostartfiles
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := eng
include $(BUILD_EXECUTABLE)

endif
