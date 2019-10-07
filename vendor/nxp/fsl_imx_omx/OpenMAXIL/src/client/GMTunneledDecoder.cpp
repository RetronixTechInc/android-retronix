/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */


#include "GMTunneledDecoder.h"
#include "GMTunneledDecoderWrapper.h"

#ifdef ANDROID_BUILD
#if defined(USE_GPU)
#include "GPUMemory.h"
#endif
#include "PlatformResourceMgrItf.h"
#endif

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif

#define AUDIO_CLOCK_PORT 0
#define VIDEO_CLOCK_PORT 1
#define FILTER_INPUT_PORT 0
#define FILTER_OUT_PORT 1
#define GMTDEC_QUIT_EVENT ((OMX_EVENTTYPE)(OMX_EventMax - 1))
#define CHECKRET(x) if((x) != OMX_ErrorNone) {return x;}

#define DEFAULT_VIDEO_RENDER "video_render.ipulib"
#define OPTIONAL_VIDEO_RENDER "video_render.mx8mqfb"

static OMX_ERRORTYPE GMTDecGetVersion(
        OMX_HANDLETYPE handle,
        OMX_STRING pComponentName,
        OMX_VERSIONTYPE* pComponentVersion,
        OMX_VERSIONTYPE* pSpecVersion,
        OMX_UUIDTYPE* pComponentUUID)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    return OMX_GetComponentVersion(pDec->coreDecHandle, pComponentName, pComponentVersion, pSpecVersion, pComponentUUID);
}

static OMX_ERRORTYPE GMTDecGetState(OMX_HANDLETYPE handle, OMX_STATETYPE* pState)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    return OMX_GetState(pDec->coreDecHandle, pState);
}


static OMX_ERRORTYPE GMTDecSendCommand(
        OMX_HANDLETYPE handle,
        OMX_COMMANDTYPE Cmd,
        OMX_U32 nParam1,
        OMX_PTR pCmdData)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("GMTDecSendCommand Cmd=%d, nParam1=%d, pCmdData=%p\n", Cmd, nParam1, pCmdData);

    switch(Cmd) {
        case OMX_CommandStateSet:
            {
                OMX_STATETYPE state;
                OMX_GetState(pDec->coreDecHandle, &state);
                if (nParam1 == OMX_StateIdle) {
                    if (state == OMX_StateLoaded) {
                        ret = pDec->Prepare();
                    }
                    else if (state == OMX_StatePause || state == OMX_StateExecuting)
                        ret = pDec->Stop(); // downward
                    else
                        ret = OMX_ErrorBadParameter;
                }
                else if (nParam1 == OMX_StateExecuting)
                    ret = pDec->Start();
                else if (nParam1 == OMX_StatePause)
                    ret = pDec->Pause();
                else if (nParam1 == OMX_StateLoaded)
                    ret = pDec->Unprepare();
            }
            break;
        case OMX_CommandFlush:
            {
                ret = pDec->Flush(nParam1);
            }
            break;
        case OMX_CommandPortDisable:
            {
                ret = pDec->PortEnable(nParam1, OMX_FALSE);
            }
            break;
        case OMX_CommandPortEnable:
            {
                ret = pDec->PortEnable(nParam1, OMX_TRUE);
            }
            break;
        default:
            ret = OMX_SendCommand(pDec->coreDecHandle, Cmd, nParam1, pCmdData);
            break;

    }
    return ret;
}

static OMX_ERRORTYPE GMTDecGetConfig(OMX_HANDLETYPE handle, OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    return OMX_GetConfig(pDec->coreDecHandle, nParamIndex, pStructure);
}

static OMX_ERRORTYPE GMTDecSetConfig(OMX_HANDLETYPE handle, OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch((int)nParamIndex) {
        case OMX_IndexConfigVideoMediaTime:
            {
                // Update clock media time(scale) to sync a/v
                OMX_CONFIG_VIDEO_MEDIA_TIME *pInfo = (OMX_CONFIG_VIDEO_MEDIA_TIME*)pStructure;
                pDec->UpdateClockMediaTime(pInfo);
                ret = OMX_SetConfig(pDec->coreDecHandle, nParamIndex, pStructure);
            }
            break;
        case OMX_IndexParamConfigureVideoTunnelMode:
            {
                OMX_PARAM_VIDEO_TUNNELED_MODE *pMode;
                pMode = (OMX_PARAM_VIDEO_TUNNELED_MODE *)pStructure;
                ret = pDec->ConfigTunneledMode(pMode);
            }
            break;
        default:
            ret = OMX_SetConfig(pDec->coreDecHandle, nParamIndex, pStructure);
            break;
    }

    return ret;
}

static OMX_ERRORTYPE GMTDecTunnelRequest(
        OMX_HANDLETYPE handle,
        OMX_U32 nPortIndex,
        OMX_HANDLETYPE hTunneledComp,
        OMX_U32 nTunneledPort,
        OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    return OMX_ErrorNotImplemented; //GMTunneledDecoder doesn't support TunnelRequest yet.
}

