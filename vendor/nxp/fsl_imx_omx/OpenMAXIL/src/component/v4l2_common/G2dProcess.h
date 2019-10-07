/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef G2DPROCESS_H
#define G2DPROCESS_H

#include "Process3.h"

class G2dProcess : public Process3{
public:
    G2dProcess();
    ~G2dProcess();
private:
    OMX_ERRORTYPE Process(DmaBufferHdr *inBufHdlr, OMX_BUFFERHEADERTYPE *outBufHdlr);
    OMX_ERRORTYPE Process(OMX_BUFFERHEADERTYPE *inBufHdlr, DmaBufferHdr *outBufHdlr);
    OMX_U32 getG2dFormat(OMX_COLOR_FORMATTYPE color_format);

    OMX_U32 cnt;
    void *g2d_handle;
};
#endif

