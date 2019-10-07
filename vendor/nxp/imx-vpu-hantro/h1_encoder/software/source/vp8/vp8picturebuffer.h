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

#ifndef VP8PICTURE_BUFFER_H
#define VP8PICTURE_BUFFER_H

/*------------------------------------------------------------------------------
	Include headers
------------------------------------------------------------------------------*/
#include "basetype.h"
#include "vp8entropytools.h"
#include "encasiccontroller.h"

/*------------------------------------------------------------------------------
	External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
	Module defines
------------------------------------------------------------------------------*/
#define BUFFER_SIZE	3

typedef struct {
	i32 lumWidth;		/* Width of *lum */
	i32 lumHeight;		/* Height of *lum */
	i32 chWidth;		/* Width of *cb and *cr */
	i32 chHeight;		/* Height of *cb and *cr */
    ptr_t lum;
    ptr_t cb;
} picture;

typedef struct refPic {
	picture picture;	/* Image data */
	entropy *entropy;	/* Entropy store of picture */
	i32 poc;		/* Picture order count */

	bool i_frame;		/* I frame (key frame), only intra mb */
	bool p_frame;		/* P frame, intra and inter mb */
	bool show;		/* Frame is for display (showFrame flag) */
	bool ipf;		/* Frame is immediately previous frame */
	bool arf;		/* Frame is altref frame */
	bool grf;		/* Frame is golden frame */
	bool search;		/* Frame is used for motion estimation */
	struct refPic *refPic;	/* Back reference pointer to itself */
} refPic;

typedef struct {
	i32 size;		/* Amount of allocated reference pictures */
	picture input;		/* Input picture */
	refPic refPic[BUFFER_SIZE + 1];	/* Reference picture store */
	refPic refPicList[BUFFER_SIZE];	/* Reference picture list */
	refPic *cur_pic;	/* Pointer to picture under reconstruction */
	refPic *last_pic;	/* Last picture */
} picBuffer;

/*------------------------------------------------------------------------------
	Function prototypes
------------------------------------------------------------------------------*/
i32 PictureBufferAlloc(picBuffer *picBuffer, i32 width, i32 height);
void PictureBufferFree(picBuffer *picBuffer);
void InitializePictureBuffer(picBuffer *picBuffer);
void UpdatePictureBuffer(picBuffer *picBuffer);
void PictureBufferSetRef(picBuffer *picBuffer, asicData_s *asic);

#endif
