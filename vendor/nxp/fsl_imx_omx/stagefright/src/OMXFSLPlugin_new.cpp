/**
 *  Copyright (C) 2011-2016 Freescale Semiconductor, Inc.
 *  Copyright 2017-2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */
//#define LOG_NDEBUG 0
#define LOG_TAG "OMXFSLPlugin"
#if (ANDROID_VERSION <= ICS)
    #include <media/stagefright/OMXPluginBase.h>
    #include <media/stagefright/HardwareAPI.h>
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
    #include <OMXPluginBase.h>
    #include <HardwareAPI.h>
    #include <media/hardware/MetadataBufferType.h>
#endif

#include <ui/GraphicBuffer.h>
#if (ANDROID_VERSION < ANDROID_O)
#include <gralloc_priv.h>
#else
#include "Memory.h"
#include "MemoryManager.h"
#endif
#include <graphics_ext.h>
#include <ui/Fence.h>
#include <ui/Rect.h>
#include <ui/GraphicBufferMapper.h>
#include <inttypes.h>

#include "OMX_Index.h"
#include "OMX_Implement.h"
#include "PlatformResourceMgrItf.h"
#include "GMTunneledDecoderWrapper.h"


#if (ANDROID_VERSION >= JELLY_BEAN_42)
#define LOGE ALOGE
#define LOGV ALOGV
#endif

#if (ANDROID_VERSION >= MARSH_MALLOW_600)
#include "media/openmax/OMX_IndexExt.h"
#include "media/openmax/OMX_AudioExt.h"
#endif

#include <binder/MemoryDealer.h>
#include <cutils/ashmem.h>
#include <cutils/native_handle.h>
#include <cutils/properties.h>
#define MAX_BUFFER_CNT (32)
#define kFenceTimeoutMs 1000  // keep align with IOMX.h

// flac definitions keep align with OMX_Audio.h and OMX_Index in frameworks/native/include/media/openmax
#define OMX_AUDIO_CodingAndroidFLAC 28
#define OMX_IndexParamAudioAndroidFlac 0x400001C

typedef struct OMX_AUDIO_PARAM_ANDROID_FLACTYPE {
    OMX_U32 nSize;            /**< size of the structure in bytes */
    OMX_VERSIONTYPE nVersion; /**< OMX specification version information */
    OMX_U32 nPortIndex;       /**< port that this structure applies to */
    OMX_U32 nChannels;        /**< Number of channels */
    OMX_U32 nSampleRate;      /**< Sampling rate of the source data.  Use 0 for
                                   unknown sampling rate. */
    OMX_U32 nCompressionLevel;/**< FLAC compression level, from 0 (fastest compression)
                                   to 8 (highest compression */
} OMX_AUDIO_PARAM_ANDROID_FLACTYPE;


namespace android {

class FSLOMXWrapper {
    public:
        FSLOMXWrapper(const char *name);
        OMX_COMPONENTTYPE *MakeWapper(OMX_HANDLETYPE pHandle);
        OMX_ERRORTYPE MakeTunneledWapper();
        OMX_COMPONENTTYPE *GetComponentHandle();
        GMTunneledDecoderWrapper *GetTunneledWrapper();
        OMX_ERRORTYPE BackupCallbacks(OMX_CALLBACKTYPE *pCb, OMX_PTR pAppData);
        OMX_BOOL IsTunneled();
        OMX_ERRORTYPE GetVersion(OMX_STRING pComponentName, OMX_VERSIONTYPE* pComponentVersion,
                                 OMX_VERSIONTYPE* pSpecVersion, OMX_UUIDTYPE* pComponentUUID) {
            return OMX_GetComponentVersion(
                    ComponentHandle, pComponentName, pComponentVersion, pSpecVersion, pComponentUUID);
        }
        OMX_ERRORTYPE SendCommand(OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData) {
            return OMX_SendCommand(ComponentHandle, Cmd, nParam1, pCmdData);
        }
        OMX_ERRORTYPE GetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE SetParameter(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE GetExtensionIndex(OMX_STRING cParameterName, OMX_INDEXTYPE* pIndexType);
        OMX_ERRORTYPE GetState(OMX_STATETYPE* pState) {
            return OMX_GetState(ComponentHandle, pState);
        }
        OMX_ERRORTYPE TunnelRequest(OMX_U32 nPortIndex, OMX_HANDLETYPE hTunneledComp,
                                    OMX_U32 nTunneledPort, OMX_TUNNELSETUPTYPE* pTunnelSetup) {
            return ComponentHandle->ComponentTunnelRequest(
                    ComponentHandle, nPortIndex, hTunneledComp, nTunneledPort, pTunnelSetup);
        }
        OMX_ERRORTYPE UseBuffer(OMX_BUFFERHEADERTYPE** ppBuffer, OMX_U32 nPortIndex,
                                OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer){
            return OMX_UseBuffer(ComponentHandle, ppBuffer, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
        }
        OMX_ERRORTYPE AllocateBuffer(OMX_BUFFERHEADERTYPE** ppBuffer, OMX_U32 nPortIndex,
                                     OMX_PTR pAppPrivate, OMX_U32 nSizeBytes);
        OMX_ERRORTYPE FreeBuffer(OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBuffer);
        OMX_ERRORTYPE EmptyThisBuffer(OMX_BUFFERHEADERTYPE* pBuffer);
        OMX_ERRORTYPE FillThisBuffer(OMX_BUFFERHEADERTYPE* pBuffer);
        OMX_ERRORTYPE SetCallbacks(OMX_CALLBACKTYPE* pCbs, OMX_PTR pAppData) {
            return ComponentHandle->SetCallbacks(ComponentHandle, pCbs, pAppData);
        }
        OMX_ERRORTYPE ComponentDeInit() {
            return ComponentHandle->ComponentDeInit(ComponentHandle);
        }
        OMX_ERRORTYPE UseEGLImage(OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex,
                                  OMX_PTR pAppPrivate, void *eglImage) {
            return OMX_UseEGLImage(ComponentHandle, ppBufferHdr, nPortIndex, pAppPrivate, eglImage);
        }
        OMX_ERRORTYPE ComponentRoleEnum(OMX_U8 *cRole, OMX_U32 nIndex) {
            return ComponentHandle->ComponentRoleEnum(ComponentHandle, cRole, nIndex);
        }

        native_handle_t* tunneled_handle;
        static OMX_CALLBACKTYPE wrapperCallback;

    private:
        typedef struct {
            OMX_BUFFERHEADERTYPE *pBufferHdr;
            sp<GraphicBuffer> mGraphicBuffer;
            OMX_BOOL tileCompressTableOffsetWritten;
        }BufferMapper;

        OMX_COMPONENTTYPE WrapperHandle;
        OMX_COMPONENTTYPE *ComponentHandle;
        OMX_BOOL bEnableNativeBuffers;
        OMX_U32 nNativeBuffersUsage;
        OMX_BOOL bStoreMetaData;
        OMX_BOOL bStoreANWBufferInMetadata;
        OMX_BOOL bSetGrallocBufferParameter;
        OMX_BOOL bAllocateNativeHandle;
        OMX_BOOL bANWOutputBuferForDecoder;
        BufferMapper sBufferMapper[MAX_BUFFER_CNT];
        OMX_S32 nBufferCnt;
        OMX_BOOL bTunneled;
        OMX_BOOL bTileCompressedFormat;
        GMTunneledDecoderWrapper *pGMDecWrapper;
        OMX_CALLBACKTYPE *pCallbacks;
        OMX_PTR pAppPrivateData;
        OMX_PARAM_VIDEO_TUNNELED_MODE sTunneledMode;
        OMX_BOOL bIsEncoder;
        typedef enum {
            COLOR_INFO_INVALID = 0,
            COLOR_INFO_NONE,
            COLOR_INFO_CHANGED,
            COLOR_INFO_TO_WRITE,
        }COLOR_INFO_STATE;
        COLOR_INFO_STATE eColorInfoState;
        OMX_ERRORTYPE DoUseNativeBuffer(UseAndroidNativeBufferParams *pNativBufferParam);
        OMX_U32 ConvertAndroidToOMXColorFormat(OMX_U32 index);
        OMX_U32 ConvertOMXToAndroidColorFormat(OMX_U32 index);

        static OMX_ERRORTYPE OnEvent(
                OMX_IN OMX_HANDLETYPE hComponent,
                OMX_IN OMX_PTR pAppData,
                OMX_IN OMX_EVENTTYPE eEvent,
                OMX_IN OMX_U32 nData1,
                OMX_IN OMX_U32 nData2,
                OMX_IN OMX_PTR pEventData);

        static OMX_ERRORTYPE OnEmptyBufferDone(
                OMX_IN OMX_HANDLETYPE hComponent,
                OMX_IN OMX_PTR pAppData,
                OMX_IN OMX_BUFFERHEADERTYPE *pBuffer);

        static OMX_ERRORTYPE OnFillBufferDone(
                OMX_IN OMX_HANDLETYPE hComponent,
                OMX_IN OMX_PTR pAppData,
                OMX_IN OMX_BUFFERHEADERTYPE *pBuffer);
};

class FSLOMXPlugin : public OMXPluginBase {
    public:
        FSLOMXPlugin() {
            OMX_Init();
        };

        virtual ~FSLOMXPlugin() {
            OMX_Deinit();
        };

        OMX_ERRORTYPE makeComponentInstance(
                const char *name,
                const OMX_CALLBACKTYPE *callbacks,
                OMX_PTR appData,
                OMX_COMPONENTTYPE **component);

        OMX_ERRORTYPE destroyComponentInstance(
                OMX_COMPONENTTYPE *component);

        OMX_ERRORTYPE enumerateComponents(
                OMX_STRING name,
                size_t size,
                OMX_U32 index);

        OMX_ERRORTYPE getRolesOfComponent(
                const char *name,
                Vector<String8> *roles);
    private:
};

#define GET_WRAPPER(handle) \
    ({\
        FSLOMXWrapper *wrapper = NULL; \
        if(handle == NULL) return OMX_ErrorInvalidComponent; \
        OMX_COMPONENTTYPE *hComp = (OMX_COMPONENTTYPE*)handle; \
        wrapper = (FSLOMXWrapper*)(hComp->pComponentPrivate); \
        wrapper; \
    })

static OMX_ERRORTYPE WrapperGetComponentVersion(
        OMX_IN  OMX_HANDLETYPE hComponent,
        OMX_OUT OMX_STRING pComponentName,
        OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
        OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
        OMX_OUT OMX_UUIDTYPE* pComponentUUID)
{
    return GET_WRAPPER(hComponent)->GetVersion(pComponentName, pComponentVersion, pSpecVersion, pComponentUUID);
}

static OMX_ERRORTYPE WrapperSendCommand(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_COMMANDTYPE Cmd,
            OMX_IN  OMX_U32 nParam1,
            OMX_IN  OMX_PTR pCmdData)
{
    return GET_WRAPPER(hComponent)->SendCommand(Cmd, nParam1, pCmdData);
}

static OMX_ERRORTYPE WrapperGetParameter(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_INDEXTYPE nParamIndex,
            OMX_INOUT OMX_PTR pStructure)
{
    return GET_WRAPPER(hComponent)->GetParameter(nParamIndex, pStructure);
}

static OMX_ERRORTYPE WrapperSetParameter(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_INDEXTYPE nParamIndex,
            OMX_INOUT OMX_PTR pStructure)
{
    return GET_WRAPPER(hComponent)->SetParameter(nParamIndex, pStructure);
}

static OMX_ERRORTYPE WrapperGetConfig(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    return GET_WRAPPER(hComponent)->GetConfig(nIndex, pComponentConfigStructure);
}


static OMX_ERRORTYPE WrapperSetConfig(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_IN  OMX_PTR pComponentConfigStructure)
{
    return GET_WRAPPER(hComponent)->SetConfig(nIndex, pComponentConfigStructure);
}

static OMX_ERRORTYPE WrapperGetExtensionIndex(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_STRING cParameterName,
            OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    return GET_WRAPPER(hComponent)->GetExtensionIndex(cParameterName, pIndexType);
}

static OMX_ERRORTYPE WrapperGetState(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_OUT OMX_STATETYPE* pState)
{
    return GET_WRAPPER(hComponent)->GetState(pState);
}

static OMX_ERRORTYPE WrapperComponentTunnelRequest(
        OMX_IN  OMX_HANDLETYPE hComponent,
        OMX_IN  OMX_U32 nPort,
        OMX_IN  OMX_HANDLETYPE hTunneledComp,
        OMX_IN  OMX_U32 nTunneledPort,
        OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    return GET_WRAPPER(hComponent)->TunnelRequest(nPort, hTunneledComp, nTunneledPort, pTunnelSetup);
}


static OMX_ERRORTYPE WrapperUseBuffer(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer)
{
    return GET_WRAPPER(hComponent)->UseBuffer(ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
}


static OMX_ERRORTYPE WrapperAllocateBuffer(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes)
{
    return GET_WRAPPER(hComponent)->AllocateBuffer(ppBuffer, nPortIndex, pAppPrivate, nSizeBytes);
}

static OMX_ERRORTYPE WrapperFreeBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    return GET_WRAPPER(hComponent)->FreeBuffer(nPortIndex, pBuffer);
}

static OMX_ERRORTYPE WrapperEmptyThisBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    return GET_WRAPPER(hComponent)->EmptyThisBuffer(pBufferHdr);
}


static OMX_ERRORTYPE WrapperFillThisBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    return GET_WRAPPER(hComponent)->FillThisBuffer(pBuffer);
}


static OMX_ERRORTYPE WrapperSetCallbacks(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_CALLBACKTYPE* pCbs,
            OMX_IN  OMX_PTR pAppData)
{
    return GET_WRAPPER(hComponent)->BackupCallbacks(pCbs, pAppData);
}


static OMX_ERRORTYPE WrapperComponentDeInit(
            OMX_IN  OMX_HANDLETYPE hComponent)
{
    return GET_WRAPPER(hComponent)->ComponentDeInit();
}


static OMX_ERRORTYPE WrapperUseEGLImage(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN void* eglImage)
{
    return GET_WRAPPER(hComponent)->UseEGLImage(ppBufferHdr, nPortIndex, pAppPrivate, eglImage);
}


static OMX_ERRORTYPE WrapperComponentRoleEnum(
        OMX_IN OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_U8 *cRole,
		OMX_IN OMX_U32 nIndex)
{
    return GET_WRAPPER(hComponent)->ComponentRoleEnum(cRole, nIndex);
}

#define GET_TUNNELED_WRAPPER(handle) \
    ({\
        GMTunneledDecoderWrapper *wrapper = NULL; \
        if(handle == NULL) return OMX_ErrorInvalidComponent; \
        OMX_COMPONENTTYPE *hComp = (OMX_COMPONENTTYPE*)handle; \
        wrapper = ((FSLOMXWrapper*)(hComp->pComponentPrivate))->GetTunneledWrapper(); \
        wrapper; \
    })


static OMX_ERRORTYPE TunneledWrapperGetComponentVersion(
        OMX_IN  OMX_HANDLETYPE hComponent,
        OMX_OUT OMX_STRING pComponentName,
        OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
        OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
        OMX_OUT OMX_UUIDTYPE* pComponentUUID)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->GetVersion(pWrapper->pPrivateData, pComponentName, pComponentVersion, pSpecVersion, pComponentUUID);
}

