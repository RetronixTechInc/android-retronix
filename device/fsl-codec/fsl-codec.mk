LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := \
	lib/libfsl_jpeg_enc_arm11_elinux.so \
	lib/lib_g.711_dec_arm11_elinux.so \
	lib/lib_g.711_enc_arm11_elinux.so \
	lib/lib_g.723.1_dec_arm11_elinux.so \
	lib/lib_g.723.1_enc_arm11_elinux.so \
	lib/lib_g.726_dec_arm11_elinux.so \
	lib/lib_g.726_enc_arm11_elinux.so \
	lib/lib_g.729ab_dec_arm11_elinux.so \
	lib/lib_g.729ab_enc_arm11_elinux.so \
\
	lib/lib_aac_dec_v2_arm12_elinux.so \
	lib/lib_flac_dec_v2_arm11_elinux.so \
	lib/lib_H263_dec_v2_arm11_elinux.so \
	lib/lib_mp3_dec_v2_arm12_elinux.so \
	lib/lib_mp3_enc_v2_arm12_elinux.so \
	lib/lib_nb_amr_dec_v2_arm9_elinux.so \
	lib/lib_nb_amr_enc_v2_arm11_elinux.so \
	lib/lib_oggvorbis_dec_v2_arm11_elinux.so \
	lib/lib_peq_v2_arm11_elinux.so \
	lib/lib_wb_amr_dec_arm9_elinux.so \
	lib/lib_wb_amr_enc_arm11_elinux.so \
\
	lib/lib_avi_parser_arm11_elinux.3.0.so \
	lib/lib_aac_parser_arm11_elinux.so \
	lib/lib_flac_parser_arm11_elinux.so \
	lib/lib_flv_parser_arm11_elinux.3.0.so \
	lib/lib_mkv_parser_arm11_elinux.3.0.so \
	lib/lib_mp3_parser_v2_arm11_elinux.so \
	lib/lib_mp4_muxer_arm11_elinux.so \
	lib/lib_mp4_parser_arm11_elinux.3.0.so \
	lib/lib_mpg2_parser_arm11_elinux.3.0.so \
	lib/lib_ogg_parser_arm11_elinux.3.0.so \
	lib/lib_wav_parser_arm11_elinux.so \
    lib/lib_amr_parser_arm11_elinux.3.0.so \
	lib/lib_aac_parser_arm11_elinux.3.0.so \
	lib/lib_aacd_wrap_arm12_elinux_android.so \
	lib/lib_mp3d_wrap_arm12_elinux_android.so \
	lib/lib_vorbisd_wrap_arm11_elinux_android.so \
	lib/lib_mp3_parser_arm11_elinux.3.0.so \
	lib/lib_wav_parser_arm11_elinux.3.0.so \
	lib/lib_flac_parser_arm11_elinux.3.0.so \
    lib/lib_ape_parser_arm11_elinux.3.0.so

            	
LOCAL_MODULE_TAGS := eng
include $(BUILD_MULTI_PREBUILT)

