/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file BsacDec.h
 *  @brief Class definition of UniaDec Component
 *  @ingroup UniaDec
 */

#ifndef BsacDec_h
#define BsacDec_h

#include "UniaDecoder.h"

class BsacDec : public UniaDecoder{
    public:
        BsacDec();
    private:
        OMX_ERRORTYPE AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value);
        OMX_ERRORTYPE UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value);
        OMX_ERRORTYPE UniaDecoderGetDecoderProp(AUDIOFORMAT *formatType, OMX_BOOL *isHwBased);
        OMX_AUDIO_PARAM_BSACTYPE BsacType;
};

#endif
/* File EOF */
