LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(BUILD_MULTI_PREBUILT)

MAJOR_VERSION := $(shell echo $(PLATFORM_VERSION) | cut -d "." -f1)
ANDROID_VERSION_GE_O := $(shell if [ $(MAJOR_VERSION) -ge 8 ];then echo "true";fi)
ANDROID_VERSION_GE_P := $(shell if [ $(MAJOR_VERSION) -ge 9 ];then echo "true";fi)

ifeq ($(ANDROID_VERSION_GE_O), true)
    FSL_CODEC_OUT_PATH := $(TARGET_OUT_VENDOR)
else
    FSL_CODEC_OUT_PATH := $(TARGET_OUT)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := lib_nb_amr_dec_v2_arm9_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq ($(ANDROID_VERSION_GE_O), true)
LOCAL_VENDOR_MODULE := true
endif
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := ./lib64/lib_nb_amr_dec_arm_android.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := ./lib/lib_nb_amr_dec_v2_arm9_elinux.so
else
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_nb_amr_dec_v2_arm9_elinux.so
endif
ifeq ($(TARGET_ARCH),arm64)
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_nb_amr_dec_v2_arm9_elinux.so lib/lib_nb_amr_dec.so; \
                          ln -sf ./lib_nb_amr_dec_v2_arm9_elinux.so lib64/lib_nb_amr_dec.so; \
                          ln -sf ./lib_aac_dec_v2_arm12_elinux.so lib/lib_aac_dec.so; \
                          ln -sf ./lib_aac_dec_v2_arm12_elinux.so lib64/lib_aac_dec.so; \
                          ln -sf ./lib_mp3_dec_v2_arm12_elinux.so lib/lib_mp3_dec.so; \
                          ln -sf ./lib_mp3_dec_v2_arm12_elinux.so lib64/lib_mp3_dec.so;
else
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_nb_amr_dec_v2_arm9_elinux.so lib/lib_nb_amr_dec.so; \
                          ln -sf ./lib_aac_dec_v2_arm12_elinux.so lib/lib_aac_dec.so; \
                          ln -sf ./lib_mp3_dec_v2_arm12_elinux.so lib/lib_mp3_dec.so;
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_wb_amr_dec_arm9_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq ($(ANDROID_VERSION_GE_O), true)
LOCAL_VENDOR_MODULE := true
endif
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := lib64/lib_wb_amr_dec_arm_android.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/lib_wb_amr_dec_arm9_elinux.so
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_wb_amr_dec_arm9_elinux.so lib/lib_wb_amr_dec.so; \
                          ln -sf ./lib_wb_amr_dec_arm9_elinux.so lib64/lib_wb_amr_dec.so;
else
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_wb_amr_dec_arm9_elinux.so lib/lib_wb_amr_dec.so;
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_wb_amr_dec_arm9_elinux.so
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_flac_dec_v2_arm11_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq ($(ANDROID_VERSION_GE_O), true)
LOCAL_VENDOR_MODULE := true
endif
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := lib64/lib_flac_dec_arm_android.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/lib_flac_dec_v2_arm11_elinux.so
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_flac_dec_v2_arm11_elinux.so lib/lib_flac_dec.so; \
                          ln -sf ./lib_flac_dec_v2_arm11_elinux.so lib64/lib_flac_dec.so;
else
LOCAL_POST_INSTALL_CMD := cd $(FSL_CODEC_OUT_PATH); \
                          ln -sf ./lib_flac_dec_v2_arm11_elinux.so lib/lib_flac_dec.so;
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_flac_dec_v2_arm11_elinux.so
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_H263_dec_v2_arm11_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq ($(ANDROID_VERSION_GE_O), true)
LOCAL_VENDOR_MODULE := true
endif
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := 32
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/lib_H263_dec_v2_arm11_elinux.so
else
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_H263_dec_v2_arm11_elinux.so
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_peq_v2_arm11_elinux
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
ifeq ($(ANDROID_VERSION_GE_O), true)
LOCAL_VENDOR_MODULE := true
endif
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := 32
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/lib_peq_v2_arm11_elinux.so
else
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/lib_peq_v2_arm11_elinux.so
endif
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := libvpu-malone
LOCAL_MODULE_SUFFIX := .so
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_VENDOR_MODULE := true
ifeq ($(TARGET_ARCH),arm64)
LOCAL_MULTILIB := both
LOCAL_MODULE_PATH_64 := $(FSL_CODEC_OUT_PATH)/lib64/
LOCAL_SRC_FILES_64 := lib64/libvpu-malone.so
LOCAL_MODULE_PATH_32 := $(FSL_CODEC_OUT_PATH)/lib/
LOCAL_SRC_FILES_32 := lib/libvpu-malone.so
else
LOCAL_MODULE_PATH := $(FSL_CODEC_OUT_PATH)/lib
LOCAL_SRC_FILES := lib/libvpu-malone.so
endif
include $(BUILD_PREBUILT)

# When version >= pi9, install parsers by Android.mk because ExtractorPkg needs it when compiling.
ifeq ($(ANDROID_VERSION_GE_P), true)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_avi_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_flv_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_mkv_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_mp4_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_mpg2_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_ogg_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_amr_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_aac_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_mp3_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_wav_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_flac_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_ape_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := lib_dsf_parser_arm11_elinux.3.0
include $(LOCAL_PATH)/library_common.mk
include $(BUILD_PREBUILT)

endif # GE_P

