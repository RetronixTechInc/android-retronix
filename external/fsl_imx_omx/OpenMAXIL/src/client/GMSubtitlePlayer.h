/**
 *  Copyright (c) 2013-2014, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

/**
 *  @file GMSubtitlePlayer_h
 *  @brief Class definition of subtitle playback
 *  @ingroup GraphManager
 */

#ifndef GMSubtitlePlayer_h
#define GMSubtitlePlayer_h

#include "GMSubtitleSource.h"

class GMSubtitlePlayer {
    public:
        GMSubtitlePlayer(GMPlayer *player);
        OMX_ERRORTYPE Init();
        OMX_ERRORTYPE DeInit();
        OMX_ERRORTYPE SetSource(GMSubtitleSource *source);
        OMX_ERRORTYPE Start();
        OMX_ERRORTYPE Stop();
        OMX_ERRORTYPE Reset();
        OMX_BOOL isStoped() {return bThreadStop;};

    private:
        GMPlayer *mAVPlayer;
        fsl_osal_ptr pThread;
        fsl_osal_sem pSem;
        fsl_osal_ptr Cond;
        OMX_BOOL bThreadStop;
        OMX_BOOL bReset;
        GMSubtitleSource *mSource;
        GMSubtitleSource::SUBTITLE_SAMPLE mSample;
        GMSubtitleSource::SUBTITLE_SAMPLE delayedSample;

        static fsl_osal_ptr ThreadFunc(fsl_osal_ptr arg);
        OMX_ERRORTYPE ThreadHandler();
        OMX_ERRORTYPE RenderOneSample();
};

#endif
