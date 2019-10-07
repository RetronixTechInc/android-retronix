/**
 *  Copyright 2017 NXP
 *
 *  The following programs are the sole property of NXP,
 *  and contain its proprietary and confidential information.
 *
 */

typedef struct GMTunneledDecoderWrapper {
        OMX_PTR pPrivateData;
        OMX_ERRORTYPE (*GetVersion)(OMX_HANDLETYPE handle, OMX_STRING pComponentName, OMX_VERSIONTYPE* pComponentVersion,
                                 OMX_VERSIONTYPE* pSpecVersion, OMX_UUIDTYPE* pComponentUUID);
        OMX_ERRORTYPE (*GetState)(OMX_HANDLETYPE handle, OMX_STATETYPE* pState);
        OMX_ERRORTYPE (*SendCommand)(OMX_HANDLETYPE handle, OMX_COMMANDTYPE Cmd, OMX_U32 nParam1, OMX_PTR pCmdData);
        OMX_ERRORTYPE (*GetConfig)(OMX_HANDLETYPE handle, OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE (*SetConfig)(OMX_HANDLETYPE handle, OMX_INDEXTYPE nParamIndex, OMX_PTR pStructure);
        OMX_ERRORTYPE (*TunnelRequest)(OMX_HANDLETYPE handle, OMX_U32 nPortIndex, OMX_HANDLETYPE hTunneledComp,
                                    OMX_U32 nTunneledPort, OMX_TUNNELSETUPTYPE* pTunnelSetup);
        OMX_ERRORTYPE (*UseBuffer)(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE** ppBuffer, OMX_U32 nPortIndex,
                                OMX_PTR pAppPrivate, OMX_U32 nSizeBytes, OMX_U8* pBuffer);
        OMX_ERRORTYPE (*AllocateBuffer)(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE** ppBuffer, OMX_U32 nPortIndex,
                                     OMX_PTR pAppPrivate, OMX_U32 nSizeBytes);
        OMX_ERRORTYPE (*FreeBuffer)(OMX_HANDLETYPE handle, OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBuffer);
        OMX_ERRORTYPE (*EmptyThisBuffer)(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE* pBuffer);
        OMX_ERRORTYPE (*FillThisBuffer)(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE* pBuffer);
        OMX_ERRORTYPE (*SetCallbacks)(OMX_HANDLETYPE handle, OMX_CALLBACKTYPE* pCbs, OMX_PTR pAppData);
        OMX_ERRORTYPE (*ComponentDeInit)(OMX_HANDLETYPE handle);
        OMX_ERRORTYPE (*UseEGLImage)(OMX_HANDLETYPE handle, OMX_BUFFERHEADERTYPE** ppBufferHdr, OMX_U32 nPortIndex,
                                  OMX_PTR pAppPrivate, void *eglImage);
        OMX_ERRORTYPE (*ComponentRoleEnum)(OMX_HANDLETYPE handle, OMX_U8 *cRole, OMX_U32 nIndex);

}GMTunneledDecoderWrapper;

GMTunneledDecoderWrapper * GMTunneledDecoderWrapperCreate(OMX_HANDLETYPE handle, OMX_CALLBACKTYPE *callbacks, OMX_PTR appData);

// EOF
