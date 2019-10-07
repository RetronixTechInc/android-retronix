/**
 *  Copyright (c) 2009-2013, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include <stdlib.h>
#include <sys/time.h>
#include "Clock.h"


#define WALLTIME2TICKS(tv) \
    ((OMX_S64)tv.sec * OMX_TICKS_PER_SECOND + tv.usec)

#define WALLTIMESUB(a, b) \
	WALLTIME2TICKS(a) - WALLTIME2TICKS(b)

Clock::Clock()
{
    fsl_osal_strcpy((fsl_osal_char*)name, "OMX.Freescale.std.clocksrc.sw-based");
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"clocksrc";
    bInContext = OMX_FALSE;
    nPorts = PORT_NUM;
    Scale = 1*Q16_SHIFT;
}

OMX_ERRORTYPE Clock::InitComponent()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_BUFFERSUPPLIERTYPE SupplierType;
    OMX_U32 i;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.eDir = OMX_DirOutput;
    sPortDef.eDomain = OMX_PortDomainOther;
    sPortDef.format.other.eFormat = OMX_OTHER_FormatTime;
    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_FALSE;
    sPortDef.nBufferCountMin = 1;
    sPortDef.nBufferCountActual = 5;
    sPortDef.nBufferSize = sizeof(OMX_TIME_MEDIATIMETYPE);

    SupplierType = OMX_BufferSupplyOutput;

    for(i=0; i<PORT_NUM; i++) {
        sPortDef.nPortIndex = i;
        ports[i]->SetPortDefinition(&sPortDef);
        ports[i]->SetSupplierType(SupplierType);
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&lock, fsl_osal_mutex_normal)) {
        LOG_ERROR("Create mutext for clock failed.\n");
        return OMX_ErrorInsufficientResources;
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_cond_create(&Cond)) {
        LOG_ERROR("Create condition variable for clock failed.\n");
        return OMX_ErrorInsufficientResources;
    }

    OMX_INIT_STRUCT(&sState, OMX_TIME_CONFIG_CLOCKSTATETYPE);
    sState.eState = CurState = OMX_TIME_ClockStateStopped;
    RefClock = OMX_TIME_RefClockNone;
    SegmentStartTime = -1L;
    bPaused = OMX_FALSE;
    OMX_INIT_STRUCT(&sPlaybackType, OMX_TIME_CONFIG_PLAYBACKTYPE);
    sPlaybackType.ePlayMode = NORMAL_MODE;
    sPlaybackType.pPrivateData = NULL;
    sPlaybackType.nPrivateDataSize = 0;
    sPlaybackType.xScale = Scale;

    return ret;
}

OMX_ERRORTYPE Clock::DeInitComponent()
{
    if(lock != NULL) {
        fsl_osal_mutex_destroy(lock);
        lock = NULL;
    }

    if(Cond != NULL) {
        fsl_osal_cond_destroy(Cond);
        Cond = NULL;
    }

    if(sPlaybackType.pPrivateData != NULL)
        FSL_FREE(sPlaybackType.pPrivateData);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::GetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexConfigTimeScale:
            {
                OMX_TIME_CONFIG_SCALETYPE *pScale = NULL;
                pScale = (OMX_TIME_CONFIG_SCALETYPE*)pStructure;
                OMX_CHECK_STRUCT(pScale, OMX_TIME_CONFIG_SCALETYPE, ret);
                if(ret != OMX_ErrorNone)
                    return ret;
                pScale->xScale = Scale;
            }
            break;
        case OMX_IndexConfigTimeClockState:
            {
                OMX_TIME_CONFIG_CLOCKSTATETYPE *pState = NULL;
                pState = (OMX_TIME_CONFIG_CLOCKSTATETYPE*)pStructure;
                OMX_CHECK_STRUCT(pState, OMX_TIME_CONFIG_CLOCKSTATETYPE, ret);
                if(ret != OMX_ErrorNone)
                    return ret;
                fsl_osal_memcpy(pState, &sState, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
            }
            break;
        case OMX_IndexConfigTimeActiveRefClock:
            {
                OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE *pRefClock = NULL;
                pRefClock = (OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE*)pStructure;
                OMX_CHECK_STRUCT(pRefClock, OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE, ret);
                if(ret != OMX_ErrorNone)
                    return ret;
                pRefClock->eClock = RefClock;
            }
            break;
        case OMX_IndexConfigTimeCurrentMediaTime:
            {
                OMX_TIME_CONFIG_TIMESTAMPTYPE *pCurMediaTime = NULL;
                OMX_TICKS CurMediaTime = 0;
                pCurMediaTime = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pStructure;
                OMX_CHECK_STRUCT(pCurMediaTime, OMX_TIME_CONFIG_TIMESTAMPTYPE, ret);
                if(ret != OMX_ErrorNone)
                    return ret;

                if(sState.eState != OMX_TIME_ClockStateRunning) {
                    pCurMediaTime->nTimestamp = 0;
                    break;
                }

                CurMediaAndWallTime(&CurMediaTime, NULL);
                pCurMediaTime->nTimestamp = CurMediaTime;
            }
            break;
        case OMX_IndexConfigTimeCurrentWallTime:
            {
                OMX_TIME_CONFIG_TIMESTAMPTYPE *pCurWallTime = NULL;
                fsl_osal_timeval tv;
                pCurWallTime = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pStructure;
                OMX_CHECK_STRUCT(pCurWallTime, OMX_TIME_CONFIG_TIMESTAMPTYPE, ret);
                if(ret != OMX_ErrorNone)
                    return ret;
                CurMediaAndWallTime(NULL, &tv);
                pCurWallTime->nTimestamp = WALLTIME2TICKS(tv);
            }
            break;
        case OMX_IndexConfigPlaybackRate:
            {
                OMX_TIME_CONFIG_PLAYBACKTYPE *pPlayback = NULL;
                pPlayback = (OMX_TIME_CONFIG_PLAYBACKTYPE *)pStructure;
                OMX_CHECK_STRUCT(pPlayback, OMX_TIME_CONFIG_PLAYBACKTYPE, ret);
                if(ret != OMX_ErrorNone)
                    return ret;
                fsl_osal_memcpy(pPlayback, &sPlaybackType, sizeof(OMX_TIME_CONFIG_PLAYBACKTYPE));
            }
            break;
        default :
            ret = OMX_ErrorUnsupportedIndex;
            return ret;
            break;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::SetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((int)nParamIndex) {
        case OMX_IndexConfigTimeScale:
            ret = SetTimeScale((OMX_TIME_CONFIG_SCALETYPE*)pStructure);
            break;
        case OMX_IndexConfigTimeClockState:
            ret = SetState((OMX_TIME_CONFIG_CLOCKSTATETYPE*)pStructure);
            break;
        case OMX_IndexConfigTimeActiveRefClock:
            ret = SetRefClock((OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE*)pStructure);
            break;
        case OMX_IndexConfigTimeCurrentAudioReference:
            ret = SetRefTime((OMX_TIME_CONFIG_TIMESTAMPTYPE*)pStructure, OMX_TIME_RefClockAudio);
            break;
        case OMX_IndexConfigTimeCurrentVideoReference:
            ret = SetRefTime((OMX_TIME_CONFIG_TIMESTAMPTYPE*)pStructure, OMX_TIME_RefClockVideo);
            break;
        case OMX_IndexConfigTimeMediaTimeRequest:
            ret = MediaTimeRequest((OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE*)pStructure);
            break;
        case OMX_IndexConfigTimeClientStartTime:
            ret = SetStartTime((OMX_TIME_CONFIG_TIMESTAMPTYPE*)pStructure);
            break;
        case OMX_IndexConfigTimeVideoLate:
            ret = SetVideoLate((OMX_TIME_CONFIG_TIMEVIDEOLATE*)pStructure);
            break;
        case OMX_IndexConfigPlaybackRate:
            ret = SetPlaybackRate((OMX_TIME_CONFIG_PLAYBACKTYPE*)pStructure);
            break;
        default :
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}

OMX_ERRORTYPE Clock::SetPlaybackRate(
    OMX_TIME_CONFIG_PLAYBACKTYPE *pRate)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TIME_CONFIG_SCALETYPE sScale;

    OMX_CHECK_STRUCT(pRate, OMX_TIME_CONFIG_PLAYBACKTYPE, ret);
    if(ret != OMX_ErrorNone)
        return ret;

    sPlaybackType.ePlayMode = pRate->ePlayMode;
    sPlaybackType.xScale = pRate->xScale;

    if(pRate->pPrivateData) {
        if(NULL == sPlaybackType.pPrivateData) {
            sPlaybackType.pPrivateData = FSL_MALLOC(pRate->nPrivateDataSize);
            if(NULL == sPlaybackType.pPrivateData)
                return OMX_ErrorInsufficientResources;
        }
        fsl_osal_memcpy(sPlaybackType.pPrivateData, pRate->pPrivateData, pRate->nPrivateDataSize);
        sPlaybackType.nPrivateDataSize = pRate->nPrivateDataSize;
    }

    OMX_INIT_STRUCT(&sScale, OMX_TIME_CONFIG_SCALETYPE);
    sScale.xScale = pRate->xScale;

    return SetTimeScale(&sScale);
}

OMX_ERRORTYPE Clock::SetTimeScale(
        OMX_TIME_CONFIG_SCALETYPE *pScale)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TIME_MEDIATIMETYPE UpdateType;
    OMX_U32 i;

    OMX_CHECK_STRUCT(pScale, OMX_TIME_CONFIG_SCALETYPE, ret);
    if(ret != OMX_ErrorNone)
        return ret;

    if(pScale->xScale == Scale) {
        LOG_INFO("Set same scale value:[%d:%d]\n", pScale->xScale, Scale);
        return OMX_ErrorNone;
    }

    fsl_osal_mutex_lock(lock);

    CurMediaAndWallTime(&MediaTimeBase, NULL);
    CurMediaAndWallTime(NULL, &WallTimeBase);

    Scale = pScale->xScale;

    OMX_INIT_STRUCT(&UpdateType, OMX_TIME_MEDIATIMETYPE);
    UpdateType.eUpdateType = OMX_TIME_UpdateScaleChanged;
    UpdateType.xScale = Scale;
    UpdateType.nClientPrivate = (unsigned long)&sPlaybackType;

    fsl_osal_mutex_unlock(lock);

    for(i=0; i<PORT_NUM; i++) {
        if(ports[i]->IsEnabled() == OMX_TRUE)
            MediaTimeUpdate(&UpdateType, i);
    }

    fsl_osal_cond_broadcast(Cond);


    return ret;
}

OMX_ERRORTYPE Clock::SetState(
        OMX_TIME_CONFIG_CLOCKSTATETYPE *pState)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TIME_CLOCKSTATE NewState;
    OMX_TIME_MEDIATIMETYPE UpdateType;
    OMX_U32 i;

    OMX_CHECK_STRUCT(pState, OMX_TIME_CONFIG_CLOCKSTATETYPE, ret);
    if(ret != OMX_ErrorNone)
        return ret;

    NewState = pState->eState;
    if(NewState == CurState) {
        LOG_INFO("Set same state to clock: [%d:%d]\n", pState->eState, CurState);
        return OMX_ErrorNone;
    }

    fsl_osal_mutex_lock(lock);

    switch (NewState)
    {
        case OMX_TIME_ClockStateRunning:
            if(CurState == OMX_TIME_ClockStateWaitingForStartTime) {
                LOG_ERROR("Clock can't switch from WaitForStartTime to Running directly.\n");
                ret = OMX_ErrorIncorrectStateTransition;
                goto err;
            }
            /* Stop -> Running */
            fsl_osal_memcpy(&sState, pState, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
            SegmentStartTime = sState.nStartTime;
            break;
        case OMX_TIME_ClockStateWaitingForStartTime:
            if(CurState == OMX_TIME_ClockStateRunning) {
                LOG_ERROR("Clock can't switch from Running to WaitForStartTime.\n");
                ret = OMX_ErrorIncorrectStateTransition;
                goto err;
            }
            /* Stop -> WaitForStartTime */
            fsl_osal_memcpy(&sState, pState, sizeof(OMX_TIME_CONFIG_CLOCKSTATETYPE));
            StartTimeWaitMask = sState.nWaitMask;
            break;
        case OMX_TIME_ClockStateStopped:
            /* Running->Stop / WaitForStartTime->Stop */
            sState.eState = OMX_TIME_ClockStateStopped;
            break;
        default :
            LOG_ERROR("Invalid clock state setting: %d\n", pState->eState);
            ret = OMX_ErrorIncorrectStateTransition;
            goto err;
    }

    CurState = sState.eState;
    OMX_INIT_STRUCT(&UpdateType, OMX_TIME_MEDIATIMETYPE);
    UpdateType.eUpdateType = OMX_TIME_UpdateClockStateChanged;
    UpdateType.eState = sState.eState;

    fsl_osal_mutex_unlock(lock);

    for(i=0; i<PORT_NUM; i++) {
        if(ports[i]->IsEnabled() == OMX_TRUE)
            MediaTimeUpdate(&UpdateType, i);
    }

    if(sState.eState == OMX_TIME_ClockStateStopped)
        fsl_osal_cond_broadcast(Cond);

    return ret;

