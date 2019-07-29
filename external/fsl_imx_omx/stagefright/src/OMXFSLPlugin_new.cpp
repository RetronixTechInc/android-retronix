/**
 *  Copyright (c) 2011-2015, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#if (ANDROID_VERSION <= ICS)
    #include <media/stagefright/OMXPluginBase.h>
    #include <media/stagefright/HardwareAPI.h>
#elif (ANDROID_VERSION >= JELLY_BEAN_42)
    #include <OMXPluginBase.h>
    #include <HardwareAPI.h>
    #include <media/hardware/MetadataBufferType.h>
#endif

#include <ui/GraphicBuffer.h>
#include <gralloc_priv.h>
#include <ui/Rect.h>
#include <ui/GraphicBufferMapper.h>

#include "OMX_Index.h"
#include "OMX_Implement.h"
#include "PlatformResourceMgrItf.h"

#if (ANDROID_VERSION >= JELLY_BEAN_42)
#define LOGE ALOGE
#define LOGV ALOGV
#endif

#if (ANDROID_VERSION >= MARSH_MALLOW_600)
#include "media/openmax/OMX_IndexExt.h"
#include "media/openmax/OMX_AudioExt.h"
#endif

#define MAX_BUFFER_CNT (32)

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
        FSLOMXWrapper();
        OMX_COMPONENTTYPE *MakeWapper(OMX_HANDLETYPE pHandle);
        OMX_COMPONENTTYPE *GetComponentHandle();

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
        OMX_ERRORTYPE GetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure) {
            return OMX_GetConfig(ComponentHandle, nParamIndex, pStructure);
        }
        OMX_ERRORTYPE SetConfig(OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure) {
            return OMX_SetConfig(ComponentHandle, nParamIndex, pStructure);
        }
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
                                OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer) {
            return OMX_UseBuffer(ComponentHandle, ppBuffer, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
        }
        OMX_ERRORTYPE AllocateBuffer(OMX_BUFFERHEADERTYPE** ppBuffer, OMX_U32 nPortIndex,
                                     OMX_PTR pAppPrivate, OMX_U32 nSizeBytes) {
            return OMX_AllocateBuffer(ComponentHandle, ppBuffer, nPortIndex, pAppPrivate, nSizeBytes);
        }
        OMX_ERRORTYPE FreeBuffer(OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBuffer);
        OMX_ERRORTYPE EmptyThisBuffer(OMX_BUFFERHEADERTYPE* pBuffer);
        OMX_ERRORTYPE FillThisBuffer(OMX_BUFFERHEADERTYPE* pBuffer) {
            return OMX_FillThisBuffer(ComponentHandle, pBuffer);
        }
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

    private:
        typedef struct {
            OMX_BUFFERHEADERTYPE *pBufferHdr;
            sp<GraphicBuffer> mGraphicBuffer;
        }BufferMapper;

        OMX_COMPONENTTYPE WrapperHandle;
        OMX_COMPONENTTYPE *ComponentHandle;
        OMX_BOOL bEnableNativeBuffers;
        OMX_U32 nNativeBuffersUsage;
        OMX_BOOL bStoreMetaData;
        OMX_BOOL bSetGrallocBufferParameter;
        BufferMapper sBufferMapper[MAX_BUFFER_CNT];
        OMX_S32 nBufferCnt;
        OMX_BOOL bGotPixelFormat;

        OMX_ERRORTYPE DoUseNativeBuffer(UseAndroidNativeBufferParams *pNativBufferParam);
        OMX_U32 ConvertAndroidToOMXColorFormat(OMX_U32 index);
        OMX_U32 ConvertOMXToAndroidColorFormat(OMX_U32 index);
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
    return GET_WRAPPER(hComponent)->SetCallbacks(pCbs, pAppData);
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

FSLOMXWrapper::FSLOMXWrapper()
{
    nBufferCnt = 0;
    ComponentHandle = NULL;
    nNativeBuffersUsage = GRALLOC_USAGE_SW_READ_NEVER | GRALLOC_USAGE_SW_WRITE_NEVER | GRALLOC_USAGE_FORCE_CONTIGUOUS;
    bStoreMetaData = OMX_FALSE;
    bSetGrallocBufferParameter = OMX_FALSE;
    bGotPixelFormat = OMX_FALSE;
    memset(&WrapperHandle, 0, sizeof(OMX_COMPONENTTYPE));
    memset(sBufferMapper, 0, sizeof(BufferMapper) * MAX_BUFFER_CNT);
    bEnableNativeBuffers = OMX_FALSE;
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

OMX_COMPONENTTYPE * FSLOMXWrapper::GetComponentHandle()
{
    return ComponentHandle;
}

typedef struct{
  OMX_U32 omx_format;
  OMX_U32 pixel_format;
}OMX_FORMAT_CONVERT_TABLE;
static OMX_FORMAT_CONVERT_TABLE format_table[] = {{OMX_COLOR_FormatYUV420SemiPlanar,HAL_PIXEL_FORMAT_YCbCr_420_SP},
                                {OMX_COLOR_FormatYUV420Planar,HAL_PIXEL_FORMAT_YCbCr_420_P},
                                {OMX_COLOR_Format16bitRGB565,HAL_PIXEL_FORMAT_RGB_565},
                                {OMX_COLOR_FormatYUV422Planar,HAL_PIXEL_FORMAT_YCbCr_422_P},
                                {OMX_COLOR_FormatYUV422SemiPlanar,HAL_PIXEL_FORMAT_YCbCr_422_SP}};
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
        case OMX_IndexParamEnableAndroidNativeBuffers:
            {
                EnableAndroidNativeBuffersParams *pParams = (EnableAndroidNativeBuffersParams*)pStructure;
                if(pParams->nPortIndex != 1)
                    return OMX_ErrorUnsupportedIndex;
                pParams->enable = bEnableNativeBuffers;
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
                *usageBits = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_FORCE_CONTIGUOUS;
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
    //once got the pixel format, it will report OMX color format when calling the function again.
    if(bEnableNativeBuffers && bGotPixelFormat && OMX_IndexParamPortDefinition == nParamIndex){
        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = (OMX_PARAM_PORTDEFINITIONTYPE*)pStructure;
        if(pPortDef->nPortIndex == 1 && OMX_PortDomainVideo == pPortDef->eDomain){
            pPortDef->format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)ConvertOMXToAndroidColorFormat(pPortDef->format.video.eColorFormat);
            bGotPixelFormat = OMX_FALSE;
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
        case OMX_IndexParamEnableAndroidNativeBuffers:
            {
                EnableAndroidNativeBuffersParams *pParams = (EnableAndroidNativeBuffersParams*)pStructure;
                if(pParams->nPortIndex != 1)
                    return OMX_ErrorUnsupportedIndex;
                bEnableNativeBuffers = pParams->enable;
                if(bEnableNativeBuffers)
                    bGotPixelFormat = OMX_TRUE;
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
                    info.nMaxBufferCnt = 20;
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
    else if(!strcmp(cParameterName, "OMX.google.android.index.storeGraphicBufferInMetaData"))
        *pIndexType = OMX_IndexParamStoreMetaDataInBuffers;
    else if(!strcmp(cParameterName, "OMX.google.android.index.useAndroidNativeBuffer"))
        *pIndexType = OMX_IndexParamUseAndroidNativeBuffer;
    else if(!strcmp(cParameterName, "OMX.google.android.index.prependSPSPPSToIDRFrames"))
        *pIndexType = OMX_IndexParamUseAndroidPrependSPSPPStoIDRFrames;
    else if(!strcmp(cParameterName, "OMX.google.android.index.disableAVCReorder"))
        *pIndexType = OMX_IndexParamVideoDecReorderDisable;
    else if(!strcmp(cParameterName, "OMX.google.android.index.prepareForAdaptivePlayback"))
        *pIndexType = OMX_IndexParamAndroidAdaptivePlayback;
    else
        return OMX_GetExtensionIndex(ComponentHandle, cParameterName, pIndexType);

    return OMX_ErrorNone;
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
    private_handle_t *prvHandle = (private_handle_t*)pGraphicBuffer->getNativeBuffer()->handle;

    OMX_PTR vAddr = NULL;
    pGraphicBuffer->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, &vAddr);
    if(vAddr == NULL) {
        LOGE("Failed to get native buffer virtual address.\n");
        return OMX_ErrorUndefined;
    }

    LOGV("native buffer handle %p, phys %p, virs %p, size %d\n", prvHandle, (OMX_PTR)prvHandle->phys, vAddr, prvHandle->size);

    AddHwBuffer((OMX_PTR)prvHandle->phys, vAddr);

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
    if (bStoreMetaData == OMX_TRUE && pBufferHdr->nFilledLen >= 8) {
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

        private_handle_t* pGrallocHandle;
        buffer_handle_t  tBufHandle;

        pTempBuffer++;

        if(bGraphicBuffer == OMX_TRUE){
            GraphicBuffer *buffer = (GraphicBuffer *)*pTempBuffer;
            tBufHandle = buffer->handle;
        }else{
            tBufHandle =  *((buffer_handle_t *)pTempBuffer);
        }

        pGrallocHandle = (private_handle_t*) tBufHandle;
        LOGV("Grallloc buffer recieved in metadata buffer 0x%x",pGrallocHandle );

        ((METADATA_BUFFER *)(pBufferHdr->pBuffer))->pPhysicAddress = \
           (OMX_PTR) pGrallocHandle->phys;
        LOGV("%s Gralloc=0x%x, phys = 0x%x", __FUNCTION__, pGrallocHandle,
                pGrallocHandle->phys);

        if (bSetGrallocBufferParameter == OMX_FALSE) {
            GRALLOC_BUFFER_PARAMETER sGrallocBufferParam;

            OMX_INIT_STRUCT(&sGrallocBufferParam, GRALLOC_BUFFER_PARAMETER);
            sGrallocBufferParam.nPortIndex = 0;

            sGrallocBufferParam.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
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

    return OMX_EmptyThisBuffer(ComponentHandle, pBufferHdr);
}

// FSLOMXPlugin class

OMX_ERRORTYPE FSLOMXPlugin::makeComponentInstance(
        const char *name,
        const OMX_CALLBACKTYPE *callbacks,
        OMX_PTR appData,
        OMX_COMPONENTTYPE **component)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("makeComponentInstance, appData: %p", appData);

    OMX_HANDLETYPE handle = NULL;
    ret = OMX_GetHandle(&handle, (char*)name, appData, (OMX_CALLBACKTYPE*)callbacks);
    if(ret != OMX_ErrorNone)
        return ret;

    FSLOMXWrapper *pWrapper = new FSLOMXWrapper;
    if(pWrapper == NULL)
        return OMX_ErrorInsufficientResources;

    *component = pWrapper->MakeWapper(handle);
    if(*component == NULL)
        return OMX_ErrorUndefined;

    LOGV("makeComponentInstance done, instance is: %p", *component);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FSLOMXPlugin::destroyComponentInstance(
            OMX_COMPONENTTYPE *component)
{
    LOGV("destroyComponentInstance, %p", component);

    FSLOMXWrapper *pWrapper = (FSLOMXWrapper*)component->pComponentPrivate;
    OMX_FreeHandle(pWrapper->GetComponentHandle());
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
