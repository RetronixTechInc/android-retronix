LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_VENDOR_MODULE := $(FSL_OMX_TARGET_OUT_VENDOR)

LOCAL_SDK_VERSION := current

LOCAL_PACKAGE_NAME := ExtractorPkg

LOCAL_COMPRESSED_MODULE := true

LOCAL_JNI_SHARED_LIBRARIES := libfslextractor \
        lib_avi_parser_arm11_elinux.3.0 \
        lib_flv_parser_arm11_elinux.3.0 \
        lib_mkv_parser_arm11_elinux.3.0 \
        lib_mp4_parser_arm11_elinux.3.0 \
        lib_mpg2_parser_arm11_elinux.3.0 \
        lib_ogg_parser_arm11_elinux.3.0 \
        lib_amr_parser_arm11_elinux.3.0 \
        lib_aac_parser_arm11_elinux.3.0 \
        lib_mp3_parser_arm11_elinux.3.0 \
        lib_wav_parser_arm11_elinux.3.0 \
        lib_dsf_parser_arm11_elinux.3.0 \
        lib_flac_parser_arm11_elinux.3.0 \
        lib_ape_parser_arm11_elinux.3.0

-include vendor/nxp-private/fsl-restricted-codec/fsl_ms_codec/extractorpkg.mk
-include vendor/nxp-private/fsl-restricted-codec/fsl_real_dec/extractorpkg.mk

LOCAL_POST_INSTALL_CMD := \
        if [ -f $(TARGET_OUT_VENDOR)/app/$(LOCAL_PACKAGE_NAME)/$(LOCAL_PACKAGE_NAME).apk.gz ]; then \
          rm -f $(TARGET_OUT_VENDOR)/app/$(LOCAL_PACKAGE_NAME)/$(LOCAL_PACKAGE_NAME).apk; \
          gunzip $(TARGET_OUT_VENDOR)/app/$(LOCAL_PACKAGE_NAME)/$(LOCAL_PACKAGE_NAME).apk.gz; \
        fi

include $(BUILD_PACKAGE)