static OMX_ERRORTYPE TunneledWrapperGetState(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_OUT OMX_STATETYPE* pState)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->GetState(pWrapper->pPrivateData, pState);
}


static OMX_ERRORTYPE TunneledWrapperSendCommand(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_COMMANDTYPE Cmd,
            OMX_IN  OMX_U32 nParam1,
            OMX_IN  OMX_PTR pCmdData)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->SendCommand(pWrapper->pPrivateData, Cmd, nParam1, pCmdData);
}


static OMX_ERRORTYPE TunneledWrapperGetConfig(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->GetConfig(pWrapper->pPrivateData, nIndex, pComponentConfigStructure);
}


static OMX_ERRORTYPE TunneledWrapperSetConfig(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_INDEXTYPE nIndex,
            OMX_IN  OMX_PTR pComponentConfigStructure)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->SetConfig(pWrapper->pPrivateData, nIndex, pComponentConfigStructure);
}


static OMX_ERRORTYPE TunneledWrapperComponentTunnelRequest(
        OMX_IN  OMX_HANDLETYPE hComponent,
        OMX_IN  OMX_U32 nPort,
        OMX_IN  OMX_HANDLETYPE hTunneledComp,
        OMX_IN  OMX_U32 nTunneledPort,
        OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->TunnelRequest(pWrapper->pPrivateData, nPort, hTunneledComp, nTunneledPort, pTunnelSetup);
}


static OMX_ERRORTYPE TunneledWrapperUseBuffer(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->UseBuffer(pWrapper->pPrivateData, ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
}

static OMX_ERRORTYPE TunneledWrapperAllocateBuffer(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->AllocateBuffer(pWrapper->pPrivateData, ppBuffer, nPortIndex, pAppPrivate, nSizeBytes);
}


static OMX_ERRORTYPE TunneledWrapperFreeBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->FreeBuffer(pWrapper->pPrivateData, nPortIndex, pBuffer);
}


static OMX_ERRORTYPE TunneledWrapperEmptyThisBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->EmptyThisBuffer(pWrapper->pPrivateData, pBufferHdr);
}


static OMX_ERRORTYPE TunneledWrapperFillThisBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->FillThisBuffer(pWrapper->pPrivateData, pBuffer);
}

static OMX_ERRORTYPE TunneledWrapperSetCallbacks(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_CALLBACKTYPE* pCbs,
            OMX_IN  OMX_PTR pAppData)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->SetCallbacks(pWrapper->pPrivateData, pCbs, pAppData);
}

static OMX_ERRORTYPE TunneledWrapperComponentDeInit(
            OMX_IN  OMX_HANDLETYPE hComponent)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->ComponentDeInit(pWrapper->pPrivateData);
}

static OMX_ERRORTYPE TunneledWrapperUseEGLImage(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN void* eglImage)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->UseEGLImage(pWrapper->pPrivateData, ppBufferHdr, nPortIndex, pAppPrivate, eglImage);
}


static OMX_ERRORTYPE TunneledWrapperComponentRoleEnum(
        OMX_IN OMX_HANDLETYPE hComponent,
		OMX_OUT OMX_U8 *cRole,
		OMX_IN OMX_U32 nIndex)
{
    GMTunneledDecoderWrapper *pWrapper = GET_TUNNELED_WRAPPER(hComponent);
    return pWrapper->ComponentRoleEnum(pWrapper->pPrivateData, cRole, nIndex);
}



FSLOMXWrapper::FSLOMXWrapper(const char *name)
{
    nBufferCnt = 0;
    ComponentHandle = NULL;
    nNativeBuffersUsage = GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_NEVER;
    #if (ANDROID_VERSION < ANDROID_O) && (!defined(NO_FORCE_CONTIGUOUS))
	nNativeBuffersUsage |= GRALLOC_USAGE_FORCE_CONTIGUOUS;
	#endif

    if(name == NULL)
        return;

    #ifdef HANTRO_VPU
    if(strstr(name,"hw-based")){
        nNativeBuffersUsage |= GRALLOC_USAGE_PRIVATE_3;
    }
    #ifdef ALWAYS_ENABLE_SECURE_PLAYBACK
    nNativeBuffersUsage |= GRALLOC_USAGE_PROTECTED;
    #endif
    #endif

    #ifdef MALONE_VPU
    if(strstr(name,"hw-based")){
        nNativeBuffersUsage |= GRALLOC_USAGE_PRIVATE_3;
    }
    #endif

    if(!strncmp(name, "OMX.Freescale.std.video_encoder", 31)
        && strstr(name,"hw-based")){
        nNativeBuffersUsage |= GRALLOC_USAGE_HW_VIDEO_ENCODER;
    }

    if(!strncmp(name, "OMX.Freescale.std.video_decoder", 31)
        && strstr(name,"sw-based")){
        nNativeBuffersUsage |= GRALLOC_USAGE_SW_WRITE_OFTEN;
    }

    bStoreMetaData = OMX_FALSE;
    bStoreANWBufferInMetadata = OMX_FALSE;
    bSetGrallocBufferParameter = OMX_FALSE;
    bAllocateNativeHandle = OMX_FALSE;
    bANWOutputBuferForDecoder = OMX_FALSE;
    memset(&WrapperHandle, 0, sizeof(OMX_COMPONENTTYPE));
    memset(sBufferMapper, 0, sizeof(BufferMapper) * MAX_BUFFER_CNT);
    bEnableNativeBuffers = OMX_FALSE;
    bTunneled = OMX_FALSE;
    bTileCompressedFormat = OMX_FALSE;
    pGMDecWrapper = NULL;
    pCallbacks = NULL;
    pAppPrivateData = NULL;
    tunneled_handle = NULL;
    OMX_INIT_STRUCT(&sTunneledMode,OMX_PARAM_VIDEO_TUNNELED_MODE);
    bIsEncoder = (strncmp(name, "OMX.Freescale.std.video_encoder", 31) ? OMX_FALSE : OMX_TRUE);

    if(!strncmp(name, "OMX.Freescale.std.video_decoder", 31))
        eColorInfoState = COLOR_INFO_NONE;
    else
        eColorInfoState = COLOR_INFO_INVALID;
}

OMX_COMPONENTTYPE *FSLOMXWrapper::MakeWapper(OMX_HANDLETYPE pHandle)
{
    if(pHandle == NULL)
        return NULL;

    ComponentHandle = (OMX_COMPONENTTYPE*)pHandle;

    WrapperHandle.pComponentPrivate = this;
    WrapperHandle.GetComponentVersion = WrapperGetComponentVersion;
    WrapperHandle.SendCommand = WrapperSendCommand;
    WrapperHandle.GetParameter = WrapperGetParameter;
    WrapperHandle.SetParameter = WrapperSetParameter;
    WrapperHandle.GetConfig = WrapperGetConfig;
    WrapperHandle.SetConfig = WrapperSetConfig;
    WrapperHandle.GetExtensionIndex = WrapperGetExtensionIndex;
    WrapperHandle.GetState = WrapperGetState;
    WrapperHandle.ComponentTunnelRequest = WrapperComponentTunnelRequest;
    WrapperHandle.UseBuffer = WrapperUseBuffer;
    WrapperHandle.AllocateBuffer = WrapperAllocateBuffer;
    WrapperHandle.FreeBuffer = WrapperFreeBuffer;
    WrapperHandle.EmptyThisBuffer = WrapperEmptyThisBuffer;
    WrapperHandle.FillThisBuffer = WrapperFillThisBuffer;
    WrapperHandle.SetCallbacks = WrapperSetCallbacks;
    WrapperHandle.ComponentDeInit = WrapperComponentDeInit;
    WrapperHandle.UseEGLImage = WrapperUseEGLImage;
    WrapperHandle.ComponentRoleEnum = WrapperComponentRoleEnum;

    return &WrapperHandle;
}