static OMX_ERRORTYPE GMTDecUseBuffer(
        OMX_HANDLETYPE handle,
        OMX_BUFFERHEADERTYPE** ppBuffer,
        OMX_U32 nPortIndex,
        OMX_PTR pAppPrivate,
        OMX_U32 nSizeBytes,
        OMX_U8* pBuffer)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (nPortIndex != FILTER_INPUT_PORT) {
        LOG_ERROR("not support out port to call UseBuffer");
        return OMX_ErrorBadPortIndex;
    }

    return pDec->AddBuffer(ppBuffer, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
}

static OMX_ERRORTYPE GMTDecAllocateBuffer(
        OMX_HANDLETYPE handle,
        OMX_BUFFERHEADERTYPE** ppBuffer,
        OMX_U32 nPortIndex,
        OMX_PTR pAppPrivate,
        OMX_U32 nSizeBytes)
{
    return OMX_ErrorNotImplemented; //GMTunneledDecoder doesn't support allocate buffer yet.
}

static OMX_ERRORTYPE GMTDecEmptyThisBuffer(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE* pBuffer)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    OMX_BUFFERHEADERTYPE *pBufferHdr;

    pBufferHdr = pDec->MapBufferHdr(pBuffer, OMX_FALSE/*downward*/);

    return OMX_EmptyThisBuffer(pDec->coreDecHandle, pBufferHdr);
}


static OMX_ERRORTYPE GMTDecFillThisBuffer(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE* pBuffer)
{
    return OMX_ErrorNotImplemented; //GMTunneledDecoder doesn't support FillThisBuffer yet.
}


static OMX_ERRORTYPE GMTDecFreeBuffer(OMX_HANDLETYPE handle, OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBuffer)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;

    // Free bufferHdr that we allocated here in UseBuffer, the another bufferHdr will free in GM component.
    return pDec->FreeBuffer(pBuffer);
}

static OMX_ERRORTYPE GMTDecSetCallbacks(OMX_HANDLETYPE handle, OMX_CALLBACKTYPE* pCbs, OMX_PTR pAppData)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    return pDec->coreDecHandle->SetCallbacks(pDec->coreDecHandle, pCbs, pAppData);
}

static OMX_ERRORTYPE GMTDecComponentDeInit(OMX_HANDLETYPE handle)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = pDec->DeInit();
    delete pDec;

    return ret;
}

static OMX_ERRORTYPE GMTDecUseEGLImage(
    OMX_HANDLETYPE handle,
    OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_U32 nPortIndex,
    OMX_PTR pAppPrivate,
    void *eglImage)
{
    return OMX_ErrorNotImplemented; //GMTunneledDecoder doesn't support UseEGLImage yet.
}


static OMX_ERRORTYPE GMTDecComponentRoleEnum(OMX_HANDLETYPE handle, OMX_U8 *cRole, OMX_U32 nIndex)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder *)handle;
    return pDec->coreDecHandle->ComponentRoleEnum(pDec->coreDecHandle, cRole, nIndex);
}



static OMX_ERRORTYPE GetBufferCallback (
        OMX_HANDLETYPE hComponent,
        OMX_U32 nPortIndex,
        OMX_PTR *pBuffer,
        GMBUFFERINFO *pInfo,
        OMX_PTR pAppData)
{
    GMTunneledDecoder *pGMDec = NULL;

    pGMDec = (GMTunneledDecoder*)pAppData;

    if (nPortIndex != FILTER_INPUT_PORT) {
        LOG_ERROR("not support out port to call GetBufferCallback\n");
        return OMX_ErrorBadPortIndex;
    }

    return pGMDec->GetBuffer((OMX_U8**)pBuffer);
}

static OMX_ERRORTYPE FreeBufferCallback(
        OMX_HANDLETYPE hComponent,
        OMX_U32 nPortIndex,
        OMX_PTR pBuffer,
        GMBUFFERINFO *pInfo,
        OMX_PTR pAppData)
{
    GMTunneledDecoder *pGMDec = NULL;

    pGMDec = (GMTunneledDecoder*)pAppData;

    if (nPortIndex != FILTER_INPUT_PORT) {
        LOG_ERROR("not support out port to call FreeBufferCallback\n");
        return OMX_ErrorBadPortIndex;
    }

    return OMX_ErrorNone;
}

/* Client allocate buffer callbacks */
static GM_BUFFERCB gBufferCb = {
    GetBufferCallback,
    FreeBufferCallback,
};