err:
    fsl_osal_mutex_unlock(lock);
    return ret;
}

OMX_ERRORTYPE Clock::SetRefClock(
        OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE *pRefClock)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    OMX_CHECK_STRUCT(pRefClock, OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE, ret);
    if(ret != OMX_ErrorNone)
        return ret;


    LOG_DEBUG("%s,%d,Set reference clock %d, curr ref clock %d\n",__FUNCTION__,__LINE__, pRefClock->eClock, RefClock);
    if(RefClock == pRefClock->eClock) {
        LOG_INFO("Set same reference clock:[%d:%d]\n", pRefClock->eClock, RefClock);
        return OMX_ErrorNone;
    }

    fsl_osal_mutex_lock(lock);
    RefClock = pRefClock->eClock;
    fsl_osal_cond_broadcast(Cond);
    fsl_osal_mutex_unlock(lock);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::SetRefTime(
        OMX_TIME_CONFIG_TIMESTAMPTYPE *pTs,
        OMX_TIME_REFCLOCKTYPE eClock)
{
    if(eClock != RefClock)
        return OMX_ErrorNone;

    fsl_osal_mutex_lock(lock);
    CurMediaAndWallTime(NULL, &WallTimeBase);
    if(pTs->nTimestamp >= 0)
        MediaTimeBase = pTs->nTimestamp;
    fsl_osal_mutex_unlock(lock);
    LOG_DEBUG("%s,%d,Set reference time with ts %lld,from ref clock  %d, result wall time base %lld\n",__FUNCTION__,__LINE__,MediaTimeBase, RefClock,WallTimeBase);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::MediaTimeRequest(
        OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE *pRequst)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TIME_MEDIATIMETYPE UpdateType;
    OMX_TICKS TargetMediaTime, CurMediaTime;
    fsl_osal_timeval CurWallTime;
    OMX_S64 WaitTicks;

    if(CurState != OMX_TIME_ClockStateRunning) {
        LOG_WARNING("Clock is not running.\n");
        return OMX_ErrorIncorrectStateOperation;
    }

    OMX_CHECK_STRUCT(pRequst, OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE, ret);
    if(ret != OMX_ErrorNone)
        return ret;

    OMX_INIT_STRUCT(&UpdateType, OMX_TIME_MEDIATIMETYPE);
    UpdateType.eUpdateType = OMX_TIME_UpdateRequestFulfillment;
    UpdateType.nClientPrivate = (unsigned long) pRequst->pClientPrivate;

    if(sPlaybackType.ePlayMode != NORMAL_MODE) {

        WaitTicks = (OMX_S64)OMX_TICKS_PER_SECOND*Q16_SHIFT/Scale;
        WaitTicks = Scale > 0 ? WaitTicks : -WaitTicks;

        CurMediaAndWallTime(NULL, &CurWallTime);
        LOG_DEBUG("Scale %d, Cur Wall Time: %lld, Wait Time: %lld\n", Scale,CurWallTime, WaitTicks);
        fsl_osal_cond_timedwait(Cond, WaitTicks);
        /* Not drop any frames in trick mode */
        UpdateType.nMediaTimestamp = pRequst->nMediaTimestamp;
        UpdateType.nWallTimeAtMediaTime = WALLTIME2TICKS(CurWallTime);
    }
    else {
        TargetMediaTime = pRequst->nMediaTimestamp;
        CurMediaAndWallTime(&CurMediaTime, &CurWallTime);
        WaitTicks = (TargetMediaTime - pRequst->nOffset - CurMediaTime)*Q16_SHIFT/Scale;
        LOG_DEBUG("Target: %lld, Cur: %lld, Diff: %lld\n", TargetMediaTime, CurMediaTime, WaitTicks);
        if(WaitTicks > OMX_TICKS_PER_SECOND/4) {
            fsl_osal_cond_timedwait(Cond, OMX_TICKS_PER_SECOND/4);
            return OMX_ErrorNotReady;
        }
        if(WaitTicks > 0) {
            fsl_osal_cond_timedwait(Cond, WaitTicks);
            CurMediaAndWallTime(&CurMediaTime, &CurWallTime);
            WaitTicks = (TargetMediaTime - pRequst->nOffset - CurMediaTime)*Q16_SHIFT/Scale;
            if(WaitTicks > 20000)
                return OMX_ErrorNotReady;
            CurMediaTime = pRequst->nMediaTimestamp;
        }
        UpdateType.nMediaTimestamp = CurMediaTime;
        UpdateType.nWallTimeAtMediaTime = WALLTIME2TICKS(CurWallTime);
    }

    MediaTimeUpdate(&UpdateType, pRequst->nPortIndex);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::SetStartTime(
        OMX_TIME_CONFIG_TIMESTAMPTYPE *pStartTime)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TIME_MEDIATIMETYPE UpdateType;
    OMX_U32 nPortIndex;
    OMX_U32 i;

    if(CurState != OMX_TIME_ClockStateWaitingForStartTime)
        return OMX_ErrorIncorrectStateOperation;

    OMX_CHECK_STRUCT(pStartTime, OMX_TIME_CONFIG_TIMESTAMPTYPE, ret);
    if(ret != OMX_ErrorNone)
        return ret;

    nPortIndex = pStartTime->nPortIndex;
    if(nPortIndex >= nPorts) {
        LOG_ERROR("Bad port index[%d] when set start time to clock.\n", nPortIndex);
        return OMX_ErrorBadPortIndex;
    }

    fsl_osal_mutex_lock(lock);
    SegmentStartTime = MIN(SegmentStartTime, (OMX_U64)pStartTime->nTimestamp);
    LOG_DEBUG("%s,%d,set start time from port %d, ts %lld,SegmentStartTime %lld\n",__FUNCTION__,__LINE__,nPortIndex,pStartTime->nTimestamp,SegmentStartTime);
    StartTimeWaitMask &= ~(1 << nPortIndex);
    if(StartTimeWaitMask) {
        fsl_osal_mutex_unlock(lock);
        return OMX_ErrorNone;
    }

    /* start media clock now */
    CurState = OMX_TIME_ClockStateRunning;
    sState.eState = CurState;
    MediaTimeBase = SegmentStartTime;
    LOG_DEBUG("%s,%d,start new segment,set media time base to %lld\n",__FUNCTION__,__LINE__,MediaTimeBase);
    CurMediaAndWallTime(NULL, &WallTimeBase);

    OMX_INIT_STRUCT(&UpdateType, OMX_TIME_MEDIATIMETYPE);
    UpdateType.eUpdateType = OMX_TIME_UpdateClockStateChanged;
    UpdateType.eState = sState.eState;
    UpdateType.nMediaTimestamp = SegmentStartTime;
    SegmentStartTime = -1L;

    fsl_osal_mutex_unlock(lock);

    for(i=0; i<PORT_NUM; i++) {
        if(ports[i]->IsEnabled() == OMX_TRUE)
            MediaTimeUpdate(&UpdateType, i);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::SetVideoLate(
        OMX_TIME_CONFIG_TIMEVIDEOLATE *pVideoLate)
{
    OMX_TIME_MEDIATIMETYPE UpdateType;
    OMX_S32 i;

    OMX_INIT_STRUCT(&UpdateType, OMX_TIME_MEDIATIMETYPE);
    if(pVideoLate->bLate == OMX_TRUE) {
        UpdateType.eUpdateType = OMX_TIME_UpdateVideoLate;
        for(i=0; i<PORT_NUM; i++) {
            if(i != (OMX_S32)pVideoLate->nPortIndex && ports[i]->IsEnabled() == OMX_TRUE)
                MediaTimeUpdate(&UpdateType, i);
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::MediaTimeUpdate(
        OMX_TIME_MEDIATIMETYPE *pUpdateType,
        OMX_U32 nPortIndex)
{
    OMX_BUFFERHEADERTYPE *pBufferHdr = NULL;
    OMX_TIME_MEDIATIMETYPE *pUpdate = NULL;

    ports[nPortIndex]->GetBuffer(&pBufferHdr);
    pUpdate = (OMX_TIME_MEDIATIMETYPE*)pBufferHdr->pBuffer;
    fsl_osal_memcpy(pUpdate, pUpdateType, sizeof(OMX_TIME_MEDIATIMETYPE));
    if(OMX_ErrorNone != ports[nPortIndex]->SendBuffer(pBufferHdr))
        ports[nPortIndex]->AddBuffer(pBufferHdr);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE Clock::CurMediaAndWallTime(
        OMX_TICKS *pMediaTime,
        fsl_osal_timeval *pWallTime)
{
    fsl_osal_timeval tv;

    fsl_osal_systime(&tv);
    if(pWallTime != NULL) {
        pWallTime->sec = tv.sec;
        pWallTime->usec = tv.usec;
    }
    LOG_DEBUG("CurWallTime: [%ds : %dus]\n", tv.sec, tv.usec);

    if(pMediaTime != NULL) {
        if(bPaused != OMX_TRUE && NORMAL_MODE == sPlaybackType.ePlayMode)
        {
            OMX_S64 diff = (OMX_S64)WALLTIMESUB(tv, WallTimeBase);
            *pMediaTime = MediaTimeBase + diff*Scale/Q16_SHIFT;
        }
        else
            *pMediaTime = MediaTimeBase;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::ProcessDataBuffer()
{
    return OMX_ErrorNoMore;
}

OMX_ERRORTYPE Clock::ProcessClkBuffer()
{
    return OMX_ErrorNoMore;
}

OMX_ERRORTYPE Clock::DoExec2Pause()
{
    CurMediaAndWallTime(&MediaTimeBase, NULL);

    bPaused = OMX_TRUE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Clock::DoPause2Exec()
{
    CurMediaAndWallTime(NULL, &WallTimeBase);
    bPaused = OMX_FALSE;

    return OMX_ErrorNone;
}


/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE ClockInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        Clock *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(Clock, ());
        if(obj == NULL)
            return OMX_ErrorInsufficientResources;

        base = (ComponentBase*)obj;
        ret = base->ConstructComponent(pHandle);
        if(ret != OMX_ErrorNone)
            return ret;

        return ret;
    }
}

/* File EOF */
