/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017-2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file Mp3Dec.h
 *  @brief Class definition of UniaDec Component
 *  @ingroup UniaDec
 */

#ifndef Mp3Dec_h
#define Mp3Dec_h

#include "UniaDecoder.h"

class Mp3Dec : public UniaDecoder{
    public:
        Mp3Dec();
    private:
        OMX_ERRORTYPE AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pComponentParameterStructure);
        OMX_ERRORTYPE UniaDecoderSetParameter(UA_ParaType index,OMX_S32 value);
        OMX_ERRORTYPE UniaDecoderGetParameter(UA_ParaType index,OMX_S32 * value);
        OMX_ERRORTYPE AudioFilterCheckFrameHeader();
        OMX_ERRORTYPE UniaDecoderParseFrame(OMX_U8* pBuffer,OMX_U32 len,UniaDecFrameInfo *info);
        OMX_ERRORTYPE UniaDecoderGetDecoderProp(AUDIOFORMAT *formatType, OMX_BOOL *isHwBased);
        OMX_ERRORTYPE AudioFilterHandleBOS();
        OMX_ERRORTYPE AudioFilterHandleEOS();
        OMX_AUDIO_PARAM_MP3TYPE Mp3Type;
        OMX_U32 delayLeft;
};

#endif
/* File EOF */