static OMX_ERRORTYPE SysEventCallback(
        GMComponent *pComponent,
        OMX_PTR pAppData,
        OMX_EVENTTYPE eEvent,
        OMX_U32 nData1,
        OMX_U32 nData2,
        OMX_PTR pEventData)
{
    GMTunneledDecoder *pGMDecoder = NULL;
    GMTDEC_SYSEVENT SysEvent;

    pGMDecoder = (GMTunneledDecoder*)pAppData;

    SysEvent.pComponent = pComponent;
    SysEvent.eEvent = eEvent;
    SysEvent.nData1 = nData1;
    SysEvent.nData2 = nData2;
    SysEvent.pEventData = pEventData;

    pGMDecoder->AddSysEvent(&SysEvent);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE EmptyDoneCallback(
        GMComponent *pComponent,
        OMX_PTR pAppData,
        OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    GMTunneledDecoder *pGMDecoder = NULL;

    pGMDecoder = (GMTunneledDecoder*)pAppData;

    return pGMDecoder->CbEmptyDone(pComponent, pBufferHdr);
}


static OMX_PTR SysEventThreadFunc(fsl_osal_ptr arg)
{
    GMTunneledDecoder *pDec = (GMTunneledDecoder*)arg;
    GMTDEC_SYSEVENT SysEvent;

    while(1) {
        pDec->GetSysEvent(&SysEvent);
        if(SysEvent.eEvent == GMTDEC_QUIT_EVENT)
            break;
        pDec->SysEventHandler(&SysEvent);
    }

    return NULL;
}

OMX_ERRORTYPE GMTunneledDecoder::SysEventHandler(GMTDEC_SYSEVENT *pSysEvent)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    GMComponent *pComponent = pSysEvent->pComponent;
    OMX_EVENTTYPE eEvent = pSysEvent->eEvent;
    OMX_U32 nData1 = pSysEvent->nData1;
    OMX_U32 nData2 = pSysEvent->nData2;
    OMX_PTR pEventData = pSysEvent->pEventData;

    switch((int)eEvent) {
        case OMX_EventBufferFlag:
            {
                if(nData2 == OMX_BUFFERFLAG_EOS) {
                    if(pComponent == pVideoRender) {
                        LOG_DEBUG("GMTunneledDecoder get Video EOS event.\n");
                        sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, eEvent, nData1, nData2, NULL);
                    }
                }
            }
            break;
        case OMX_EventPortSettingsChanged:
            {
                if (pComponent == pVideoDecoder && 0 == nData2) {
                    sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, eEvent, nData1, nData2, NULL);
                    pVideoDecoder->DynamicPortSettingChange(FILTER_OUT_PORT);
                }
            }
            break;
        default:
            break;
    }
    return ret;
}


GMTunneledDecoder::GMTunneledDecoder(OMX_HANDLETYPE handle)
{
    pClock = NULL;
    pVideoRender = NULL;
    pVideoDecoder = NULL;
    coreDecHandle = (OMX_COMPONENTTYPE*)handle;
    nInBufferTotalCount = 0;
    nInBufferActualCount = 0;
    Pipeline = NULL;
    SysEventThread = NULL;
    SysEventQueue = NULL;
    eState = GMTDEC_STATE_INVALID;
    fsl_osal_memset(&sAppCallback, 0, sizeof(GMTDEC_APPCALLBACK));
    fsl_osal_memset(&sInBufferHdrManager, 0, sizeof(GMTDEC_BUFFERHDR_MANAGER));
}

GMTunneledDecoder::~GMTunneledDecoder()
{
    LOG_DEBUG("~GMTunneledDecoder\n");
}


OMX_ERRORTYPE GMTunneledDecoder::Init()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    Pipeline = FSL_NEW(List<GMComponent>, ());
    if(Pipeline == NULL)
        return OMX_ErrorInsufficientResources;

    sInBufferHdrManager.srcBufferHdrList = FSL_NEW(List<OMX_BUFFERHEADERTYPE>, ());
    sInBufferHdrManager.dstBufferHdrList = FSL_NEW(List<OMX_BUFFERHEADERTYPE>, ());
    if(sInBufferHdrManager.srcBufferHdrList == NULL || sInBufferHdrManager.dstBufferHdrList == NULL)
        return OMX_ErrorInsufficientResources;

    SysEventQueue = FSL_NEW(Queue, ());
    if(SysEventQueue == NULL)
        return OMX_ErrorInsufficientResources;

    if(QUEUE_SUCCESS != SysEventQueue->Create(128, sizeof(GMTDEC_SYSEVENT), E_FSL_OSAL_TRUE)) {
        return OMX_ErrorInsufficientResources;
    }

    if(E_FSL_OSAL_SUCCESS != fsl_osal_thread_create(&SysEventThread, NULL, SysEventThreadFunc, this)) {
        return OMX_ErrorInsufficientResources;
    }

    ret = LoadClock();
    CHECKRET(ret)

    ret = SetupVideoPipeline();
    CHECKRET(ret)

    OMX_CONFIG_CLOCK sClock;
    OMX_INIT_STRUCT(&sClock, OMX_CONFIG_CLOCK);
    sClock.hClock = pClock->hComponent;
    OMX_SetConfig(pVideoDecoder->hComponent, OMX_IndexConfigClock, &sClock);

    eState = GMTDEC_STATE_LOADED;
    LOG_DEBUG("GMTunneledDecoder Init ok\n");

    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::Prepare()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (eState == GMTDEC_STATE_LOADED) {
        pVideoDecoder->PortDisable(FILTER_INPUT_PORT);
        PipelineStateTransfer(OMX_StateIdle, OMX_TRUE);
        eState = GMTDEC_STATE_PREPARING;
        return OMX_ErrorNone;
    }

    OMX_U32 i;
    OMX_BUFFERHEADERTYPE *pBufferHdr, *pSrcBufferHdr;

    pVideoDecoder->PortEnable(FILTER_INPUT_PORT);

    // map GMComponent input bufferHdr to GMTunneledDecoder bufferHdr
    for (i = 0; i < nInBufferActualCount; i++) {
        ret = pVideoDecoder->GetBufferHdrOnPort(FILTER_INPUT_PORT, i, &pBufferHdr);
        if (ret != OMX_ErrorNone || NULL == pBufferHdr)
            return ret;

        pSrcBufferHdr = sInBufferHdrManager.srcBufferHdrList->GetNode(i);
        if (pSrcBufferHdr->pBuffer != pBufferHdr->pBuffer) {
            LOG_ERROR("GMTunneledDecoder input buffer address can't map\n");
            return OMX_ErrorUndefined;
        }

        sInBufferHdrManager.dstBufferHdrList->Add(pBufferHdr);
    }

    sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateIdle, NULL);
    eState = GMTDEC_STATE_PREPARED;

    return ret;
}


