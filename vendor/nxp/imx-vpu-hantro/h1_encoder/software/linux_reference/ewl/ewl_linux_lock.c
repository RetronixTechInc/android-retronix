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
--  Description : Locking semaphore for hardware sharing
--
------------------------------------------------------------------------------*/
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include "ewl_linux_lock.h"
#ifdef USE_SYS_V_IPC
#include <sys/ipc.h>
#include <sys/sem.h>
#endif

#ifdef _SEM_SEMUN_UNDEFINED
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short int *array;
    struct seminfo *__buf;
};
#endif

/* Obtain a binary semaphore's ID, allocating if necessary.  */
#ifdef USE_SYS_V_IPC  /*SYSTEM_V*/
int binary_semaphore_allocation(key_t key, int nsem, int sem_flags)
{
    return semget(key, nsem, sem_flags);
}

/* Deallocate a binary semaphore.  All users must have finished their
   use.  Returns -1 on failure.  */
int binary_semaphore_deallocate(int semid)
{
    return semctl(semid, 0, IPC_RMID);
}

/* Wait on a binary semaphore.  Block until the semaphore value is
   positive, then decrement it by one.  */
int binary_semaphore_wait(int semid, int sem_num)
{
    int ret;
    struct sembuf operations[1];

    /* Use 'sem_num' semaphore from the set.  */
    operations[0].sem_num = sem_num;
    /* Decrement by 1.  */
    operations[0].sem_op = -1;
    /* Permit undo'ing.  */
    operations[0].sem_flg = SEM_UNDO;

    /* signal safe */
    do
    {
        ret = semop(semid, operations, 1);
    }
    while(ret == -1 && errno == EINTR);

    return ret;
}

/* Post to a binary semaphore: increment its value by one */
int binary_semaphore_post(int semid, int sem_num)
{
    int ret;
    struct sembuf operations[1];

    /* Use 'sem_num' semaphore from the set.  */
    operations[0].sem_num = sem_num;
    /* Increment by 1.  */
    operations[0].sem_op = 1;
    /* Permit undo'ing.  */
    operations[0].sem_flg = SEM_UNDO;

    /* signal safe */
    do
    {
        ret = semop(semid, operations, 1);
    }
    while(ret == -1 && errno == EINTR);

    return ret;
}

/* Initialize a binary semaphore with a value of one.  */
int binary_semaphore_initialize(int semid, int semnum)
{
    union semun argument;

    argument.val = 1;
    return semctl(semid, semnum, SETVAL, argument);
}

/* Get the value of a binary semaphore  */
int binary_semaphore_getval(int semid, int semnum)
{
    return semctl(semid, semnum, GETVAL);
}
#endif
