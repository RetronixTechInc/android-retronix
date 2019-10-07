/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */

#include "LibavDecInterfaceMgr.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
}


#define LIBAV_CODEC_LIBRARY_NAME "libavcodec.so"
#define LIBAV_UTIL_LIBRARY_NAME "libavutil.so"
#define LIBAV_SWRESAMPLE_LIBRARY_NAME "libswresample.so"

#define IS_LIBAV_CODEC_INTERFACE(id) ((id) >= AVCODEC_REGISTER_ALL && (id) <= AVCODEC_FLUSH_BUFFERS)
#define IS_LIBAV_UTIL_INTERFACE(id)  ((id) >= AV_FRAME_ALLOC && (id) <= AV_LOG_SET_CALLBACK)
#define IS_LIB_SWRESAMPLE_INTERFACE(id) ((id) >= SWR_ALLOC && (id) <= SWR_FREE)



enum {
    AVCODEC_REGISTER_ALL = 0,
    AVCODEC_FIND_DECODER,
    AVCODEC_ALLOC_CONTEXT3,
    AVCODEC_FREE_CONTEXT,
    AVCODEC_OPEN2,
    AVCODEC_CLOSE,
    AV_INIT_PACKET,
    AVCODEC_SEND_PACKET,
    AVCODEC_RECEIVE_FRAME,
    AVCODEC_FLUSH_BUFFERS,

    AV_FRAME_ALLOC,
    AV_FRAME_FREE,
    AV_SAMPLES_GET_BUFFER_SIZE,
    AV_GET_BYTES_PER_SAMPLE,
    AV_OPT_SET_INT,
    AV_FREE,
    AV_LOG_SET_LEVEL,
    AV_LOG_SET_CALLBACK,

    SWR_ALLOC,
    SWR_INIT,
    SWR_CONVERT,
    SWR_CLOSE,
    SWR_FREE,
};


typedef struct {
    const char* name;
    int id;
} LIBAV_DEC_INTERFACE_NAME;

static LIBAV_DEC_INTERFACE_NAME libav_map_table[] = {
    {"avcodec_register_all", AVCODEC_REGISTER_ALL},
    {"avcodec_find_decoder", AVCODEC_FIND_DECODER},
    {"avcodec_alloc_context3", AVCODEC_ALLOC_CONTEXT3},
    {"avcodec_free_context", AVCODEC_FREE_CONTEXT},
    {"avcodec_open2", AVCODEC_OPEN2},
    {"avcodec_close", AVCODEC_CLOSE},
    {"av_init_packet", AV_INIT_PACKET},
    {"avcodec_send_packet", AVCODEC_SEND_PACKET},
    {"avcodec_receive_frame", AVCODEC_RECEIVE_FRAME},

    {"av_frame_alloc", AV_FRAME_ALLOC},
    {"av_frame_free", AV_FRAME_FREE},
    {"av_samples_get_buffer_size", AV_SAMPLES_GET_BUFFER_SIZE},
    {"av_get_bytes_per_sample", AV_GET_BYTES_PER_SAMPLE},
    {"av_opt_set_int", AV_OPT_SET_INT},
    {"av_free", AV_FREE},
    {"av_log_set_level", AV_LOG_SET_LEVEL},
    {"av_log_set_callback", AV_LOG_SET_CALLBACK},

    {"swr_alloc", SWR_ALLOC},
    {"swr_init", SWR_INIT},
    {"swr_convert", SWR_CONVERT},
    {"swr_close", SWR_CLOSE},
    {"swr_free", SWR_FREE},

};

LibavDecInterfaceMgr::LibavDecInterfaceMgr()
{
    libMgr = NULL;
    hLibavcodec = NULL;
    hLibavutil = NULL;
    hLibswresample = NULL;
}