OMX_ERRORTYPE GMTunneledDecoder::Start()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch(eState) {
        case GMTDEC_STATE_RUNNING:
            {
                LOG_WARNING("GMTunneledDecoder already in running state\n");
            }
            break;
        case GMTDEC_STATE_PREPARED:
            {
                PipelineStateTransfer(OMX_StateExecuting, OMX_TRUE);
                AttachClock(VIDEO_CLOCK_PORT, pVideoRender, 1);
                pVideoDecoder->StartProcessNoWait(FILTER_OUT_PORT);
                eState = GMTDEC_STATE_RUNNING;
                sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateExecuting, NULL);
            }
            break;
        case GMTDEC_STATE_PAUSE:
            {
                ret = Resume();
            }
            break;
        default:
            {
                LOG_ERROR("GMTunneledDecoder start from wrong state %d\n", eState);
                ret = OMX_ErrorInvalidState;
            }
            break;
    }

    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::Pause()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_DEBUG("Pause");

    switch(eState) {
        case GMTDEC_STATE_PREPARED:
            {
                PipelineStateTransfer(OMX_StatePause, OMX_TRUE);
                AttachClock(VIDEO_CLOCK_PORT, pVideoRender, 1);
                pVideoDecoder->StartProcessNoWait(FILTER_OUT_PORT);
            }
            break;
        case GMTDEC_STATE_RUNNING:
            {
                PipelineStateTransfer(OMX_StatePause, OMX_FALSE);
            }
            break;
        case GMTDEC_STATE_PAUSE:
            {
                LOG_WARNING("GMTunneledDecoder already in pause state\n");
            }
            break;
        default:
            {
                LOG_ERROR("GMTunneledDecoder start from wrong state %d\n", eState);
                ret = OMX_ErrorInvalidState;
            }
            break;
    }

    if (ret == OMX_ErrorNone) {
        eState = GMTDEC_STATE_PAUSE;
        sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StatePause, NULL);
    }

    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::Resume()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_DEBUG("Resume");

    switch(eState) {
        case GMTDEC_STATE_PAUSE:
            {
                PipelineStateTransfer(OMX_StateExecuting, OMX_TRUE);
            }
            break;
        default:
            {
                LOG_ERROR("GMTunneledDecoder start from wrong state %d\n", eState);
                ret = OMX_ErrorInvalidState;
            }
            break;
    }

    if (ret == OMX_ErrorNone) {
        eState = GMTDEC_STATE_RUNNING;
        sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    }

    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::Stop()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TIME_CONFIG_CLOCKSTATETYPE ClockState;

    PipelineStateTransfer(OMX_StateIdle, OMX_FALSE);
    sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateIdle, NULL);

    OMX_INIT_STRUCT(&ClockState, OMX_TIME_CONFIG_CLOCKSTATETYPE);
    ClockState.eState = OMX_TIME_ClockStateStopped;
    OMX_SetConfig(pClock->hComponent, OMX_IndexConfigTimeClockState, &ClockState);

    eState = GMTDEC_STATE_STOP;

    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::Unprepare()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    // Don't transfer pipleline to Loaded state until all client buffers are free.
    if (sInBufferHdrManager.srcBufferHdrList->GetNodeCnt() > 0)
        return ret;

    DeAttachClock(VIDEO_CLOCK_PORT, pVideoRender, 1);
    PipelineStateTransfer(OMX_StateLoaded, OMX_FALSE);
    sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    eState = GMTDEC_STATE_LOADED;

    return ret;
}


