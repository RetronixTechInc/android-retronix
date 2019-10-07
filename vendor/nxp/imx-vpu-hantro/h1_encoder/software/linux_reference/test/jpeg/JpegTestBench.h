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
--  Abstract : Jpeg Encoder testbench
--
------------------------------------------------------------------------------*/
#ifndef _JPEGTESTBENCH_H_
#define _JPEGTESTBENCH_H_

#include "basetype.h"

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Maximum lenght of the file path */
#ifndef MAX_PATH
#define MAX_PATH   256
#endif

#define DEFAULT -255

/* Structure for command line options */
typedef struct
{
    char input[MAX_PATH];
    char output[MAX_PATH];
    char inputThumb[MAX_PATH];
    i32 firstPic;
    i32 lastPic;
    i32 width;
    i32 height;
    i32 lumWidthSrc;
    i32 lumHeightSrc;
    i32 horOffsetSrc;
    i32 verOffsetSrc;
    i32 restartInterval;
    i32 frameType;
    i32 colorConversion;
    i32 rotation;
    i32 partialCoding;
    i32 codingMode;
    i32 markerType;
    i32 qLevel;
    i32 unitsType;
    i32 xdensity;
    i32 ydensity;
    i32 thumbnail;
    i32 widthThumb;
    i32 heightThumb;
    i32 lumWidthSrcThumb;
    i32 lumHeightSrcThumb;
    i32 horOffsetSrcThumb;
    i32 verOffsetSrcThumb;
    i32 write;
    i32 comLength;
    char com[MAX_PATH];
    i32 inputLineBufMode;
    i32 inputLineBufDepth;
}
commandLine_s;

#endif
