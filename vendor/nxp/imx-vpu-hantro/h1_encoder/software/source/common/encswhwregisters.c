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
--  Description :  Encoder SW/HW interface register definitions
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines

------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "encswhwregisters.h"
#include "enccommon.h"
#include "ewl.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Define this to print debug info for every register write.
#define DEBUG_PRINT_REGS */

/*------------------------------------------------------------------------------

    EncAsicSetRegisterValue

    Set a value into a defined register field

------------------------------------------------------------------------------*/
void EncAsicSetRegisterValue(u32 *regMirror, regName name, u32 value)
{
    const regField_s *field;
    u32 regVal;

    field = &asicRegisterDesc[name];

#ifdef DEBUG_PRINT_REGS
    printf("EncAsicSetRegister 0x%2x  0x%08x  Value: %10d  %s\n",
            field->base, field->mask, value, field->description);
#endif

    /* Check that value fits in field */
    ASSERT(field->name == name);
    ASSERT(((field->mask >> field->lsb) << field->lsb) == field->mask);
    ASSERT((field->mask >> field->lsb) >= value);
    ASSERT(field->base < ASIC_SWREG_AMOUNT*4);

    /* Clear previous value of field in register */
    regVal = regMirror[field->base/4] & ~(field->mask);

    /* Put new value of field in register */
    regMirror[field->base/4] = regVal | ((value << field->lsb) & field->mask);
}

/*------------------------------------------------------------------------------

    EncAsicWriteRegisterValue

    Write a value into a defined register field (write will happens actually).

------------------------------------------------------------------------------*/
void EncAsicWriteRegisterValue(const void *ewl, u32 *regMirror, regName name, u32 value)
{
    const regField_s *field;
    u32 regVal;

    field = &asicRegisterDesc[name];

#ifdef DEBUG_PRINT_REGS
    printf("EncAsicSetRegister 0x%2x  0x%08x  Value: %10d  %s\n",
            field->base, field->mask, value, field->description);
#endif

    /* Check that value fits in field */
    ASSERT(field->name == name);
    ASSERT(((field->mask >> field->lsb) << field->lsb) == field->mask);
    ASSERT((field->mask >> field->lsb) >= value);
    ASSERT(field->base < ASIC_SWREG_AMOUNT*4);

    /* Clear previous value of field in register */
    regVal = regMirror[field->base/4] & ~(field->mask);

    /* Put new value of field in register */
    regMirror[field->base/4] = regVal | ((value << field->lsb) & field->mask);

    /* write it into HW registers */
    EWLWriteReg(ewl, field->base, regMirror[field->base/4]);
}

/*------------------------------------------------------------------------------

    EncAsicGetRegisterValue

    Get an unsigned value from the ASIC registers

------------------------------------------------------------------------------*/
u32 EncAsicGetRegisterValue(const void *ewl, u32 *regMirror, regName name)
{
    const regField_s *field;
    u32 value;

    field = &asicRegisterDesc[name];

    ASSERT(field->base < ASIC_SWREG_AMOUNT*4);

    value = regMirror[field->base/4] = EWLReadReg(ewl, field->base);
    value = (value & field->mask) >> field->lsb;

    return value;
}



