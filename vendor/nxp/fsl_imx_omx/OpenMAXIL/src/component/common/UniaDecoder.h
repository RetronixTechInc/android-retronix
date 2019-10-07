/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file UniaDecoder.h
 *  @brief Class definition of UniaDecoder Component
 *  @ingroup UniaDecoder
 */
#ifndef UniaDecoder_h
#define UniaDecoder_h

#include "AudioFilter.h"
#include "ShareLibarayMgr.h"
#include "fsl_unia.h"

typedef struct
{
    UniACodecVersionInfo                GetVersionInfo;
    UniACodecCreate                     CreateDecoder;
    UniACodecCreatePlus                 CreateDecoderPlus;
    UniACodecDelete                     DeleteDecoder;
    UniACodecReset                      ResetDecoder;
    UniACodecSetParameter               SetParameter;
    UniACodecGetParameter               GetParameter;
    UniACodec_decode_frame              DecodeFrame;
    UniACodec_get_last_error            GetLastError;

}UniaDecInterface;

typedef struct UniaDecFrameInfo {
    efsl_osal_bool bGotOneFrame;//true when get one frame and next frame's header
    fsl_osal_u32 nHeaderCount;
    fsl_osal_u32 nConsumedOffset;//frame header offset if get one frame
    fsl_osal_u32 nFrameSize;//frame size
    fsl_osal_u32 nNextSize;//frame size
    fsl_osal_u32 nHeaderSize;
} UniaDecFrameInfo;

class UniaDecoder : public AudioFilter {
    public:
        UniaDecoder();
        void SetDecLibName(const char * name);
    protected:
        OMX_S32 codingType;
        OMX_U32 outputPortBufferSize;
        const char * decoderLibName;
        const char * optionaDecoderlLibName;
        OMX_BOOL frameInput;
    private:
        OMX_ERRORTYPE InitComponent();
        OMX_ERRORTYPE DeInitComponent();
        OMX_ERRORTYPE AudioFilterInstanceInit();
        OMX_ERRORTYPE AudioFilterCodecInit();
        OMX_ERRORTYPE AudioFilterInstanceDeInit();
        AUDIO_FILTERRETURNTYPE AudioFilterFrame();
        OMX_ERRORTYPE AudioFilterReset();
        OMX_ERRORTYPE AudioFilterCheckCodecConfig();
        OMX_ERRORTYPE CreateDecoderInterface();
        OMX_ERRORTYPE InitPort();
        OMX_ERRORTYPE SetParameterToDecoder();
        OMX_ERRORTYPE ResetParameterWhenOutputChange();
        OMX_ERRORTYPE MapOutputLayoutChannel(UniAcodecOutputPCMFormat * outputValue);
        OMX_ERRORTYPE PassParamToOutputWithoutDecode();
        virtual OMX_ERRORTYPE UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value) = 0;
        virtual OMX_ERRORTYPE UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value) = 0;
        virtual OMX_ERRORTYPE UniaDecoderCheckParameter();
        virtual OMX_ERRORTYPE UniaDecoderParseFrame(OMX_U8* pBuffer,OMX_U32 len,UniaDecFrameInfo *info);
        virtual OMX_ERRORTYPE UniaDecoderGetDecoderProp(AUDIOFORMAT *formatType, OMX_BOOL *isHwBased);
        ShareLibarayMgr *libMgr;
        OMX_PTR hLib;
        UniaDecInterface * IDecoder;

        UniACodecMemoryOps memOps;
        UniACodec_Handle uniaHandle;

        OMX_S32 errorCount;//debug
        OMX_S32 profileErrorCount;

        UniACodecParameterBuffer codecConfig;
        CHAN_TABLE channelTable;
        OMX_U32 inputFrameCount;
        OMX_U32 consumeFrameCount;
        OMX_BOOL bSendEvent;

};
#endif
