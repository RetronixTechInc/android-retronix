/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */

#include <HardwareAPI.h>
#include "GMComponent.h"
#include "Queue.h"


typedef struct {
    GMComponent *pComponent;
    OMX_EVENTTYPE eEvent;
    OMX_U32 nData1;
    OMX_U32 nData2;
    OMX_PTR pEventData;
}GMTDEC_SYSEVENT;


typedef struct
{
    OMX_PTR pAppData;
    OMX_CALLBACKTYPE *pCallbacks;
} GMTDEC_APPCALLBACK;


typedef struct
{
    List<OMX_BUFFERHEADERTYPE> *srcBufferHdrList;    // bufferHdr in IL client
    List<OMX_BUFFERHEADERTYPE> *dstBufferHdrList;    // bufferHdr in OMX component
    OMX_U32 nUsedNum;                                // number of buffers which are used by component.
}GMTDEC_BUFFERHDR_MANAGER;


typedef enum {
    GMTDEC_STATE_INVALID = 0,
    GMTDEC_STATE_LOADED,
    GMTDEC_STATE_PREPARING,
    GMTDEC_STATE_PREPARED,
    GMTDEC_STATE_RUNNING,
    GMTDEC_STATE_PAUSE,
    GMTDEC_STATE_STOP
}GMTDEC_STATE;


class GMTunneledDecoder {
    public:
        GMTunneledDecoder(OMX_HANDLETYPE handle);
        ~GMTunneledDecoder();
        OMX_ERRORTYPE Init();
        OMX_ERRORTYPE Prepare();
        OMX_ERRORTYPE Start();
        OMX_ERRORTYPE Pause();
        OMX_ERRORTYPE Resume();
        OMX_ERRORTYPE Unprepare();
        OMX_ERRORTYPE DeInit();
        OMX_ERRORTYPE Stop();
        OMX_ERRORTYPE Flush(OMX_U32 nPortIndex);
        OMX_ERRORTYPE PortEnable(OMX_U32 nPortIndex, OMX_BOOL bEnable);
        OMX_ERRORTYPE SysEventHandler(GMTDEC_SYSEVENT *pSysEvent);
        OMX_ERRORTYPE CbEmptyDone(GMComponent *pComponent, OMX_BUFFERHEADERTYPE *pBufferHdr);
        OMX_ERRORTYPE UpdateClockMediaTime(OMX_CONFIG_VIDEO_MEDIA_TIME *pInfo);
        OMX_ERRORTYPE AddSysEvent(GMTDEC_SYSEVENT *pSysEvent);
        OMX_ERRORTYPE GetSysEvent(GMTDEC_SYSEVENT *pSysEvent);
        OMX_ERRORTYPE SetAppCallbacks(OMX_CALLBACKTYPE *PCb, OMX_PTR pData);
        OMX_ERRORTYPE AddBuffer(OMX_BUFFERHEADERTYPE** ppBuffer, OMX_U32 nPortIndex, OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer);
        OMX_ERRORTYPE GetBuffer(OMX_U8** ppBuffer);
        OMX_ERRORTYPE FreeBuffer(OMX_BUFFERHEADERTYPE *pBufferHdr);
        OMX_BUFFERHEADERTYPE* MapBufferHdr(OMX_BUFFERHEADERTYPE *pBufferHdr, OMX_BOOL upward);
        OMX_ERRORTYPE ConfigTunneledMode(OMX_PARAM_VIDEO_TUNNELED_MODE *pMode);
        OMX_COMPONENTTYPE* coreDecHandle;

    private:
        OMX_ERRORTYPE LoadComponent(OMX_STRING role, OMX_HANDLETYPE handle, GMComponent **ppComponent);
        OMX_ERRORTYPE ConnectComponent(GMComponent *pOutComp, OMX_U32 nOutPortIndex, GMComponent *pInComp, OMX_U32 nInPortIndex);
        OMX_ERRORTYPE AttachClock(OMX_U32 nClockPortIndex, GMComponent *pComponent, OMX_U32 nPortIndex);
        OMX_ERRORTYPE DeAttachClock(OMX_U32 nClockPortIndex, GMComponent *pComponent, OMX_U32 nPortIndex);
        OMX_ERRORTYPE SetupVideoPipeline();
        OMX_ERRORTYPE LoadClock();
        OMX_ERRORTYPE DoLoaded2Idle();
        OMX_ERRORTYPE DoPortDisable(OMX_U32 nPortIndex);
        OMX_ERRORTYPE DoPortEnable(OMX_U32 nPortIndex);
        OMX_ERRORTYPE PipelineStateTransfer(OMX_STATETYPE state, OMX_BOOL upward);
        OMX_ERRORTYPE GetVideoRole(OMX_STRING role);

        GMComponent* pClock;
        GMComponent* pVideoDecoder;
        GMComponent* pVideoRender;
        GMTDEC_APPCALLBACK sAppCallback;
        OMX_U32 nInBufferTotalCount;
        OMX_U32 nInBufferActualCount;
        OMX_PTR SysEventThread;
        Queue *SysEventQueue;
        GMTDEC_STATE eState;
        List<GMComponent> *Pipeline;
        GMTDEC_BUFFERHDR_MANAGER sInBufferHdrManager;
};
