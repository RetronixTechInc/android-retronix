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
--  Description :  Encoder system model, stream tracing
--
------------------------------------------------------------------------------*/

#include <string.h>
#include "enctracestream.h"

/*------------------------------------------------------------------------------
  External compiler flags
--------------------------------------------------------------------------------

NO_2GB_LIMIT: Don't check 2GB limit when writing stream.trc

--------------------------------------------------------------------------------
  Module defines
------------------------------------------------------------------------------*/

/* Structure for storing stream trace information */
traceStream_s traceStream;

/*------------------------------------------------------------------------------
  Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
  EncOpenStreamTrace()
------------------------------------------------------------------------------*/
i32 EncOpenStreamTrace(const char *filename)
{
	FILE *file;
        u32 reopen = 0;

        if (traceStream.file) {
                reopen = 1;
                EncCloseStreamTrace();
        }

	traceStream.file = fopen(filename, "a");

	if (traceStream.file == NULL) {
		fprintf(stderr, "Unable to open <%s>.", filename);
		return -1;
	}

	file = traceStream.file;
	fprintf(file, " Vop\t   Bit\tID   Data  Len\tBitPattern\t\t\tDescription\n");
	fprintf(file, "-----------------------------------------------");
	fprintf(file, "-----------------------------------------\n");

        /* If reopening trace file don't reset the tracing */
        if (reopen)
                return 0;

	traceStream.frameNum = 0;
	traceStream.bitCnt = 0;
	traceStream.disableStreamTrace = 0;
	traceStream.id = 0;

	return 0;
}

/*------------------------------------------------------------------------------
  EncCloseStreamTrace()
------------------------------------------------------------------------------*/
void EncCloseStreamTrace(void)
{
	if (traceStream.file != NULL)
		fclose(traceStream.file);
	traceStream.file = NULL;
}



/*------------------------------------------------------------------------------
  EncTraceStream()  Writes per symbol trace in ASCII format. Trace can be
  disable with traceStream.disableStreamTrace e.g. for unwanted headers.
  Id can be used to differentiate the source of the stream eg. SW=0 HW=1
------------------------------------------------------------------------------*/
void EncTraceStream(i32 value, i32 numberOfBits)
{
	static u32 lines = 0;
	FILE *file = traceStream.file;
	i32 i, j = 0, k = 0;

#ifndef NO_2GB_LIMIT
	/* Check for file size limit 2GB, approx 50 bytes/line */
	lines++;
	if (file && (lines*50 >= (1U<<31))) {
		fclose(file); file = traceStream.file = NULL;
	}
#endif

	if (file != NULL && !traceStream.disableStreamTrace) {
		fprintf(file,"%4i\t%6i\t%2i %6i  %2i\t",
				traceStream.frameNum, traceStream.bitCnt,
				traceStream.id, value, numberOfBits);

                if (numberOfBits < 0)
                    return;

		for (i = numberOfBits; i; i--) {
			if (value & (1 << (i - 1))) {
				fprintf(file, "1");
			} else {
				fprintf(file, "0");
			}
			j++;
			if (j == 4) {
				fprintf(file, " ");
				j = 0;
				k++;
			}
		}
		for (i = numberOfBits + k; i < 32; i += 8) {
			fprintf(file, "\t");
		}
	}

	traceStream.bitCnt += numberOfBits;
}

/*------------------------------------------------------------------------------
  EncCommentMbType()
------------------------------------------------------------------------------*/
void EncCommentMbType(const char *comment, i32 mbNum)
{
	FILE *file = traceStream.file;

	if (file == NULL || traceStream.disableStreamTrace) {
		return;
	}
	fprintf(file, "mb_type %-22s [%.3i]\n", comment, mbNum);
}

/*------------------------------------------------------------------------------
  EncComment()
------------------------------------------------------------------------------*/
void EncComment(const char *comment)
{
	FILE *file = traceStream.file;

	if (file == NULL || traceStream.disableStreamTrace) {
		return;
	}
	fprintf(file, "%s\n", comment);
}


