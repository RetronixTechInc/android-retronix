LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libfslextractor

LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)

LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES := \
        frameworks/av/ \
        frameworks/av/include \
        frameworks/av/media/libstagefright/ \
        frameworks/av/media/libmedia/include/ \
        frameworks/av/media/libstagefright/foundation/include/ \
        frameworks/av/media/libmediaextractor/include/media/stagefright/ \
        frameworks/av/media/libmediaextractor/include/media/ \
        vendor/nxp/fsl-codec/ghdr/common \
        frameworks/native/headers/media_plugin/media/openmax/ \
        external/libvpx/libwebm/

LOCAL_SRC_FILES := FslExtractor.cpp FslInspector.cpp

ifeq ($(TARGET_CPU_ABI), arm64-v8a)
LOCAL_CFLAGS += -DARM64-V8A
endif

ifeq ($(TARGET_CPU_ABI), armeabi-v7a)
LOCAL_CFLAGS += -DARMEABI-V7A
endif

LOCAL_SHARED_LIBRARIES := \
        liblog \
        libstagefright_foundation \
        libcutils \
        libutils \
        libmediaextractor \
        libc \
        libbinder

LOCAL_POST_INSTALL_CMD := \
            if [ -f $(TARGET_OUT_VENDOR)/lib64/libfslextractor.so ]; then \
                ls -l $(TARGET_OUT_VENDOR)/lib64/libfslextractor.so; \
                rm -f $(TARGET_OUT_VENDOR)/lib64/libfslextractor.so; \
                sync; \
            fi; \
            if [ -f $(TARGET_OUT_VENDOR)/lib/libfslextractor.so ]; then \
                ls -l $(TARGET_OUT_VENDOR)/lib/libfslextractor.so; \
                rm -f $(TARGET_OUT_VENDOR)/lib/libfslextractor.so; \
                sync; \
            fi


include $(BUILD_SHARED_LIBRARY)

