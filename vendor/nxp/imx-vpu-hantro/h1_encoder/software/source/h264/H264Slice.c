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
--  Abstract  :
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "H264Slice.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

	H264SliceInit

------------------------------------------------------------------------------*/
void H264SliceInit(slice_s * slice)
{
    slice->byteStream = ENCHW_YES;
    slice->nalUnitType = IDR;
    slice->sliceType = ISLICE;
    slice->picParameterSetId = 0;
    slice->frameNum = 0;
    slice->frameNumBits = 16;
    slice->idrPicId = 0;
    slice->nalRefIdc = 1;
    slice->disableDeblocking = 0;
    slice->filterOffsetA = -4;
    slice->filterOffsetB = 6;
    slice->sliceSize = 0;
    slice->cabacInitIdc = 0;
    slice->quarterPixelMv = 1;

    return;
}
