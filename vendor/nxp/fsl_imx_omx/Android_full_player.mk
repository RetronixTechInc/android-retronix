
LOCAL_PATH := $(call my-dir)

# LOCAL_PATH will be changed by each Android.mk under this. So save it firstly
FSL_OMX_PATH := $(LOCAL_PATH)

include $(CLEAR_VARS)
FSL_OMX_CFLAGS += -DANDROID_BUILD -D_POSIX_SOURCE -UDOMX_MEM_CHECK -Wno-unused-parameter -Werror

FSL_OMX_LDFLAGS := -Wl,--fatal-warnings

FSL_OMX_INCLUDES := \
	$(LOCAL_PATH)/OSAL/ghdr \
	$(LOCAL_PATH)/utils \
	$(LOCAL_PATH)/utils/colorconvert/include \
	$(LOCAL_PATH)/utils/audio_frame_parser \
	$(LOCAL_PATH)/OpenMAXIL/ghdr \
	$(LOCAL_PATH)/OpenMAXIL/src/core_mgr \
	$(LOCAL_PATH)/OpenMAXIL/src/core \
	$(LOCAL_PATH)/OpenMAXIL/src/content_pipe \
	$(LOCAL_PATH)/OpenMAXIL/src/resource_mgr \
	$(LOCAL_PATH)/OpenMAXIL/src/graph_manager_interface \
	$(LOCAL_PATH)/OpenMAXIL/src/client \
	$(LOCAL_PATH)/OpenMAXIL/src/component/common \
	$(IMX_LIB_PATH)/imx-lib/ipu \
	$(IMX_VPU_CNM_PATH)/imx-vpu-cnm \
	frameworks/base/include \
	frameworks/base/include/media \
	frameworks/base/include/utils \
	frameworks/base/include/surfaceflinger \
	frameworks/base/media/libmediaplayerservice \
	frameworks/av/include \
	frameworks/av/include/media \
	frameworks/av/include/utils \
	frameworks/av/include/surfaceflinger \
	frameworks/av/media \
	frameworks/av/media/libmediaplayerservice \
	frameworks/native/include \
	frameworks/native/include/media/hardware \
	$(IMX_PATH)/imx/include/ \
	system/core/include


# Check Android Version
ifeq ($(findstring x2.2,x$(PLATFORM_VERSION)), x2.2)
    FSL_OMX_CFLAGS += -DOMX_STEREO_OUTPUT
endif

ifeq ($(findstring x2.3,x$(PLATFORM_VERSION)), x2.3)
    FSL_OMX_CFLAGS += -DMEDIA_SCAN_2_3_3_API -DOMX_STEREO_OUTPUT
    FSL_OMX_INCLUDES += frameworks/base/media/libstagefright/include \
			device/fsl/proprietary/codec/ghdr \
			device/fsl/proprietary/codec/ghdr/common \
			device/fsl/proprietary/codec/ghdr/wma10
else
    FSL_OMX_INCLUDES += $(FSL_CODEC_PATH)/fsl-codec/ghdr \
			$(FSL_CODEC_PATH)/fsl-codec/ghdr/common \
			$(FSL_CODEC_PATH)/fsl-codec/ghdr/wma10
endif

ifeq ($(findstring x3.,x$(PLATFORM_VERSION)), x3.)
    FSL_OMX_CFLAGS += -DMEDIA_SCAN_2_3_3_API -DHWC_RENDER -DOMX_STEREO_OUTPUT
    FSL_OMX_INCLUDES += frameworks/base/media/libstagefright/include \
	frameworks/base/include/ui/egl
    ifeq ($(TARGET_BOARD_PLATFORM), imx5x)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/mx5x/libgralloc
    endif
    ifeq ($(TARGET_BOARD_PLATFORM), imx6)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/mx6/libgralloc
    endif
endif

ifeq ($(findstring x4.,x$(PLATFORM_VERSION)), x4.)
    FSL_OMX_CFLAGS += -DMEDIA_SCAN_2_3_3_API
    ifeq ($(findstring x4.0,x$(PLATFORM_VERSION)), x4.0)
	FSL_OMX_CFLAGS += -DOMX_STEREO_OUTPUT
    else
	FSL_OMX_CFLAGS += -UDOMX_STEREO_OUTPUT
    endif

    FSL_OMX_INCLUDES += frameworks/base/media/libstagefright/include \
	frameworks/base/include/ui/egl
    FSL_OMX_INCLUDES += frameworks/av/include/media/stagefright/ \
	frameworks/av/include/ui/egl
    ifeq ($(TARGET_BOARD_PLATFORM), imx5x)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/mx5x/libgralloc
        FSL_OMX_CFLAGS += -DMX5X
    endif
    ifeq ($(TARGET_BOARD_PLATFORM), imx6)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v2
		FSL_OMX_CFLAGS += -DMX6X
    endif
endif
ifeq ($(findstring x5.,x$(PLATFORM_VERSION)), x5.)
    FSL_OMX_CFLAGS += -DMEDIA_SCAN_2_3_3_API
    FSL_OMX_CFLAGS += -UDOMX_STEREO_OUTPUT

    FSL_OMX_INCLUDES += frameworks/base/media/libstagefright/include \
	frameworks/base/include/ui/egl
    FSL_OMX_INCLUDES += frameworks/av/include/media/stagefright/ \
	frameworks/av/include/ui/egl
    ifeq ($(TARGET_BOARD_PLATFORM), imx5x)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/mx5x/libgralloc
        FSL_OMX_CFLAGS += -DMX5X
    endif
    ifeq ($(TARGET_BOARD_PLATFORM), imx6)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v2
		FSL_OMX_CFLAGS += -DMX6X
    endif
    ifeq ($(TARGET_BOARD_PLATFORM), imx7)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v2
		FSL_OMX_CFLAGS += -DMX7X
    endif
