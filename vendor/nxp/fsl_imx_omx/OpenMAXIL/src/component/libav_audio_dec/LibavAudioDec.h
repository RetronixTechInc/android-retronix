/**
 *  Copyright (c) 2014, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#ifndef LibavAudioDec_h
#define LibavAudioDec_h

#include "AudioFilter.h"
#include "OMX_Implement.h"
#include "ShareLibarayMgr.h"
#include "LibavDecInterfaceMgr.h"


extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
}

#define MAX_LIBAVAUDIO_CODEC_DATA_SIZE 1024


class LibavAudioDec : public AudioFilter {
    public:
        LibavAudioDec();
    private:
        OMX_ERRORTYPE InitComponent();
        OMX_ERRORTYPE DeInitComponent();
        OMX_ERRORTYPE AudioFilterInstanceInit();
        OMX_ERRORTYPE AudioFilterCodecInit();
        OMX_ERRORTYPE AudioFilterInstanceDeInit();
        OMX_ERRORTYPE AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE AudioFilterSetParameterPCM();
        AUDIO_FILTERRETURNTYPE AudioFilterFrame();
        OMX_ERRORTYPE AudioFilterCheckCodecConfig();
        OMX_ERRORTYPE AudioFilterReset();
        OMX_ERRORTYPE CheckPortFmt();
        OMX_ERRORTYPE GetCodecID();
        OMX_ERRORTYPE SetRoleFormat(OMX_STRING role);
        OMX_ERRORTYPE AVConvertSample(OMX_U32 *len);
        OMX_ERRORTYPE GetCodecContext();
        union {
        OMX_AUDIO_PARAM_PCMMODETYPE PcmMode;
        OMX_AUDIO_PARAM_ADPCMMODETYPE AdpcmMode;
        OMX_AUDIO_PARAM_OPUSTYPE OpusMode;
        OMX_AUDIO_PARAM_APETYPE ApeMode;
        } InputMode;
        OMX_AUDIO_CODINGTYPE CodingType;
        enum AVCodecID codecID;
        AVCodecContext *codecContext;
        SwrContext *avr;
        AVFrame *frame;
        OMX_U8 cRole[OMX_MAX_STRINGNAME_SIZE];
        OMX_U8 codecData[MAX_LIBAVAUDIO_CODEC_DATA_SIZE];
        OMX_U32 codecDataSize;
        OMX_U32 errorCnt;
        OMX_BOOL bNeedInput;
        OMX_U32 nInputSize;
        LibavDecInterface *ILibavDec;
        LibavDecInterfaceMgr * libavDecMgr;
};

#endif
/* File EOF */

