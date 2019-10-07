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
--  Abstract  :   Encoder instance
--
------------------------------------------------------------------------------*/

#ifndef __VP8_INSTANCE_H__
#define __VP8_INSTANCE_H__

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "enccommon.h"
#include "encpreprocess.h"
#include "encasiccontroller.h"

#include "vp8seqparameterset.h"
#include "vp8picparameterset.h"
#include "vp8picturebuffer.h"
#include "vp8putbits.h"
#include "vp8ratecontrol.h"
#include "vp8quanttable.h"

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

enum VP8EncStatus
{
    VP8ENCSTAT_INIT = 0xA1,
    VP8ENCSTAT_KEYFRAME,
    VP8ENCSTAT_START_FRAME,
    VP8ENCSTAT_ERROR
};

enum VP8EncQualityMetric {
    VP8ENC_PSNR = 0,
    VP8ENC_SSIM = 1
};

typedef struct {
    i32 quant[2];
    i32 zbin[2];
    i32 round[2];
    i32 dequant[2];
} qp;

typedef struct {
    /* Approximate bit cost of mode. IOW bits used when selected mode is
     * boolean encoded using appropriate tree and probabilities. Note that
     * this value is scale up with SCALE (=256) */
    i32 intra16ModeBitCost[4 + 1];
    i32 intra4ModeBitCost[14 + 1];
} mbs;

typedef struct {
    /* Enable signals */
    i32 goldenUpdateEnable;
    i32 goldenBoostEnable;
    
    /* MV accumulators for period */
    i32 *mvSumX, *mvSumY; 
    
    /* MB type counters for one period*/
    i32 goldenCnt;  /* Count of golden mbs */
    i32 goldenDiv;  /* Max nbr of golden */
    i32 intraCnt;   /* P-frame Intra coded mbs */
    i32 skipCnt;    /* P-frame skipped mbs */
    i32 skipDiv;    /* Nbr of P-frame macroblocks */
} statPeriod;

typedef struct
{
    u32 encStatus;
    u32 mbPerFrame;
    u32 mbPerRow;
    u32 mbPerCol;
    u32 frameCnt;
    u32 testId;
    u32 numRefBuffsLum;
    u32 numRefBuffsChr;
    u32 prevFrameLost;
    
    i32 qualityMetric;

    i32 maxNumPasses;
    u32 passNbr;
    u32 layerId;
        
    statPeriod statPeriod;
    preProcess_s preProcess;
    vp8RateControl_s rateControl;
    picBuffer picBuffer;         /* Reference picture container */
    sps sps;                     /* Sequence parameter set */
    ppss ppss;                   /* Picture parameter set */
    vp8buffer buffer[10];         /* Stream buffer per partition */
    qp qpY1[QINDEX_RANGE];  /* Quant table for 1'st order luminance */
    qp qpY2[QINDEX_RANGE];  /* Quant table for 2'nd order luminance */
    qp qpCh[QINDEX_RANGE];  /* Quant table for chrominance */
    mbs mbs;
    asicData_s asic;
    u32 *pOutBuf;                   /* User given stream output buffer */
    const void *inst;               /* Pointer to this instance for checking */
#ifdef VIDEOSTAB_ENABLED
    HWStabData vsHwData;
    SwStbData vsSwData;
#endif
    entropy entropy[1];
    u16 probCountStore[ASIC_VP8_PROB_COUNT_SIZE/2];
} vp8Instance_s;

#endif
