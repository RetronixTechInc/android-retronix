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
--  Abstract : Encoder Wrapper Layer for 6280/7280/8270/8290 without interrupts
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "ewl.h"
#include "ewl_linux_lock.h"
#include "ewl_x280_common.h"
#include "encswhwregisters.h"

#include "linux/hx280enc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#ifndef EWL_NO_HW_TIMEOUT
#define EWL_WAIT_HW_TIMEOUT 2   /* HW IRQ timeout in seconds */
#endif

extern volatile u32 asic_status;

/*******************************************************************************
 Function name   : EWLWaitHwRdy
 Description     : Poll the encoder interrupt register to notice IRQ
 Return type     : i32 
 Argument        : void
*******************************************************************************/
i32 EWLWaitHwRdy(const void *inst, u32 *slicesReady)
{
    hx280ewl_t *enc = (hx280ewl_t *) inst;
    volatile u32 irq_stats;
    u32 prevSlicesReady = 0;
    i32 ret = EWL_HW_WAIT_TIMEOUT;
    struct timespec t;
    u32 timeout = 1000;     /* Polling interval in microseconds */
    int loop = 100;         /* How many times to poll before timeout */
    u32 clrByWrite1 = (EWLReadReg(inst, BASE_HWFuse2) & HWCFGIrqClearSupport);

    assert(enc != NULL);

    PTRACE("EWLWaitHwRdy\n");
    (void) enc;

    /* The function should return when a slice is ready */
    if (slicesReady)
        prevSlicesReady = *slicesReady;

    if(timeout == (u32) (-1) )
    {
        loop = -1;   /* wait forever (almost) */
        timeout = 1000; /* 1ms polling interval */
    }

    t.tv_sec = 0;
    t.tv_nsec = timeout - t.tv_sec * 1000;
    t.tv_nsec = 100 * 1000 * 1000;

    do
    {
        /* Get the number of completed slices from ASIC registers. */
        if (slicesReady)
            *slicesReady = (enc->pRegBase[21] >> 16) & 0xFF;

        #ifdef PCIE_FPGA_VERIFICATION
        /* Only for verification purpose, to test input line buffer in hw-handshake mode or sw-irq is disabled. */
        if (pollInputLineBufTestFunc) pollInputLineBufTestFunc();
       #endif

        irq_stats = enc->pRegBase[1];

        PTRACE("EWLWaitHw: IRQ stat = %08x\n", irq_stats);

        /* ignore the irq status of input line buffer in hw handshake mode */
        if ((irq_stats == ASIC_STATUS_LINE_BUFFER_DONE) && (enc->pRegBase[BASE_HEncInstantInput/4] & (1<<29)))
            continue;

        if((irq_stats & ASIC_STATUS_ALL))
        {
            /* clear IRQ and slice ready status */
            u32 clr_stats = irq_stats & ASIC_STATUS_ALL;
            irq_stats &= (~(ASIC_STATUS_SLICE_READY|ASIC_IRQ_LINE));

            if (clrByWrite1)
              clr_stats = ASIC_STATUS_SLICE_READY | ASIC_IRQ_LINE; //(clr_stats & ASIC_STATUS_SLICE_READY) | ASIC_IRQ_LINE;
            else
              clr_stats = irq_stats;

            EWLWriteReg(inst, 0x04, clr_stats);

            ret = EWL_OK;
            loop = 0;
        }

        if (slicesReady)
        {
            if (*slicesReady > prevSlicesReady)
            {
                ret = EWL_OK;
                loop = 0;
            }
        }

        if (loop)
        {
            if(nanosleep(&t, NULL) != 0)
                PTRACE("EWLWaitHw: Sleep interrupted!\n");
        }
    }
    while(loop--);

    asic_status = irq_stats; /* update the buffered asic status */

    if (slicesReady)
        PTRACE("EWLWaitHw: slicesReady = %d\n", *slicesReady);
    PTRACE("EWLWaitHw: asic_status = %x\n", asic_status);
    PTRACE("EWLWaitHw: OK!\n");

    return ret;
}
