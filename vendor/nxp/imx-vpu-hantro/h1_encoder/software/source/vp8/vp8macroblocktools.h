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

#ifndef VP8MACROBLOCK_TOOLS_H
#define VP8MACROBLOCK_TOOLS_H

/*------------------------------------------------------------------------------
	Include headers
------------------------------------------------------------------------------*/
#include "basetype.h"
#include "vp8instance.h"

/*------------------------------------------------------------------------------
	External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
	Module defines
------------------------------------------------------------------------------*/

typedef enum {
	/* Intra luma 16x16 or intra chroma 8x8 prediction modes */
	DC_PRED,
	V_PRED,
	H_PRED,
	TM_PRED,

	/* Common name of intra predicted mb where partition size is 4x4 */
	B_PRED,

	/* Intra 4x4 prediction modes */
	B_DC_PRED,
	B_TM_PRED,
	B_VE_PRED,
	B_HE_PRED,
	B_LD_PRED,
	B_RD_PRED,
	B_VR_PRED,
	B_VL_PRED,
	B_HD_PRED,
	B_HU_PRED,

	/* Inter prediction (partitioning) types */
	P_16x16,		/* [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15] */
	P_16x8,			/* [0,1,2,3,4,5,6,7,8][9,10,11,12,13,14,15] */
	P_8x16,			/* [0,1,4,5,8,9,12,13][2,3,6,7,10,11,14,15] */
	P_8x8,			/* [0,1,4,5][2,3,6,7][8,9,12,13][10,11,14,15] */
	P_4x4			/* Every subblock gets its own vector */
} type;

/*------------------------------------------------------------------------------
	Function prototypes
------------------------------------------------------------------------------*/
void InitQuantTables(vp8Instance_s *);

#endif
