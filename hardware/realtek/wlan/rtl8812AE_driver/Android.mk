#==============================================================================
# Author(s): ="Retronix"
#==============================================================================

#~ ifeq ($(WIFI_DRIVER_MODULE_NAME), 8812ae)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

RTX_ANDROID_SRC_BASE:= $(LOCAL_PATH)

RTX_ANDROID_ROOT:= $(CURDIR)

RTX_LINUXPATH=$(RTX_ANDROID_ROOT)/kernel_imx
RTX_CROSS_COMPILE=${RTX_ANDROID_ROOT}/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-

mod_cleanup := $(RTX_ANDROID_ROOT)/$(RTX_ANDROID_SRC_BASE)/dummy

$(mod_cleanup) :
	$(MAKE) -C $(RTX_ANDROID_SRC_BASE) ARCH=arm CROSS_COMPILE=$(RTX_CROSS_COMPILE) KSRC=$(RTX_LINUXPATH) clean
	mkdir -p $(TARGET_OUT)/lib/modules/

rtl8812ae_module_file :=8812ae.ko
$(RTX_ANDROID_SRC_BASE)/$(rtl8812ae_module_file):$(mod_cleanup) $(TARGET_PREBUILT_KERNEL) $(ACP)
	$(MAKE) -C $(RTX_ANDROID_SRC_BASE) O=$(RTX_LINUXPATH) ARCH=arm CROSS_COMPILE=$(RTX_CROSS_COMPILE) KSRC=$(RTX_LINUXPATH)
	$(ACP) -fpt $(RTX_ANDROID_SRC_BASE)/8812ae.ko $(TARGET_OUT)/lib/modules/

include $(CLEAR_VARS)
LOCAL_MODULE := 8812ae.ko
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/lib/modules
LOCAL_SRC_FILES := $(rtl8812ae_module_file)
include $(BUILD_PREBUILT)

#~ include $(CLEAR_VARS)
#~ LOCAL_MODULE := check_wifi_mac.sh
#~ LOCAL_MODULE_TAGS := optional
#~ LOCAL_MODULE_CLASS := EXECUTABLES
#~ LOCAL_MODULE_PATH := $(TARGET_OUT)/etc
#~ LOCAL_SRC_FILES := check_wifi_mac.sh
#~ include $(BUILD_PREBUILT)

#~ endif
