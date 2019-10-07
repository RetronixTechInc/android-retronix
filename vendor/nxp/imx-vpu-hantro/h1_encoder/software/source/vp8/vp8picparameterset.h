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
--  Abstract  :   Picture parameter set
--
------------------------------------------------------------------------------*/

#ifndef VP8PIC_PARAMETER_SET_H
#define VP8PIC_PARAMETER_SET_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "enccommon.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
#define SGM_CNT 4

#define SGM_BG 0
#define SGM_AROI 1
#define SGM_ROI1 2
#define SGM_ROI2 3

typedef struct sgm {
	bool mapModified;	/* Segmentation map has been modified */
	i32  idCnt[SGM_CNT];	/* Id counts because of probability */
	/* Segment ID map is stored in ASIC SW/HW mem regs->segmentMap */
} sgm;

typedef struct {
	struct sgm sgm;		/* Segmentation data */
	i32 qp;			/* Final qp value of current macroblock */
	bool segmentEnabled;	/* Segmentation enabled */
	i32 qpSgm[SGM_CNT];	/* Qp if segments enabled (encoder set) */
	i32 levelSgm[SGM_CNT];	/* Level if segments enabled (encoder set) */
	i32 sgmQpMapping[SGM_CNT];/* Map segments: AROI, ROI1, ROI2 into IDs */
} pps;

typedef struct {
	pps *store;		/* Picture parameter set tables */
	i32 size;		/* Size of above storage table */
	pps *pps;		/* Active picture parameter set */
	pps *prevPps;		/* Previous picture parameter set */
	i32 qpSgm[SGM_CNT];	/* Current qp and level of segmentation... */
	i32 levelSgm[SGM_CNT];	/* ...which are written to the stream */
} ppss;

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/
i32 PicParameterSetAlloc(ppss *ppss, i32 mbPerPicture);
void PicParameterSetFree(ppss *ppss);

#endif