OMX_ERRORTYPE FSLOMXWrapper::MakeTunneledWapper()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    pGMDecWrapper = GMTunneledDecoderWrapperCreate((OMX_HANDLETYPE)ComponentHandle, &wrapperCallback, this);
    if (NULL == pGMDecWrapper) {
        LOGE("Failed to make GMDecoder Wrapper.\n");
        return OMX_ErrorUndefined;
    }

    // These APIs will direct go to the tunneled decoder, tunneled decoder will redefine these APIs.
    WrapperHandle.GetComponentVersion = TunneledWrapperGetComponentVersion;
    WrapperHandle.GetState = TunneledWrapperGetState;
    WrapperHandle.SendCommand = TunneledWrapperSendCommand;
    WrapperHandle.ComponentTunnelRequest = TunneledWrapperComponentTunnelRequest;
    WrapperHandle.UseBuffer = TunneledWrapperUseBuffer;
    WrapperHandle.AllocateBuffer = TunneledWrapperAllocateBuffer;
    WrapperHandle.FreeBuffer = TunneledWrapperFreeBuffer;
    WrapperHandle.EmptyThisBuffer = TunneledWrapperEmptyThisBuffer;
    WrapperHandle.FillThisBuffer = TunneledWrapperFillThisBuffer;
    WrapperHandle.SetCallbacks = TunneledWrapperSetCallbacks;
    WrapperHandle.ComponentDeInit = TunneledWrapperComponentDeInit;
    WrapperHandle.UseEGLImage = TunneledWrapperUseEGLImage;
    WrapperHandle.GetConfig = TunneledWrapperGetConfig;
    WrapperHandle.SetConfig = TunneledWrapperSetConfig;
    WrapperHandle.ComponentRoleEnum = TunneledWrapperComponentRoleEnum;

    LOGV("MakeTunneledWrapper ok\n");

    return ret;
}

GMTunneledDecoderWrapper* FSLOMXWrapper::GetTunneledWrapper()
{
    return pGMDecWrapper;
}


OMX_COMPONENTTYPE * FSLOMXWrapper::GetComponentHandle()
{
    return ComponentHandle;
}

OMX_BOOL FSLOMXWrapper::IsTunneled()
{
    return bTunneled;
}

OMX_ERRORTYPE FSLOMXWrapper::BackupCallbacks(OMX_CALLBACKTYPE *pCb, OMX_PTR pAppData)
{
    pCallbacks = pCb;
    pAppPrivateData = pAppData;
    return OMX_ErrorNone;
}


typedef struct{
  OMX_U32 omx_format;
  OMX_U32 pixel_format;
}OMX_FORMAT_CONVERT_TABLE;
static OMX_FORMAT_CONVERT_TABLE format_table[] = {{OMX_COLOR_FormatYUV420SemiPlanar,HAL_PIXEL_FORMAT_YCbCr_420_SP},
                                {OMX_COLOR_FormatYUV420Planar,HAL_PIXEL_FORMAT_YCbCr_420_P},
                                {OMX_COLOR_Format16bitRGB565,HAL_PIXEL_FORMAT_RGB_565},
                                {OMX_COLOR_FormatYUV422Planar,HAL_PIXEL_FORMAT_YCbCr_422_P},
                                {OMX_COLOR_FormatYUV422SemiPlanar,HAL_PIXEL_FORMAT_YCbCr_422_SP},
                                {OMX_COLOR_FormatYUV420SemiPlanar8x128Tiled,HAL_PIXEL_FORMAT_NV12_TILED},
                                {OMX_COLOR_FormatYUV420SemiPlanar4x4Tiled,HAL_PIXEL_FORMAT_NV12_G2_TILED},
                                {OMX_COLOR_FormatYUV420SemiPlanar8x4Tiled,HAL_PIXEL_FORMAT_NV12_G1_TILED},
                                {OMX_COLOR_FormatYUV420SemiPlanar4x4TiledCompressed,HAL_PIXEL_FORMAT_NV12_G2_TILED_COMPRESSED},
                                {OMX_COLOR_FormatYUV420SemiPlanarHDR10,HAL_PIXEL_FORMAT_P010},
                                {OMX_COLOR_FormatYUV420SemiPlanarHDR10Tiled,HAL_PIXEL_FORMAT_P010_TILED},
                                {OMX_COLOR_FormatYUV420SemiPlanarHDR10TiledCompressed,HAL_PIXEL_FORMAT_P010_TILED_COMPRESSED},
                                {OMX_COLOR_FormatYCbYCr,HAL_PIXEL_FORMAT_YCBCR_422_I},
                                };
OMX_U32 FSLOMXWrapper::ConvertAndroidToOMXColorFormat(OMX_U32 index)
{
    OMX_U32 i = 0;
    for(i = 0; i < sizeof(format_table)/sizeof(OMX_FORMAT_CONVERT_TABLE); i++){
        if(index == format_table[i].pixel_format){
            return format_table[i].omx_format;
        }
    }
    return 0;
}
OMX_U32 FSLOMXWrapper::ConvertOMXToAndroidColorFormat(OMX_U32 index)
{
    OMX_U32 i = 0;
    for(i = 0; i < sizeof(format_table)/sizeof(OMX_FORMAT_CONVERT_TABLE); i++){
        if(index == format_table[i].omx_format){
            return format_table[i].pixel_format;
        }
    }
    return 0;
}

OMX_ERRORTYPE FSLOMXWrapper::GetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(pStructure == NULL)
        return OMX_ErrorBadParameter;

    switch((int)nParamIndex) {
        case OMX_IndexParamConfigureVideoTunnelMode:
            {
                //add for test
                OMX_PARAM_VIDEO_TUNNELED_MODE *pMode;
                pMode = (OMX_PARAM_VIDEO_TUNNELED_MODE *)pStructure;
                memcpy(pMode, &sTunneledMode,sizeof(OMX_PARAM_VIDEO_TUNNELED_MODE));
            }
            break;
        case OMX_IndexParamEnableAndroidNativeBuffers:
            {
                EnableAndroidNativeBuffersParams *pParams = (EnableAndroidNativeBuffersParams*)pStructure;
                if(pParams->nPortIndex != 1)
                    return OMX_ErrorUnsupportedIndex;
                pParams->enable = bEnableNativeBuffers;
            }
            break;
        case OMX_IndexParamAllocateNativeHandle:
            {
                AllocateNativeHandleParams *pParams = (AllocateNativeHandleParams*)pStructure;
                if(pParams->nPortIndex != 0)
                    return OMX_ErrorUnsupportedIndex;
                pParams->enable = bAllocateNativeHandle;
            }
            break;
        case OMX_IndexParamNativeBufferUsage:
            {
                GetAndroidNativeBufferUsageParams *pParams = (GetAndroidNativeBufferUsageParams*)pStructure;
                if(pParams->nPortIndex != 1)
                    return OMX_ErrorUnsupportedIndex;
                pParams->nUsage = nNativeBuffersUsage;
            }
            break;
        case OMX_IndexParamStoreMetaDataInBuffers:
            {
                StoreMetaDataInBuffersParams *pParams = (StoreMetaDataInBuffersParams*)pStructure;
                if(pParams->nPortIndex != 0)
                    return OMX_ErrorUnsupportedIndex;
                pParams->bStoreMetaData = bStoreMetaData;
            }
            break;
        case OMX_IndexParamStoreANWBufferInMetadata:
            {
                StoreMetaDataInBuffersParams *pParams = (StoreMetaDataInBuffersParams*)pStructure;
                if(pParams->nPortIndex == 1){
                    #if (ANDROID_VERSION < ANDROID_O)
                        return OMX_ErrorUnsupportedIndex;
                    #endif
                    pParams->bStoreMetaData = bANWOutputBuferForDecoder;
                }else
                    pParams->bStoreMetaData = bStoreANWBufferInMetadata;
            }
            break;
#if (ANDROID_VERSION >= KITKAT_44)
        case OMX_IndexParamVideoPortFormat:
            {
                ret = OMX_GetParameter(ComponentHandle, nParamIndex, pStructure);
                OMX_VIDEO_PARAM_PORTFORMATTYPE *pParams = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pStructure;
                if(pParams->eCompressionFormat == OMX_VIDEO_CodingVP8)
                    pParams->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)9;// defined in framework/native/include/media/openmax/OMX_Video.h

                if(pParams->eCompressionFormat == OMX_VIDEO_CodingVP9)
                    pParams->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)10;

                if(pParams->eCompressionFormat == OMX_VIDEO_CodingHEVC)
                    pParams->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)11;
            }
            break;
#endif

#if (ANDROID_VERSION >= MARSH_MALLOW_600)
            case OMX_IndexParamConsumerUsageBits:
            {
                OMX_U32 *usageBits = (OMX_U32 *)pStructure;
                *usageBits = GRALLOC_USAGE_HW_VIDEO_ENCODER;
                #if (ANDROID_VERSION < ANDROID_O) && (!defined(NO_FORCE_CONTIGUOUS))
                nNativeBuffersUsage |= GRALLOC_USAGE_FORCE_CONTIGUOUS;
                #endif
            }
            break;
            case OMX_IndexParamAudioAndroidAc3:
            {
                OMX_AUDIO_PARAM_ANDROID_AC3TYPE *pAc3Type;
                OMX_AUDIO_PARAM_AC3TYPE Ac3Type;

                pAc3Type = (OMX_AUDIO_PARAM_ANDROID_AC3TYPE*)pStructure;
                OMX_INIT_STRUCT(&Ac3Type,OMX_AUDIO_PARAM_AC3TYPE);
                Ac3Type.nPortIndex = pAc3Type->nPortIndex;
                ret = OMX_GetParameter(ComponentHandle, OMX_IndexParamAudioAc3, &Ac3Type);
                if(ret != OMX_ErrorNone)
                    return OMX_ErrorUndefined;

                pAc3Type->nChannels = Ac3Type.nChannels;
                pAc3Type->nSampleRate = Ac3Type.nSampleRate;
                break;
            }
            case OMX_IndexParamAudioAndroidEac3:
            {
                OMX_AUDIO_PARAM_ANDROID_EAC3TYPE *pEac3Type;
                OMX_AUDIO_PARAM_EC3TYPE Eac3Type;

                pEac3Type = (OMX_AUDIO_PARAM_ANDROID_EAC3TYPE*)pStructure;
                OMX_INIT_STRUCT(&Eac3Type,OMX_AUDIO_PARAM_EC3TYPE);
                Eac3Type.nPortIndex = pEac3Type->nPortIndex;
                ret = OMX_GetParameter(ComponentHandle, OMX_IndexParamAudioEc3, &Eac3Type);
                if(ret != OMX_ErrorNone)
                    return OMX_ErrorUndefined;

                pEac3Type->nChannels = Eac3Type.nChannels;
                pEac3Type->nSampleRate = Eac3Type.nSampleRate;
                break;
            }
            case OMX_IndexParamPortDefinition:
            {
                ret = OMX_GetParameter(ComponentHandle, nParamIndex, pStructure);
                if(ret != OMX_ErrorNone)
                    return ret;

                OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
                pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*)pStructure;
                if (pPortDef->eDomain == OMX_PortDomainAudio) {
                    OMX_AUDIO_PORTDEFINITIONTYPE *audioDef = &(pPortDef->format.audio);
                    switch ((int)audioDef->eEncoding) {
                        case OMX_AUDIO_CodingAC3:
                            audioDef->eEncoding = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingAndroidAC3;
                            break;
                        case OMX_AUDIO_CodingEC3:
                            audioDef->eEncoding = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingAndroidEAC3;
                            break;
                        case OMX_AUDIO_CodingFLAC:
                            audioDef->eEncoding = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingAndroidFLAC;
                            break;
                        default:
                            break;
                    }
                }else if(pPortDef->eDomain == OMX_PortDomainVideo) {
                    OMX_VIDEO_PORTDEFINITIONTYPE *videoDef = &(pPortDef->format.video);
                    switch ((int)videoDef->eCompressionFormat) {
                        case OMX_VIDEO_CodingVP8:
                            videoDef->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)9;// defined in framework/native/include/media/openmax/OMX_Video.h
                            break;
                        case OMX_VIDEO_CodingVP9:
                            videoDef->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)10;
                            break;
                        case OMX_VIDEO_CodingHEVC:
                            videoDef->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)11;
                            break;
                        default:
                            break;
                    }
                }
                break;
            }
            case OMX_IndexParamAudioAndroidFlac:
                {
                    OMX_AUDIO_PARAM_FLACTYPE flacType;
                    OMX_AUDIO_PARAM_ANDROID_FLACTYPE *pAndroidFlacType = (OMX_AUDIO_PARAM_ANDROID_FLACTYPE*)pStructure;
                    if(pAndroidFlacType->nPortIndex != 0)
                        return OMX_ErrorUnsupportedIndex;

                    OMX_INIT_STRUCT(&flacType, OMX_AUDIO_PARAM_FLACTYPE);
                    flacType.nPortIndex = pAndroidFlacType->nPortIndex;
                    ret = OMX_GetParameter(ComponentHandle, OMX_IndexParamAudioFlac, &flacType);
                    if(ret == OMX_ErrorNone){
                        pAndroidFlacType->nChannels = flacType.nChannels;
                        pAndroidFlacType->nSampleRate = flacType.nSampleRate;
                    }
                }
                break;

