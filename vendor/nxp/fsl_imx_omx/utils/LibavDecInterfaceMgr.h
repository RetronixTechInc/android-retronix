/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */

#ifndef _LIBAV_DEC_INTERFACE_MGR_H_
#define _LIBAV_DEC_INTERFACE_MGR_H_

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
}

#include "fsl_unia.h"
#include "ShareLibarayMgr.h"

/* libavcodec APIs*/
typedef void (*avcodec_register_all_t)(void);

typedef AVCodec* (*avcodec_find_decoder_t)(enum AVCodecID id);

typedef AVCodecContext* (*avcodec_alloc_context3_t)(const AVCodec *codec);

typedef void (*avcodec_free_context_t)(AVCodecContext **avctx);

typedef int (*avcodec_open2_t)(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);

typedef int (*avcodec_close_t)(AVCodecContext *avctx);

typedef void (*av_init_packet_t)(AVPacket *pkt);

typedef int (*avcodec_send_packet_t)(AVCodecContext *avctx, const AVPacket *avpkt);

typedef int (*avcodec_receive_frame_t)(AVCodecContext *avctx, AVFrame *frame);

typedef void (*avcodec_flush_buffers_t)(AVCodecContext *avctx);


/* libavutil APIs */
typedef AVFrame* (*av_frame_alloc_t)(void);

typedef void (*av_frame_free_t)(AVFrame **frame);

typedef int (*av_samples_get_buffer_size_t)(int *linesize, int nb_channels, int nb_samples, enum AVSampleFormat sample_fmt, int align);

typedef int (*av_get_bytes_per_sample_t)(enum AVSampleFormat sample_fmt);

typedef int (*av_opt_set_int_t)(void *obj, const char *name, int64_t     val, int search_flags);

typedef void (*av_free_t)(void *ptr);

typedef void (*av_log_set_level_t)(int level);

typedef void (*av_log_set_callback_t)(void (*callback)(void*, int, const char*, va_list));




/* libswresample APIs */
typedef struct SwrContext* (*swr_alloc_t)(void);

typedef int (*swr_init_t)(struct SwrContext *s);

typedef int (*swr_convert_t)(struct SwrContext *s, uint8_t **out, int out_count, const uint8_t **in , int in_count);

typedef void (*swr_close_t)(struct SwrContext *s);

typedef void (*swr_free_t)(struct SwrContext **s);


typedef struct {

    avcodec_register_all_t         AvcodecRegisterAll;
    avcodec_find_decoder_t         AvcodecFindDecoder;
    avcodec_alloc_context3_t       AvcodecAllocContext3;
    avcodec_free_context_t         AvcodecFreeContext;
    avcodec_open2_t                AvcodecOpen2;
    avcodec_close_t                AvcodecClose;
    av_init_packet_t               AvInitPacket;
    avcodec_send_packet_t          AvcodecSendPacket;
    avcodec_receive_frame_t        AvcodecReceiveFrame;
    avcodec_flush_buffers_t        AvcodecFlushBuffers;

    av_frame_alloc_t               AvFrameAlloc;
    av_frame_free_t                AvFrameFree;
    av_samples_get_buffer_size_t   AvSamplesGetBufferSize;
    av_get_bytes_per_sample_t      AvGetBytesPerSample;
    av_opt_set_int_t               AvOptSetInt;
    av_free_t                      AvFree;
    av_log_set_level_t             AvLogSetLevel;
    av_log_set_callback_t          AvLogSetCallback;

    swr_alloc_t                    SwrAlloc;
    swr_init_t                     SwrInit;
    swr_convert_t                  SwrConvert;
    swr_close_t                    SwrClose;
    swr_free_t                     SwrFree;
} LibavDecInterface;



class LibavDecInterfaceMgr {
public:
    LibavDecInterfaceMgr();
    fsl_osal_s32 CreateLibavDecInterfaces(LibavDecInterface* ILibavDec);
    fsl_osal_s32 DeleteLibavDecInterfaces(LibavDecInterface* ILibavDec);

private:
    ShareLibarayMgr *libMgr;
    fsl_osal_ptr hLibavcodec;
    fsl_osal_ptr hLibavutil;
    fsl_osal_ptr hLibswresample;
};



#endif
/* EOF */
