
include $(CLEAR_VARS)
FSL_OMX_CFLAGS += -DANDROID_BUILD -D_POSIX_SOURCE -UDOMX_MEM_CHECK -Wno-unused-parameter -Werror

FSL_OMX_LDFLAGS := -Wl,--fatal-warnings

FSL_OMX_INCLUDES := \
	$(LOCAL_PATH)/OSAL/ghdr \
	$(LOCAL_PATH)/utils \
	$(LOCAL_PATH)/utils/audio_frame_parser \
	$(LOCAL_PATH)/OpenMAXIL/ghdr \
	$(LOCAL_PATH)/OpenMAXIL/src/core_mgr \
	$(LOCAL_PATH)/OpenMAXIL/src/core \
	$(LOCAL_PATH)/OpenMAXIL/src/content_pipe \
	$(LOCAL_PATH)/OpenMAXIL/src/component/common \
	frameworks/av/include/media \
	frameworks/native/include/media/hardware \
	$(IMX_PATH)/imx/include/ \
	$(FSL_CODEC_PATH)/fsl-codec/ghdr \
	$(FSL_CODEC_PATH)/fsl-codec/ghdr/common \
	$(IMX_VPU_CNM_PATH)/imx-vpu-cnm



FSL_OMX_CFLAGS += -UDOMX_STEREO_OUTPUT

ifeq ($(use_gralloc_v3), true)
    FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v3 \
                        $(IMX_PATH)/imx/display/display
endif
ifeq ($(TARGET_BOARD_PLATFORM), imx6)
	FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v2
	FSL_OMX_CFLAGS += -DMX6X
endif
ifeq ($(TARGET_BOARD_PLATFORM), imx7)
	FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v2
	FSL_OMX_CFLAGS += -DMX7X
endif
ifeq ($(TARGET_BOARD_PLATFORM), imx8)
    FSL_OMX_INCLUDES += $(IMX_PATH)/imx/display/gralloc_v2
    FSL_OMX_CFLAGS += -DMX8X
endif

include $(FSL_OMX_PATH)/utils/id3_parser/Android.mk
include $(FSL_OMX_PATH)/OSAL/linux/Android.mk
include $(FSL_OMX_PATH)/utils/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/resource_mgr/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/core_mgr/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/core/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/content_pipe/local_file_pipe/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/content_pipe/shared_fd_pipe/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/common/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/clock/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/fsl_parser/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/mp3_parser/Android.mk

include $(FSL_OMX_PATH)/OpenMAXIL/src/component/amr_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/flac_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/pcm_dec/Android.mk

ifneq ($(TARGET_BOARD_PLATFORM), imx8)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/audio_processor/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/android_audio_render/Android.mk
endif

include $(FSL_OMX_PATH)/OpenMAXIL/src/component/sorenson_dec/Android.mk

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
include $(FSL_OMX_PATH)/OpenMAXIL/test/vpu_test/Android.mk
endif


include $(FSL_OMX_PATH)/stagefright/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/client/Android.mk
include $(FSL_OMX_PATH)/Android/Android_simple_player.mk
include $(FSL_OMX_PATH)/OpenMAXIL/release/registry/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/wmv_dec/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/libav_video_dec/Android.mk
#include $(FSL_OMX_PATH)/OpenMAXIL/src/component/libav_audio_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/surface_render/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/soft_hevc_dec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/mp3_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/aac_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/wma_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/ac3_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/ec3_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/realaudio_udec/Android.mk
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/bsac_udec/Android.mk
include $(FSL_OMX_PATH)/CactusPlayer/Android.mk

ifeq ($(HAVE_FSL_IMX_IPU),true)
include $(FSL_OMX_PATH)/OpenMAXIL/src/component/ipulib_render/Android.mk
endif

include $(FSL_OMX_PATH)/OpenMAXIL/src/component/mx8mq_fb_render/Android.mk