OMX_ERRORTYPE GMTunneledDecoder::DeInit()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_CONFIG_CLOCK sClock;
    OMX_U32 i, componentNum;
    GMComponent *pComponent;
    LOG_DEBUG("DeInit");

    OMX_INIT_STRUCT(&sClock, OMX_CONFIG_CLOCK);
    sClock.hClock = NULL;
    if (pVideoDecoder == NULL)
        return ret;
    OMX_SetConfig(pVideoDecoder->hComponent, OMX_IndexConfigClock, &sClock);

    // Remove pipeline
    if (Pipeline != NULL) {
        componentNum = Pipeline->GetNodeCnt();
        for(i = 0; i < componentNum; i++) {
            pComponent = Pipeline->GetNode(0);
            Pipeline->Remove(pComponent);
            pComponent->UnLoad();
            FSL_DELETE(pComponent);
            pComponent = NULL;
        }
        FSL_DELETE(Pipeline);
    }
    FSL_DELETE(sInBufferHdrManager.srcBufferHdrList);
    FSL_DELETE(sInBufferHdrManager.dstBufferHdrList);

    if(SysEventThread != NULL) {
        GMTDEC_SYSEVENT SysEvent;
        SysEvent.eEvent = GMTDEC_QUIT_EVENT;
        AddSysEvent(&SysEvent);
        fsl_osal_thread_destroy(SysEventThread);
    }

    if(SysEventQueue != NULL) {
        SysEventQueue->Free();
        FSL_DELETE(SysEventQueue);
    }

    eState = GMTDEC_STATE_INVALID;

    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::Flush(OMX_U32 nPortIndex)
{
    OMX_S32 i;
    OMX_TIME_CONFIG_CLOCKSTATETYPE ClockState;

    //stop clock
    OMX_INIT_STRUCT(&ClockState, OMX_TIME_CONFIG_CLOCKSTATETYPE);
    OMX_GetConfig(pClock->hComponent, OMX_IndexConfigTimeClockState, &ClockState);
    ClockState.eState = OMX_TIME_ClockStateStopped;
    OMX_SetConfig(pClock->hComponent, OMX_IndexConfigTimeClockState, &ClockState);

    if (nPortIndex == FILTER_INPUT_PORT || nPortIndex == OMX_ALL) {
        pVideoDecoder->PortFlush(FILTER_INPUT_PORT);
        pVideoRender->PortFlush(FILTER_INPUT_PORT);
        sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandFlush, FILTER_INPUT_PORT, NULL);
    }

    if (nPortIndex == FILTER_OUT_PORT || nPortIndex == OMX_ALL) {
        pVideoDecoder->PortFlush(FILTER_OUT_PORT);
        pVideoRender->PortFlush(FILTER_OUT_PORT);
        sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandFlush, FILTER_OUT_PORT, NULL);
    }

    //start clock with WaitingForStartTime
    ClockState.eState = OMX_TIME_ClockStateWaitingForStartTime;
    ClockState.nWaitMask = 1 << VIDEO_CLOCK_PORT;
    OMX_SetConfig(pClock->hComponent, OMX_IndexConfigTimeClockState, &ClockState);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMTunneledDecoder::PortEnable(OMX_U32 nPortIndex, OMX_BOOL bEnable)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (nPortIndex != FILTER_OUT_PORT) {
        LOG_ERROR("Only allow enable/disable port on output ports\n");
        return OMX_ErrorBadParameter;
    }

    if (bEnable)
        ret = DoPortEnable(nPortIndex);
    else
        ret = DoPortDisable(nPortIndex);

    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::CbEmptyDone(
        GMComponent *pComponent,
        OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    if (NULL == pBufferHdr || pComponent != pVideoDecoder)
        return OMX_ErrorBadParameter;

    OMX_BUFFERHEADERTYPE *pMapBufferHdr;

    pMapBufferHdr = MapBufferHdr(pBufferHdr, OMX_TRUE /*upward*/);

    return sAppCallback.pCallbacks->EmptyBufferDone(coreDecHandle, sAppCallback.pAppData, pMapBufferHdr);
}