#endif


        default:
            ret = OMX_GetParameter(ComponentHandle, nParamIndex, pStructure);
    };
    //when enable native buffer, the color format required in ACodec.cpp is pixel format.
    //so need to convert OMX format to androd pixel format.
    if(bEnableNativeBuffers && OMX_IndexParamPortDefinition == nParamIndex){
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*)pStructure;
        if(pPortDef->nPortIndex == 1 && OMX_PortDomainVideo == pPortDef->eDomain){
            pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)ConvertOMXToAndroidColorFormat(pPortDef->format.video.eColorFormat);
            if(pPortDef->format.video.eColorFormat == HAL_PIXEL_FORMAT_NV12_G2_TILED_COMPRESSED
                || pPortDef->format.video.eColorFormat == HAL_PIXEL_FORMAT_P010_TILED_COMPRESSED){
                bTileCompressedFormat = OMX_TRUE;
                ALOGV("bTileCompressedFormat TRUE");
            }else
                bTileCompressedFormat = OMX_FALSE;
        }
    }

    return ret;
}

OMX_ERRORTYPE FSLOMXWrapper::SetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(pStructure == NULL)
        return OMX_ErrorBadParameter;

    switch((int)nParamIndex) {
        case OMX_IndexParamConfigureVideoTunnelMode:
            {
                OMX_PARAM_VIDEO_TUNNELED_MODE *pMode;
                pMode = (OMX_PARAM_VIDEO_TUNNELED_MODE *)pStructure;
                if(pMode->bTunneled == OMX_TRUE){
                    if(tunneled_handle == NULL){
                        int numFds = 1;
                        int numInts = 8;
                        size_t size = 8;
                        const size_t pagesize = getpagesize();
                        size = ((size + pagesize-1) & ~(pagesize-1));

                        int fd = ashmem_create_region("TunneledPlayback", size);
                        if(fd < 0) {
                            LOGE("ashmem_create_region TunneledPlayback size %zu failed", size);
                            return OMX_ErrorInsufficientResources;
                        }
                        int *buf = (int*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                        if ((void*)buf == (void*)-1) {
                            LOGE("mmap sideband handle failed");
                            return OMX_ErrorInsufficientResources;
                        }
                        else {
                            memset(buf, 0, size);
                            munmap(buf, size);
                        }

                        tunneled_handle = native_handle_create(numFds,numInts);
                        if(tunneled_handle == NULL)
                            return OMX_ErrorInsufficientResources;

                        memcpy(tunneled_handle->data, &fd, numFds * sizeof(int));
                    }
                    pMode->pSidebandWindow = (OMX_PTR)tunneled_handle;
                    bTunneled = OMX_TRUE;
                    memcpy(&sTunneledMode, pMode, sizeof(OMX_PARAM_VIDEO_TUNNELED_MODE));

                    if (MakeTunneledWapper() != OMX_ErrorNone)
                        return OMX_ErrorBadParameter;

                    //set handle to wrapper
                    pGMDecWrapper->SetConfig(pGMDecWrapper->pPrivateData, OMX_IndexParamConfigureVideoTunnelMode, pMode);
                }
                else{
                    pMode->pSidebandWindow = NULL;
                    bTunneled = OMX_FALSE;
                    ret = OMX_ErrorBadParameter;
                }
            }
            break;
        case OMX_IndexParamEnableAndroidNativeBuffers:
            {
                EnableAndroidNativeBuffersParams *pParams = (EnableAndroidNativeBuffersParams*)pStructure;
                if(pParams->nPortIndex != 1)
                    return OMX_ErrorUnsupportedIndex;
                bEnableNativeBuffers = pParams->enable;
            }
            break;
        case OMX_IndexParamAllocateNativeHandle:
            {
                AllocateNativeHandleParams *pParams = (AllocateNativeHandleParams*)pStructure;
                if(pParams->nPortIndex != 0)
                    return OMX_ErrorUnsupportedIndex;
                bAllocateNativeHandle = pParams->enable;
                ret = OMX_SetParameter(ComponentHandle, nParamIndex, pStructure);
                ALOGV("set OMX_IndexParamAllocateNativeHandle =%d",bAllocateNativeHandle);
            }
            break;
        case OMX_IndexParamNativeBufferUsage:
            {
                GetAndroidNativeBufferUsageParams *pParams = (GetAndroidNativeBufferUsageParams*)pStructure;
                if(pParams->nPortIndex != 1)
                    return OMX_ErrorUnsupportedIndex;
                nNativeBuffersUsage = pParams->nUsage;
            }
            break;
        case OMX_IndexParamStoreMetaDataInBuffers:
            {
                StoreMetaDataInBuffersParams *pParams = (StoreMetaDataInBuffersParams*)pStructure;
                if(pParams->nPortIndex != 0)
                    return OMX_ErrorUnsupportedIndex;
                bStoreMetaData = pParams->bStoreMetaData;

                OMX_CONFIG_BOOLEANTYPE sStoreMetaData;
                OMX_INIT_STRUCT(&sStoreMetaData, OMX_CONFIG_BOOLEANTYPE);
                sStoreMetaData.bEnabled = bStoreMetaData;
                ret = OMX_SetParameter(ComponentHandle, nParamIndex, &sStoreMetaData);

                if (bStoreMetaData == OMX_TRUE) {
                    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

                    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                    sPortDef.nPortIndex = pParams->nPortIndex;
                    OMX_GetParameter(ComponentHandle, OMX_IndexParamPortDefinition, &sPortDef);
                    sPortDef.nBufferSize = 4 + sizeof(buffer_handle_t);
                    OMX_SetParameter(ComponentHandle, OMX_IndexParamPortDefinition, &sPortDef);
                    bSetGrallocBufferParameter = OMX_FALSE;
                }
            }
            break;
        case OMX_IndexParamStoreANWBufferInMetadata:
            {
                StoreMetaDataInBuffersParams *pParams = (StoreMetaDataInBuffersParams*)pStructure;
                if(pParams->nPortIndex == 1){
                    #if (ANDROID_VERSION < ANDROID_O)
                        return OMX_ErrorUnsupportedIndex;
                    #endif
                    // comment out this line to disable ANW for all decoders
                    //#ifdef HANTRO_VPU
                    //disable the feature as hantro decoder do not support adaptive playback
                    return OMX_ErrorUnsupportedIndex;
                    //#endif
                    bANWOutputBuferForDecoder = pParams->bStoreMetaData;
                    ALOGV("set OMX_IndexParamStoreANWBufferInMetadata =%d",bANWOutputBuferForDecoder);

                    if(bANWOutputBuferForDecoder){
                        OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

                        OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                        sPortDef.nPortIndex = pParams->nPortIndex;
                        OMX_GetParameter(ComponentHandle, OMX_IndexParamPortDefinition, &sPortDef);
                        ALOGV("OMX_IndexParamStoreANWBufferInMetadata buffer size from %d to 12",sPortDef.nBufferSize);
                        sPortDef.nBufferSize = sizeof(VideoNativeMetadata);
                        OMX_SetParameter(ComponentHandle, OMX_IndexParamPortDefinition, &sPortDef);
                    }

                }else{
                    bStoreANWBufferInMetadata = pParams->bStoreMetaData;

                    OMX_CONFIG_BOOLEANTYPE sStoreMetaData;
                    OMX_INIT_STRUCT(&sStoreMetaData, OMX_CONFIG_BOOLEANTYPE);
                    sStoreMetaData.bEnabled = bStoreANWBufferInMetadata;
                    nParamIndex = OMX_IndexParamStoreMetaDataInBuffers;
                    ret = OMX_SetParameter(ComponentHandle, nParamIndex, &sStoreMetaData);

                    if (bStoreANWBufferInMetadata == OMX_TRUE) {
                        OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

                        OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                        sPortDef.nPortIndex = pParams->nPortIndex;
                        OMX_GetParameter(ComponentHandle, OMX_IndexParamPortDefinition, &sPortDef);
                        sPortDef.nBufferSize = 4 + sizeof(buffer_handle_t) + sizeof(int);
                        OMX_SetParameter(ComponentHandle, OMX_IndexParamPortDefinition, &sPortDef);
                        bSetGrallocBufferParameter = OMX_FALSE;
                    }
                }
            }
            break;
        case OMX_IndexParamUseAndroidNativeBuffer:
            {
                UseAndroidNativeBufferParams *pParams = (UseAndroidNativeBufferParams*)pStructure;
                if(pParams->nPortIndex != 1)
                    return OMX_ErrorUnsupportedIndex;
                ret = DoUseNativeBuffer(pParams);
            }
            break;
#if (ANDROID_VERSION >= JELLY_BEAN_42)
        case OMX_IndexParamUseAndroidPrependSPSPPStoIDRFrames:
            {
                PrependSPSPPSToIDRFramesParams *pParams = (PrependSPSPPSToIDRFramesParams*)pStructure;
                OMX_PARAM_PREPEND_SPSPPS_TO_IDR sPrependSPSPPSToIDR;
                OMX_INIT_STRUCT(&sPrependSPSPPSToIDR, OMX_PARAM_PREPEND_SPSPPS_TO_IDR);
                sPrependSPSPPSToIDR.bEnableSPSToIDR= pParams->bEnable;
                ret = OMX_SetParameter(ComponentHandle, nParamIndex, &sPrependSPSPPSToIDR);
            }
            break;
#endif
#if (ANDROID_VERSION >= KITKAT_44)
        case OMX_IndexParamVideoPortFormat:
            {
                OMX_VIDEO_PARAM_PORTFORMATTYPE *pParams = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)pStructure;
                if(pParams->eCompressionFormat == 9) // defined in framework/native/include/media/openmax/OMX_Video.h
                    pParams->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP8;
                if(pParams->eCompressionFormat == 10)
                    pParams->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP9;
                if(pParams->eCompressionFormat == 11)
                    pParams->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC;
                ret = OMX_SetParameter(ComponentHandle, nParamIndex, pStructure);
            }
            break;
        case OMX_IndexParamVideoDecReorderDisable:
            {
                DisableAVCReorderParams  *pParams = (DisableAVCReorderParams *)pStructure;
                OMX_DECODER_REORDER sDecoderReorder;
                OMX_INIT_STRUCT(&sDecoderReorder, OMX_DECODER_REORDER);
                sDecoderReorder.bDisable= pParams->bDisable;
                ret = OMX_SetParameter(ComponentHandle, nParamIndex, &sDecoderReorder);
            }
            break;
#endif
        case OMX_IndexParamAndroidAdaptivePlayback:
            {
                OMX_PARAM_PREPARE_ANDROID_ADAPTIVE_PLAYBACK * pAdaptivePlayback=
                    (OMX_PARAM_PREPARE_ANDROID_ADAPTIVE_PLAYBACK *)pStructure;
                OMX_VIDEO_REG_FRM_EXT_INFO info;
                OMX_INIT_STRUCT(&info,OMX_VIDEO_REG_FRM_EXT_INFO);
                info.nPortIndex=1;
                if(pAdaptivePlayback->bEnable){
                    info.nWidthStride = pAdaptivePlayback->nMaxFrameWidth;
                    info.nHeightStride = pAdaptivePlayback->nMaxFrameHeight;
                    info.nMaxBufferCnt = 16;
                }else{
                    info.nWidthStride = -1;
                    info.nHeightStride = -1;
                    info.nMaxBufferCnt = -1;
                }
                ret = OMX_SetParameter(ComponentHandle,OMX_IndexParamVideoRegisterFrameExt,&info);

            }
            break;
        case OMX_IndexParamPortDefinition:
        {
            OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*)pStructure;
            ALOGV("Set OMX_IndexParamPortDefinition index %d",pPortDef->nPortIndex);
            if (pPortDef->eDomain == OMX_PortDomainAudio) {
                OMX_AUDIO_PORTDEFINITIONTYPE *audioDef = &(pPortDef->format.audio);
                switch ((int)audioDef->eEncoding) {
                    case OMX_AUDIO_CodingAndroidAC3:
                        audioDef->eEncoding = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingAC3;
                        break;
                    case OMX_AUDIO_CodingAndroidEAC3:
                        audioDef->eEncoding = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingEC3;
                        break;
                    case OMX_AUDIO_CodingAndroidFLAC:
                        audioDef->eEncoding = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingFLAC;
                        break;
                    default:
                        break;
                }
            }
			else if(pPortDef->eDomain == OMX_PortDomainVideo) {
				OMX_VIDEO_PORTDEFINITIONTYPE *videoDef = &(pPortDef->format.video);
				switch ((int)videoDef->eCompressionFormat) {
					// defined in framework/native/include/media/openmax/OMX_Video.h
					case 9:
						videoDef->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP8;// defined in framework/native/include/media/openmax/OMX_Video.h
						break;
					case 10:
						videoDef->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVP9;
						break;
					case 11:
						videoDef->eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingHEVC;
						break;
					default:
						break;
				}
				ALOGV("set OMX_IndexParamPortDefinition BUFFER SIZE=%d color format=%x",pPortDef->nBufferSize,videoDef->eColorFormat);
			}

            //when enable native buffer, the color format set from ACodec.cpp is pixel format.
            //so need to convert to OMX color format.
            if(bEnableNativeBuffers){
                if(pPortDef->nPortIndex == 1 && OMX_PortDomainVideo == pPortDef->eDomain){
                    OMX_U32 format = 0;
                    format = ConvertAndroidToOMXColorFormat(pPortDef->format.video.eColorFormat);
                    if(format > 0)
                        pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)format;
                }
            }

            // buffer size has been set to 12 in SetParameter(OMX_IndexParamStoreANWBufferInMetadata).
            // should not modify it to a value like width*height*1.5, Port::AllocateBuffer will fail.
            if (pPortDef->nPortIndex == 0 && bStoreANWBufferInMetadata == OMX_TRUE) {
                OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
                OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                sPortDef.nPortIndex = pPortDef->nPortIndex;
                OMX_GetParameter(ComponentHandle, OMX_IndexParamPortDefinition, &sPortDef);
                if(pPortDef->nBufferSize != 12)
                    pPortDef->nBufferSize = 12;
                ALOGV("Set OMX_IndexParamPortDefinition nPortIndex=0  colorFormat=%x,compressed format=%x",pPortDef->format.video.eColorFormat,pPortDef->format.video.eCompressionFormat);
            }
            ret = OMX_SetParameter(ComponentHandle, nParamIndex, pStructure);
            break;
        }
