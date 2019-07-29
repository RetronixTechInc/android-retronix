/**
 *  Copyright (c) 2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file IEC937Convert.h
 *  @brief Class definition of IEC937Convert Component
 *  @ingroup IEC937Convert
 */

#ifndef IEC937Convert_h
#define IEC937Convert_h

#include "AudioFilter.h"


class IEC937Convert : public AudioFilter {
    public:
        typedef struct {
            OMX_U32 nOffset; //Frame start offset of the input buffer
            OMX_U32 nSize;   //Frame size
        }FRAME_INFO;

        typedef enum {
            PARSE_FRAME_SUCCUSS = 0,
            PARSE_FRAME_NOT_FOUND,
            PARSE_FRAME_MORE_DATA,
        }PARSE_FRAME_RET;

        IEC937Convert();
    protected:
        OMX_U32 nInBufferSize;
        OMX_U32 nOutBufferSize;
        OMX_AUDIO_CODINGTYPE eInCodingType;
    private:
        OMX_ERRORTYPE InitComponent();
        OMX_ERRORTYPE DeInitComponent();
        OMX_ERRORTYPE AudioFilterInstanceInit() {return OMX_ErrorNone;}
        OMX_ERRORTYPE AudioFilterCodecInit() {return OMX_ErrorNone;}
        OMX_ERRORTYPE AudioFilterInstanceDeInit() {return OMX_ErrorNone;}

        AUDIO_FILTERRETURNTYPE AudioFilterFrame();
        OMX_ERRORTYPE AudioFilterReset() {return OMX_ErrorNone;}

        virtual PARSE_FRAME_RET ParseOneFrame(OMX_U8 *pBuffer, OMX_U32 nSize, FRAME_INFO *pInfo) = 0;
        virtual OMX_TICKS GetFrameDuration() = 0;
        virtual OMX_U16 GetBurstType() = 0;
        OMX_ERRORTYPE IEC937Package(OMX_U8 *pBuffer, OMX_U32 nSize);
        virtual OMX_BOOL NeedSwitchWord() {return OMX_FALSE;}
};

#endif
/* File EOF */
