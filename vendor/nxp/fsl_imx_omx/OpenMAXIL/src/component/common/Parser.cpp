/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "Parser.h"
#include "string.h"

//#undef LOG_DEBUG
//#define LOG_DEBUG printf
//#undef LOG_LOG
//#define LOG_LOG printf

#define HTTP_CACHE_TIME 1
#define LOW_LATENCY_DROP_TIME 3*1000000
#define LOW_LATENCY_AUDIO_RESET_TS 500000

OMX_STRING Parser::GetMimeFromComponentRole(OMX_U8 *componentRole)
{
    struct RoleToMime {
              const char *mRole;
              const char *mVideoMime;
              const char *mVideoUnsupportMime;
              const char *mAudioMime;
              const char *mAudioUnsupportMime;
    };
    RoleToMime kRoleToMime[] = {
            { "parser.avi", "video/avi",        "video_unsupport/avi",          "audio/avi" ,           "audio_unsupport/avi"},
            { "parser.mp4", "video/mp4",        "video_unsupport/mp4",          "audio/mp4",            "audio_unsupport/mp4" },
            { "parser.asf", "video/x-ms-wmv",   "video_unsupport/x-ms-wmv",     "audio/x-ms-wma",       "audio_unsupport/x-ms-wma" },
            { "parser.mkv", "video/x-matroska",   "video_unsupport/x-matroska",     "audio/x-matroska",       "audio_unsupport/x-matroska" },
            { "parser.flv", "video/flv",        "video_unsupport/flv",          "audio/flv",            "audio_unsupport/flv" },
            { "parser.mpg2","video/mp2p",       "video_unsupport/mp2p",         "audio/mp2p",           "audio_unsupport/mp2p" },
            { "parser.rmvb","video/rmff",       "video_unsupport/rmff",         "audio/rmff",           "audio_unsupport/rmff" },
            { "parser.mp3", NULL,               NULL,                           "audio/mpeg",           "audio_unsupport/mpeg" },
            { "parser.aac", NULL,               NULL,                           "audio/aac",            "audio_unsupport/aac" },
            { "parser.ac3", NULL,               NULL,                           "audio/ac3",            "audio_unsupport/ac3" },
            { "parser.wav", NULL,               NULL,                           "audio/x-wav",          "audio_unsupport/x-wav" },
            { "parser.flac",NULL,               NULL,                           "audio/flac",           "audio_unsupport/flac" },
            { "parser.ogg", NULL,               NULL,                           "application/ogg",      "application_unsupport/ogg" },
            { "parser.amr", NULL,               NULL,                           "audio/amr",            "audio_unsupport/amr" },

    };

    for (OMX_U32 i = 0; i < sizeof(kRoleToMime) / sizeof(kRoleToMime[0]); ++i) {
        if (!strcmp((const char *)componentRole, kRoleToMime[i].mRole)) {
            if(nNumAvailVideoStream > 0){
                if(bHasVideoTrack)
                    return (OMX_STRING)kRoleToMime[i].mVideoMime;
                else
                    return (OMX_STRING)kRoleToMime[i].mVideoUnsupportMime;
            }
            else{
                if (bHasAudioTrack)
                    return (OMX_STRING)kRoleToMime[i].mAudioMime;
                else
                    return (OMX_STRING)kRoleToMime[i].mAudioUnsupportMime;
            }
        }
    }

    return NULL;
}

/*****************************************************************************
 * Function:    appLocalFileOpen
 *
 * Description: Implements to Open the file.
 *
 * Return:      File Pointer.
 ****************************************************************************/
static FslFileHandle appLocalFileOpen ( const uint8 * file_path, const uint8 * mode, void * context)
{
    if(NULL == mode || !context)
    {
        LOG_ERROR("appLocalFileOpen: Invalid parameter\n");
        return NULL;
    }

    Parser *h = (Parser *)context;
    CPresult nContentPipeResult;
    FslFileHandle sourceFileHandle;
    OMX_BOOL bStreaming = h->IsStreamingSource();

    LOG_DEBUG("appLocalFileOpen parser %p, pipehandle %p file_path %s, h->pMediaName %s",h, h->hPipeHandle, file_path, h->pMediaName);

    if(h->hPipeHandle){
        // reuse hPipeHandle, do not need to call open
        sourceFileHandle = h->hPipeHandle;

        nContentPipeResult = h->hPipe->SetPosition(sourceFileHandle, 0, CP_OriginBegin);
        if (nContentPipeResult != 0)
            LOG_ERROR("appLocalFileOpen fail to set pipe position to 0");
    }
    else{
        nContentPipeResult = h->hPipe->Open(&sourceFileHandle, (char *)h->pMediaName, CP_AccessRead);
        if(nContentPipeResult != 0){
            LOG_ERROR("appLocalFileOpen: Fail to open file \n");
            return NULL;
        }
    }

    if(bStreaming == OMX_TRUE) {
        if(OMX_ErrorNone != h->CheckContentAvailableBytes(sourceFileHandle, 1)){
            LOG_ERROR("appLocalFileOpen: CheckContentAvailableBytes fail");
            if(!h->hPipeHandle)
                h->hPipe->Close(sourceFileHandle);
            return NULL;
        }

        nContentPipeResult = h->hPipe->SetPosition(sourceFileHandle, 0, CP_OriginSeekable);
        if(nContentPipeResult == KD_EUNSEEKABLE){
            printf("appLocalFileOpen() set to live mode!!!");
            h->SetLiveSource(OMX_TRUE);
        }
    }

    return sourceFileHandle;
}

/*****************************************************************************
 * Function:    appLocalReadFile
 *
 * Description: Implements to read the stream from the file.
 *
 * Return:      Total number of bytes read.
 *
 ****************************************************************************/
static uint32  appLocalReadFile( FslFileHandle file_handle, void * buffer, uint32 nb, void * context)
{
    Parser *h = (Parser *)context;
    CPresult nContentPipeResult;
    uint32 dwRestBytes;
    uint32 dwReadBytes;
    CPint64 qwPos;
    CPint64 size;
    if (!file_handle || !context || !buffer)
        return 0;

    if(h->bStopReading){
        return 0;
    }
#if 0
    if(OMX_ErrorNone != h->CheckContentAvailableBytes(file_handle, nb))
        return 0;

    nContentPipeResult = h->hPipe->Read(file_handle, (CPbyte *)buffer, nb);
    if(nContentPipeResult == 0)
    {
        return nb; /* bytes read */
    }
    else {
        return 0; /* take as nothing read */
    }
#else
    LOG_LOG("appLocalReadFile nb %d(0x%x)", nb,nb);

    if(OMX_ErrorNone != h->CheckContentAvailableBytes(file_handle, nb)){
        if(h->IsStreamingSource()){
            return 0;
        }
        LOG_DEBUG("appLocalReadFile NOT AVAILABLE");
        nContentPipeResult = h->hPipe->GetPosition(file_handle,&qwPos);
        if(nContentPipeResult != 0){
            return 0;
        }
        nContentPipeResult = h->hPipe->SetPosition(file_handle, 0, CP_OriginEnd);
        if(nContentPipeResult != 0){
            return 0;
        }
        nContentPipeResult = h->hPipe->GetPosition(file_handle,&size);
        if(nContentPipeResult != 0){
            return 0;
        }
        nContentPipeResult = h->hPipe->SetPosition(file_handle,qwPos, CP_OriginBegin);
        if(nContentPipeResult != 0){
            return 0;
        }
        h->nContentLen = size;
        dwRestBytes = ((CPint64)h->nContentLen >  qwPos) ? (uint32)(h->nContentLen -  qwPos) : 0;
        dwReadBytes = (dwRestBytes > nb) ? nb : dwRestBytes;
        if(dwReadBytes== 0){
            return 0;
        }
    }else{
        dwReadBytes = nb;
    }

    if(h->Lock)
        fsl_osal_mutex_lock(h->Lock);

    nContentPipeResult = h->hPipe->Read(file_handle, (CPbyte *)buffer, dwReadBytes);

    if(h->Lock)
        fsl_osal_mutex_unlock(h->Lock);

    if(nContentPipeResult == 0)
    {
        LOG_LOG("appLocalReadFile dwReadBytes=%d",dwReadBytes);
        return dwReadBytes;
    }
    else
    {
        LOG_LOG("appLocalReadFile 0");
        return 0;
    }
#endif

}

/*****************************************************************************
 * Function:    appLocalSeekFile
 *
 * Description: seek the file. whence: SEEK_SET, SEEK_CUR, or SEEK_END
 *
 * Return:      File Pointer.
 *
 ****************************************************************************/

static int32   appLocalSeekFile( FslFileHandle file_handle, int64 offset, int32 whence, void * context)
{

    Parser *h = (Parser *)context;
    CPresult nContentPipeResult = 0;
    if (!file_handle || !context)
        return -1;

    LOG_LOG("appLocalSeekFile offset %lld, whence %d", offset, whence);

    if (SEEK_CUR == whence)
        nContentPipeResult = h->hPipe->SetPosition(file_handle,offset, CP_OriginCur);
    else if (SEEK_SET == whence)
        nContentPipeResult = h->hPipe->SetPosition(file_handle,offset, CP_OriginBegin);
    else if (SEEK_END == whence)
        nContentPipeResult = h->hPipe->SetPosition(file_handle,offset, CP_OriginEnd);

    if( 0 == nContentPipeResult) /* success */
        return 0;

    LOG_ERROR("appLocalFileSeek fail %d", nContentPipeResult);
    return -1;
}


/*****************************************************************************
 * Function:    appLocalGetCurrentFilePos
 *
 * Description: Gets file position.
 *
 * Return:      File Pointer.
 *
 ****************************************************************************/
