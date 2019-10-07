/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
#ifndef VTHREAD_H
#define VTHREAD_H
#include "fsl_osal.h"
#include "Mem.h"
#include "Log.h"
#include "ComponentBase.h"

typedef void* (*eventHandler)(void * pMe);

class VThread{
public:
    OMX_ERRORTYPE create(void * pMe,OMX_BOOL runOnce,eventHandler vHandler);
    OMX_ERRORTYPE start();
    OMX_ERRORTYPE pause();
    
    OMX_ERRORTYPE run_once();
    void destroy();
    friend fsl_osal_ptr EventThreadFunc(fsl_osal_ptr arg);
    OMX_ERRORTYPE flush();

private:
    void * parent;
    fsl_osal_ptr pThreadId;
    fsl_osal_mutex sMutex;
    pthread_cond_t sCond; 
    OMX_BOOL bRunning;
    OMX_BOOL bStop;
    OMX_BOOL bRunOnce;
    OMX_BOOL bFlush;

    fsl_osal_sem pFlushSem;
    eventHandler handler;
};
#endif