#if (ANDROID_VERSION >= MARSH_MALLOW_600)
        case OMX_IndexParamAudioAndroidAc3:
        {
            OMX_AUDIO_PARAM_ANDROID_AC3TYPE *pAc3Type;
            OMX_AUDIO_PARAM_AC3TYPE Ac3Type;
            OMX_INIT_STRUCT(&Ac3Type,OMX_AUDIO_PARAM_AC3TYPE);

            pAc3Type = (OMX_AUDIO_PARAM_ANDROID_AC3TYPE*)pStructure;
            Ac3Type.nPortIndex = pAc3Type->nPortIndex;
            Ac3Type.nChannels = pAc3Type->nChannels;
            Ac3Type.nSampleRate = pAc3Type->nSampleRate;
            ret = OMX_SetParameter(ComponentHandle, (OMX_INDEXTYPE)OMX_IndexParamAudioAc3, &Ac3Type);
            break;
        }
        case OMX_IndexParamAudioAndroidEac3:
        {
            OMX_AUDIO_PARAM_ANDROID_EAC3TYPE *pEac3Type;
            OMX_AUDIO_PARAM_EC3TYPE Eac3Type;
            OMX_INIT_STRUCT(&Eac3Type,OMX_AUDIO_PARAM_EC3TYPE);

            pEac3Type = (OMX_AUDIO_PARAM_ANDROID_EAC3TYPE*)pStructure;
            Eac3Type.nPortIndex = pEac3Type->nPortIndex;
            Eac3Type.nChannels = pEac3Type->nChannels;
            Eac3Type.nSampleRate = pEac3Type->nSampleRate;
            ret = OMX_SetParameter(ComponentHandle, (OMX_INDEXTYPE)OMX_IndexParamAudioEc3, &Eac3Type);
            break;
        }
        case OMX_IndexParamAudioAndroidFlac:
        {
            OMX_AUDIO_PARAM_ANDROID_FLACTYPE *pAndroidFlacType;
            pAndroidFlacType = (OMX_AUDIO_PARAM_ANDROID_FLACTYPE*)pStructure;
            if(pAndroidFlacType->nPortIndex != 0)
                return OMX_ErrorUnsupportedIndex;

            OMX_AUDIO_PARAM_FLACTYPE flacType;
            OMX_INIT_STRUCT(&flacType,OMX_AUDIO_PARAM_FLACTYPE);
            flacType.nPortIndex = pAndroidFlacType->nPortIndex;
            flacType.nChannels = pAndroidFlacType->nChannels;
            flacType.nSampleRate = pAndroidFlacType->nSampleRate;
            ret = OMX_SetParameter(ComponentHandle, OMX_IndexParamAudioFlac, &flacType);
            break;
        }
#endif
        default:
            ret = OMX_SetParameter(ComponentHandle, nParamIndex, pStructure);
    };

    return ret;
}

OMX_ERRORTYPE FSLOMXWrapper::GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(pStructure == NULL)
        return OMX_ErrorBadParameter;

    switch((int)nParamIndex) {
        case OMX_IndexConfigDescribeColorInfo:
        {
            OMX_CONFIG_VPU_COLOR_DESC vpuColorDesc;
            DescribeColorAspectsParams * pParam = (DescribeColorAspectsParams*)pStructure;

            if(pParam->nPortIndex == 0){
                ret = bIsEncoder ? OMX_ErrorNone : OMX_ErrorUndefined;
                break;
            }

            OMX_INIT_STRUCT(&vpuColorDesc,OMX_CONFIG_VPU_COLOR_DESC);
            vpuColorDesc.nPortIndex = pParam->nPortIndex;
            ret = OMX_GetConfig(ComponentHandle, nParamIndex, &vpuColorDesc);
            if(ret != OMX_ErrorNone)
                break;

            pParam->sAspects.mRange = (android::ColorAspects::Range)vpuColorDesc.fullRange;
            pParam->sAspects.mPrimaries = (android::ColorAspects::Primaries)vpuColorDesc.primaries;
            pParam->sAspects.mTransfer = (android::ColorAspects::Transfer)vpuColorDesc.transfer;
            pParam->sAspects.mMatrixCoeffs = (android::ColorAspects::MatrixCoeffs)vpuColorDesc.matrixCoeffs;

            break;
        }
        case OMX_IndexConfigDescribeHDRColorInfo:
        {
            OMX_CONFIG_VPU_STATIC_HDR_INFO hdrInfo;
            DescribeHDRStaticInfoParams* pOut = (DescribeHDRStaticInfoParams*)pStructure;

            if(pOut->nSize != sizeof(DescribeHDRStaticInfoParams)){
                ret = OMX_ErrorBadParameter;
                break;
            }

            if (pOut->nPortIndex == 0) {
                ret = bIsEncoder ? OMX_ErrorNone : OMX_ErrorUndefined;
                break;
            }

            HDRStaticInfo *pParam = &pOut->sInfo;

            if(pParam->mID != 0){
                ret = OMX_ErrorBadParameter;
                break;
            }

            OMX_INIT_STRUCT(&hdrInfo,OMX_CONFIG_VPU_STATIC_HDR_INFO);
            hdrInfo.nPortIndex = 1;
            hdrInfo.nID = 0;

            ret = OMX_GetConfig(ComponentHandle, nParamIndex, &hdrInfo);
            if(ret != OMX_ErrorNone)
                break;

            pParam->mID = HDRStaticInfo::kType1;
            pParam->sType1.mR.x = hdrInfo.mR[0];
            pParam->sType1.mR.y = hdrInfo.mR[1];
            pParam->sType1.mG.x = hdrInfo.mG[0];
            pParam->sType1.mG.y = hdrInfo.mG[1];
            pParam->sType1.mB.x = hdrInfo.mB[0];
            pParam->sType1.mB.y = hdrInfo.mB[1];
            pParam->sType1.mW.x = hdrInfo.mW[0];
            pParam->sType1.mW.y = hdrInfo.mW[1];
            pParam->sType1.mMaxDisplayLuminance = hdrInfo.mMaxDisplayLuminance;
            pParam->sType1.mMinDisplayLuminance = hdrInfo.mMinDisplayLuminance;
            pParam->sType1.mMaxContentLightLevel = hdrInfo.mMaxContentLightLevel;
            pParam->sType1.mMaxFrameAverageLightLevel = hdrInfo.mMaxFrameAverageLightLevel;
            ALOGV("get OMX_IndexConfigDescribeHDRColorInfo pParam->mID=%d",pParam->mID);
            break;
        }
        default:
            ret = OMX_GetConfig(ComponentHandle, nParamIndex, pStructure);
        break;
    }
    return ret;
}

