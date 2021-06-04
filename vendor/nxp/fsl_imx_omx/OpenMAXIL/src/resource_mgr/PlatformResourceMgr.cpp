/**
 *  Copyright (c) 2009-2010, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "PlatformResourceMgr.h"
#include "PlatformResourceMgrItf.h"

OMX_ERRORTYPE PlatformResourceMgr::Init()
{
    PlatformDataList = NULL;
    PlatformDataList = FSL_NEW(List<PLATFORM_DATA>, ());
    if(PlatformDataList == NULL)
        return OMX_ErrorInsufficientResources;

    lock = NULL;
    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
        DeInit();
        return OMX_ErrorInsufficientResources;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE PlatformResourceMgr::DeInit()
{
    if(PlatformDataList != NULL)
        FSL_DELETE(PlatformDataList);

    if(lock != NULL)
        fsl_osal_mutex_destroy(lock);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE PlatformResourceMgr::AddHwBuffer(
        OMX_PTR pPhyiscAddr,
        OMX_PTR pVirtualAddr)
{
    PLATFORM_DATA *pData = NULL;

    pData = (PLATFORM_DATA*)FSL_MALLOC(sizeof(PLATFORM_DATA));
    if(pData == NULL)
        return OMX_ErrorInsufficientResources;

    pData->pVirtualAddr = pVirtualAddr;
    pData->pPhyiscAddr = pPhyiscAddr;
    pData->nFd = -1;
    pData->pFdAddr = NULL;

    fsl_osal_mutex_lock(lock);
    if(LIST_SUCCESS != PlatformDataList->Add(pData)) {
        FSL_FREE(pData);
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorUndefined;
    }
    fsl_osal_mutex_unlock(lock);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE PlatformResourceMgr::RemoveHwBuffer(
        OMX_PTR pVirtualAddr)
{
    PLATFORM_DATA *pData = NULL;

    fsl_osal_mutex_lock(lock);
    pData = (PLATFORM_DATA*) SearchData(pVirtualAddr);
    if(pData == NULL) {
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorUndefined;
    }
    PlatformDataList->Remove(pData);
    FSL_FREE(pData);
    fsl_osal_mutex_unlock(lock);

    return OMX_ErrorNone;
}

OMX_PTR PlatformResourceMgr::GetHwBuffer(
        OMX_PTR pVirtualAddr)
{
    PLATFORM_DATA *pData = NULL;
    OMX_PTR ptr = NULL;

    fsl_osal_mutex_lock(lock);
    pData = (PLATFORM_DATA*) SearchData(pVirtualAddr);
    if(pData == NULL) {
        fsl_osal_mutex_unlock(lock);
        return NULL;
    }
    ptr = pData->pPhyiscAddr;
    fsl_osal_mutex_unlock(lock);

    return ptr;
}

OMX_ERRORTYPE PlatformResourceMgr::GetFdAndAddr(
        OMX_PTR pVirtualAddr, OMX_S32 * fd, OMX_PTR *pFdAddr)
{
    PLATFORM_DATA *pData = NULL;
    if(fd == NULL || pFdAddr == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_mutex_lock(lock);
    pData = (PLATFORM_DATA*) SearchData(pVirtualAddr);
    if(pData == NULL) {
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorInsufficientResources;
    }
    *fd = pData->nFd;
    *pFdAddr = pData->pFdAddr;
    fsl_osal_mutex_unlock(lock);

    return OMX_ErrorNone;
}

OMX_PTR PlatformResourceMgr::SearchData(
        OMX_PTR pVirtualAddr)
{
    PLATFORM_DATA *pData = NULL;
    fsl_osal_u32 i = 0, cnt;

    cnt = PlatformDataList->GetNodeCnt();
    for(i=0; i<cnt; i++) {
        pData = PlatformDataList->GetNode(i);
        if(pData->pVirtualAddr == pVirtualAddr)
            break;
        pData = NULL;
    }

    return pData;
}
OMX_ERRORTYPE PlatformResourceMgr::ModifyFdAndAddr(OMX_PTR pVirtualAddr,OMX_S32 fd, OMX_PTR pAddr)
{
    PLATFORM_DATA *pData = NULL;
    if(fd == -1 || pAddr == NULL)
        return OMX_ErrorBadParameter;

    fsl_osal_mutex_lock(lock);
    pData = (PLATFORM_DATA*) SearchData(pVirtualAddr);
    if(pData == NULL) {
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorResourcesLost;
    }

    pData->nFd = fd;
    pData->pFdAddr = pAddr;
    
    fsl_osal_mutex_unlock(lock);
    return OMX_ErrorNone;
}

/**< C style functions to expose entry point for the shared library */
PlatformResourceMgr *gPlatformResMgr = NULL;

OMX_ERRORTYPE CreatePlatformResMgr()
{
    if(NULL == gPlatformResMgr) {
        gPlatformResMgr = FSL_NEW(PlatformResourceMgr, ());
        if(NULL == gPlatformResMgr)
            return OMX_ErrorInsufficientResources;
        gPlatformResMgr->Init();
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE DestroyPlatformResMgr()
{
    if(NULL != gPlatformResMgr) {
        gPlatformResMgr->DeInit();
        FSL_DELETE(gPlatformResMgr);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE AddHwBuffer(
        OMX_PTR pPhyiscAddr,
        OMX_PTR pVirtualAddr)
{
    if(gPlatformResMgr == NULL)
        return OMX_ErrorResourcesLost;

    return gPlatformResMgr->AddHwBuffer(pPhyiscAddr, pVirtualAddr);
}

OMX_ERRORTYPE ModifyFdAndAddr(
        OMX_PTR pVirtualAddr, OMX_S32 nfd, OMX_PTR pAddr)
{
    if(gPlatformResMgr == NULL)
        return OMX_ErrorResourcesLost;

    return gPlatformResMgr->ModifyFdAndAddr(pVirtualAddr, nfd, pAddr);
}

OMX_ERRORTYPE RemoveHwBuffer(
        OMX_PTR pVirtualAddr)
{
    if(gPlatformResMgr == NULL)
        return OMX_ErrorResourcesLost;

    return gPlatformResMgr->RemoveHwBuffer(pVirtualAddr);
}

OMX_ERRORTYPE GetHwBuffer(
        OMX_PTR pVirtualAddr,
        OMX_PTR *ppPhyiscAddr)
{
    if(gPlatformResMgr == NULL)
        return OMX_ErrorResourcesLost;

    *ppPhyiscAddr = gPlatformResMgr->GetHwBuffer(pVirtualAddr);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GetFdAndAddr(OMX_PTR pVirtualAddr, OMX_S32 *pfd,OMX_PTR *ppAddr)
{
    if(gPlatformResMgr == NULL)
        return OMX_ErrorResourcesLost;

    return gPlatformResMgr->GetFdAndAddr(pVirtualAddr,pfd,ppAddr);
}
/* File EOF */
