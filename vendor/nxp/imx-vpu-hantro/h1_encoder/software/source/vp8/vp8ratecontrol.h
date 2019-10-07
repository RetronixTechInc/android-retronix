/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
--                                                                            --
--  Description : Rate control structures and function prototypes
--
------------------------------------------------------------------------------*/

#ifndef VP8_RATE_CONTROL_H
#define VP8_RATE_CONTROL_H

#include "enccommon.h"

enum
{ VP8RC_OVERFLOW = -1 };

#define RC_TABLE_LENGTH     10  /* DO NOT CHANGE THIS */

typedef struct {
    i32  a1;               /* model parameter */
    i32  a2;               /* model parameter */
    i32  qp_prev;          /* previous QP */
    i32  qs[RC_TABLE_LENGTH+1];   /* quantization step size */
    i32  bits[RC_TABLE_LENGTH+1]; /* Number of bits needed to code residual */
    i32  pos;              /* current position */
    i32  len;              /* current lenght */
    i32  zero_div;         /* a1 divisor is 0 */
} linReg_s;

/* Virtual buffer */
typedef struct
{
    i32 bufferSize;          /* size of the virtual buffer */
    i32 bitRate;             /* input bit rate per second */
    i32 bitPerPic;           /* average number of bits per picture */
    i32 picTimeInc;          /* time stamp modulo second */
    i32 timeScale;           /* input frame rate numerator */
    i32 virtualBitCnt;       /* virtual (channel) bit count */
    i32 realBitCnt;          /* real bit count */
    i32 bufferOccupancy;     /* number of bits in the buffer */
    i32 skipFrameTarget;     /* how many frames should be skipped in a row */
    i32 skippedFrames;       /* how many frames have been skipped in a row */
    i32 bucketFullness;      /* Leaky Bucket fullness */
    i32 seconds;             /* Full seconds elapsed */
    i32 averageBitRate;      /* This buffer average bitrate for full seconds */
    i32 outRateDenom;        /* Average time increment for this layer */
    i32 qpPrev;              /* QP for previous frame in this layer */
} vp8VirtualBuffer_s;

typedef struct
{
    true_e picRc;
    true_e picSkip;          /* Frame Skip enable */
    true_e frameCoded;       /* Frame coded or not */
    i32 mbPerPic;            /* Number of macroblock per picture */
    i32 mbRows;              /* MB rows in picture */
    i32 currFrameIntra;      /* Is current frame intra frame? */
    i32 prevFrameIntra;      /* Was previous frame intra frame? */
    i32 currFrameGolden;     /* Is current frame golden frame? */
    i32 fixedQp;             /* Pic header qp when fixed */
    i32 qpHdr;               /* Pic header qp of current voded picture */
    i32 qpMin;               /* Pic header minimum qp, user set */
    i32 qpMax;               /* Pic header maximum qp, user set */
    i32 qpHdrPrev;           /* Pic header qp of previous coded picture */
    i32 qpHdrPrev2;           /* Pic header qp of previous coded picture */    
    i32 qpHdrGolden;         /* Pic header qp of previous coded picture */    
    i32 outRateNum;
    i32 outRateDenom;        /* Average time increment for whole stream */
    vp8VirtualBuffer_s virtualBuffer[4];  /* Bit buffer for each temporal layer */
   /* for frame QP rate control */
    linReg_s linReg;       /* Data for R-Q model */
    linReg_s rError[4];    /* Rate prediction error for each layer (bits) */
    linReg_s intra;        /* Data for intra frames */
    linReg_s intraError;   /* Prediction error for intra frames */
    linReg_s gop;          /* Data for GOP */
    i32 targetPicSize;
    i32 frameBitCnt;
    i32 sumQp;
    i32 sumBitrateError;
    i32 sumFrameError;
   /* for GOP rate control */
    i32 gopQpSum;
    i32 gopQpDiv;
    i32 gopBitCnt;          /* Current GOP bit count so far */
    i32 gopAvgBitCnt;       /* Previous GOP average bit count */
    i32 frameCnt;
    i32 windowLen;
    i32 windowRem;          /* Number of frames remaining in this GOP */
    i32 intraInterval;      /* Distance between two previous I-frames */
    i32 intraIntervalCtr;
    i32 intraQpDelta;
    i32 fixedIntraQp;
    i32 mbQpAdjustment;     /* QP delta for MAD macroblock QP adjustment */
    i32 intraPictureRate;
    i32 goldenPictureRate;
    i32 altrefPictureRate;
    i32 goldenPictureBoost;
    
    i32 adaptiveGoldenBoost;
    i32 adaptiveGoldenUpdate;
    
    /* adaptive QP adjustment values, not visible through API */
    i32 goldenRefreshThreshold;
    i32 goldenBoostThreshold;        
    u32 layerAmount;
} vp8RateControl_s;

/*------------------------------------------------------------------------------
    Function prototypes
------------------------------------------------------------------------------*/
void VP8InitRc(vp8RateControl_s * rc, u32 newStream);
void VP8BeforePicRc(vp8RateControl_s * rc, u32 timeInc, u32 frameTypeIntra,
                    u32 goldenRefresh, i32 boostPct, u32 layerId);
void VP8AfterPicRc(vp8RateControl_s * rc, u32 byteCnt, u32 layerId);
i32 VP8Calculate(i32 a, i32 b, i32 c);
#endif /* VP8_RATE_CONTROL_H */