OMX_ERRORTYPE FSLOMXWrapper::SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(pStructure == NULL) {
        return OMX_ErrorBadParameter;
    }

    switch((int)nParamIndex) {
        case OMX_IndexConfigDescribeColorInfo:
        {
            OMX_CONFIG_VPU_COLOR_DESC vpuColorDesc;
            DescribeColorAspectsParams * pParam = (DescribeColorAspectsParams*)pStructure;

            if(pParam->nPortIndex == 0){
                ret = bIsEncoder ? OMX_ErrorNone : OMX_ErrorUndefined;
                break;
            }

            OMX_INIT_STRUCT(&vpuColorDesc,OMX_CONFIG_VPU_COLOR_DESC);
            vpuColorDesc.nPortIndex = pParam->nPortIndex;

            vpuColorDesc.primaries = pParam->sAspects.mPrimaries;
            vpuColorDesc.fullRange = pParam->sAspects.mRange;
            vpuColorDesc.transfer = pParam->sAspects.mTransfer;
            vpuColorDesc.matrixCoeffs = pParam->sAspects.mMatrixCoeffs;
            ALOGV("SetConfig bRequestingDataSpace=%d,bDataSpaceChanged=%d,nPixelFormat=%d,nDataSpace=%d",
                pParam->bRequestingDataSpace,pParam->bDataSpaceChanged,pParam->nPixelFormat,pParam->nDataSpace);
            ALOGV("SetConfig P=%d,T=%d,M=%d,R=%d",
                vpuColorDesc.primaries,vpuColorDesc.transfer,vpuColorDesc.matrixCoeffs,vpuColorDesc.fullRange);
            ret = OMX_SetConfig(ComponentHandle, nParamIndex, &vpuColorDesc);
            break;
        }
        case OMX_IndexConfigDescribeHDRColorInfo:
        {
            OMX_CONFIG_VPU_STATIC_HDR_INFO hdrInfo;
            DescribeHDRStaticInfoParams* pIn = (DescribeHDRStaticInfoParams*)pStructure;

            if(pIn->nSize != sizeof(DescribeHDRStaticInfoParams)){
                ret = OMX_ErrorBadParameter;
                break;
            }

            if (pIn->nPortIndex == 0) {
                ret = bIsEncoder ? OMX_ErrorNone : OMX_ErrorUndefined;
                break;
            }

            HDRStaticInfo *pParam = &pIn->sInfo;

            if(pParam->mID != HDRStaticInfo::kType1){
                ret = OMX_ErrorBadParameter;
                break;
            }

            OMX_INIT_STRUCT(&hdrInfo,OMX_CONFIG_VPU_STATIC_HDR_INFO);
            hdrInfo.nPortIndex = 1;
            hdrInfo.nID = 0;
            hdrInfo.mR[0] = pParam->sType1.mR.x;
            hdrInfo.mR[1] = pParam->sType1.mR.y;
            hdrInfo.mG[0] = pParam->sType1.mG.x;
            hdrInfo.mG[1] = pParam->sType1.mG.y;
            hdrInfo.mB[0] = pParam->sType1.mB.x;
            hdrInfo.mB[1] = pParam->sType1.mB.y;
            hdrInfo.mW[0] = pParam->sType1.mW.x;
            hdrInfo.mW[1] = pParam->sType1.mW.y;
            hdrInfo.mMaxDisplayLuminance = pParam->sType1.mMaxDisplayLuminance;
            hdrInfo.mMinDisplayLuminance = pParam->sType1.mMinDisplayLuminance;
            hdrInfo.mMaxContentLightLevel = pParam->sType1.mMaxContentLightLevel;
            hdrInfo.mMaxFrameAverageLightLevel = pParam->sType1.mMaxFrameAverageLightLevel;

            ret = OMX_SetConfig(ComponentHandle, nParamIndex, &hdrInfo);
            break;
        }
        default:
            ret = OMX_SetConfig(ComponentHandle, nParamIndex, pStructure);
            break;
    };

    return ret;
}

