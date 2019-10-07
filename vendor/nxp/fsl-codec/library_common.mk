LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE := true
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64
LOCAL_SRC_FILES_64 := lib64/$(LOCAL_MODULE).so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/$(LOCAL_MODULE).so
else
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/$(LOCAL_MODULE).so
endif
LOCAL_POST_INSTALL_CMD := \
            if [ -f $(TARGET_OUT_VENDOR)/lib64/$(LOCAL_MODULE).so ]; then \
                rm -f $(TARGET_OUT_VENDOR)/lib64/$(LOCAL_MODULE).so; \
            fi; \
            if [ -f $(TARGET_OUT_VENDOR)/lib/$(LOCAL_MODULE).so ]; then \
                rm -f $(TARGET_OUT_VENDOR)/lib/$(LOCAL_MODULE).so; \
            fi
