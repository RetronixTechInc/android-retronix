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

#ifndef VP8QUANT_TABLE_H
#define VP8QUANT_TABLE_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "basetype.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/
#define QINDEX_RANGE 128

static const int DcQLookup[QINDEX_RANGE] = {
	4,   5,   6,   7,   8,   9,   10,  10,  11,  12,
	13,  14,  15,  16,  17,  17,  18,  19,  20,  20,
	21,  21,  22,  22,  23,  23,  24,  25,  25,  26,
	27,  28,  29,  30,  31,  32,  33,  34,  35,  36,
	37,  37,  38,  39,  40,  41,  42,  43,  44,  45,
	46,  46,  47,  48,  49,  50,  51,  52,  53,  54,
	55,  56,  57,  58,  59,  60,  61,  62,  63,  64,
	65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
	75,  76,  76,  77,  78,  79,  80,  81,  82,  83,
	84,  85,  86,  87,  88,  89,  91,  93,  95,  96,
	98,  100, 101, 102, 104, 106, 108, 110, 112, 114,
	116, 118, 122, 124, 126, 128, 130, 132, 134, 136,
	138, 140, 143, 145, 148, 151, 154, 157
};

static const int AcQLookup[QINDEX_RANGE] = {
	4,   5,   6,   7,   8,   9,   10,  11,  12,  13,
	14,  15,  16,  17,  18,  19,  20,  21,  22,  23,
	24,  25,  26,  27,  28,  29,  30,  31,  32,  33,
	34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
	44,  45,  46,  47,  48,  49,  50,  51,  52,  53,
	54,  55,  56,  57,  58,  60,  62,  64,  66,  68,
	70,  72,  74,  76,  78,  80,  82,  84,  86,  88,
	90,  92,  94,  96,  98,  100, 102, 104, 106, 108,
	110, 112, 114, 116, 119, 122, 125, 128, 131, 134,
	137, 140, 143, 146, 149, 152, 155, 158, 161, 164,
	167, 170, 173, 177, 181, 185, 189, 193, 197, 201,
	205, 209, 213, 217, 221, 225, 229, 234, 239, 245,
	249, 254, 259, 264, 269, 274, 279, 284
};

static const i32 const QRoundingFactors[QINDEX_RANGE] = {
	56, 56, 56, 56, 56, 56, 56, 56, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48
};

static const i32 const QZbinFactors[QINDEX_RANGE] = {
	64, 64, 64, 64, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80, 80, 80,
	80, 80, 80, 80, 80, 80, 80, 80
};

static const i32 ZbinBoost[16] = {
	0,   0,  8, 10, 12, 14, 16, 20, 24, 28,
	32, 36, 40, 44, 44, 44
};


#endif