fsl_osal_s32 LibavDecInterfaceMgr::CreateLibavDecInterfaces(LibavDecInterface* ILibavDec)
{
    if (!ILibavDec)
        return -1;

    libMgr = FSL_NEW(ShareLibarayMgr, ());
    if(libMgr == NULL){
        FSL_DELETE(ILibavDec);
        return -1;
    }

    hLibavcodec = libMgr->load((fsl_osal_char *)LIBAV_CODEC_LIBRARY_NAME);
    hLibavutil = libMgr->load((fsl_osal_char *)LIBAV_UTIL_LIBRARY_NAME);
    hLibswresample = libMgr->load((fsl_osal_char *)LIBAV_SWRESAMPLE_LIBRARY_NAME);

    if (!hLibavcodec || !hLibavutil || !hLibswresample) {
        LOG_ERROR("load libav libraries failed, %p %p %p\n", hLibavcodec, hLibavutil, hLibswresample);
        return -1;
    }

    fsl_osal_ptr libhandle, ptr;
    fsl_osal_u32 i;

    for (i = 0; i < sizeof(libav_map_table)/sizeof(libav_map_table[0]); i++) {

        if (IS_LIBAV_CODEC_INTERFACE(libav_map_table[i].id))
            libhandle = hLibavcodec;
        else if (IS_LIBAV_UTIL_INTERFACE(libav_map_table[i].id))
            libhandle = hLibavutil;
        else if (IS_LIB_SWRESAMPLE_INTERFACE(libav_map_table[i].id))
            libhandle = hLibswresample;
        else
            return -1;

        ptr = libMgr->getSymbol(libhandle, (fsl_osal_char *)libav_map_table[i].name);
        if (ptr == NULL) {
            LOG_ERROR("getSymbol for %s failed\n", libav_map_table[i].name);
            return -1;
        }

        switch(libav_map_table[i].id) {
            case AVCODEC_REGISTER_ALL:
                ILibavDec->AvcodecRegisterAll = (avcodec_register_all_t)ptr;
                break;
            case AVCODEC_FIND_DECODER:
                ILibavDec->AvcodecFindDecoder = (avcodec_find_decoder_t)ptr;
                break;
            case AVCODEC_ALLOC_CONTEXT3:
                ILibavDec->AvcodecAllocContext3 = (avcodec_alloc_context3_t)ptr;
                break;
            case AVCODEC_FREE_CONTEXT:
                ILibavDec->AvcodecFreeContext = (avcodec_free_context_t)ptr;
                break;
            case AVCODEC_OPEN2:
                ILibavDec->AvcodecOpen2 = (avcodec_open2_t)ptr;
                break;
            case AVCODEC_CLOSE:
                ILibavDec->AvcodecClose = (avcodec_close_t)ptr;
                break;
            case AV_INIT_PACKET:
                ILibavDec->AvInitPacket = (av_init_packet_t)ptr;
                break;
            case AVCODEC_SEND_PACKET:
                ILibavDec->AvcodecSendPacket = (avcodec_send_packet_t)ptr;
                break;
            case AVCODEC_RECEIVE_FRAME:
                ILibavDec->AvcodecReceiveFrame = (avcodec_receive_frame_t)ptr;
                break;
            case AVCODEC_FLUSH_BUFFERS:
                ILibavDec->AvcodecFlushBuffers = (avcodec_flush_buffers_t)ptr;
                break;
            case AV_FRAME_ALLOC:
                ILibavDec->AvFrameAlloc = (av_frame_alloc_t)ptr;
                break;
            case AV_FRAME_FREE:
                ILibavDec->AvFrameFree = (av_frame_free_t)ptr;
                break;
            case AV_SAMPLES_GET_BUFFER_SIZE:
                ILibavDec->AvSamplesGetBufferSize = (av_samples_get_buffer_size_t)ptr;
                break;
            case AV_GET_BYTES_PER_SAMPLE:
                ILibavDec->AvGetBytesPerSample = (av_get_bytes_per_sample_t)ptr;
                break;
            case AV_OPT_SET_INT:
                ILibavDec->AvOptSetInt = (av_opt_set_int_t)ptr;
                break;
            case AV_FREE:
                ILibavDec->AvFree = (av_free_t)ptr;
                break;
            case AV_LOG_SET_LEVEL:
                ILibavDec->AvLogSetLevel = (av_log_set_level_t)ptr;
                break;
            case AV_LOG_SET_CALLBACK:
                ILibavDec->AvLogSetCallback = (av_log_set_callback_t)ptr;
                break;
            case SWR_ALLOC:
                ILibavDec->SwrAlloc = (swr_alloc_t)ptr;
                break;
            case SWR_INIT:
                ILibavDec->SwrInit = (swr_init_t)ptr;
                break;
            case SWR_CONVERT:
                ILibavDec->SwrConvert = (swr_convert_t)ptr;
                break;
            case SWR_CLOSE:
                ILibavDec->SwrClose = (swr_close_t)ptr;
                break;
            case SWR_FREE:
                ILibavDec->SwrFree = (swr_free_t)ptr;
                break;
            default:
                LOG_ERROR("unknown libav dec interface id!\n");
                break;
        }
    }

    LOG_DEBUG("ILibavDec codec api: %p %p %p %p %p %p %p %p %p\n", \
        ILibavDec->AvcodecRegisterAll,
        ILibavDec->AvcodecFindDecoder,
        ILibavDec->AvcodecAllocContext3,
        ILibavDec->AvcodecFreeContext,
        ILibavDec->AvcodecOpen2,
        ILibavDec->AvcodecClose,
        ILibavDec->AvInitPacket,
        ILibavDec->AvcodecSendPacket,
        ILibavDec->AvcodecReceiveFrame);

    LOG_DEBUG("ILibavDec util api: %p %p %p %p %p %p\n", \
        ILibavDec->AvFrameAlloc,
        ILibavDec->AvFrameFree,
        ILibavDec->AvSamplesGetBufferSize,
        ILibavDec->AvGetBytesPerSample,
        ILibavDec->AvOptSetInt,
        ILibavDec->AvFree);

    LOG_DEBUG("ILibavDec swresample api: %p %p %p %p %p\n", \
        ILibavDec->SwrAlloc,
        ILibavDec->SwrInit,
        ILibavDec->SwrConvert,
        ILibavDec->SwrClose,
        ILibavDec->SwrFree);

    return 0;
}

fsl_osal_s32 LibavDecInterfaceMgr::DeleteLibavDecInterfaces(LibavDecInterface* ILibavDec)
{
    if (libMgr) {

        if (hLibavcodec)
            libMgr->unload(hLibavcodec);

        if (hLibavutil)
            libMgr->unload(hLibavutil);

        if (hLibswresample)
            libMgr->unload(hLibswresample);

        FSL_DELETE(libMgr);
    }

    return 0;
}

/* EOF */