static int64  appLocalGetCurrentFilePos( FslFileHandle file_handle, void * context)
{

    CPint64 pos;
    Parser *h = (Parser *)context;
    CPresult nContentPipeResult;
    if (!file_handle || !context)
        return -1;

    nContentPipeResult = h->hPipe->GetPosition(file_handle,&pos);
    if (0 == nContentPipeResult)
        return pos;
    else
        return -1;
}

/*****************************************************************************
 * Function:    appLocalFileSize. For source file only
 *
 * Description: get the size of the file
 *
 * Return:      File Pointer.
 *
 ****************************************************************************/

static int64   appLocalFileSize( FslFileHandle file_handle, void * context)
{
	Parser *h = (Parser *)context;

    if(NULL == file_handle)
    {
        file_handle = appLocalFileOpen((const uint8*) h->pMediaName,(const uint8*) "rb", context);
    }

    if(file_handle)
    {
        CPint64 size;
        CPint64 pos;
        CPresult nContentPipeResult;
        Parser *h = (Parser *)context;
        nContentPipeResult = h->hPipe->GetPosition(file_handle,&pos); if (0 != nContentPipeResult) return -1;
        nContentPipeResult = h->hPipe->SetPosition(file_handle, 0, CP_OriginEnd);if (0 != nContentPipeResult) return -1;
        nContentPipeResult = h->hPipe->GetPosition(file_handle,&size); if (0 != nContentPipeResult) return -1;
        nContentPipeResult = h->hPipe->SetPosition(file_handle,pos, CP_OriginBegin);if (0 != nContentPipeResult) return -1;

        h->nContentLen = size;
        LOG_INFO("appLocalFileSize: Content Len = %lld\n", h->nContentLen);

        return size;
    }
    else
        return -1;
}


/*****************************************************************************
 * Function:    appLocalFileClose
 *
 * Description: Implements to close the file.
 *
 * Return:      Sucess or EOF.
 *
 ****************************************************************************/
static int32 appLocalFileClose( FslFileHandle file_handle, void * context)
{
	Parser *h = (Parser *)context;

    LOG_DEBUG("appLocalFileClose parser %p, h->hPipeHandle %p, file_handle %x pMediaName %s",h, h->hPipeHandle, file_handle, h->pMediaName);

    if(file_handle)
    {
        CPresult nContentPipeResult;
        Parser *h = (Parser *)context;
        OMX_BOOL bStreaming = h->IsStreamingSource();

        // reusing pipeHandle, let GMPlayer to close it
        if(h->hPipeHandle == file_handle)
            return 0;

        if(bStreaming == OMX_TRUE)
            h->SendEvent(OMX_EventBufferingData, 0, 0, NULL);
        nContentPipeResult = h->hPipe->Close(file_handle);
        if(bStreaming == OMX_TRUE)
            h->SendEvent(OMX_EventBufferingDone, 0, 0, NULL);
        file_handle = NULL;

        if (0 != nContentPipeResult)
            return -1;
    }

    return 0;
}

static int64   appLocalCheckAvailableBytes(FslFileHandle file_handle, int64 bytesRequested, void * context)
{
    Parser *h = (Parser *)context;
    if(file_handle)
    {
        LOG_LOG("appLocalCheckAvailableBytes request %lld", bytesRequested);
        if(OMX_ErrorNone != h->CheckContentAvailableBytes(file_handle, bytesRequested))
            return 0;
    }
    return bytesRequested;
}


/*****************************************************************************
 * Function:    appLocalGetFlag
 *
 * Description: Implements to get properties of file source
 *
 * Return:      flag of properties
 *
 ****************************************************************************/
static uint32  appLocalGetFlag( FslFileHandle file_handle, void * context)
{
    uint32 flag = 0;
    Parser *h = (Parser *)context;

    if(h->IsLiveSource()){
        flag |= FILE_FLAG_NON_SEEKABLE;
        flag |= FILE_FLAG_READ_IN_SEQUENCE;
    }
    if(h->IsStreamingSource())
        flag |= FILE_FLAG_READ_IN_SEQUENCE;

    LOG_DEBUG("appLocalGetFlag %x", flag);

    return flag;
}

static void *appLocalCalloc(uint32 TotalNumber, uint32 TotalSize)
{
    void *PtrCalloc = NULL;

    if((0 == TotalSize)||(0==TotalNumber))
        LOG_WARNING("\nWarning: ZERO size IN LOCAL CALLOC");

	PtrCalloc = FSL_MALLOC(TotalNumber*TotalSize);

	if (PtrCalloc == NULL) {

		LOG_ERROR("\nError: MEMORY FAILURE IN LOCAL CALLOC");
                return NULL;
	}
    fsl_osal_memset(PtrCalloc, 0, TotalSize*TotalNumber);
	return (PtrCalloc);
}


/*****************************************************************************
 * Function:    appLocalMalloc
 *
 * Description: Implements the local malloc
 *
 * Return:      Value of the address.
 *
 ****************************************************************************/

static void* appLocalMalloc (uint32 TotalSize)
{

	void *PtrMalloc = NULL;

    if(0 == TotalSize)
        LOG_WARNING("\nWarning: ZERO size IN LOCAL MALLOC");

	PtrMalloc = FSL_MALLOC(TotalSize);

	if (PtrMalloc == NULL) {

		LOG_ERROR("\nError: MEMORY FAILURE IN LOCAL MALLOC");
	}
	return (PtrMalloc);
}


/*****************************************************************************
 * Function:    appLocalFree
 *
 * Description: Implements to Free the memory.
 *
 * Return:      Void
 *
 ****************************************************************************/
static void appLocalFree (void *MemoryBlock)
{
    if(MemoryBlock)
        FSL_FREE(MemoryBlock);
}


/*****************************************************************************
 * Function:    appLocalReAlloc
 *
 * Description: Implements to Free the memory.
 *
 * Return:      Void
 *
 ****************************************************************************/
static void * appLocalReAlloc (void *MemoryBlock, uint32 TotalSize)
{
    void *PtrMalloc = NULL;

    if(0 == TotalSize)
        LOG_WARNING("\nWarning: ZERO size IN LOCAL REALLOC");

    PtrMalloc = (void *)FSL_REALLOC(MemoryBlock, TotalSize);
    if (PtrMalloc == NULL) {
    LOG_ERROR("\nError: MEMORY FAILURE IN LOCAL REALLOC");
    }

    return PtrMalloc;
}

