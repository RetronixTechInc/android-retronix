/**
 *  Copyright (c) 2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file Ec3Dec.h
 *  @brief Class definition of UniaDec Component
 *  @ingroup UniaDec
 */

#ifndef Ec3Dec_h
#define Ec3Dec_h

#include "UniaDecoder.h"

class Ec3Dec : public UniaDecoder{
    public:
        Ec3Dec();
    private:
        OMX_ERRORTYPE AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value);
		OMX_ERRORTYPE UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value);
		OMX_AUDIO_PARAM_EC3TYPE Ec3Type;
};

#endif
/* File EOF */
