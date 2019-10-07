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
--  Abstract : 
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	Include headers
------------------------------------------------------------------------------*/
#include "vp8macroblocktools.h"
#include "vp8quanttable.h"

/*------------------------------------------------------------------------------
	External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	Module defines
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
	Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
	InitQuantTables		TODO delta
------------------------------------------------------------------------------*/
void InitQuantTables(vp8Instance_s *mbs)
{
	i32 i, j, tmp;
	qp *qp;

	for (i = 0; i < QINDEX_RANGE; i++) {
		/* Quant table for 1'st order luminance */
		qp = &mbs->qpY1[i];
		for (j = 0; j < 2; j++) {
			if (j == 0) {
				tmp = DcQLookup[CLIP3(i, 0, QINDEX_RANGE - 1)];
			} else {
				tmp = AcQLookup[CLIP3(i, 0, QINDEX_RANGE - 1)];
			}
			qp->quant[j]   = MIN((1 << 16) / tmp, 0x3FFF);
			qp->zbin[j]    = ((QZbinFactors[i] * tmp) + 64) >> 7;
			qp->round[j]   = (QRoundingFactors[i] * tmp) >> 7;
			qp->dequant[j] = tmp;
		}

		/* Quant table for 2'st order luminance */
		qp = &mbs->qpY2[i];
		for (j = 0; j < 2; j++) {
			if (j == 0) {
				tmp = DcQLookup[CLIP3(i, 0, QINDEX_RANGE - 1)];
				tmp = tmp * 2;
			} else {
				tmp = AcQLookup[CLIP3(i, 0, QINDEX_RANGE - 1)];
				tmp = (tmp * 155) / 100;
				if (tmp < 8) tmp = 8;
			}
			qp->quant[j]   = MIN((1 << 16) / tmp, 0x3FFF);
			qp->zbin[j]    = ((QZbinFactors[i] * tmp) + 64) >> 7;
			qp->round[j]   = (QRoundingFactors[i] * tmp) >> 7;
			qp->dequant[j] = tmp;
		}

		/* Quant table for chrominance */
		qp = &mbs->qpCh[i];
		for (j = 0; j < 2; j++) {
			if (j == 0) {
				tmp = DcQLookup[CLIP3(i, 0, QINDEX_RANGE - 1)];
				if (tmp > 132) tmp = 132;
			} else {
				tmp = AcQLookup[CLIP3(i, 0, QINDEX_RANGE - 1)];
			}
			qp->quant[j]   = MIN((1 << 16) / tmp, 0x3FFF);
			qp->zbin[j]    = ((QZbinFactors[i] * tmp) + 64) >> 7;
			qp->round[j]   = (QRoundingFactors[i] * tmp) >> 7;
			qp->dequant[j] = tmp;
		}
	}
}