OMX_ERRORTYPE GMTunneledDecoder::AddSysEvent(
        GMTDEC_SYSEVENT *pSysEvent)
{
    SysEventQueue->Add((fsl_osal_ptr)pSysEvent);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMTunneledDecoder::GetSysEvent(
        GMTDEC_SYSEVENT *pSysEvent)
{
    SysEventQueue->Get((fsl_osal_ptr)pSysEvent);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMTunneledDecoder::SetAppCallbacks(OMX_CALLBACKTYPE *pCb, OMX_PTR pData)
{
    sAppCallback.pCallbacks = pCb;
    sAppCallback.pAppData = pData;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMTunneledDecoder::AddBuffer(
        OMX_BUFFERHEADERTYPE** ppBuffer,
        OMX_U32 nPortIndex,
        OMX_PTR pAppPrivate,
        OMX_U32 nSizeBytes,
        OMX_U8* pBuffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHdr;

    pBufferHdr = (OMX_BUFFERHEADERTYPE *)FSL_MALLOC(sizeof(OMX_BUFFERHEADERTYPE));
    if(pBufferHdr == NULL)
        return OMX_ErrorInsufficientResources;

    fsl_osal_memset(pBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));
    OMX_INIT_STRUCT(pBufferHdr, OMX_BUFFERHEADERTYPE);

    *ppBuffer = pBufferHdr;
    pBufferHdr->pBuffer = pBuffer;
    pBufferHdr->nInputPortIndex = nPortIndex;
    pBufferHdr->pAppPrivate = pAppPrivate;
    pBufferHdr->nAllocLen = nSizeBytes;

    sInBufferHdrManager.srcBufferHdrList->Add(pBufferHdr);
    nInBufferActualCount++;

    if (nInBufferActualCount == nInBufferTotalCount)
        ret = Prepare();

    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::GetBuffer(OMX_U8** ppBuffer)
{
    OMX_U32 index = sInBufferHdrManager.nUsedNum;
    OMX_BUFFERHEADERTYPE *pBufferHdr;

    if (sInBufferHdrManager.srcBufferHdrList->GetNodeCnt() == index) {
        *ppBuffer = NULL;
        return OMX_ErrorUndefined;
    }

    pBufferHdr = sInBufferHdrManager.srcBufferHdrList->GetNode(index);
    *ppBuffer = pBufferHdr->pBuffer;
    sInBufferHdrManager.nUsedNum++;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMTunneledDecoder::FreeBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 i, bufferNum;

    bufferNum = sInBufferHdrManager.srcBufferHdrList->GetNodeCnt();

    for(i = 0; i < bufferNum; i++) {
        if (sInBufferHdrManager.srcBufferHdrList->GetNode(i) == pBufferHdr) {
            sInBufferHdrManager.srcBufferHdrList->Remove(pBufferHdr);
            FSL_FREE(pBufferHdr);
            break;
        }
    }

    if (sInBufferHdrManager.srcBufferHdrList->GetNodeCnt() == 0) {
        nInBufferActualCount = 0;
        ret = Unprepare();
    }

    return ret;
}


// [in] upward: it is the map direction, true: from OMX map to IL client, false: from IL client map to OMX.
OMX_BUFFERHEADERTYPE* GMTunneledDecoder::MapBufferHdr(OMX_BUFFERHEADERTYPE *pBufferHdr, OMX_BOOL upward)
{
    OMX_U32 i, bufferNum;
    List<OMX_BUFFERHEADERTYPE> *pDstList;
    OMX_BUFFERHEADERTYPE *pDstBufferHdr;

    if (upward) {
        pDstList = sInBufferHdrManager.srcBufferHdrList;
    } else {
        pDstList = sInBufferHdrManager.dstBufferHdrList;
    }

    bufferNum = pDstList->GetNodeCnt();
    for(i = 0; i < bufferNum; i++) {
        pDstBufferHdr = pDstList->GetNode(i);
        if (pDstBufferHdr->pBuffer == pBufferHdr->pBuffer) {
            pDstBufferHdr->nFilledLen = pBufferHdr->nFilledLen;
            pDstBufferHdr->nFlags = pBufferHdr->nFlags;
            pDstBufferHdr->nTimeStamp = pBufferHdr->nTimeStamp;
            return pDstBufferHdr;
        }
    }
    return NULL;
}

OMX_ERRORTYPE GMTunneledDecoder::ConfigTunneledMode(OMX_PARAM_VIDEO_TUNNELED_MODE *pMode)
{
    // nAudioHwSync is not used for now.
    return OMX_SetConfig(pVideoRender->hComponent, OMX_IndexParamConfigureVideoTunnelMode, pMode->pSidebandWindow);
}


OMX_ERRORTYPE GMTunneledDecoder::UpdateClockMediaTime(OMX_CONFIG_VIDEO_MEDIA_TIME *pInfo)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (pInfo->nScale == 0) {
        pVideoRender->StateTransDownWard(OMX_StatePause);
        pClock->StateTransDownWard(OMX_StatePause);
        eState = GMTDEC_STATE_PAUSE;
    } else {
        OMX_TIME_CONFIG_TIMESTAMPTYPE sRefTime;
        OMX_TIME_CONFIG_SCALETYPE sScale;

        OMX_INIT_STRUCT(&sRefTime, OMX_TIME_CONFIG_TIMESTAMPTYPE);
        sRefTime.nTimestamp = pInfo->nTime;
        OMX_SetConfig(pClock->hComponent, OMX_IndexConfigTimeCurrentVideoReference, &sRefTime);
        ret = OMX_SetConfig(pClock->hComponent, OMX_IndexConfigVideoMediaTime, pInfo);

        OMX_INIT_STRUCT(&sScale, OMX_TIME_CONFIG_SCALETYPE);
        sScale.xScale = pInfo->nScale;
        OMX_SetConfig(pClock->hComponent, OMX_IndexConfigTimeScale, &sScale);

        if (eState == GMTDEC_STATE_PAUSE) {
            pVideoRender->StateTransUpWard(OMX_StateExecuting);
            pClock->StateTransUpWard(OMX_StateExecuting);
            eState = GMTDEC_STATE_RUNNING;
        }
    }
    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::LoadComponent(
        OMX_STRING role,
        OMX_HANDLETYPE handle,
        GMComponent **ppComponent)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    GMComponent *pComponent = NULL;

    *ppComponent = NULL;
    pComponent = FSL_NEW(GMComponent, ());
    if(pComponent == NULL)
        return OMX_ErrorInsufficientResources;

    if (handle) {
        ret = pComponent->LoadWithComponentHandle(handle, role);
    } else {
        ret = pComponent->Load(role);
    }

    if(ret != OMX_ErrorNone) {
        FSL_DELETE(pComponent);
        return ret;
    }

    *ppComponent = pComponent;
    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::ConnectComponent(
        GMComponent *pOutComp,
        OMX_U32 nOutPortIndex,
        GMComponent *pInComp,
        OMX_U32 nInPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = pOutComp->Link(nOutPortIndex,pInComp,nInPortIndex);
    CHECKRET(ret)

    ret = pInComp->Link(nInPortIndex, pOutComp, nOutPortIndex);
    if(ret != OMX_ErrorNone) {
        pOutComp->Link(nOutPortIndex, NULL, 0);
        return ret;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMTunneledDecoder::LoadClock()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TIME_CONFIG_CLOCKSTATETYPE ClockState;
    OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE RefClock;
    OMX_S32 i;

    ret = LoadComponent((OMX_STRING)"clocksrc", NULL, &pClock);
    CHECKRET(ret)

    for(i = 0; i < (OMX_S32)pClock->nPorts; i++)
        pClock->PortDisable(i);

    OMX_INIT_STRUCT(&ClockState, OMX_TIME_CONFIG_CLOCKSTATETYPE);
    ClockState.eState = OMX_TIME_ClockStateWaitingForStartTime;
    ClockState.nWaitMask |= 1 << VIDEO_CLOCK_PORT;
    OMX_SetConfig(pClock->hComponent, OMX_IndexConfigTimeClockState, &ClockState);

    OMX_INIT_STRUCT(&RefClock, OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE);
    RefClock.eClock = OMX_TIME_RefClockVideo;
    OMX_SetConfig(pClock->hComponent, OMX_IndexConfigTimeActiveRefClock, &RefClock);

    Pipeline->Add(pClock);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMTunneledDecoder::AttachClock(
        OMX_U32 nClockPortIndex,
        GMComponent *pComponent,
        OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = OMX_SetupTunnel(pClock->hComponent, nClockPortIndex, pComponent->hComponent, nPortIndex);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Can't attach clock to component %s in port %d\n", pComponent->name, nPortIndex);
        return ret;
    }

    pComponent->TunneledPortEnable(nPortIndex);
    pClock->TunneledPortEnable(nClockPortIndex);
    pClock->WaitTunneledPortEnable(nClockPortIndex);
    pComponent->WaitTunneledPortEnable(nPortIndex);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE GMTunneledDecoder::SetupVideoPipeline()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef1, sPortDef2;
    OMX_U8 videoRole[MAX_NAME_LEN];

    ret = GetVideoRole((OMX_STRING)videoRole);
    CHECKRET(ret)
    ret = LoadComponent((OMX_STRING)videoRole, (OMX_HANDLETYPE)coreDecHandle, &pVideoDecoder);
    CHECKRET(ret)
    ret = LoadComponent((OMX_STRING)DEFAULT_VIDEO_RENDER, NULL, &pVideoRender);
    if (ret) {
        ret = LoadComponent((OMX_STRING)OPTIONAL_VIDEO_RENDER, NULL, &pVideoRender);
        /* fb1 render delay dequeue one buffer each time, need to add vpu out buffer count to make sure vpu can work */
        pVideoDecoder->SetPortBufferNumberType(1, GM_CUMULATE);
    }
    CHECKRET(ret)

    // register callbacks. Render need send out EOS event, Decoder need call EmptDone, FreeBuffer and portSettingChanged.
    pVideoRender->RegistSysCallback(SysEventCallback, (OMX_PTR)this);
    pVideoDecoder->RegistSysCallback(SysEventCallback, (OMX_PTR)this);
    pVideoDecoder->RegistEmptyDoneCallback(EmptyDoneCallback, (OMX_PTR)this);
    pVideoDecoder->RegistBufferCallback(&gBufferCb, (OMX_PTR)this);

    OMX_INIT_STRUCT(&sPortDef1, OMX_PARAM_PORTDEFINITIONTYPE);
    OMX_INIT_STRUCT(&sPortDef2, OMX_PARAM_PORTDEFINITIONTYPE);

    sPortDef1.nPortIndex = FILTER_OUT_PORT;
    OMX_GetParameter(pVideoDecoder->hComponent, OMX_IndexParamPortDefinition, &sPortDef1);
    sPortDef2.nPortIndex = FILTER_INPUT_PORT;
    OMX_GetParameter(pVideoRender->hComponent, OMX_IndexParamPortDefinition, &sPortDef2);
    sPortDef2.format.video.eColorFormat = sPortDef1.format.video.eColorFormat;
    OMX_SetParameter(pVideoRender->hComponent, OMX_IndexParamPortDefinition, &sPortDef2);

    OMX_INIT_STRUCT(&sPortDef1, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef1.nPortIndex = FILTER_INPUT_PORT;
    OMX_GetParameter(pVideoDecoder->hComponent, OMX_IndexParamPortDefinition, &sPortDef1);
    nInBufferTotalCount = sPortDef1.nBufferCountActual;

    ret = ConnectComponent(pVideoDecoder, FILTER_OUT_PORT, pVideoRender, FILTER_INPUT_PORT);
    CHECKRET(ret)

    pVideoDecoder->SetPortBufferAllocateType(FILTER_INPUT_PORT, GM_CLIENT_ALLOC);

    Pipeline->Add(pVideoDecoder);
    Pipeline->Add(pVideoRender);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE GMTunneledDecoder::DoPortDisable(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = pVideoRender->PortDisable(FILTER_INPUT_PORT);
    CHECKRET(ret)
    ret = pVideoDecoder->PortDisable(FILTER_OUT_PORT);
    CHECKRET(ret)

    sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandPortDisable, nPortIndex, NULL);

    return ret;
}


OMX_ERRORTYPE GMTunneledDecoder::DoPortEnable(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    ret = pVideoDecoder->PortEnable(FILTER_OUT_PORT);
    CHECKRET(ret)
    ret = pVideoRender->PortEnable(FILTER_INPUT_PORT);
    CHECKRET(ret)

    // invoke OMX_FillThisBuffer manully after port disable & enable
    pVideoDecoder->StartProcessNoWait(FILTER_OUT_PORT);

    sAppCallback.pCallbacks->EventHandler(coreDecHandle, sAppCallback.pAppData, OMX_EventCmdComplete, OMX_CommandPortEnable, nPortIndex, NULL);
    return ret;
}

OMX_ERRORTYPE GMTunneledDecoder::PipelineStateTransfer(OMX_STATETYPE state, OMX_BOOL upward)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 i,componentNum;
    GMComponent *pComponent;

    componentNum = Pipeline->GetNodeCnt();

    for(i = 0; i < componentNum; i++) {
        pComponent = Pipeline->GetNode(i);
        if (upward)
            pComponent->StateTransUpWard(state);
        else
            pComponent->StateTransDownWard(state);
    }

    return ret;
}


OMX_ERRORTYPE GMTunneledDecoder::DeAttachClock(
        OMX_U32 nClockPortIndex,
        GMComponent *pComponent,
        OMX_U32 nPortIndex)
{
    pClock->TunneledPortDisable(nClockPortIndex);
    pComponent->TunneledPortDisable(nPortIndex);
    pComponent->WaitTunneledPortDisable(nPortIndex);
    pClock->WaitTunneledPortDisable(nClockPortIndex);

    OMX_SetupTunnel(pClock->hComponent, nClockPortIndex, NULL, 0);
    OMX_SetupTunnel(pComponent->hComponent, nPortIndex, NULL, 0);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE GMTunneledDecoder::GetVideoRole(OMX_STRING role)
{
    if (role == NULL)
        return OMX_ErrorUndefined;

    OMX_U8 videoCompName[MAX_NAME_LEN];
    OMX_VERSIONTYPE componentVersion, specVersion;
    OMX_UUIDTYPE componentUUID;

    OMX_GetComponentVersion(coreDecHandle, (OMX_STRING)videoCompName, &componentVersion, &specVersion, &componentUUID);

    OMX_STRING pRoleStart = fsl_osal_strstr((OMX_STRING)videoCompName, (OMX_STRING)"video_decoder");
    OMX_STRING pRoleEnd = fsl_osal_strstr((OMX_STRING)videoCompName, (OMX_STRING)".hw-based");
    if (pRoleStart && pRoleEnd) {
        OMX_U32 roleLen = pRoleEnd - pRoleStart;
        fsl_osal_strncpy(role, pRoleStart, roleLen);
    } else {
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}


GMTunneledDecoderWrapper * GMTunneledDecoderWrapperCreate(
            OMX_HANDLETYPE handle,
            OMX_CALLBACKTYPE *callbacks,
            OMX_PTR appData)
{
    if (NULL == callbacks || NULL == appData)
        return NULL;

    GMTunneledDecoderWrapper *pWrapper = new GMTunneledDecoderWrapper();
    GMTunneledDecoder *pDec = new GMTunneledDecoder(handle);

    if (!pWrapper || !pDec) {
        FSL_DELETE(pWrapper);
        FSL_DELETE(pDec);
        return NULL;
    }

    pDec->SetAppCallbacks(callbacks, appData);
    if (pDec->Init() != OMX_ErrorNone) {
        pDec->DeInit();
        FSL_DELETE(pDec);
        FSL_DELETE(pWrapper);
        return NULL;
    }

    pWrapper->pPrivateData = pDec;
    pWrapper->GetVersion = GMTDecGetVersion;
    pWrapper->GetState = GMTDecGetState;
    pWrapper->SendCommand = GMTDecSendCommand;
    pWrapper->GetConfig = GMTDecGetConfig;
    pWrapper->SetConfig = GMTDecSetConfig;
    pWrapper->TunnelRequest = GMTDecTunnelRequest;
    pWrapper->UseBuffer = GMTDecUseBuffer;
    pWrapper->AllocateBuffer = GMTDecAllocateBuffer;
    pWrapper->FreeBuffer = GMTDecFreeBuffer;
    pWrapper->EmptyThisBuffer = GMTDecEmptyThisBuffer;
    pWrapper->FillThisBuffer = GMTDecFillThisBuffer;
    pWrapper->SetCallbacks = GMTDecSetCallbacks;
    pWrapper->ComponentDeInit = GMTDecComponentDeInit;
    pWrapper->UseEGLImage = GMTDecUseEGLImage;
    pWrapper->ComponentRoleEnum = GMTDecComponentRoleEnum;

    return pWrapper;
}