Parser::Parser()
{
    ComponentVersion.s.nVersionMajor = 0x0;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x0;
    ComponentVersion.s.nStep = 0x0;
    bInContext = OMX_FALSE;
    nPorts = 3;
    hPipe = NULL;
    hPipeHandle = NULL;
    pAudioOutBufHdr = pVideoOutBufHdr = pClockBufHdr = NULL;
    nClockScale = 1*Q16_SHIFT;
    nMediaNameLen = 0;
    bAudioNewSegment = OMX_TRUE;
    bVideoNewSegment = OMX_TRUE;
    bSubtitleNewSegment=OMX_TRUE;
    bAudioEOS = OMX_FALSE;
    bVideoEOS = OMX_FALSE;
    bSubtitleEOS = OMX_FALSE;
    sCurrAudioTime = 0;
    sCurrVideoTime = 0;
    sCurrSubtitleTime=0;
    pMediaName = NULL;
    hMedia = NULL;
    OMX_INIT_STRUCT(&sSeekMode, OMX_TIME_CONFIG_SEEKMODETYPE);
    sSeekMode.eType = OMX_TIME_SeekModeFast;
    sAudioSeekPos = 0;
    sVideoSeekPos = 0;
    sSubtitleSeekPos=0;
    bSeekable = OMX_FALSE;
    OMX_INIT_STRUCT(&sAudioPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    OMX_INIT_STRUCT(&sVideoPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE);
    sAudioPortFormat.eEncoding = OMX_AUDIO_CodingAutoDetect;
    sVideoPortFormat.eCompressionFormat = OMX_VIDEO_CodingAutoDetect;
    bVideoBOS = OMX_FALSE;
    nNumAvailAudioStream = 0;
    nNumAvailVideoStream = 0;
    nNumAvailSubtitleStream = 0;
    nActiveAudioNum = 0;
	nActiveAudioNumToClient = 0;
	nActiveSubtitleNumToClient=-1;	//not actived
	nActiveVideoNum = 0;
	nActiveSubtitleNum = 0;
    usDuration = 0;
    nAudioDuration = 0;
    nVideoDuration = 0;
    nCommonRotate = 0;
    bStopReading = OMX_FALSE;

    fsl_osal_memset (&fileOps, 0, sizeof(FslFileStream));
    fileOps.Open = appLocalFileOpen;
    fileOps.Read= appLocalReadFile;
    fileOps.Seek = appLocalSeekFile;
    fileOps.Tell = appLocalGetCurrentFilePos;
    fileOps.Size= appLocalFileSize;
    fileOps.Close = appLocalFileClose;
    fileOps.CheckAvailableBytes = appLocalCheckAvailableBytes;
    fileOps.GetFlag = appLocalGetFlag;

    memOps.Calloc = appLocalCalloc;
    memOps.Malloc = appLocalMalloc;
    memOps.Free= appLocalFree;
    memOps.ReAlloc= appLocalReAlloc;

    fsl_osal_memset(nAudioTrackNum, 0, sizeof(OMX_U32)*MAX_AVAIL_TRACK);
    fsl_osal_memset(nVideoTrackNum, 0, sizeof(OMX_U32)*MAX_AVAIL_TRACK);
    fsl_osal_memset(nSubtitleTrackNum, 0, sizeof(OMX_U32)*MAX_AVAIL_TRACK);
    OMX_INIT_STRUCT(&sCompRole, OMX_PARAM_COMPONENTROLETYPE);
	fsl_osal_memset(sCompRole.cRole, 0, OMX_MAX_STRINGNAME_SIZE);
    numTracks = 0;
    fsl_osal_memset(index_file_name, 0, INDEX_FILE_NAME_MAX_LEN);
    bSendAudioFrameFirst = OMX_FALSE;
    sActualVideoSeekPos = sActualAudioSeekPos =sActualSubtitleSeekPos= 0;
    bHasVideoTrack = bHasAudioTrack = OMX_FALSE;
    bAudioCodecDataSent = bVideoCodecDataSent = OMX_FALSE;
    bStreamCorrupted = OMX_FALSE;
    bSkip2Iframe = OMX_FALSE;
    bAudioActived = bVideoActived = bSubtitleActived = OMX_FALSE;
    OMX_INIT_STRUCT(&sMatadataItemCount, OMX_CONFIG_METADATAITEMCOUNTTYPE);
	for (OMX_U32 i = 0; i < MAX_MATADATA_NUM; i ++)
	{
		psMatadataItem[i] = NULL;
	}
    isLiveSource = OMX_FALSE;
	bGetMetadata = OMX_FALSE;
    isStreamingSource = OMX_FALSE;
    nPreCacheSize = 0;
    nCheckCacheOff = 0;
    nContentLen = 0;
    bAbortBuffering = OMX_FALSE;
    nVideoCodecDataLen = 0;
    bInsertVideoCodecData = OMX_FALSE;
    Lock = NULL;
    bLowLatency = OMX_FALSE;
    bDropAudio= OMX_FALSE;
    playbackMode = NORMAL_MODE;
}


OMX_ERRORTYPE Parser::CheckContentAvailableBytes(OMX_PTR file_handle, OMX_U32 nb)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    CPresult nContentPipeResult;
    CP_CHECKBYTESRESULTTYPE eReady;
    OMX_U32 cBytes = nb;

    if(file_handle == NULL)
        return OMX_ErrorBadParameter;

    nContentPipeResult = hPipe->CheckAvailableBytes(file_handle, cBytes, &eReady);
    if(nContentPipeResult != 0
        || (eReady != CP_CheckBytesOk && eReady != CP_CheckBytesInsufficientBytes))
    {
        LOG_ERROR("Parser::CheckContentAvailableBytes fail eReady %x", eReady);
        return OMX_ErrorNoMore;
    }

    if (eReady == CP_CheckBytesInsufficientBytes) {
        LOG_DEBUG("Resource is NOT available: %d\n", cBytes);
        if(isStreamingSource != OMX_TRUE)
            return OMX_ErrorNoMore;
        else {
            SendEvent(OMX_EventBufferingData, 0, 0, NULL);
            cBytes = nPreCacheSize;
            if(cBytes == 0)
                cBytes = MAX(nb, 16*1024);
            else {
                nCheckCacheOff = 0;
                if(0 == hPipe->GetPosition(file_handle, &nCheckCacheOff))
                    nCheckCacheOff += cBytes;
            }

            cBytes = MAX(cBytes, nb);
            while(1) {
                fsl_osal_sleep(10000);
                if (bAbortBuffering == OMX_TRUE) {
                    printf("Parser::CheckContentAvailableBytes Abort buffering data.\n");
                    SendEvent(OMX_EventBufferingDone, 0, 0, NULL);
                    return OMX_ErrorUndefined;
                }
                if (OMX_TRUE == bHasCmdToProcess()) {
                    printf("Has command to process in parser component, abort buffering data.\n");
                    SendEvent(OMX_EventBufferingDone, 0, 0, NULL);
                    break;
                }
                nContentPipeResult = hPipe->CheckAvailableBytes(file_handle,cBytes,&eReady);
                if(nContentPipeResult == 0 && eReady == CP_CheckBytesInsufficientBytes)
                    continue;
                else{
                    SendEvent(OMX_EventBufferingDone, 0, 0, NULL);
                    if(eReady == CP_CheckBytesOk)
                        break;
                    else{
                        LOG_ERROR("Parser::CheckContentAvailableBytes fail: eReady %x\n", eReady);
                        return OMX_ErrorNoMore;
                    }
                }
            }
        }
    }

    if(isStreamingSource == OMX_TRUE && nContentLen > 0 && nCheckCacheOff > 0) {
        hPipe->CheckAvailableBytes(file_handle, -1, &eReady);
        OMX_S32 nAvailabe = (OMX_S32)eReady;
        CPint64 nPos = 0;
        hPipe->GetPosition(file_handle, &nPos);
        nPos += eReady;
        if(nPos >= nCheckCacheOff) {
            OMX_S32 nPercentage = 100 * nPos / nContentLen;
            SendEvent(OMX_EventBufferingUpdate, nPercentage, 0, NULL);
            nCheckCacheOff = nPos + nPreCacheSize;
        }
        if((OMX_U64)nPos >= nContentLen) {
            SendEvent(OMX_EventBufferingUpdate, 100, 0, NULL);
            nCheckCacheOff = 0;
        }
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE Parser::InitComponent()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    if(E_FSL_OSAL_SUCCESS != fsl_osal_mutex_init(&Lock, fsl_osal_mutex_normal)) {
        ret = OMX_ErrorInsufficientResources;
        return ret;
    }

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = AUDIO_OUTPUT_PORT;
    sPortDef.eDir = OMX_DirOutput;
    sPortDef.eDomain = OMX_PortDomainAudio;
    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_TRUE;
    sPortDef.nBufferCountMin = 1;
    sPortDef.nBufferCountActual = 3;
    sPortDef.nBufferSize = DEFAULT_PARSER_AUDIO_OUTPUT_BUFSIZE;
    ret = ports[AUDIO_OUTPUT_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for port[%d] failed.\n", AUDIO_OUTPUT_PORT);
        return ret;
    }

    sPortDef.nPortIndex = VIDEO_OUTPUT_PORT;
    sPortDef.eDir = OMX_DirOutput;
    sPortDef.eDomain = OMX_PortDomainVideo;
    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_TRUE;
    sPortDef.nBufferCountMin = 1;
    sPortDef.nBufferCountActual = 5;
    sPortDef.nBufferSize = DEFAULT_PARSER_OUTPUT_BUFSIZE;
    ret = ports[VIDEO_OUTPUT_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for port[%d] failed.\n", VIDEO_OUTPUT_PORT);
        return ret;
    }

	sPortDef.nPortIndex = CLOCK_PORT;
	sPortDef.eDir = OMX_DirInput;
	sPortDef.eDomain = OMX_PortDomainOther;
	sPortDef.bPopulated = OMX_FALSE;
	sPortDef.bEnabled = OMX_FALSE;
	sPortDef.nBufferCountMin = 1;
	sPortDef.nBufferCountActual = 1;
	sPortDef.nBufferSize = sizeof(OMX_TIME_MEDIATIMETYPE);
	sPortDef.format.other.eFormat = OMX_OTHER_FormatTime;
	ret = ports[CLOCK_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for port[%d] failed.\n", CLOCK_PORT);
        return ret;
    }

    if(!hPipe)
    {
        ret = OMX_GetContentPipe((void **)&hPipe, (OMX_STRING)"LOCAL_FILE_PIPE_NEW");
    }

    return ret;
}

OMX_ERRORTYPE Parser::DeInitComponent()
{
    if(Lock != NULL) {
        fsl_osal_mutex_destroy(Lock);
        Lock = NULL;
    }

    if (pMediaName)
    {
        FSL_FREE(pMediaName);
        pMediaName = NULL;
    }

	for (OMX_U32 i = 0; i < MAX_MATADATA_NUM; i ++)
	{
		FSL_FREE(psMatadataItem[i]);;
	}

    return OMX_ErrorNone;
}


OMX_ERRORTYPE Parser::GetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

	switch ((int)nParamIndex) {
		case OMX_IndexParamStandardComponentRole:
			{
                OMX_PARAM_COMPONENTROLETYPE *pRole;
                pRole = (OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure;

                OMX_CHECK_STRUCT(pRole, OMX_PARAM_COMPONENTROLETYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;

				fsl_osal_strcpy((OMX_STRING)pRole->cRole, (OMX_STRING)sCompRole.cRole);

				break;
			}
		case OMX_IndexParamContentURI:
            {
                OMX_PARAM_CONTENTURITYPE * pContentURI = (OMX_PARAM_CONTENTURITYPE *)pComponentParameterStructure;

                strcpy((char *)&pContentURI->contentURI,(const char*)pMediaName);
            }
            break;
		case OMX_IndexParamNumAvailableStreams:
			{
				OMX_PARAM_U32TYPE *sU32Type = (OMX_PARAM_U32TYPE*)pComponentParameterStructure;
				if (sU32Type->nPortIndex == VIDEO_OUTPUT_PORT)
				{
	                sU32Type->nU32 = nNumAvailVideoStream;
				}
				else if (sU32Type->nPortIndex == AUDIO_OUTPUT_PORT)
				{
	                sU32Type->nU32 = nNumAvailAudioStream;
				}
				break;
			}
		case OMX_IndexParamActiveStream:
			{
				OMX_PARAM_U32TYPE *sU32Type = (OMX_PARAM_U32TYPE*)pComponentParameterStructure;
				if (sU32Type->nPortIndex == VIDEO_OUTPUT_PORT)
				{
	                sU32Type->nU32 = nActiveVideoNum;
				}
				else if (sU32Type->nPortIndex == AUDIO_OUTPUT_PORT)
				{
	                sU32Type->nU32 = nActiveAudioNumToClient;
				}
				break;
			}
        case OMX_IndexParamTrackDuration:
            {
                OMX_TRACK_DURATION *pTrackDuration = (OMX_TRACK_DURATION *)pComponentParameterStructure;
                if (pTrackDuration->nPortIndex == VIDEO_OUTPUT_PORT)
                {
                    pTrackDuration->sTrackDuration = (OMX_TICKS)nVideoDuration;
                }
                else if (pTrackDuration->nPortIndex == AUDIO_OUTPUT_PORT)
                {
                    pTrackDuration->sTrackDuration = (OMX_TICKS)nAudioDuration;
                }
                break;
            }
        case OMX_IndexParamMediaSeekable:
            {
                OMX_PARAM_CAPABILITY  *pMediaSeekable;
                pMediaSeekable    = (OMX_PARAM_CAPABILITY *)pComponentParameterStructure;
                pMediaSeekable->bCapability = bSeekable;
                break;
            }
         case OMX_IndexParamMediaDuration:
            {
                OMX_TIME_CONFIG_TIMESTAMPTYPE  *pMediaDuration
                    = (OMX_TIME_CONFIG_TIMESTAMPTYPE  *)pComponentParameterStructure;
                pMediaDuration->nTimestamp  = (OMX_TICKS)usDuration;
                break;
            }
	case OMX_IndexParamSubtitleNumAvailableStreams:
	    OMX_S32 *pCnt;
	     pCnt= (OMX_S32*)pComponentParameterStructure;
            *pCnt=nNumAvailSubtitleStream;
	     break;
       case OMX_IndexParamSubtitleSelect:
            OMX_S32* pIndex;
	     pIndex =(OMX_S32*)pComponentParameterStructure;
            *pIndex=nActiveSubtitleNumToClient;
            break;
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }


    return ret;
}

OMX_ERRORTYPE Parser::SetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

	switch ((int)nParamIndex) {
		case OMX_IndexParamStandardComponentRole:
			{
                OMX_PARAM_COMPONENTROLETYPE *pRole;
                pRole = (OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure;

                OMX_CHECK_STRUCT(pRole, OMX_PARAM_COMPONENTROLETYPE, ret);
                if(ret != OMX_ErrorNone)
                    break;

				fsl_osal_strcpy( (fsl_osal_char *)sCompRole.cRole, (fsl_osal_char *)pRole->cRole);
				break;
			}
		case OMX_IndexParamCustomContentPipe:
			hPipe =(CP_PIPETYPE* ) (((OMX_PARAM_CONTENTPIPETYPE *)pComponentParameterStructure)->hPipe);
			break;
		case OMX_IndexParamCustomContentPipeHandle:
        {
			hPipeHandle =((OMX_PARAM_CONTENTPIPEHANDLETYPE *)pComponentParameterStructure)->hPipeHandle;
            if(hPipeHandle && hPipe){
                CPresult nContentPipeResult = hPipe->SetPosition(hPipeHandle, 0, CP_OriginSeekable);
                if(nContentPipeResult == KD_EUNSEEKABLE){
                    printf("Parser SetParameter: set to live mode!!!");
                    isLiveSource = OMX_TRUE;
                }
            }

			break;
        }
		case OMX_IndexParamContentURI:
		{
			OMX_PARAM_CONTENTURITYPE * pContentURI = (OMX_PARAM_CONTENTURITYPE *)pComponentParameterStructure;
			nMediaNameLen = strlen((const char *)&(pContentURI->contentURI)) + 1;

            pMediaName = (OMX_S8 *)FSL_MALLOC(nMediaNameLen);
            if (!pMediaName)
            {
                ret = OMX_ErrorInsufficientResources;
                break;
            }
			strcpy((char *)pMediaName, (const char *)&(pContentURI->contentURI));
                        if(fsl_osal_strncmp((fsl_osal_char*)pMediaName, "http://", 7) == 0
                                || fsl_osal_strncmp((fsl_osal_char*)pMediaName, "udp://", 6) == 0
                                || fsl_osal_strncmp((fsl_osal_char*)pMediaName, "rtsp://", 7) == 0
                                || fsl_osal_strncmp((fsl_osal_char*)pMediaName, "rtp://", 6) == 0
                                || fsl_osal_strncmp((fsl_osal_char*)pMediaName, "mms://", 6) == 0)
                            isStreamingSource = OMX_TRUE;
                        if(fsl_osal_strncmp((fsl_osal_char*)pMediaName, "udp://", 6) == 0
                                || fsl_osal_strncmp((fsl_osal_char*)pMediaName, "rtp://", 6) == 0
                            )
                            isLiveSource = OMX_TRUE;

			break;
		}
        case OMX_IndexParamActiveStream:
            OMX_PARAM_U32TYPE *sU32Type;
            sU32Type = (OMX_PARAM_U32TYPE*)pComponentParameterStructure;

            if (sU32Type->nPortIndex == VIDEO_OUTPUT_PORT)
            {
                if((OMX_S32)sU32Type->nU32 < 0) {
                    DisableTrack(nActiveVideoNum);
                    break;
                }

                if (nActiveVideoNum != nVideoTrackNum[sU32Type->nU32])
                    DisableTrack(nActiveVideoNum);
                nActiveVideoNum = nVideoTrackNum[sU32Type->nU32];
                OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                sPortDef.nPortIndex = VIDEO_OUTPUT_PORT;
                ports[VIDEO_OUTPUT_PORT]->GetPortDefinition(&sPortDef);
                SetVideoCodecType(&sPortDef,nActiveVideoNum);
                ports[VIDEO_OUTPUT_PORT]->SetPortDefinition(&sPortDef);
                ActiveTrack(nActiveVideoNum);
                bVideoActived = OMX_TRUE;
            }
            else if (sU32Type->nPortIndex == AUDIO_OUTPUT_PORT)
            {
                if((OMX_S32)sU32Type->nU32 < 0) {
                    DisableTrack(nActiveAudioNum);
                    break;
                }

                if (sU32Type->nU32 > nNumAvailAudioStream)
                {
                    LOG_WARNING("Audio track is out of range.\n");
                    ret = OMX_ErrorBadParameter;
                    break;
                }

                if(nActiveAudioNumToClient != sU32Type->nU32)
                    DisableTrack(nActiveAudioNum);
                nActiveAudioNumToClient = sU32Type->nU32;
                nActiveAudioNum = nAudioTrackNum[sU32Type->nU32];
                OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
                sPortDef.nPortIndex = AUDIO_OUTPUT_PORT;
                ports[AUDIO_OUTPUT_PORT]->GetPortDefinition(&sPortDef);
                SetAudioCodecType(&sPortDef,nActiveAudioNum);
                ports[AUDIO_OUTPUT_PORT]->SetPortDefinition(&sPortDef);
                ActiveTrack(nActiveAudioNum);
                bAudioActived = OMX_TRUE;
                bAudioNewSegment = OMX_TRUE;
                bAudioEOS = OMX_FALSE;
            }
            break;
		case OMX_IndexParamIsGetMetadata:
		{
			OMX_PARAM_IS_GET_METADATA * pIsGetMetadata = (OMX_PARAM_IS_GET_METADATA *)pComponentParameterStructure;
			bGetMetadata = pIsGetMetadata->bGetMetadata;
			break;
		}
        case OMX_IndexParamSubtitleSelect:
           OMX_S32 Index;
	    Index =*((OMX_S32*)pComponentParameterStructure);
           if(Index < 0) {
                DisableTrack(nActiveSubtitleNum);  //disable subtitle
                nActiveSubtitleNumToClient=-1;
                bSubtitleActived=OMX_FALSE;
                break;
           }
           if(Index>=(OMX_S32)nNumAvailSubtitleStream){
                return OMX_ErrorBadParameter;   //invalid subtitle track number
           }
           if ((nActiveSubtitleNum != nSubtitleTrackNum[Index])&&(OMX_TRUE==bSubtitleActived))
                DisableTrack(nActiveSubtitleNum);
           nActiveSubtitleNumToClient=Index;
           nActiveSubtitleNum = nSubtitleTrackNum[Index];
           ActiveTrack(nActiveSubtitleNum);
           bSubtitleActived = OMX_TRUE;
           break;
       case OMX_IndexParamParserLowLatency:
           OMX_PARAM_PARSERLOWLATENCY *pParam;
           pParam = (OMX_PARAM_PARSERLOWLATENCY*)pComponentParameterStructure;
           bLowLatency = pParam->bEnable;
           LOG_DEBUG("set OMX_IndexParamParserLowLatency bEnable");
           break;

        default :
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }
    return ret;
}

OMX_ERRORTYPE Parser::GetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch ((int)nParamIndex )
    {
        case OMX_IndexConfigTimeSeekMode :
            if (sizeof(*(OMX_TIME_CONFIG_SEEKMODETYPE*)pComponentConfigStructure) < sizeof(OMX_TIME_CONFIG_SEEKMODETYPE))
            {
                ret = OMX_ErrorInsufficientResources;
                break;
            }
            fsl_osal_memcpy(pComponentConfigStructure,&sSeekMode, sizeof(OMX_TIME_CONFIG_SEEKMODETYPE));
            break;
        case OMX_IndexConfigTimePosition:
            OMX_TIME_CONFIG_TIMESTAMPTYPE* pCurrPos;
            pCurrPos = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure;

            if (pCurrPos->nPortIndex == VIDEO_OUTPUT_PORT)
                pCurrPos->nTimestamp = sCurrVideoTime;
            else
                pCurrPos->nTimestamp = sCurrAudioTime;

           break;
		case OMX_IndexConfigMetadataItemCount:
		   OMX_CONFIG_METADATAITEMCOUNTTYPE* pMatadataItemCount;
		   pMatadataItemCount = (OMX_CONFIG_METADATAITEMCOUNTTYPE*)pComponentConfigStructure;

		   pMatadataItemCount->nMetadataItemCount = sMatadataItemCount.nMetadataItemCount;

		   break;
		case OMX_IndexConfigMetadataItem:
		   OMX_CONFIG_METADATAITEMTYPE* pMatadataItem;
		   pMatadataItem = (OMX_CONFIG_METADATAITEMTYPE*)pComponentConfigStructure;

		   if (pMatadataItem->nMetadataItemIndex >= sMatadataItemCount.nMetadataItemCount)
			   break;

		   if (pMatadataItem->eSearchMode == OMX_MetadataSearchValueSizeByIndex) {
			   pMatadataItem->nValueMaxSize = psMatadataItem[pMatadataItem->nMetadataItemIndex]->nValueMaxSize;
		   } else if (pMatadataItem->eSearchMode == OMX_MetadataSearchItemByIndex) {
			   fsl_osal_memcpy(pMatadataItem->nKey, psMatadataItem[pMatadataItem->nMetadataItemIndex]->nKey, 128);
			   pMatadataItem->nKeySizeUsed = 128;
			   fsl_osal_memcpy(pMatadataItem->nValue, psMatadataItem[pMatadataItem->nMetadataItemIndex]->nValue, psMatadataItem[pMatadataItem->nMetadataItemIndex]->nValueSizeUsed);
			   pMatadataItem->nValueSizeUsed = psMatadataItem[pMatadataItem->nMetadataItemIndex]->nValueSizeUsed;
		   }

		   break;
                case OMX_IndexParamSubtitleNumAvailableStreams:
                   OMX_S32 *pCnt;
                   pCnt= (OMX_S32*)pComponentConfigStructure;
                   *pCnt=nNumAvailSubtitleStream;
                   break;
                case OMX_IndexParamSubtitleSelect:
                   OMX_S32* pIndex;
                   pIndex =(OMX_S32*)pComponentConfigStructure;
                   *pIndex=nActiveSubtitleNumToClient;
                   break;
                case OMX_IndexParamSubtitleNextSample:
                   OMX_SUBTITLE_SAMPLE* pSampleHdr;
                   pSampleHdr=(OMX_SUBTITLE_SAMPLE*)pComponentConfigStructure;
                   pSampleHdr->pBuffer=NULL;
                   if(OMX_FALSE==bSubtitleActived){
                       LOG_ERROR("not subtitle actived ");
                       return OMX_ErrorUndefined;
                   }
                   ret=GetAndSendOneSubtitleBuf(pSampleHdr);
                   if(ret!=OMX_ErrorNone){
                        pSampleHdr->pBuffer=NULL;  //avoid user use invalid data
                   }
                   break;
        case OMX_IndexConfigCommonRotate:
            OMX_CONFIG_ROTATIONTYPE *pRotation;
            pRotation = (OMX_CONFIG_ROTATIONTYPE*)pComponentConfigStructure;
            pRotation->nRotation = (OMX_S32)nCommonRotate;
            break;
		default :
		   break;
	}
	return ret;

}

OMX_ERRORTYPE Parser::SetConfig(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch ((int)nParamIndex )
    {
        case OMX_IndexConfigTimePosition :
            OMX_TIME_CONFIG_TIMESTAMPTYPE* pSeekPos;
            pSeekPos = (OMX_TIME_CONFIG_TIMESTAMPTYPE*)pComponentConfigStructure;

            OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
            OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
            sPortDef.nPortIndex = VIDEO_OUTPUT_PORT;
            ports[VIDEO_OUTPUT_PORT]->GetPortDefinition(&sPortDef);

            if (!bSeekable && pSeekPos->nTimestamp > 0)
                return OMX_ErrorUndefined;

            // seeking to 0 is not supported by VPU for h264 in TS
            if(!bSeekable && pSeekPos->nTimestamp==0
                    && sPortDef.format.video.eCompressionFormat == OMX_VIDEO_CodingAVC
                    && strcmp((char *)sCompRole.cRole, "parser.mpg2")==0)
                return OMX_ErrorUndefined;

            if (pSeekPos->nPortIndex == AUDIO_OUTPUT_PORT)
            {
                sAudioSeekPos = pSeekPos->nTimestamp;
            }
            else
            {
                sAudioSeekPos = pSeekPos->nTimestamp;
                sVideoSeekPos = pSeekPos->nTimestamp;
                sSubtitleSeekPos=pSeekPos->nTimestamp;
            }

            Seek(pSeekPos->nPortIndex);

            break;
        case OMX_IndexConfigTimeSeekMode:
            if (sizeof(*(OMX_TIME_CONFIG_SEEKMODETYPE*)pComponentConfigStructure) < sizeof(OMX_TIME_CONFIG_SEEKMODETYPE))
            {
                ret = OMX_ErrorInsufficientResources;
                break;
            }
            fsl_osal_memcpy(&sSeekMode, pComponentConfigStructure, sizeof(OMX_TIME_CONFIG_SEEKMODETYPE));
            break;

        case OMX_IndexConfigParserSendAudioFirst:
            OMX_CONFIG_SENDAUDIOFIRST *pConfig;
            pConfig = (OMX_CONFIG_SENDAUDIOFIRST*)pComponentConfigStructure;
            bSendAudioFrameFirst = pConfig->bSendAudioFrameFirst;
            break;

        case OMX_IndexConfigAbortBuffering:
            OMX_CONFIG_ABORTBUFFERING *pAbort;
            pAbort = (OMX_CONFIG_ABORTBUFFERING*)pComponentConfigStructure;
            bAbortBuffering = pAbort->bAbort;
            AbortReadSource(bAbortBuffering);
            printf("Parser::SetConfig(), bAbortBuffering %d", bAbortBuffering);
            break;
        case OMX_IndexParamSubtitleSelect:
            OMX_S32 Index;
            Index =*((OMX_S32*)pComponentConfigStructure);
            if(Index < 0) {
                DisableTrack(nActiveSubtitleNum);  //disable subtitle
                nActiveSubtitleNumToClient=-1;
                bSubtitleActived=OMX_FALSE;
                break;
            }
            if(Index>=(OMX_S32)nNumAvailSubtitleStream){
                return OMX_ErrorBadParameter;   //invalid subtitle track number
            }
            if ((nActiveSubtitleNum != nSubtitleTrackNum[Index])&&(OMX_TRUE==bSubtitleActived))
                DisableTrack(nActiveSubtitleNum);
            nActiveSubtitleNumToClient=Index;
            nActiveSubtitleNum = nSubtitleTrackNum[Index];
            ActiveTrack(nActiveSubtitleNum);
            bSubtitleActived = OMX_TRUE;
            break;
        case OMX_IndexParamSubtitleConfigTimePosition:
            OMX_TICKS ts;
            ts=*((OMX_TICKS*)pComponentConfigStructure);
            if (!bSeekable || !bSubtitleActived)  return OMX_ErrorUndefined;
            bSubtitleNewSegment=OMX_TRUE;
            bSubtitleEOS=OMX_FALSE;
            sSubtitleSeekPos=ts;
            ret=DoSubtitleSeek(SEEK_FLAG_NO_LATER);
            if (ret == OMX_ErrorStreamCorrupt)
                bStreamCorrupted = OMX_TRUE;
            if (ret == OMX_ErrorNoMore)
                bSubtitleEOS = OMX_TRUE;
            sSubtitleSeekPos=sActualSubtitleSeekPos;
            sCurrSubtitleTime=sActualSubtitleSeekPos;
            break;
        case OMX_IndexConfigInsertVideoCodecData:
            bInsertVideoCodecData = *(OMX_BOOL*)pComponentConfigStructure;
            break;
        default:
            break;
    }
    return ret;
}


OMX_ERRORTYPE Parser::GetAndSendOneVideoBuf()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if ((bVideoEOS && !bVideoNewSegment) || bStreamCorrupted)
        return ret;

    ports[VIDEO_OUTPUT_PORT]->GetBuffer(&pVideoOutBufHdr);
    if(pVideoOutBufHdr == NULL)
        return OMX_ErrorUnderflow;

        static FILE *pfTest = NULL;
        /*
        if (pfTest==NULL) {
            int i = (int)random();
            int j = i%1000;
            char name[50], number[10];
            strcpy(name, "/data/DumpData");
            sprintf(number,"_%d", j);
            strcat(name, number);
            strcat(name,".mp4");
            pfTest = fopen(name, "wb");
            if(pfTest == NULL)
                printf("Unable to open dump file! %s\n", name);
            else
                printf("open dump file %s\n", name);
        }
        */

    if(bInsertVideoCodecData && nVideoCodecDataLen > 0) {
        LOG_DEBUG("Send video codec specific data.\n");
        fsl_osal_memcpy(pVideoOutBufHdr->pBuffer, VideoCodecData, nVideoCodecDataLen);
        pVideoOutBufHdr->nFlags = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;
        pVideoOutBufHdr->nFilledLen = nVideoCodecDataLen;
        bInsertVideoCodecData = OMX_FALSE;
        if(pfTest != NULL  && pVideoOutBufHdr->nFilledLen > 0)  {
                fwrite(pVideoOutBufHdr->pBuffer, sizeof(char), pVideoOutBufHdr->nFilledLen, pfTest);
                fflush(pfTest);
         }
        ports[VIDEO_OUTPUT_PORT]->SendBuffer(pVideoOutBufHdr);
        return OMX_ErrorNone;
    }

    if(bSkip2Iframe != OMX_TRUE)
        ret = GetNextSample(ports[VIDEO_OUTPUT_PORT],pVideoOutBufHdr);
    else {
        do{
            ret = GetNextSample(ports[VIDEO_OUTPUT_PORT],pVideoOutBufHdr);
            if (ret != OMX_ErrorNone)
                break;
        }while(!(pVideoOutBufHdr->nFlags&OMX_BUFFERFLAG_SYNCFRAME));
        bSkip2Iframe = OMX_FALSE;
    }

    if (ret == OMX_ErrorNotReady) {
        ports[VIDEO_OUTPUT_PORT]->AddBuffer(pVideoOutBufHdr);
        fsl_osal_sleep(10000);
        return OMX_ErrorNone;
    }

    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;

    if (pVideoOutBufHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
        LOG_DEBUG("Send video codec specific data.\n");
        if(pVideoOutBufHdr->nFilledLen <= VIDEO_CODEC_DATA_BUFFER_LEN) {
            fsl_osal_memcpy(VideoCodecData, pVideoOutBufHdr->pBuffer, pVideoOutBufHdr->nFilledLen);
            nVideoCodecDataLen = pVideoOutBufHdr->nFilledLen;
        }
        else {
            LOG_ERROR("can't copy video codec data, buffer is not enough! (%d), (%d)",
                    pVideoOutBufHdr->nFilledLen, VIDEO_CODEC_DATA_BUFFER_LEN);
        }
        if(pfTest != NULL  && pVideoOutBufHdr->nFilledLen > 0)  {
                fwrite(pVideoOutBufHdr->pBuffer, sizeof(char), pVideoOutBufHdr->nFilledLen, pfTest);
                fflush(pfTest);
         }
        ports[VIDEO_OUTPUT_PORT]->SendBuffer(pVideoOutBufHdr);
        return OMX_ErrorNone;
    }

    if (!(pVideoOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS))
        sCurrVideoTime = pVideoOutBufHdr->nTimeStamp;

    if (bVideoNewSegment)
    {
        pVideoOutBufHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        LOG_DEBUG("%s,%d,send starttime, ts %lld.\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nTimeStamp);
        bVideoNewSegment = OMX_FALSE;
		if (pVideoOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS)
		{
			LOG_DEBUG("Seek to EOS.\n");
			pVideoOutBufHdr->nTimeStamp = sActualVideoSeekPos;
		}

        /* Adjust actualVideo seek ts if returned from parser is not correct */
        if(sActualVideoSeekPos != pVideoOutBufHdr->nTimeStamp) {
            LOG_WARNING("Adjust actual video seek ts from %lld to %lld\n", sActualVideoSeekPos, pVideoOutBufHdr->nTimeStamp);
            sActualVideoSeekPos = pVideoOutBufHdr->nTimeStamp;
            //drop video frames for rtp/udp low-latency streaming (video only)
            if(!bAudioActived && bLowLatency)
                sActualVideoSeekPos = pVideoOutBufHdr->nTimeStamp + LOW_LATENCY_DROP_TIME;
        }
    }

    //drop audio frames for rtp/udp low-latency streaming
    if(bLowLatency && pVideoOutBufHdr->nTimeStamp < sActualVideoSeekPos){
        LOG_DEBUG("GetAndSendOneVideoBuf parser skip seekpos=%lld",sActualVideoSeekPos);
        pVideoOutBufHdr->nFlags |= OMX_BUFFERFLAG_DECODEONLY;
    }

    if (sSeekMode.eType == OMX_TIME_SeekModeAccurate)
    {
        if (pVideoOutBufHdr->nTimeStamp < sVideoSeekPos)
        {
            LOG_DEBUG("%s,%d,Decode only ts %lld\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nTimeStamp);
            pVideoOutBufHdr->nFlags |= OMX_BUFFERFLAG_DECODEONLY;
        }
        else
            sSeekMode.eType = OMX_TIME_SeekModeFast;
    }

    if (bAbortBuffering == OMX_TRUE) {
        pVideoOutBufHdr->nFilledLen = 0;
        pVideoOutBufHdr->nFlags |= OMX_BUFFERFLAG_EOS;
        printf("set eos because of bAbortBuffering");
    }

    if (pVideoOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS)
    {
        ComponentBase::SendEvent(OMX_EventBufferFlag,pVideoOutBufHdr->nOutputPortIndex,pVideoOutBufHdr->nFlags, NULL);
        LOG_DEBUG("%s,%d,eos  ts %lld\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nTimeStamp);
        bVideoEOS = OMX_TRUE;
    }

    LOG_DEBUG("%s,%d,send , ts %lld, flag: %x, len: %d.\n",__FUNCTION__,__LINE__,
            pVideoOutBufHdr->nTimeStamp, pVideoOutBufHdr->nFlags, pVideoOutBufHdr->nFilledLen);
    LOG_DEBUG("%s,%d,send , nFilledLen %d.\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nFilledLen);
    LOG_DEBUG("%s,%d,send , nAllocLen %d.\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nAllocLen);
    LOG_DEBUG("%s,%d,send , begin data %p.\n",__FUNCTION__,__LINE__,*((OMX_S32 *)pVideoOutBufHdr->pBuffer));
    if(pfTest != NULL  && pVideoOutBufHdr->nFilledLen > 0)  {
            fwrite(pVideoOutBufHdr->pBuffer, sizeof(char), pVideoOutBufHdr->nFilledLen, pfTest);
            fflush(pfTest);
     }
    ports[VIDEO_OUTPUT_PORT]->SendBuffer(pVideoOutBufHdr);
    return ret;
}

OMX_ERRORTYPE Parser::GetAndSendOneAudioBuf()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TICKS sActualSyncPoint = 0LL;
    OMX_TICKS mediaTs = 0LL;

    if ((bAudioEOS && !bAudioNewSegment) || bStreamCorrupted)
        return ret;

    ports[AUDIO_OUTPUT_PORT]->GetBuffer(&pAudioOutBufHdr);
    if(pAudioOutBufHdr == NULL)
        return OMX_ErrorUnderflow;
	pAudioOutBufHdr->nFlags = 0;

    ret = GetNextSample(ports[AUDIO_OUTPUT_PORT],pAudioOutBufHdr);
    if (ret == OMX_ErrorNotReady) {
        ports[AUDIO_OUTPUT_PORT]->AddBuffer(pAudioOutBufHdr);
        fsl_osal_sleep(10000);
        return OMX_ErrorNone;
    }

    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;

    if (pAudioOutBufHdr->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
        LOG_DEBUG("Send audio codec specific data.\n");
        ports[AUDIO_OUTPUT_PORT]->SendBuffer(pAudioOutBufHdr);
        return OMX_ErrorNone;
    }

    if (bAudioNewSegment)
    {
        if (sSeekMode.eType == OMX_TIME_SeekModeAccurate)
            sActualSyncPoint = sVideoSeekPos;
        else
            sActualSyncPoint = sActualVideoSeekPos;

        while (pAudioOutBufHdr->nTimeStamp < sActualSyncPoint && !(pAudioOutBufHdr->nFlags&OMX_BUFFERFLAG_CODECCONFIG)
            && !(pAudioOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS))
        {
            LOG_INFO("%s,%d,drop one audio buffer in parser, ts %lld sActualSyncPoint: %lld\n",__FUNCTION__,__LINE__,pAudioOutBufHdr->nTimeStamp, sActualSyncPoint);
            ret = GetNextSample(ports[AUDIO_OUTPUT_PORT],pAudioOutBufHdr);
            if(ret != OMX_ErrorNone)
                break;
        }
    }
    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;
    if (ret == OMX_ErrorNoMore)
        bAudioEOS = OMX_TRUE;

    if (bAudioEOS && bAudioNewSegment)
            pAudioOutBufHdr->nTimeStamp = sActualSyncPoint;

    if (!(pAudioOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS))
        sCurrAudioTime = pAudioOutBufHdr->nTimeStamp;

    if (bAudioNewSegment)
    {
        pAudioOutBufHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        LOG_DEBUG("%s,%d,send starttime, ts %lld.\n",__FUNCTION__,__LINE__,pAudioOutBufHdr->nTimeStamp);
        bAudioNewSegment = OMX_FALSE;
    }



    if(bLowLatency){
        mediaTs = GetCurrentMediaTime();

        //send audio start time after longtime playback
        if(mediaTs > 0 && sCurrAudioTime > mediaTs+LOW_LATENCY_AUDIO_RESET_TS && sCurrVideoTime > mediaTs){
            bAudioNewSegment = OMX_TRUE;
            sActualVideoSeekPos = pAudioOutBufHdr->nTimeStamp;
            LOG_DEBUG("GetAndSendOneAudioBuf LowLatency reset audio time");
        }

        //drop audio frames for rtp/udp low-latency streaming at the beginning
        if(bDropAudio == OMX_FALSE && mediaTs > 0){
            bAudioNewSegment = OMX_TRUE;
            sActualVideoSeekPos = pAudioOutBufHdr->nTimeStamp + LOW_LATENCY_DROP_TIME;
            bDropAudio = OMX_TRUE;
            LOG_DEBUG("GetAndSendOneAudioBuf LowLatency seekpos=%lld,nParserSkipDuration=%lld",sActualVideoSeekPos);
        }
    }

    if (bAbortBuffering == OMX_TRUE) {
        pAudioOutBufHdr->nFilledLen = 0;
        pAudioOutBufHdr->nFlags |= OMX_BUFFERFLAG_EOS;
    }

    if (pAudioOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS)
    {
        ComponentBase::SendEvent(OMX_EventBufferFlag,pAudioOutBufHdr->nOutputPortIndex,pAudioOutBufHdr->nFlags, NULL);
        LOG_DEBUG("%s,%d,send eos.\n",__FUNCTION__,__LINE__);
        bAudioEOS = OMX_TRUE;
    }

    LOG_DEBUG("%s,%d,send , ts %lld, flag: %x, len: %d.\n",__FUNCTION__,__LINE__,
            pAudioOutBufHdr->nTimeStamp, pAudioOutBufHdr->nFlags, pAudioOutBufHdr->nFilledLen);
    ports[AUDIO_OUTPUT_PORT]->SendBuffer(pAudioOutBufHdr);


    return ret;
}

OMX_ERRORTYPE Parser::GetAndSendOneSubtitleBuf(OMX_SUBTITLE_SAMPLE* pSubtitleOutBufHdr)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_TICKS sActualSyncPoint = 0LL;

    if ((bSubtitleEOS && !bSubtitleNewSegment) || bStreamCorrupted)
        return OMX_ErrorNoMore;//return ret;

    ret = GetNextSubtitleSample(pSubtitleOutBufHdr);

    //LOG_INFO("ret: 0x%X, get one subtitle: track: %d, ts: %lld , length: %d, flag: 0x%X \r\n",ret,nActiveSubtitleNum,pSubtitleOutBufHdr->nTimeStamp,pSubtitleOutBufHdr->nFilledLen, pSubtitleOutBufHdr->nFlags);

    if(ret == OMX_ErrorNotReady)
        return ret;

    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;

    if (bSubtitleNewSegment)
    {
        if (sSeekMode.eType == OMX_TIME_SeekModeAccurate)
            sActualSyncPoint = sSubtitleSeekPos;
        else
            sActualSyncPoint = sActualSubtitleSeekPos;

        while (pSubtitleOutBufHdr->nTimeStamp < sActualSyncPoint && !(pSubtitleOutBufHdr->nFlags&OMX_BUFFERFLAG_CODECCONFIG)
            && !(pSubtitleOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS))
        {
            LOG_INFO("%s,%d,drop one subtitle buffer in parser, ts %lld sActualSyncPoint: %lld\n",__FUNCTION__,__LINE__,pSubtitleOutBufHdr->nTimeStamp, sActualSyncPoint);
            ret = GetNextSubtitleSample(pSubtitleOutBufHdr);
            if(ret != OMX_ErrorNone)
                break;
        }
    }
    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;
    if (ret == OMX_ErrorNoMore)
        bSubtitleEOS = OMX_TRUE;

    if (bSubtitleEOS && bSubtitleNewSegment)
            pSubtitleOutBufHdr->nTimeStamp = sActualSyncPoint;

    if (!(pSubtitleOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS))
        sCurrSubtitleTime = pSubtitleOutBufHdr->nTimeStamp;

    if (bSubtitleNewSegment)
    {
        pSubtitleOutBufHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME;
        LOG_DEBUG("%s,%d,send starttime, ts %lld.\n",__FUNCTION__,__LINE__,pSubtitleOutBufHdr->nTimeStamp);
        bSubtitleNewSegment = OMX_FALSE;
    }

    if (bAbortBuffering == OMX_TRUE) {
        pSubtitleOutBufHdr->nFilledLen = 0;
        pSubtitleOutBufHdr->nFlags |= OMX_BUFFERFLAG_EOS;
    }

    if (pSubtitleOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS)
    {
        bSubtitleEOS = OMX_TRUE;
    }

    return ret;
}

OMX_ERRORTYPE Parser::GetAndSendPrevSyncVideoBuf()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (bVideoBOS || bStreamCorrupted)
        return ret;

    ports[VIDEO_OUTPUT_PORT]->GetBuffer(&pVideoOutBufHdr);
    if(pVideoOutBufHdr == NULL)
        return OMX_ErrorUnderflow;

    ret = GetPrevSyncSample(ports[VIDEO_OUTPUT_PORT],pVideoOutBufHdr);
    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;

    /*equal timestamp means this buffer and its previous buffer belongs to same frame*/
    while (sCurrVideoTime != pVideoOutBufHdr->nTimeStamp &&
            sCurrVideoTime - pVideoOutBufHdr->nTimeStamp < OMX_TICKS_PER_SECOND)
    {
        ret = GetPrevSyncSample(ports[VIDEO_OUTPUT_PORT],pVideoOutBufHdr);
        if(ret != OMX_ErrorNone)
            break;
    }
    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;

    if (bVideoNewSegment)
    {
        pVideoOutBufHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME | OMX_BUFFERFLAG_STARTTRICK;
        LOG_DEBUG("%s,%d,send starttime, ts %lld.\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nTimeStamp);
        bVideoNewSegment = OMX_FALSE;
    }
    if (!(pVideoOutBufHdr->nFlags&OMX_BUFFERFLAG_EOS))
        sCurrVideoTime = pVideoOutBufHdr->nTimeStamp;
    if (pVideoOutBufHdr->nFlags & OMX_BUFFERFLAG_EOS)
    {
        ComponentBase::SendEvent(OMX_EventBufferFlag,pVideoOutBufHdr->nOutputPortIndex,pVideoOutBufHdr->nFlags, NULL);
        LOG_DEBUG("%s,%d,send eos.\n",__FUNCTION__,__LINE__);
        bVideoBOS = OMX_TRUE;
    }

    LOG_DEBUG("%s,%d,got buff ts  %lld, flag: %x\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nTimeStamp, pVideoOutBufHdr->nFlags);
    ports[VIDEO_OUTPUT_PORT]->SendBuffer(pVideoOutBufHdr);


    return ret;
}

OMX_ERRORTYPE Parser::GetNextSyncSample(Port *pPort,OMX_BUFFERHEADERTYPE *pOutBuffer)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE Parser::GetPrevSyncSample(Port *pPort,OMX_BUFFERHEADERTYPE *pOutBuffer)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE Parser::GetNextSubtitleSample(OMX_SUBTITLE_SAMPLE *pOutSample)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE Parser::DoSubtitleSeek(OMX_U32 nSeekFlag)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE Parser::GetAndSendNextSyncVideoBuf()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (bVideoEOS || bStreamCorrupted)
        return ret;

    ports[VIDEO_OUTPUT_PORT]->GetBuffer(&pVideoOutBufHdr);
    if(pVideoOutBufHdr == NULL)
        return OMX_ErrorUnderflow;

    ret = GetNextSyncSample(ports[VIDEO_OUTPUT_PORT],pVideoOutBufHdr);
    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;

    while (sCurrVideoTime != pVideoOutBufHdr->nTimeStamp &&
        pVideoOutBufHdr->nTimeStamp - sCurrVideoTime < OMX_TICKS_PER_SECOND)
    {
        ret = GetNextSyncSample(ports[VIDEO_OUTPUT_PORT],pVideoOutBufHdr);
        if(ret != OMX_ErrorNone)
            break;
    }
    if (ret == OMX_ErrorStreamCorrupt)
        bStreamCorrupted = OMX_TRUE;

    LOG_DEBUG("%s,%d,got buff ts  %lld\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nTimeStamp);
    if (bVideoNewSegment)
    {
        pVideoOutBufHdr->nFlags |= OMX_BUFFERFLAG_STARTTIME | OMX_BUFFERFLAG_STARTTRICK;
        LOG_DEBUG("%s,%d,send starttime, ts %lld.\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nTimeStamp);
        bVideoNewSegment = OMX_FALSE;
    }
    if (!(pVideoOutBufHdr->nFlags & OMX_BUFFERFLAG_EOS))
        sCurrVideoTime = pVideoOutBufHdr->nTimeStamp;
    if (pVideoOutBufHdr->nFlags & OMX_BUFFERFLAG_EOS)
    {
        LOG_DEBUG("%s,%d,send eos.\n",__FUNCTION__,__LINE__);
        ComponentBase::SendEvent(OMX_EventBufferFlag,pVideoOutBufHdr->nOutputPortIndex,pVideoOutBufHdr->nFlags, NULL);
        bVideoEOS = OMX_TRUE;
    }

    LOG_DEBUG("%s,%d,got buff ts  %lld, flag: %x\n",__FUNCTION__,__LINE__,pVideoOutBufHdr->nTimeStamp, pVideoOutBufHdr->nFlags);
    ports[VIDEO_OUTPUT_PORT]->SendBuffer(pVideoOutBufHdr);

    return ret;
}

OMX_ERRORTYPE Parser::ProcessDataBuffer()
{
    LOG_LOG("Parser Audio: %d, Video: %d\n", ports[AUDIO_OUTPUT_PORT]->BufferNum(), ports[VIDEO_OUTPUT_PORT]->BufferNum());

    if ((((bVideoEOS && !bVideoNewSegment) || ports[VIDEO_OUTPUT_PORT]->BufferNum() == 0)
				&& ((bAudioEOS && !bAudioNewSegment) || ports[AUDIO_OUTPUT_PORT]->BufferNum() == 0 ))
			|| bStreamCorrupted)
        return OMX_ErrorNoMore;

    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(ports[AUDIO_OUTPUT_PORT]->BufferNum() == 0
            && ports[VIDEO_OUTPUT_PORT]->BufferNum() == 0)
        return OMX_ErrorNoMore;

    if(nPreCacheSize == 0)
        CalcPreCacheSize();

    if (FAST_FORWARD_TRICK_MODE == playbackMode)
    {
        if(ports[VIDEO_OUTPUT_PORT]->BufferNum() > 0)
            ret = GetAndSendNextSyncVideoBuf();
    }
    else if (FAST_BACKWARD_TRICK_MODE == playbackMode)
    {
        if(ports[VIDEO_OUTPUT_PORT]->BufferNum() > 0)
            ret = GetAndSendPrevSyncVideoBuf();
    }
    else
    {
        if(ports[VIDEO_OUTPUT_PORT]->BufferNum() > 0)
            ret = GetAndSendOneVideoBuf();

        if(ports[AUDIO_OUTPUT_PORT]->BufferNum() > 0)
            ret = GetAndSendOneAudioBuf();
    }

    pAudioOutBufHdr = pVideoOutBufHdr = NULL;
    //OMX_TICKS mediaTs=GetCurrentMediaTime();
    //LOG_DEBUG("current media ts=%lld,audio+=%lld,video+=%lld",mediaTs,
    //    sCurrAudioTime-mediaTs,sCurrVideoTime-mediaTs);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE Parser::ProcessClkBuffer()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;


    if(ports[CLOCK_PORT]->BufferNum() == 0)
        return OMX_ErrorNoMore;

    ports[CLOCK_PORT]->GetBuffer(&pClockBufHdr);
    if(pClockBufHdr == NULL)
        return OMX_ErrorUnderflow;

    OMX_TIME_MEDIATIMETYPE *pTimeBuffer;
    pTimeBuffer = (OMX_TIME_MEDIATIMETYPE*) pClockBufHdr->pBuffer;


    if(pTimeBuffer->eUpdateType == OMX_TIME_UpdateScaleChanged)
    {
        OMX_TIME_CONFIG_PLAYBACKTYPE *pPlayback = (OMX_TIME_CONFIG_PLAYBACKTYPE *)(unsigned long)pTimeBuffer->nClientPrivate;
        nClockScale = pTimeBuffer->xScale;
        playbackMode = pPlayback->ePlayMode;
    }
    else if(pTimeBuffer->eUpdateType == OMX_TIME_UpdateVideoLate) {
        LOG_DEBUG("Parser skip to next I frame.\n");
        bSkip2Iframe = OMX_TRUE;
    }

    ports[CLOCK_PORT]->SendBuffer(pClockBufHdr);

    return ret;
}


OMX_ERRORTYPE Parser::Seek(OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("parser want seek to: %lld\n", sVideoSeekPos);

	if (nPortIndex == AUDIO_OUTPUT_PORT && bAudioActived)
	{
		bAudioNewSegment = OMX_TRUE;
		bAudioEOS = OMX_FALSE;
		ret = DoSeek(AUDIO_OUTPUT_PORT,SEEK_FLAG_NO_LATER);
		if (ret == OMX_ErrorStreamCorrupt)
			bStreamCorrupted = OMX_TRUE;
                if (ret == OMX_ErrorNoMore)
                    bAudioEOS = OMX_TRUE;

		sCurrAudioTime = sActualAudioSeekPos;
	}
	else
	{
        if (bVideoActived)
        {
            bVideoNewSegment = OMX_TRUE;
            bVideoEOS = OMX_FALSE;
            bVideoBOS = OMX_FALSE;
            ret = DoSeek(VIDEO_OUTPUT_PORT,SEEK_FLAG_NO_LATER);
            if (ret == OMX_ErrorStreamCorrupt)
                bStreamCorrupted = OMX_TRUE;
            if (ret == OMX_ErrorNoMore)
                bVideoEOS = OMX_TRUE;

            sAudioSeekPos = sActualVideoSeekPos;
            sSubtitleSeekPos=sActualVideoSeekPos;
        }

        if (bAudioActived)
        {
            bAudioNewSegment = OMX_TRUE;
            bAudioEOS = OMX_FALSE;
            ret = DoSeek(AUDIO_OUTPUT_PORT,SEEK_FLAG_NO_LATER);
            if (ret == OMX_ErrorStreamCorrupt)
                bStreamCorrupted = OMX_TRUE;
            if (ret == OMX_ErrorNoMore)
                bAudioEOS = OMX_TRUE;
        }
        if(bSubtitleActived)
        {
            bSubtitleNewSegment=OMX_TRUE;
            bSubtitleEOS=OMX_FALSE;
            ret=DoSubtitleSeek(SEEK_FLAG_NO_LATER);
            if (ret == OMX_ErrorStreamCorrupt)
                bStreamCorrupted = OMX_TRUE;
            if (ret == OMX_ErrorNoMore)
                bSubtitleEOS = OMX_TRUE;
            sSubtitleSeekPos=sActualSubtitleSeekPos;
            sCurrSubtitleTime=sActualSubtitleSeekPos;
        }

		sCurrVideoTime = sActualVideoSeekPos;
		sCurrAudioTime = sActualAudioSeekPos;
	}

        LOG_DEBUG("parser want seek to: %lld, actualAudio: %lld, actualVideo: %lld\n",
                sVideoSeekPos, sActualAudioSeekPos, sActualVideoSeekPos);

    return ret;
}

void Parser::MakeIndexDumpFileName(char *media_file_name)
{
    OMX_STRING idx_path = NULL;

    idx_path = fsl_osal_getenv_new("FSL_OMX_INDEX_PATH");
    if(idx_path != NULL) {
        OMX_STRING ptr, ptr1;
        ptr = fsl_osal_strrchr(media_file_name, '/') + 1;
        if ((OMX_STRING)0x1 == ptr)
            ptr = media_file_name;

        if((fsl_osal_strlen(idx_path) + fsl_osal_strlen(media_file_name)) >= INDEX_FILE_NAME_MAX_LEN - 2){
            index_file_name[0] = 0;
            LOG_ERROR("media_file_name length overflow\n");
            return;
        }
        fsl_osal_strcpy(index_file_name, idx_path);
        ptr1 = index_file_name + fsl_osal_strlen(index_file_name);
        ptr1[0] = '/';
        ptr1[1] = '.';
        fsl_osal_strcpy(ptr1+2, ptr);
        ptr1 = fsl_osal_strrchr(index_file_name, '.');
        fsl_osal_strcpy(ptr1, ".idx");
    }
    else {
        /* get idx file from the same folder of media file */
        OMX_STRING ptr, ptr1;
        if(fsl_osal_strlen(media_file_name) >= INDEX_FILE_NAME_MAX_LEN - 2){
            index_file_name[0] = 0;
            LOG_ERROR("media_file_name length overflow\n");
            return;
        }
        fsl_osal_strcpy(index_file_name, media_file_name);
        ptr = fsl_osal_strrchr(media_file_name, '/') + 1;
        if ((OMX_STRING)0x1 == ptr)
            ptr = media_file_name;

        ptr1 = fsl_osal_strrchr(index_file_name, '/') + 1;
        if ((OMX_STRING)0x1 == ptr1)
            ptr1 = index_file_name;

        ptr1[0] = '.';
        fsl_osal_strcpy(ptr1+1, ptr);
        ptr1 = fsl_osal_strrchr(index_file_name, '.');
        fsl_osal_strcpy(ptr1, ".idx");
    }

    LOG_DEBUG("Index file name: %s\n", index_file_name);

    return;
}

void Parser::SetAudioCodecType(OMX_PARAM_PORTDEFINITIONTYPE *pPortDef, uint32 audio_num)
{

}
void Parser::SetVideoCodecType(OMX_PARAM_PORTDEFINITIONTYPE *pPortDef, uint32 video_num)
{

}

void Parser::ActiveTrack(uint32 track_number)
{

}

void Parser::DisableTrack(uint32 track_number)
{

}

void Parser::AbortReadSource(OMX_BOOL bAbort)
{

}

OMX_ERRORTYPE Parser::SetMetadata(OMX_STRING key, OMX_STRING value, OMX_U32 valueSize)
{
	OMX_CONFIG_METADATAITEMTYPE* pMatadataItem;
	OMX_U8 *ptr = (OMX_U8*)FSL_MALLOC(sizeof(OMX_CONFIG_METADATAITEMTYPE) + valueSize);
	if(ptr == NULL) {
		return OMX_ErrorInsufficientResources;
	}
	fsl_osal_memset(ptr, 0, sizeof(OMX_CONFIG_METADATAITEMTYPE) + valueSize);
	pMatadataItem = (OMX_CONFIG_METADATAITEMTYPE *)ptr;

	OMX_INIT_STRUCT(pMatadataItem, OMX_CONFIG_METADATAITEMTYPE);

	sprintf((char *)pMatadataItem->nKey, "%s", key);
	fsl_osal_memcpy(&pMatadataItem->nValue, value, valueSize);
	pMatadataItem->nValueMaxSize = valueSize;
	pMatadataItem->nValueSizeUsed = valueSize;
	psMatadataItem[sMatadataItemCount.nMetadataItemCount] = pMatadataItem;
	sMatadataItemCount.nMetadataItemCount ++;
	return OMX_ErrorNone;
}

OMX_U32 Parser::CalcPreCacheSize()
{
    OMX_U32 nCacheSec = HTTP_CACHE_TIME;
    OMX_U64 nDurSec = usDuration / OMX_TICKS_PER_SECOND;

    LOG_DEBUG("nDurSec = %lld, nContentLen = %lld, nPreCacheSize = %d\n",
            nDurSec, nContentLen, nPreCacheSize);

    if(nDurSec < nCacheSec)
        nCacheSec = nDurSec;

    if(nDurSec > 0)
        nPreCacheSize = nCacheSec * nContentLen / nDurSec;

    if(nPreCacheSize == 0){
        if(bLowLatency)
            nPreCacheSize = 188;
        else
            nPreCacheSize = 256 * 1024;
    }

    nCheckCacheOff = nPreCacheSize;

    printf("nPreCacheSize = %d\n", nPreCacheSize);

    return nPreCacheSize;
}


OMX_TICKS Parser::GetCurrentMediaTime()
{
    TUNNEL_INFO hClock;
    OMX_TIME_CONFIG_TIMESTAMPTYPE sCur;

    if(ports[CLOCK_PORT]->IsEnabled() != OMX_TRUE)
        return 0;

    ports[CLOCK_PORT]->GetTunneledInfo(&hClock);
    OMX_INIT_STRUCT(&sCur, OMX_TIME_CONFIG_TIMESTAMPTYPE);
    sCur.nPortIndex = OMX_ALL;
    OMX_GetConfig(hClock.hTunneledComp, OMX_IndexConfigTimeCurrentMediaTime, &sCur);
    return sCur.nTimestamp;
}
OMX_ERRORTYPE Parser::SetStopReadingFlag()
{
    bStopReading = OMX_TRUE;
    return OMX_ErrorNone;
}
/* File EOF */