endif
ifeq ($(findstring x6.,x$(PLATFORM_VERSION)), x6.)
    FSL_OMX_CFLAGS += -DMEDIA_SCAN_2_3_3_API
    FSL_OMX_CFLAGS += -UDOMX_STEREO_OUTPUT

    FSL_OMX_INCLUDES += frameworks/base/media/libstagefright/include \
	frameworks/base/include/ui/egl
    FSL_OMX_INCLUDES += frameworks/av/include/media/stagefright/ \
	frameworks/av/include/ui/egl
    ifeq ($(TARGET_BOARD_PLATFORM), imx5x)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/mx5x/libgralloc
        FSL_OMX_CFLAGS += -DMX5X
    endif
    ifeq ($(TARGET_BOARD_PLATFORM), imx6)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v2
		FSL_OMX_CFLAGS += -DMX6X
    endif
    ifeq ($(TARGET_BOARD_PLATFORM), imx7)
        FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v2
		FSL_OMX_CFLAGS += -DMX7X
    endif
endif

include $(FSL_OMX_PATH)/utils/id3_parser/Android.mk

include $(FSL_OMX_PATH)/OSAL/linux/Android.mk
include $(FSL_OMX_PATH)/utils/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/resource_mgr/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/core_mgr/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/core/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/content_pipe/local_file_pipe/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/content_pipe/shared_fd_pipe/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/content_pipe/async_write_pipe/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/common/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/clock/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/fsl_parser/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/fsl_muxer/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/mp3_parser/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/flac_parser/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_parser/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/wav_parser/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/mp3_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/mp3_enc/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/amr_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/amr_enc/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/flac_dec/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_dec/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/vorbis_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/pcm_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/audio_processor/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/android_audio_render/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/android_audio_source/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/audio_fake_render/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/sorenson_dec/Android.mk
ifeq ($(findstring x2.3,x$(PLATFORM_VERSION)), x2.3)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/overlay_render/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/camera_source/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_enc/Android.mk
endif
ifeq ($(findstring x3.,x$(PLATFORM_VERSION)), x3.)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/camera_source/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_enc/Android.mk
endif
ifeq ($(findstring x4.,x$(PLATFORM_VERSION)), x4.)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/camera_source/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_enc/Android.mk
endif
ifeq ($(findstring x5.,x$(PLATFORM_VERSION)), x5.)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/camera_source/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_enc/Android.mk
endif
ifeq ($(findstring x6.,x$(PLATFORM_VERSION)), x6.)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/camera_source/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/surface_source/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_enc/Android.mk
endif

ifeq ($(BOARD_HAVE_VPU),true)
FSL_OMX_INCLUDES += $(IMX_LIB_PATH)/imx-lib/ipu \
                    $(IMX_VPU_CNM_PATH)/imx-vpu-cnm

FSL_OMX_INCLUDES +=	$(IMX_VPU_HANTRO_PATH)/imx-vpu-hantro/decoder_sw/software/source/inc \
					$(IMX_VPU_HANTRO_PATH)/imx-vpu-hantro/openmax_il/source/decoder \
					$(IMX_VPU_HANTRO_PATH)/imx-vpu-hantro/openmax_il/source

include $(FSL_OMX_PATH)/OpenMAXIL/src/component/vpu_wrapper/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/vpu_dec_v2/Android.mk
ifneq ($(BOARD_VPU_TYPE), hantro)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/vpu_enc/Android.mk
endif
ifeq ($(BOARD_SOC_TYPE), IMX8MM)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/vpu_enc/Android.mk
endif
#include $(FSL_OMX_PATH)/OpenMAXIL/test/vpu_test/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/test/vpu_enc_test/Android.mk
endif
include $(FSL_OMX_PATH)/stagefright/Android.mk
ifeq ($(HAVE_FSL_IMX_IPU),true)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/ipulib_render/Android.mk
endif
include $(FSL_OMX_PATH)/OpenMAXIL/src/client/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/test/OMX_GraphManager/Android.mk
include $(FSL_OMX_PATH)/Android/Android_full_player.mk

include $(FSL_OMX_PATH)/OpenMAXIL/release/registry/Android.mk

#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/wma_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/wmv_dec/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/ac3_parser/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/ac3_dec/Android.mk

#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/libav_video_dec/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/libav_audio_dec/Android.mk

ifeq ($(findstring x3.,x$(PLATFORM_VERSION)), x3.)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/surface_render/Android.mk
endif

ifeq ($(findstring x4.,x$(PLATFORM_VERSION)), x4.)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/surface_render/Android.mk
endif
ifeq ($(findstring x5.,x$(PLATFORM_VERSION)), x5.)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/surface_render/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/soft_hevc_dec/Android.mk
endif
ifeq ($(findstring x6.,x$(PLATFORM_VERSION)), x6.)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/surface_render/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/soft_hevc_dec/Android.mk
endif
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/mp3_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/wma_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/ac3_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/iec937_convert/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/ec3_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/realaudio_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/mx8mq_fb_render/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/bsac_udec/Android.mk

include $(FSL_OMX_PATH)/CactusPlayer/Android.mk

