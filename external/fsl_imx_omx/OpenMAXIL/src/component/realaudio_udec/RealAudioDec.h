/**
 *  Copyright (c) 2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file RealAudioDec.h
 *  @brief Class definition of UniaDec Component
 *  @ingroup UniaDec
 */

#ifndef RealAudioDec_h
#define RealAudioDec_h

#include "UniaDecoder.h"

class RealAudioDec : public UniaDecoder{
    public:
        RealAudioDec();
    private:
        OMX_ERRORTYPE AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value);
        OMX_ERRORTYPE UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value);
        OMX_ERRORTYPE UniaDecoderCheckParameter();
        OMX_AUDIO_PARAM_RATYPE RealAudioType;
};

#endif
/* File EOF */