OMX_ERRORTYPE FSLOMXWrapper::GetExtensionIndex(
        OMX_STRING cParameterName,
        OMX_INDEXTYPE* pIndexType)
{
    if(!strcmp(cParameterName, "OMX.google.android.index.enableAndroidNativeBuffers"))
        *pIndexType = OMX_IndexParamEnableAndroidNativeBuffers;
    else if(!strcmp(cParameterName, "OMX.google.android.index.getAndroidNativeBufferUsage"))
        *pIndexType = OMX_IndexParamNativeBufferUsage;
    else if(!strcmp(cParameterName, "OMX.google.android.index.storeMetaDataInBuffers"))
        *pIndexType = OMX_IndexParamStoreMetaDataInBuffers;
    else if(!strcmp(cParameterName, "OMX.google.android.index.storeANWBufferInMetadata"))
        *pIndexType = OMX_IndexParamStoreANWBufferInMetadata;
    else if(!strcmp(cParameterName, "OMX.google.android.index.storeGraphicBufferInMetaData"))
        *pIndexType = OMX_IndexParamStoreMetaDataInBuffers;
    else if(!strcmp(cParameterName, "OMX.google.android.index.useAndroidNativeBuffer"))
        *pIndexType = OMX_IndexParamUseAndroidNativeBuffer;
    else if(!strcmp(cParameterName, "OMX.google.android.index.prependSPSPPSToIDRFrames"))
        *pIndexType = OMX_IndexParamUseAndroidPrependSPSPPStoIDRFrames;
    else if(!strcmp(cParameterName, "OMX.google.android.index.disableAVCReorder"))
        *pIndexType = OMX_IndexParamVideoDecReorderDisable;
    #ifndef HANTRO_VPU
    else if(!strcmp(cParameterName, "OMX.google.android.index.prepareForAdaptivePlayback"))
        *pIndexType = OMX_IndexParamAndroidAdaptivePlayback;
    #endif
    else if(!strcmp(cParameterName, "OMX.google.android.index.configureVideoTunnelMode"))
        *pIndexType = OMX_IndexParamConfigureVideoTunnelMode;
    #ifdef HANTRO_VPU
    else if(!strcmp(cParameterName, "OMX.google.android.index.allocateNativeHandle"))
        *pIndexType = OMX_IndexParamAllocateNativeHandle;
    else if(!strcmp(cParameterName, "OMX.google.android.index.describeColorAspects"))
        *pIndexType = OMX_IndexConfigDescribeColorInfo;
    else if(!strcmp(cParameterName, "OMX.google.android.index.describeHDRStaticInfo"))
        *pIndexType = OMX_IndexConfigDescribeHDRColorInfo;
    #endif
    else
        return OMX_GetExtensionIndex(ComponentHandle, cParameterName, pIndexType);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE FSLOMXWrapper::AllocateBuffer(OMX_BUFFERHEADERTYPE** ppBuffer, OMX_U32 nPortIndex,
                             OMX_PTR pAppPrivate, OMX_U32 nSizeBytes) {
    if(!bAllocateNativeHandle || nPortIndex == 1)
        return OMX_AllocateBuffer(ComponentHandle, ppBuffer, nPortIndex, pAppPrivate, nSizeBytes);
    else {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        OMX_PTR bufferPtr = NULL;
        native_handle_t* buffer_handle = NULL;
        int numFds = 2;
        int numInts = 1;
        int32_t fd_32 = 0;
        int fd = 0;
        int virAddr = 0;

        if(OMX_ErrorNone != OMX_AllocateBuffer(ComponentHandle, ppBuffer, nPortIndex, pAppPrivate, nSizeBytes))
            return OMX_ErrorInsufficientResources;

        fd = (intptr_t)(*ppBuffer)->pBuffer;
        buffer_handle = native_handle_create(numFds,numInts);
        if(buffer_handle == NULL)
            return OMX_ErrorInsufficientResources;

        buffer_handle->data[0] = fd;//secure buffer fd

        #ifdef HANTRO_VPU
        const size_t pagesize = getpagesize();
        size_t size = ((nSizeBytes + pagesize-1) & ~(pagesize-1));

        //malloc shared memory here for encrypted data that used by vpu
        int fd2 = ashmem_create_region("vpu", size);
        if(fd2 < 0) {
            LOGE("ashmem_create_region size %zu failed", size);
            return OMX_ErrorInsufficientResources;
        }
        int *buf = (int*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
        if ((void*)buf == (void*)-1) {
            LOGE("mmap sideband handle failed");
            return OMX_ErrorInsufficientResources;
        }
        else {
            memset(buf, 0, size);
            ALOGV("mmap addr=%p",buf);
        }

        //store shared buffer virtual address here, vpu will use it later.
        ModifyFdAndAddr((*ppBuffer)->pBuffer,fd2,(OMX_PTR)buf);
        buffer_handle->data[1] = fd2;//shared buffer fd

        ALOGV("AllocateBuffer bAllocateNativeHandle fd=%d,fd2=%d",fd,fd2);
        ALOGV("AllocateBuffer bAllocateNativeHandle data2=%p",buf);
        buffer_handle->data[2] = 0;//secure buffer size for data[0], this is used for debug
        #endif

        (*ppBuffer)->pBuffer = (OMX_U8*)buffer_handle;
        ALOGV("AllocateBuffer buffer_handle=%p,size=%u",buffer_handle,nSizeBytes);

        return OMX_ErrorNone;
    }
}
OMX_ERRORTYPE FSLOMXWrapper::FreeBuffer(
        OMX_U32 nPortIndex,
        OMX_BUFFERHEADERTYPE* pBufferHdr)
{
    for(OMX_S32 i=0; i<MAX_BUFFER_CNT; i++) {
        if(pBufferHdr == sBufferMapper[i].pBufferHdr) {
            sBufferMapper[i].mGraphicBuffer->unlock();
            RemoveHwBuffer(pBufferHdr->pBuffer);
            sBufferMapper[i].mGraphicBuffer = NULL;
            memset(&sBufferMapper[i], 0, sizeof(BufferMapper));
            nBufferCnt--;
            break;
        }
    }

    //free shared buffer for encrypted data
    if( nPortIndex == 0 && bAllocateNativeHandle){

        native_handle_t* buffer_handle = (native_handle_t*)pBufferHdr->pBuffer;
        OMX_PTR virAddr = (OMX_PTR)(intptr_t)buffer_handle->data[0];
        pBufferHdr->pBuffer = (OMX_U8*)virAddr;

        #ifdef HANTRO_VPU
        int fd2 = buffer_handle->data[1];
        OMX_S32 fd3;
        int* dataBuf = NULL;
        //fd3 should be equal to fd2
        GetFdAndAddr(virAddr,&fd3,(OMX_PTR*)&dataBuf);
        ALOGV("FreeBuffer fd=%d,fd2=%d,fd3=%d,dataBuf=%p",buffer_handle->data[0], fd2, fd3, dataBuf);
        munmap(dataBuf,pBufferHdr->nAllocLen);
        if(fd2 > 0)
            close(fd2);
        #endif

        native_handle_delete(buffer_handle);
    }
    if(nPortIndex == 1 && bANWOutputBuferForDecoder){
        VideoNativeMetadata * pBuf = (VideoNativeMetadata *)pBufferHdr->pBuffer;
        if(pBuf->eType == kMetadataBufferTypeANWBuffer){
            ANativeWindowBuffer *pWindowBuf = pBuf->pBuffer;
            buffer_handle_t tBufHandle = pWindowBuf->handle;
            fsl::Memory *pGrallocHandle = (fsl::Memory *)tBufHandle;
            RemoveHwBuffer((OMX_PTR)pGrallocHandle->base);
            nBufferCnt--;
            ALOGV("FreeBuffer %p",(OMX_PTR)pGrallocHandle->base);
            pBufferHdr->pBuffer = (OMX_U8*)pGrallocHandle->base;
            pBufferHdr->nFilledLen = pGrallocHandle->size;
        }
    }

    OMX_FreeBuffer(ComponentHandle, nPortIndex, pBufferHdr);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FSLOMXWrapper::DoUseNativeBuffer(
        UseAndroidNativeBufferParams *pNativBufferParam)
{
    if(pNativBufferParam == NULL || pNativBufferParam->nativeBuffer == NULL
            || pNativBufferParam->pAppPrivate == NULL)
        return OMX_ErrorBadParameter;


    GraphicBuffer *pGraphicBuffer = static_cast<GraphicBuffer*>(pNativBufferParam->nativeBuffer.get());
	#if (ANDROID_VERSION < ANDROID_O)
    private_handle_t *prvHandle = (private_handle_t*)pGraphicBuffer->getNativeBuffer()->handle;
	#else
	fsl::Memory *prvHandle = (fsl::Memory*)pGraphicBuffer->getNativeBuffer()->handle;
	#endif

    OMX_PTR vAddr = NULL;
    pGraphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, &vAddr);
    if(vAddr == NULL) {
        LOGE("Failed to get native buffer virtual address.\n");
        return OMX_ErrorUndefined;
    }

    LOGV("native buffer handle %p, %" PRIx64 ", virs %p, size %d, pGraphicBuffer=%p\n", prvHandle, prvHandle->phys, vAddr, prvHandle->size,pGraphicBuffer);

    AddHwBuffer((OMX_PTR)(unsigned long)prvHandle->phys, vAddr);
    OMX_PTR dummy=(OMX_U8*)0x01;
    ModifyFdAndAddr((OMX_PTR)vAddr, prvHandle->fd, dummy);

    OMX_ERRORTYPE ret = OMX_ErrorNone;
    ret = OMX_UseBuffer(ComponentHandle, pNativBufferParam->bufferHeader,
            pNativBufferParam->nPortIndex, pNativBufferParam->pAppPrivate,
            prvHandle->size, (OMX_U8*)vAddr);
    if(ret != OMX_ErrorNone) {
        RemoveHwBuffer(vAddr);
        pGraphicBuffer->unlock();
        LOGE("Failed to use native buffer.\n");
        return ret;
    }

    sBufferMapper[nBufferCnt].pBufferHdr = *pNativBufferParam->bufferHeader;
    sBufferMapper[nBufferCnt].mGraphicBuffer = pGraphicBuffer;
    nBufferCnt ++;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FSLOMXWrapper::EmptyThisBuffer(
        OMX_BUFFERHEADERTYPE* pBufferHdr)
{
#if (ANDROID_VERSION >= JELLY_BEAN_42)
    if ((bStoreMetaData == OMX_TRUE || bStoreANWBufferInMetadata == OMX_TRUE)&& pBufferHdr->nFilledLen >= 8) {
        OMX_U32 *pTempBuffer;
        OMX_U32 nMetadataBufferType;
        OMX_BOOL bGraphicBuffer = OMX_FALSE;

        LOGV("Passing meta data to encoder");
        pTempBuffer = (OMX_U32 *) (pBufferHdr->pBuffer);
        nMetadataBufferType = *pTempBuffer;

        if(nMetadataBufferType == kMetadataBufferTypeCameraSource) {
            LOGV("MetadataBufferType is kMetadataBufferTypeCameraSource");
        }
        else if(nMetadataBufferType == kMetadataBufferTypeGrallocSource) {
            LOGV("MetadataBufferType is kMetadataBufferTypeGrallocSource");
        }
#if (ANDROID_VERSION >= LOLLIPOP_50) && (ANDROID_VERSION < MARSH_MALLOW_600)
        else if(nMetadataBufferType == kMetadataBufferTypeGraphicBuffer){
            LOGV("MetadataBufferType is kMetadataBufferTypeGraphicBuffer");
            bGraphicBuffer = OMX_TRUE;
        }
#elif (ANDROID_VERSION >= MARSH_MALLOW_600)
        else if(nMetadataBufferType == kMetadataBufferTypeANWBuffer){
            LOGV("MetadataBufferType is kMetadataBufferTypeANWBuffer");
            bGraphicBuffer = OMX_TRUE;
        }
#endif

        //ANW buffer in nougat is different with that in marshmallow, so add a new variable for it.
        if(bStoreANWBufferInMetadata)
            bGraphicBuffer = OMX_FALSE;

    #if (ANDROID_VERSION < ANDROID_O)
        private_handle_t* pGrallocHandle;
    #else
        fsl::Memory *pGrallocHandle;
    #endif
        buffer_handle_t  tBufHandle;

        pTempBuffer++;

        if(bGraphicBuffer == OMX_TRUE){
            GraphicBuffer *buffer = (GraphicBuffer *)(unsigned long)*pTempBuffer;
            tBufHandle = buffer->handle;

        }else{
            tBufHandle =  *((buffer_handle_t *)pTempBuffer);
        }

        if(bStoreANWBufferInMetadata){
            //get buffer handle from ANativeWindowBuffer pointer.
            ANativeWindowBuffer * pBuffer = (ANativeWindowBuffer *)(unsigned long)*pTempBuffer;
            tBufHandle = pBuffer->handle;
        }

    #if (ANDROID_VERSION < ANDROID_O)
        pGrallocHandle = (private_handle_t*) tBufHandle;
    #else
        pGrallocHandle = (fsl::Memory*) tBufHandle;
    #endif
        LOGV("Grallloc buffer recieved in metadata buffer 0x%p",pGrallocHandle );

        pBufferHdr->pInputPortPrivate = ((METADATA_BUFFER *)(pBufferHdr->pBuffer))->pPhysicAddress;
        ALOGV("EmptyThisBuffer origin Ptr=%p",pBufferHdr->pInputPortPrivate);
    #ifdef MALONE_VPU
        ((METADATA_BUFFER *)(pBufferHdr->pBuffer))->pPhysicAddress = \
            (OMX_PTR)(unsigned long)pGrallocHandle->fd;
    #else
        ((METADATA_BUFFER *)(pBufferHdr->pBuffer))->pPhysicAddress = \
           (OMX_PTR)(unsigned long) pGrallocHandle->phys;
    #endif
        LOGV("%s Gralloc=%p, phys = 0x%" PRIx64 ", base=0x%" PRIx64 "", __FUNCTION__, pGrallocHandle,
                pGrallocHandle->phys, pGrallocHandle->base);

        if(pBufferHdr->nFilledLen >= 12){
            pTempBuffer++;
            int nFenceFd = *pTempBuffer;
            if (nFenceFd >= 0) {
                sp<Fence> fence = new Fence(nFenceFd);
                *pTempBuffer = -1;
                status_t err = fence->wait(kFenceTimeoutMs);
                if (err != OK) {
                    ALOGE("Timed out waiting on input fence");
                    return OMX_ErrorUndefined;
                }
            }
        }

        if (bSetGrallocBufferParameter == OMX_FALSE) {
            GRALLOC_BUFFER_PARAMETER sGrallocBufferParam;

            OMX_INIT_STRUCT(&sGrallocBufferParam, GRALLOC_BUFFER_PARAMETER);
            sGrallocBufferParam.nPortIndex = 0;

            sGrallocBufferParam.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            ALOGV("pGrallocHandle format=%d",pGrallocHandle->format);
            switch(pGrallocHandle->format) {
                case HAL_PIXEL_FORMAT_YCbCr_420_SP:
                    sGrallocBufferParam.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
                    break;
                case HAL_PIXEL_FORMAT_YCbCr_420_P:
                    sGrallocBufferParam.eColorFormat = OMX_COLOR_FormatYUV420Planar;
                    break;
                case HAL_PIXEL_FORMAT_RGBA_8888:
                case HAL_PIXEL_FORMAT_RGBX_8888:
                    sGrallocBufferParam.eColorFormat = OMX_COLOR_Format32bitRGBA8888;
                    break;
                default:
                    ALOGE("Not supported color format %d!", pGrallocHandle->format);
                    break;
            }

            OMX_SetConfig(ComponentHandle, OMX_IndexConfigGrallocBufferParameter, &sGrallocBufferParam);
            bSetGrallocBufferParameter = OMX_TRUE;
        }
    }
#endif
    if(bAllocateNativeHandle){
        native_handle_t *handle_ptr = (native_handle_t *)(pBufferHdr->pBuffer);
        int fd = handle_ptr->data[0];
        pBufferHdr->pInputPortPrivate = (OMX_PTR)pBufferHdr->pBuffer;
        pBufferHdr->pBuffer = reinterpret_cast<OMX_U8*>(fd);
        #if 0//enable for debug purpose, copy shared input buffer to secure buffer.
        if(handle_ptr->data[2] == 0 && pBufferHdr->nFilledLen > 0){
            int len = pBufferHdr->nFilledLen;
            int fd1 = handle_ptr->data[1];
            char *buf0 = (char*)mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            char *buf1 = (char*)mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
            //copy data fron shared memory to secure buffer
            memcpy(buf0,buf1,len);
            munmap(buf0,len);
            munmap(buf1,len);
        }
        #endif
    }
    return OMX_EmptyThisBuffer(ComponentHandle, pBufferHdr);
}

OMX_ERRORTYPE FSLOMXWrapper::OnEvent(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_EVENTTYPE eEvent,
        OMX_IN OMX_U32 nData1,
        OMX_IN OMX_U32 nData2,
        OMX_IN OMX_PTR pEventData)
{
    FSLOMXWrapper * wrapper = (FSLOMXWrapper *)pAppData;
    if(wrapper->pCallbacks){
        if(eEvent == OMX_EventPortSettingsChanged && COLOR_INFO_NONE == wrapper->eColorInfoState){
            wrapper->eColorInfoState = COLOR_INFO_CHANGED;
            ALOGV("eColorInfoState = COLOR_INFO_CHANGED");
        }
        return wrapper->pCallbacks->EventHandler(hComponent,wrapper->pAppPrivateData,eEvent,nData1,nData2,pEventData);
    }else
        return OMX_ErrorUndefined;
}

OMX_ERRORTYPE FSLOMXWrapper::OnEmptyBufferDone(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_BUFFERHEADERTYPE *pBuffer){
    FSLOMXWrapper * wrapper = (FSLOMXWrapper *)pAppData;
    if(wrapper->pCallbacks){
        if(wrapper->bStoreMetaData || wrapper->bStoreANWBufferInMetadata){
            if(pBuffer && pBuffer->pInputPortPrivate){
                ALOGV("OnEmptyBufferDone change from %p",((METADATA_BUFFER *)(pBuffer->pBuffer))->pPhysicAddress);
                ((METADATA_BUFFER *)(pBuffer->pBuffer))->pPhysicAddress = pBuffer->pInputPortPrivate;
                ALOGV("OnEmptyBufferDone change to %p",pBuffer->pInputPortPrivate);
                pBuffer->pInputPortPrivate = NULL;
            }
        }
        if(wrapper->bAllocateNativeHandle){
            if(pBuffer && pBuffer->pInputPortPrivate){
                pBuffer->pBuffer = (OMX_U8*)pBuffer->pInputPortPrivate;
                pBuffer->pInputPortPrivate = NULL;
            }
        }
        return wrapper->pCallbacks->EmptyBufferDone(hComponent,wrapper->pAppPrivateData,pBuffer);
    }else
        return OMX_ErrorUndefined;
}


OMX_ERRORTYPE FSLOMXWrapper::OnFillBufferDone(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_BUFFERHEADERTYPE *pBuffer){
    FSLOMXWrapper * wrapper = (FSLOMXWrapper *)pAppData;
    if(wrapper->pCallbacks){
        if(wrapper->bANWOutputBuferForDecoder){
            if(pBuffer && pBuffer->pOutputPortPrivate){
                ALOGV("OnFillBufferDone change from %p,size=%d",pBuffer->pBuffer,pBuffer->nFilledLen);
                pBuffer->pBuffer = (OMX_U8*)pBuffer->pOutputPortPrivate;
                if(!(pBuffer->nFlags & OMX_BUFFERFLAG_EOS))
                    pBuffer->nFilledLen = 12;
                ALOGV("OnFillBufferDone change to %p,size=%d",pBuffer->pBuffer,pBuffer->nFilledLen);
                pBuffer->pOutputPortPrivate = NULL;
            }
        }

        int i = 0;
        OMX_BOOL find = OMX_FALSE;
        for(i = 0; i < wrapper->nBufferCnt; i++){
            if(wrapper->sBufferMapper[i].pBufferHdr == pBuffer){
                find = OMX_TRUE;
                break;
            }
        }

        ALOGV("OnFillBufferDone state %d", wrapper->eColorInfoState);

        OMX_BOOL bToWriteColorInfo = (find == OMX_TRUE && COLOR_INFO_TO_WRITE == wrapper->eColorInfoState) ? OMX_TRUE : OMX_FALSE;
        OMX_BOOL bToWriteCompressTableOffset = ((find == OMX_TRUE && wrapper->bTileCompressedFormat
            && wrapper->sBufferMapper[i].tileCompressTableOffsetWritten == OMX_FALSE)) ? OMX_TRUE : OMX_FALSE;

        if(bToWriteColorInfo == OMX_TRUE || bToWriteCompressTableOffset == OMX_TRUE){

            int fd_meta = 0;
            uint8_t *buf = NULL;
            uint8_t *bufWritePos = buf;
            int meta_size = 0;
            OMX_ERRORTYPE ret = OMX_ErrorNone;

            do{

                GraphicBuffer *pGraphicBuffer = wrapper->sBufferMapper[i].mGraphicBuffer.get();
                fsl::Memory *prvHandle = (fsl::Memory*)pGraphicBuffer->getNativeBuffer()->handle;

                fd_meta = prvHandle->fd_meta;
                uint32_t flag = 0;
                meta_size = sizeof(struct fsl::MetaData);
                if(bToWriteColorInfo == OMX_TRUE && bToWriteCompressTableOffset == OMX_TRUE){
                    meta_size += sizeof(struct fsl::MetaData);
                }

                if(fd_meta <= 0)
                    break;

                if(meta_size > ashmem_get_size_region(fd_meta)){
                    ALOGE("meta_size is too large");
                    break;
                }

                buf = (uint8_t*)mmap(0, meta_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_meta, 0);

                if(buf == NULL)
                    break;
                bufWritePos = buf;

                if(bToWriteColorInfo == OMX_TRUE){
                    OMX_CONFIG_VPU_COLOR_DESC vpuColorDesc;
                    OMX_INIT_STRUCT(&vpuColorDesc,OMX_CONFIG_VPU_COLOR_DESC);
                    vpuColorDesc.nPortIndex = 1;
                    ret = OMX_GetConfig(wrapper->ComponentHandle, OMX_IndexConfigDescribeColorInfo, &vpuColorDesc);

                    if(ret != OMX_ErrorNone)
                        break;

                    ColorAspects color;
                    //maybe need to map the value
                    color.mRange = (android::ColorAspects::Range)vpuColorDesc.fullRange;
                    color.mPrimaries = (android::ColorAspects::Primaries)vpuColorDesc.primaries;
                    color.mTransfer = (android::ColorAspects::Transfer)vpuColorDesc.transfer;
                    color.mMatrixCoeffs = (android::ColorAspects::MatrixCoeffs)vpuColorDesc.matrixCoeffs;
                    ALOGI("OnFillBufferDone bNeedWriteColorInfo %d,%d,%d,%d",
                        color.mRange,color.mPrimaries,color.mTransfer,color.mMatrixCoeffs);

                    flag = fsl::FLAGS_META_CHANGED;

                    //skip the flag field
                    memcpy(buf+4,&color,sizeof(ColorAspects));

                    OMX_CONFIG_VPU_STATIC_HDR_INFO hdrInfo;
                    OMX_INIT_STRUCT(&hdrInfo,OMX_CONFIG_VPU_STATIC_HDR_INFO);
                    hdrInfo.nPortIndex = 1;
                    hdrInfo.nID = 0;

                    ret = OMX_GetConfig(wrapper->ComponentHandle, OMX_IndexConfigDescribeHDRColorInfo, &hdrInfo);

                    if(ret != OMX_ErrorNone)
                        break;


                    HDRStaticInfo android_hdr;
                    android_hdr.mID = HDRStaticInfo::kType1;
                    android_hdr.sType1.mR.x = hdrInfo.mR[0];
                    android_hdr.sType1.mR.y = hdrInfo.mR[1];
                    android_hdr.sType1.mG.x = hdrInfo.mG[0];
                    android_hdr.sType1.mG.y = hdrInfo.mG[1];
                    android_hdr.sType1.mB.x = hdrInfo.mB[0];
                    android_hdr.sType1.mB.y = hdrInfo.mB[1];
                    android_hdr.sType1.mW.x = hdrInfo.mW[0];
                    android_hdr.sType1.mW.y = hdrInfo.mW[1];
                    android_hdr.sType1.mMaxDisplayLuminance = hdrInfo.mMaxDisplayLuminance;
                    android_hdr.sType1.mMinDisplayLuminance = hdrInfo.mMinDisplayLuminance;
                    android_hdr.sType1.mMaxContentLightLevel = hdrInfo.mMaxContentLightLevel;
                    android_hdr.sType1.mMaxFrameAverageLightLevel = hdrInfo.mMaxFrameAverageLightLevel;

                    ALOGI("OnFillBufferDone write HDR mR=%d,%d,mG=%d,%d,mB=%d,%d,mW=%d,%d mMaxDisplayLuminance=%d,%d,mMaxContentLightLevel=%d,mMaxFrameAverageLightLevel=%d"
                        ,hdrInfo.mR[0],hdrInfo.mR[1]
                        ,hdrInfo.mG[0],hdrInfo.mG[1]
                        ,hdrInfo.mB[0],hdrInfo.mB[1]
                        ,hdrInfo.mW[0],hdrInfo.mW[1]
                        ,hdrInfo.mMaxDisplayLuminance,hdrInfo.mMinDisplayLuminance
                        ,hdrInfo.mMaxContentLightLevel,hdrInfo.mMaxFrameAverageLightLevel);

                    memcpy(buf+4+sizeof(ColorAspects),&android_hdr,sizeof(HDRStaticInfo));
                    *((int*)buf) = flag;
                    bufWritePos += sizeof(struct fsl::MetaData);
                }

                if(bToWriteCompressTableOffset == OMX_TRUE){
                    flag = fsl::FLAGS_COMPRESSED_OFFSET;
                    OMX_CONFIG_VIDEO_TILE_COMPRESSED_OFFSET sVideoTileOffset;
                    OMX_INIT_STRUCT(&sVideoTileOffset,OMX_CONFIG_VIDEO_TILE_COMPRESSED_OFFSET);

                    ret = OMX_GetConfig(wrapper->ComponentHandle, OMX_IndexConfigVideoTileCompressedOffset, &sVideoTileOffset);

                    if(ret != OMX_ErrorNone)
                        break;
                    *(int*)bufWritePos = flag;
                    memcpy(bufWritePos + sizeof(OMX_U32), &sVideoTileOffset.nYOffset, sizeof(OMX_U32));
                    memcpy(bufWritePos + 2*sizeof(OMX_U32),
                        &sVideoTileOffset.nUVOffset, sizeof(OMX_U32));
                    wrapper->sBufferMapper[i].tileCompressTableOffsetWritten = OMX_TRUE;
                    ALOGI("onFillBufferDone write nYOffset=%u,nUVOffset=%u, %p", sVideoTileOffset.nYOffset, sVideoTileOffset.nUVOffset, (OMX_PTR)(unsigned long)prvHandle->phys);
                }

            }while(0);

            if(buf)
                munmap(buf,meta_size);
            if(fd_meta > 0)
                close(fd_meta);

            if(COLOR_INFO_TO_WRITE == wrapper->eColorInfoState)
                wrapper->eColorInfoState = COLOR_INFO_NONE;
        }

        return wrapper->pCallbacks->FillBufferDone(hComponent,wrapper->pAppPrivateData,pBuffer);
    }else
        return OMX_ErrorUndefined;
}

OMX_ERRORTYPE FSLOMXWrapper::FillThisBuffer(OMX_BUFFERHEADERTYPE* pBuffer){

    if(bANWOutputBuferForDecoder){
        VideoNativeMetadata * pBuf = (VideoNativeMetadata *)pBuffer->pBuffer;
        ALOGV("FillThisBuffer pBuf=%p,len=%d,type=%d",pBuf,pBuffer->nFilledLen,pBuf->eType);
        if(pBuf->eType == kMetadataBufferTypeANWBuffer){
            ANativeWindowBuffer *pWindowBuf = pBuf->pBuffer;
            buffer_handle_t tBufHandle = pWindowBuf->handle;
            fsl::Memory *pGrallocHandle = (fsl::Memory*) tBufHandle;
            OMX_PTR pPhys = NULL;

            GetHwBuffer((OMX_PTR)pGrallocHandle->base,&pPhys);
            if(pPhys == NULL){
                AddHwBuffer((OMX_PTR)pGrallocHandle->phys, (OMX_PTR)pGrallocHandle->base);
                nBufferCnt ++;
                ALOGV("AddHwBuffer phys=%p,virs=%p", (OMX_PTR)pGrallocHandle->phys, (OMX_PTR)pGrallocHandle->base);
            }

            ALOGV("FillThisBuffer from ptr=%p,len=%d",pBuffer->pBuffer,pBuffer->nFilledLen);
            pBuffer->pOutputPortPrivate = (OMX_PTR)pBuffer->pBuffer;
            pBuffer->pBuffer = (OMX_U8*)pGrallocHandle->base;
            pBuffer->nFilledLen = pGrallocHandle->size;
            ALOGV("FillThisBuffer after ptr=%p,len=%d",pBuffer->pBuffer,pBuffer->nFilledLen);
        }else
            return OMX_ErrorUndefined;
    }
    if(COLOR_INFO_CHANGED == eColorInfoState){
        eColorInfoState = COLOR_INFO_TO_WRITE;
        ALOGV("eColorInfoState = COLOR_INFO_TO_WRITE");
    }
    return OMX_FillThisBuffer(ComponentHandle, pBuffer);
}

OMX_CALLBACKTYPE FSLOMXWrapper::wrapperCallback = {&OnEvent,&OnEmptyBufferDone,&OnFillBufferDone};
// FSLOMXPlugin class

OMX_ERRORTYPE FSLOMXPlugin::makeComponentInstance(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(name == NULL)
        return OMX_ErrorBadParameter;

    FSLOMXWrapper *pWrapper = new FSLOMXWrapper(name);
    if(pWrapper == NULL)
        return OMX_ErrorInsufficientResources;

    OMX_HANDLETYPE handle = NULL;
    ret = OMX_GetHandle(&handle, (char*)name, (OMX_PTR)pWrapper, (OMX_CALLBACKTYPE*)&pWrapper->wrapperCallback);
    if(ret != OMX_ErrorNone){
        delete pWrapper;
        return ret;
    }

    *component = pWrapper->MakeWapper(handle);
    if(*component == NULL){
        delete pWrapper;
        return OMX_ErrorUndefined;
    }

    pWrapper->BackupCallbacks((OMX_CALLBACKTYPE*)callbacks, appData);

    LOGV("makeComponentInstance done, instance is: %p", *component);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FSLOMXPlugin::destroyComponentInstance(
            OMX_COMPONENTTYPE *component)
{
    LOGV("destroyComponentInstance, %p", component);

    FSLOMXWrapper *pWrapper = (FSLOMXWrapper*)component->pComponentPrivate;
    OMX_COMPONENTTYPE *hComponent = pWrapper->GetComponentHandle();

    if (pWrapper->IsTunneled()) {
        GMTunneledDecoderWrapper *pGMDecWrapper = GET_TUNNELED_WRAPPER(component);

        component->ComponentDeInit(component);
        delete pGMDecWrapper;
    }
    else
        OMX_FreeHandle(hComponent);

    delete pWrapper;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FSLOMXPlugin::enumerateComponents(
            OMX_STRING name,
            size_t size,
            OMX_U32 index)
{
    return OMX_ComponentNameEnum(name, size, index);
}

OMX_ERRORTYPE FSLOMXPlugin::getRolesOfComponent(
            const char *name,
            Vector<String8> *roles)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 numRoles;

    LOGV("Call getRolesOfComponent.\n");

    roles->clear();
    ret = OMX_GetRolesOfComponent((char*)name, &numRoles, NULL);
    if(ret != OMX_ErrorNone)
        return ret;

    if (numRoles > 0) {
        OMX_U8 **array = new OMX_U8 *[numRoles];
        OMX_S32 i;
        for (i = 0; i   < (OMX_S32)numRoles;   ++i) {
            array[i] = new OMX_U8[OMX_MAX_STRINGNAME_SIZE];
        }

        OMX_U32 numRoles2;
        ret = OMX_GetRolesOfComponent((char*)name, &numRoles2, array);
        if(ret != OMX_ErrorNone){
            for (i = 0; i < (OMX_S32)numRoles; ++i) {
                delete[] array[i];
                array[i] = NULL;
            }
            delete[] array;
            array = NULL;
            return ret;
        }
        for (i = 0; i < (OMX_S32)numRoles; ++i) {
            String8 s((const char   *)array[i]);
            roles->push(s);
            delete[] array[i];
            array[i] = NULL;
        }

        delete[] array;
        array   =   NULL;
    }

    return OMX_ErrorNone;
}

extern "C" {
    OMXPluginBase* createOMXPlugin()
    {
        LOGV("createOMXPlugin");
        return (new FSLOMXPlugin());
    }
}

}
