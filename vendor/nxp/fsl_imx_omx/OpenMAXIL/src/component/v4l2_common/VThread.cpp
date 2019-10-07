/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "VThread.h"
fsl_osal_ptr EventThreadFunc(fsl_osal_ptr arg)
{
    VThread *thread = (VThread*)arg;

    while(1) {
        fsl_osal_mutex_lock(thread->sMutex);

        if(thread->bFlush){
            fsl_osal_sem_post(thread->pFlushSem);
            thread->bFlush = OMX_FALSE;
        }

        if(thread->bRunOnce){
            pthread_cond_wait(&thread->sCond, (pthread_mutex_t *)thread->sMutex);
        }else{
            while(!thread->bRunning){
                pthread_cond_wait(&thread->sCond, (pthread_mutex_t *)thread->sMutex);
            }
        }
        
        if(thread->bFlush){
            fsl_osal_sem_post(thread->pFlushSem);
            thread->bFlush = OMX_FALSE;
        }

        fsl_osal_mutex_unlock(thread->sMutex);
        
        if(thread->bStop)
            break;
        if(thread->handler != NULL)
            thread->handler(thread->parent);

    }

    return NULL;
}

OMX_ERRORTYPE VThread::create(void * pMe,OMX_BOOL runOnce,eventHandler vHandler)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&sMutex, fsl_osal_mutex_normal)) {
        return OMX_ErrorInsufficientResources;
    }
    if(0!= pthread_cond_init(&sCond, NULL)) {
        return OMX_ErrorInsufficientResources;
    }

    bRunning = OMX_FALSE;
    parent = pMe;
    handler = vHandler;
    bStop = OMX_FALSE;
    bRunOnce = runOnce;
    //bEnableWait = OMX_TRUE;
    bFlush = OMX_FALSE;

    if(E_FSL_OSAL_SUCCESS != fsl_osal_sem_init(&pFlushSem, 0, 0)){
        ret = OMX_ErrorUndefined;
        return ret;
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_thread_create(&pThreadId, NULL, EventThreadFunc, this)) {
        ret = OMX_ErrorInsufficientResources;
        goto err;
    }

    return ret;
err:
    destroy();
    return ret;

}
OMX_ERRORTYPE VThread::start()
{
    if(!bRunning){
        bRunning = OMX_TRUE;
    }
    fsl_osal_mutex_lock(sMutex);
    pthread_cond_signal(&sCond);
    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE VThread::pause()
{
    if(bRunning){
        bRunning = OMX_FALSE;
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE VThread::run_once()
{
    fsl_osal_mutex_lock(sMutex);
    pthread_cond_signal(&sCond);
    fsl_osal_mutex_unlock(sMutex);
    return OMX_ErrorNone;
}
void VThread::destroy()
{
    bStop = OMX_TRUE;
    handler = NULL;

    if(bRunOnce)
        run_once();
    else
        start();

    fsl_osal_thread_destroy(pThreadId);
    pThreadId = NULL;
    fsl_osal_mutex_destroy(sMutex);

    if(pFlushSem){
        fsl_osal_sem_destroy(pFlushSem);
        pFlushSem = NULL;
    }

    parent = NULL;
}

OMX_ERRORTYPE VThread::flush()
{
    fsl_osal_mutex_lock(sMutex);
    bFlush = OMX_TRUE;
    pthread_cond_signal(&sCond);
    fsl_osal_mutex_unlock(sMutex);
    fsl_osal_sem_wait(pFlushSem);

    return OMX_ErrorNone;
}