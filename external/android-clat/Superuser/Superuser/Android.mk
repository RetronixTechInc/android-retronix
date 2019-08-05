LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := \
	libj:libs/org.apache.http.legacy.jar

include $(BUILD_MULTI_PREBUILT)

include $(CLEAR_VARS)

LOCAL_PACKAGE_NAME := Superuser
LOCAL_CERTIFICATE := platform
LOCAL_PRIVILEGED_MODULE := true
LOCAL_SRC_FILES := $(call all-java-files-under,src) $(call all-java-files-under,../../Widgets/Widgets/src)

LOCAL_STATIC_JAVA_LIBRARIES := android-support-v4 libj
LOCAL_AAPT_INCLUDE_ALL_RESOURCES := true
LOCAL_AAPT_FLAGS := --extra-packages com.koushikdutta.widgets -S $(LOCAL_PATH)/../../Widgets/Widgets/res --auto-add-overlay --rename-manifest-package $(SUPERUSER_PACKAGE)

include $(BUILD_PACKAGE)

