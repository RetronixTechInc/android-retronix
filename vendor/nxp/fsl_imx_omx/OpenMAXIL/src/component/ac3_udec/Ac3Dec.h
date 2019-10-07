/**
 *  Copyright (c) 2009-2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file Ac3Dec.h
 *  @brief Class definition of UniaDec Component
 *  @ingroup UniaDec
 */

#ifndef Ac3Dec_h
#define Ac3Dec_h

#include "UniaDecoder.h"

class Ac3Dec : public UniaDecoder{
    public:
        Ac3Dec();
    private:
        OMX_ERRORTYPE AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
		OMX_ERRORTYPE UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value);
		OMX_ERRORTYPE UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value);
		OMX_ERRORTYPE AudioFilterCheckFrameHeader();
		OMX_AUDIO_PARAM_AC3TYPE Ac3Type;
};

#endif
/* File EOF */
