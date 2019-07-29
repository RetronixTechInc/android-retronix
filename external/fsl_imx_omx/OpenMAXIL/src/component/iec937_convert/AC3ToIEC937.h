/**
 *  Copyright (c) 2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file AC3ToIEC937.h
 *  @brief Class definition of AC3ToIEC937 Component
 *  @ingroup AC3ToIEC937
 */

#ifndef AC3ToIEC937_h
#define AC3ToIEC937_h

#include "IEC937Convert.h"

#define IEC937_BURST_INFO_AC3   0x01

class AC3ToIEC937 : public IEC937Convert {
    public:
        AC3ToIEC937();
    private:
        OMX_ERRORTYPE AudioFilterGetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE AudioFilterSetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);

        IEC937Convert::PARSE_FRAME_RET ParseOneFrame(OMX_U8 *pBuffer, OMX_U32 nSize, FRAME_INFO *pInfo);
        OMX_TICKS GetFrameDuration();
        OMX_U16 GetBurstType() {return IEC937_BURST_INFO_AC3;}
        OMX_BOOL NeedSwitchWord() {return bSwitchWord;}

        OMX_AUDIO_PARAM_AC3TYPE Ac3Type;
        OMX_BOOL bFirstFrame;
        OMX_BOOL bSwitchWord;
};

#endif
/* File EOF */
