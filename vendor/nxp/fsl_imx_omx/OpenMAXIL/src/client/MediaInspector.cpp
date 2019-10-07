/**
 *  Copyright (c) 2010-2012, 2014, 2016, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include <inttypes.h>
#include "OMX_Core.h"
#include "MediaInspector.h"
#include "Mem.h"
#include "Log.h"
#include "ShareLibarayMgr.h"
#include "GMComponent.h"
#include "Mp3FrameParser.h"
#include "AacFrameParser.h"
#include "Ac3FrameParser.h"

//#undef LOG_DEBUG
//#define LOG_DEBUG printf
#define DETECT_BUFFER_SIZE 64
#define MKV_BUFFER_SIZE 50
#define MP3_BUFFER_SIZE 4096
#define AAC_BUFFER_SIZE 4096

#define MKV_RECTYPE "matroska"
#define MKV_WEBM_RECTYPE "webm"
#define MKV_EBML_ID_HEADER 0xA3DF451A

static const CPbyte ASF_GUID[16] = {
    0x30,0x26,0xb2,0x75,0x8e,0x66,0xcf,0x11,0xa6,0xd9,0x0,0xaa,0x0,0x62,0xce,0x6c
};

typedef struct {
    CP_PIPETYPE *hPipe;
    CPhandle hContent;
    MediaType type;
    OMX_STRING sContentURI;
    OMX_BOOL streaming;
    OMX_BOOL *abort;
    OMX_BOOL keepOpen;
}CONTENTDATA;

typedef AFP_RETURN (*CHECK_FRAME)(AUDIO_FRAME_INFO *pFrameInfo, fsl_osal_u8 *pBuffer, fsl_osal_u32 nBufferLen);


OMX_ERRORTYPE CheckMPEGType(CONTENTDATA *pData);

OMX_ERRORTYPE ReadContent(CONTENTDATA *pData, CPbyte *buffer, OMX_S32 buffer_size)
{
    CPresult nContentPipeResult = 0;

    if(pData->streaming == OMX_TRUE) {
        CP_CHECKBYTESRESULTTYPE eReady = CP_CheckBytesOk;
        while(1) {
            fsl_osal_sleep(5000);

            if(pData->abort != NULL && *(pData->abort) == OMX_TRUE)
                return OMX_ErrorTerminated;

            nContentPipeResult = pData->hPipe->CheckAvailableBytes(pData->hContent, buffer_size, &eReady);

            LOG_DEBUG("MediaInspector::ReadContent nContentPipeResult %d, eReady %x", nContentPipeResult, eReady);

            if(nContentPipeResult == 0 && eReady == CP_CheckBytesInsufficientBytes)
                continue;
            else if(nContentPipeResult == 0 && eReady == CP_CheckBytesOk)
                break;
            else{
                LOG_ERROR("ReadContent() CheckAvailableBytes fail: result %d, eReady %x\n", nContentPipeResult, eReady);
                return OMX_ErrorNoMore;
            }

        }
    }

    nContentPipeResult = pData->hPipe->Read(pData->hContent, buffer, buffer_size);

    if(nContentPipeResult == 0)
        return OMX_ErrorNone;
    else
        return OMX_ErrorUndefined;

}
OMX_ERRORTYPE DetectAudioTypeByFrame(
        CONTENTDATA *pData,
        OMX_S32 buffer_size,
        CHECK_FRAME fpCheckFrame)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    CPbyte *buffer = NULL;
    CPint64 pos = 0, posEnd = 0, nSeekPos = 0;
    OMX_S32 Id3Size = 0;
    OMX_BOOL seekable = OMX_TRUE;
    CPresult nContentPipeResult = 0;

    if(fpCheckFrame == NULL)
        return OMX_ErrorUndefined;

    buffer = (CPbyte*)FSL_MALLOC(buffer_size * sizeof(CPbyte));
    if(buffer == NULL) {
        return OMX_ErrorInsufficientResources;
    }
    fsl_osal_memset(buffer, 0, buffer_size*sizeof(CPbyte));

    if(pData->keepOpen == OMX_FALSE){
        if(0 != pData->hPipe->Open(&(pData->hContent), pData->sContentURI, CP_AccessRead)) {
            LOG_ERROR("Can't open content: %s\n", (char*)(pData->sContentURI));
            FSL_FREE(buffer);
            return OMX_ErrorUndefined;
        }
    }
    else
        pData->hPipe->SetPosition(pData->hContent, 0, CP_OriginBegin);

    if(0 != pData->hPipe->GetPosition(pData->hContent, &pos)) {
        FSL_FREE(buffer);
        pData->hPipe->Close(pData->hContent);
        return OMX_ErrorUndefined;
    }

    ret = ReadContent(pData, buffer, buffer_size);
    if(ret != OMX_ErrorNone)
        goto bail;

	if (!fsl_osal_memcmp((void *)("ID3"), buffer, 3)) {
		// Skip the ID3v2 header.
		size_t len =
			((buffer[6] & 0x7f) << 21)
			| ((buffer[7] & 0x7f) << 14)
			| ((buffer[8] & 0x7f) << 7)
			| (buffer[9] & 0x7f);

		if (len > 3 * 1024 * 1024)
			len = 3 * 1024 * 1024;

		len += 10;

		Id3Size = len;
	}

    nContentPipeResult = pData->hPipe->SetPosition(pData->hContent, 0, CP_OriginSeekable);
    if(nContentPipeResult == KD_EUNSEEKABLE){
        printf("DetectAudioTypeByFrame: server unseekable");
        seekable = OMX_FALSE;
    }

    if(seekable == OMX_TRUE){
        LOG_DEBUG("Id3Size: %d\n", Id3Size);
        pData->hPipe->SetPosition(pData->hContent, 0, CP_OriginEnd);
        pData->hPipe->GetPosition(pData->hContent, &posEnd);
        LOG_DEBUG("File size: %lld\n", posEnd);
        nSeekPos = Id3Size + ((posEnd - pos - Id3Size) >> 1);
        LOG_DEBUG("Read from: %lld\n", nSeekPos);
        pData->hPipe->SetPosition(pData->hContent, nSeekPos, CP_OriginBegin);
    }
    else if(Id3Size > buffer_size){
        //for unseekable server,  skip id3 data by reading them all
        Id3Size -= buffer_size;
        while(Id3Size > 0){
            OMX_S32 read = (Id3Size > buffer_size) ? buffer_size : Id3Size;
            ret = ReadContent(pData, buffer, read);
            if(ret != OMX_ErrorNone)
                goto bail;
            Id3Size -= read;
        }
    }

	fsl_osal_memset(buffer, 0, buffer_size*sizeof(CPbyte));
    ret = ReadContent(pData, buffer, buffer_size);
    if(ret != OMX_ErrorNone)
        goto bail;

	AUDIO_FRAME_INFO FrameInfo;
    fsl_osal_memset(&FrameInfo, 0, sizeof(AUDIO_FRAME_INFO));
    ret = OMX_ErrorUndefined;

    LOG_DEBUG("Audio frame detecting try at begining.\n");

    fpCheckFrame(&FrameInfo, (OMX_U8*)buffer, buffer_size);

    if(FrameInfo.bGotOneFrame) {
        LOG_DEBUG("Audio Frame detected.\n");
        ret = OMX_ErrorNone;
    }

bail:
    if(pData->keepOpen == OMX_FALSE)
        pData->hPipe->Close(pData->hContent);

    FSL_FREE(buffer);

    return ret;
}

OMX_ERRORTYPE TryLoadComponent(
        CONTENTDATA *pData,
        OMX_STRING role)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_S32 len = fsl_osal_strlen(pData->sContentURI) + 1;
    OMX_PTR ptr = FSL_MALLOC(sizeof(OMX_PARAM_CONTENTURITYPE) + len);
    if(ptr == NULL)
        return OMX_ErrorInsufficientResources;

    OMX_PARAM_CONTENTURITYPE *Content = (OMX_PARAM_CONTENTURITYPE*)ptr;
    if(fsl_osal_strncmp(pData->sContentURI, "file://", 7) == 0)
        fsl_osal_strcpy((fsl_osal_char*)(&Content->contentURI), pData->sContentURI + 7);
    else
        fsl_osal_strcpy((fsl_osal_char*)(&Content->contentURI), pData->sContentURI);

    GMComponent *pComponent = FSL_NEW(GMComponent, ());
    if(pComponent == NULL)
        goto err;

    ret = pComponent->Load(role);
    if(ret != OMX_ErrorNone) {
        goto err;
    }

    ret = OMX_SetParameter(pComponent->hComponent, OMX_IndexParamContentURI, Content);
    if(ret != OMX_ErrorNone)
        goto err;

    OMX_PARAM_CONTENTPIPETYPE sPipe;
    OMX_INIT_STRUCT(&sPipe, OMX_PARAM_CONTENTPIPETYPE);
    sPipe.hPipe = pData->hPipe;
    ret = OMX_SetParameter(pComponent->hComponent, OMX_IndexParamCustomContentPipe, &sPipe);
    if(ret != OMX_ErrorNone)
        goto err;

    ret = pComponent->StateTransUpWard(OMX_StateIdle);
    if(ret != OMX_ErrorNone)
        goto err;

    ret = pComponent->StateTransUpWard(OMX_StatePause);
    if(ret != OMX_ErrorNone) {
        pComponent->StateTransDownWard(OMX_StateLoaded);
        goto err;
    }

    pComponent->StateTransDownWard(OMX_StateIdle);
    pComponent->StateTransDownWard(OMX_StateLoaded);

err:
    if(pComponent) {
        if(pComponent->hComponent) {
            pComponent->UnLoad();
        }
        FSL_DELETE(pComponent);
    }

    if(Content)
        FSL_FREE(Content);

    return ret;
}

OMX_ERRORTYPE TryFlacType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    CPbyte buffer2[DETECT_BUFFER_SIZE];
    OMX_S32 Id3Size = 0;

    LOG_DEBUG("TryFlacType\n");

    pData->type = TYPE_NONE;
    if (buffer[0] == 'f' && buffer[1] == 'L' && buffer[2] == 'a' && buffer[3] == 'C') {
        pData->type = TYPE_FLAC;
        LOG_DEBUG("Content type is flac.\n");
        return ret;
    }
    else
        ret = OMX_ErrorUndefined;

    if (!fsl_osal_memcmp((const fsl_osal_ptr)("ID3"), (const fsl_osal_ptr)buffer, 3)) {
            // Skip the ID3v2 header.
            size_t len =
                ((buffer[6] & 0x7f) << 21)
                | ((buffer[7] & 0x7f) << 14)
                | ((buffer[8] & 0x7f) << 7)
                | (buffer[9] & 0x7f);

            if (len > 3 * 1024 * 1024)
                len = 3 * 1024 * 1024;

            len += 10;

            Id3Size = len;
    }
    else{
        return OMX_ErrorUndefined;
    }

    LOG_DEBUG("TryFlacType,Id3Size=%d",Id3Size);


    if(pData->keepOpen == OMX_FALSE){
        if(0 != pData->hPipe->Open(&(pData->hContent), pData->sContentURI, CP_AccessRead)) {
            LOG_ERROR("Can't open content: %s\n", (char*)(pData->sContentURI));
            return OMX_ErrorUndefined;
        }
    }
    pData->hPipe->SetPosition(pData->hContent, Id3Size, CP_OriginBegin);

    fsl_osal_memset(buffer2, 0, DETECT_BUFFER_SIZE);
    ReadContent(pData, buffer2, DETECT_BUFFER_SIZE);

    if(pData->keepOpen == OMX_FALSE)
        pData->hPipe->Close(pData->hContent);

    //LOG_DEBUG("TryFlacType,buffer2= %x %x %x %x",buffer2[0],buffer2[1],buffer2[2],buffer2[3]);

    if (buffer2[0] == 'f' && buffer2[1] == 'L' && buffer2[2] == 'a' && buffer2[3] == 'C') {
        pData->type = TYPE_FLAC;
        LOG_DEBUG("Content type is flac.\n");
        ret = OMX_ErrorNone;
    }

    return ret;
}
OMX_ERRORTYPE TryMp3Type(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    CHECK_FRAME fpCheckFrame = &Mp3CheckFrame;
    LOG_DEBUG("TryMp3Type\n");

    pData->type = TYPE_NONE;
    if(OMX_ErrorNone == DetectAudioTypeByFrame(pData, MP3_BUFFER_SIZE, fpCheckFrame)) {
        pData->type = TYPE_MP3;
        LOG_DEBUG("Content type is mp3.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}
OMX_ERRORTYPE TryAacType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    CHECK_FRAME fpCheckFrame = &AacCheckFrame;
    LOG_DEBUG("TryAacType\n");

    pData->type = TYPE_NONE;
    if(OMX_ErrorNone == DetectAudioTypeByFrame(pData, AAC_BUFFER_SIZE, fpCheckFrame)) {
        pData->type = TYPE_AAC;
        LOG_DEBUG("Content type is aac adts.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryAACADIFType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryAACADIFType\n");

    pData->type = TYPE_NONE;
    if (buffer[0] == 'A' && buffer[1] == 'D' && buffer[2] == 'I' && buffer[3] == 'F') {
        pData->type = TYPE_AAC;
        LOG_DEBUG("Content type is aac adif.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryAc3Type(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    CHECK_FRAME fpCheckFrame = &Ac3CheckFrame;
    LOG_DEBUG("TryAc3Type\n");

    pData->type = TYPE_NONE;
    if(OMX_ErrorNone == DetectAudioTypeByFrame(pData, AAC_BUFFER_SIZE, fpCheckFrame)) {
        pData->type = TYPE_AC3;
        LOG_DEBUG("Content type is ac3.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}


OMX_ERRORTYPE TryAviType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryAviType\n");

    pData->type = TYPE_NONE;
    if (buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F' &&
            buffer[8] == 'A' && buffer[9] == 'V' && buffer[10] == 'I' && buffer[11] == ' ' ) {
        pData->type = TYPE_AVI;
        LOG_DEBUG("Content type is avi.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryMp4Type(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryMp4Type\n");

    pData->type = TYPE_NONE;
    if ((buffer[4] == 'f' && buffer[5] == 't' && buffer[6] == 'y' && buffer[7] == 'p')
            || (buffer[4] == 'm' && buffer[5] == 'o' && buffer[6] == 'o' && buffer[7] == 'v')
            || (buffer[4] == 's' && buffer[5] == 'k' && buffer[6] == 'i' && buffer[7] == 'p')
            || (buffer[4] == 'm' && buffer[5] == 'd' && buffer[6] == 'a' && buffer[7] == 't')
            || (buffer[4] == 'w' && buffer[5] == 'i' && buffer[6] == 'd' && buffer[7] == 'e')) {
        pData->type = TYPE_MP4;
        LOG_DEBUG("Content type is mp4.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryRmvbType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryRmvbType\n");

    pData->type = TYPE_NONE;
    if (buffer[0] == '.' && buffer[1] == 'R' && buffer[2] == 'M' && buffer[3] == 'F') {
        pData->type = TYPE_RMVB;
        LOG_DEBUG("Content type is rmvb.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryMkvType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U32 temp = 0;
    OMX_BOOL is_matroska = OMX_FALSE;
    OMX_BOOL has_matroska = OMX_FALSE;

    LOG_DEBUG("TryMkvType\n");

    pData->type = TYPE_NONE;

    temp = *(OMX_S32*)buffer;
    if(temp == MKV_EBML_ID_HEADER)
        is_matroska = OMX_TRUE;

    OMX_S32 i;
    for (i = 0; i <= MKV_BUFFER_SIZE - (OMX_S32)(sizeof(MKV_RECTYPE)-1); i++) {
        if (!fsl_osal_memcmp((const fsl_osal_ptr)(buffer + i), (const fsl_osal_ptr)(MKV_RECTYPE), sizeof(MKV_RECTYPE)-1)) {
            has_matroska = OMX_TRUE;
            break;
        }
    }

#ifndef MX5X
    if(is_matroska == OMX_TRUE && has_matroska == OMX_FALSE)
    {
        //search syntax for ".webm"
        for (i = 0; i <= MKV_BUFFER_SIZE - (OMX_S32)(sizeof(MKV_WEBM_RECTYPE)-1); i++) {
            if (!fsl_osal_memcmp((const fsl_osal_ptr)(buffer + i), (const fsl_osal_ptr)(MKV_WEBM_RECTYPE), sizeof(MKV_WEBM_RECTYPE)-1)) {
                has_matroska = OMX_TRUE;
                break;
            }
        }
    }
#endif

    if(is_matroska == OMX_TRUE && has_matroska == OMX_TRUE) {
        pData->type = TYPE_MKV;
        LOG_DEBUG("Content type is mkv.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryAsfType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryAsfType\n");

    pData->type = TYPE_NONE;
    if (!fsl_osal_memcmp((const fsl_osal_ptr)buffer, (const fsl_osal_ptr) ASF_GUID, 16)) {
        pData->type = TYPE_ASF;
        LOG_DEBUG("Content type is asf.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryWavType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryWavType\n");

    pData->type = TYPE_NONE;
    if (buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F' &&
            buffer[8] == 'W' && buffer[9] == 'A' && buffer[10] == 'V' && buffer[11] == 'E' ) {
        pData->type = TYPE_WAV;
        LOG_DEBUG("Content type is wav.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryHttpliveType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryHttpliveType\n");

    pData->type = TYPE_NONE;

    if (!fsl_osal_strncmp(buffer, "#EXTM3U", 7)) {
        pData->type = TYPE_HTTPLIVE;
        LOG_DEBUG("Content type is Httplive.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryFlvType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryFlvType\n");

    pData->type = TYPE_NONE;
    if (buffer[0] == 'F' && buffer[1] == 'L' && buffer[2] == 'V'){
        pData->type = TYPE_FLV;
        LOG_DEBUG("Content type is flv.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryApeType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryApeType\n");

    pData->type = TYPE_NONE;
    if (buffer[0] == 'M' && buffer[1] == 'A' && buffer[2] == 'C' && buffer[3] == ' ') {
        pData->type = TYPE_APE;
        LOG_DEBUG("Content type is ape.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}


#if 1
OMX_ERRORTYPE TryMpg2type(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_ERRORTYPE callRet;
#ifdef MPEG_PERF
    timeval start, stop;
    long timeused;
#endif

    pData->type = TYPE_NONE;

#ifdef MPEG_PERF
    gettimeofday(&start, NULL);
#endif
    callRet = CheckMPEGType(pData);
#ifdef MPEG_PERF
    gettimeofday(&stop, NULL);
    timeused = 1000000 * ( stop.tv_sec - start.tv_sec ) + stop.tv_usec - start.tv_usec;
    printf("CheckMPEGType time %d us \r\n", timeused);
#endif

    if(OMX_ErrorNone == callRet) {
        pData->type = TYPE_MPG2;
        LOG_DEBUG("Content type is mpg2.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

#else //TryMpg2type
OMX_ERRORTYPE TryMpg2type(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_ERRORTYPE callRet;
#ifdef MPEG_PERF
    timeval start, stop;
    long timeused;
#endif

    pData->type = TYPE_NONE;

#ifdef MPEG_PERF
    gettimeofday(&start, NULL);
#endif
    callRet = TryLoadComponent(pData, (OMX_STRING)"parser.mpg2");
#ifdef MPEG_PERF
    gettimeofday(&stop, NULL);
    timeused = 1000000 * ( stop.tv_sec - start.tv_sec ) + stop.tv_usec - start.tv_usec;
    printf("TryLoadComponent time %d us \r\n", timeused);
#endif

    if(OMX_ErrorNone == callRet) {
        pData->type = TYPE_MPG2;
        LOG_DEBUG("Content type is mpg2.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}
#endif // TryMpg2Type

OMX_ERRORTYPE TryOggType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryOggType\n");

    pData->type = TYPE_NONE;
    if (buffer[0] == 'O' && buffer[1] == 'g' && buffer[2] == 'g' && buffer[3] == 'S' ) {
        pData->type = TYPE_OGG;
        LOG_DEBUG("Content type is ogg.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}

OMX_ERRORTYPE TryAmrType(CONTENTDATA *pData, CPbyte *buffer)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOG_DEBUG("TryAmrType\n");

    pData->type = TYPE_NONE;
    if ( !fsl_osal_memcmp((const fsl_osal_ptr)buffer, (const fsl_osal_ptr)"#!AMR\n", 6) ||
        !fsl_osal_memcmp((const fsl_osal_ptr)buffer, (const fsl_osal_ptr)"#!AMR-WB\n", 9)){
        pData->type = TYPE_AMR;
        LOG_DEBUG("Content type is amr.\n");
    }
    else
        ret = OMX_ErrorUndefined;

    return ret;
}


OMX_ERRORTYPE GetContentPipe(CONTENTDATA *pData)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    CP_PIPETYPE *Pipe = NULL;

    if(fsl_osal_strstr((fsl_osal_char*)pData->sContentURI, "sharedfd://"))
        ret = OMX_GetContentPipe((void**)&Pipe, (OMX_STRING)"SHAREDFD_PIPE");
    else if(fsl_osal_strstr((fsl_osal_char*)pData->sContentURI, "http://")) {
        pData->streaming = OMX_TRUE;
        ret = OMX_GetContentPipe((void**)&Pipe, (OMX_STRING)"HTTPS_PIPE");
    }
    else if(fsl_osal_strstr((fsl_osal_char*)pData->sContentURI, "rtp://")) {
        pData->streaming = OMX_TRUE;
        ret = OMX_GetContentPipe((void**)&Pipe, (OMX_STRING)"RTPS_PIPE");
    }
    else if(fsl_osal_strstr((fsl_osal_char*)pData->sContentURI, "udp://")) {
        pData->streaming = OMX_TRUE;
        ret = OMX_GetContentPipe((void**)&Pipe, (OMX_STRING)"UDPS_PIPE");
    }
    else
        ret = OMX_GetContentPipe((void**)&Pipe, (OMX_STRING)"LOCAL_FILE_PIPE_NEW");

    if(Pipe == NULL)
        return OMX_ErrorContentPipeCreationFailed;

    pData->hPipe = Pipe;

    return ret;
}

typedef OMX_ERRORTYPE (*TRYTYPEFUNC)(CONTENTDATA *pData, CPbyte *buffer);

static TRYTYPEFUNC TryFunc[] = {
    TryAviType,
    TryMp4Type,
    TryMkvType,
    TryAsfType,
    TryFlvType,
    TryRmvbType,
    TryAACADIFType,
    TryOggType,
    TryFlacType,
    TryWavType,
    TryHttpliveType,
    TryAmrType,
    TryMpg2type,
    TryMp3Type,
    TryAacType,
    TryAc3Type,
    TryApeType
};

MediaType GetContentType(OMX_STRING contentURI, OMX_BOOL *abort)
{
    CONTENTDATA sData;
    CPresult ret = 0;
    int fd, mMediaType = 0;
    int64_t offset =0;
    int64_t len = 0;

    if(fsl_osal_strstr((fsl_osal_char*)(contentURI), "sharedfd://")) {
        (void)sscanf(contentURI, "sharedfd://%d:%" PRId64 ":%" PRId64 ":%d", &fd, &offset, &len, &mMediaType);
        if (mMediaType)
            return (MediaType)mMediaType;
    }

    OMX_Init();

    fsl_osal_memset(&sData, 0, sizeof(CONTENTDATA));
    sData.type = TYPE_NONE;
    sData.sContentURI = contentURI;
    sData.abort = abort;
    sData.keepOpen = OMX_FALSE;
    if(OMX_ErrorNone != GetContentPipe(&sData))
        return TYPE_NONE;

    ret = sData.hPipe->Open(&sData.hContent, contentURI, CP_AccessRead);
    if(ret != 0) {
        LOG_ERROR("Can't open content: %s\n", contentURI);
        return TYPE_NONE;
    }

    CPbyte buffer[DETECT_BUFFER_SIZE];
    fsl_osal_memset(buffer, 0, DETECT_BUFFER_SIZE);
    OMX_ERRORTYPE err = ReadContent(&sData, buffer, DETECT_BUFFER_SIZE);

    sData.hPipe->Close(sData.hContent);

    if(err == OMX_ErrorNone){
        for(OMX_U32 i=0; i<sizeof(TryFunc)/sizeof(TRYTYPEFUNC); i++) {
            if(OMX_ErrorNone == (*TryFunc[i])(&sData, buffer))
                break;
        }
    }
    else{
        sData.type = TYPE_NONE;
    }

    OMX_Deinit();

    return sData.type;
}

MediaType GetContentTypeByPipeHandle(OMX_STRING contentURI, CPhandle pipeHandle, OMX_BOOL *abort)
{
    CONTENTDATA sData;
    CPresult ret = 0;
    int fd, mMediaType = 0;
    int64_t offset, len;

    printf("GetContentTypeByPipeHandle, uri %s", contentURI);

    fsl_osal_memset(&sData, 0, sizeof(CONTENTDATA));
    sData.hPipe = NULL;
    sData.hContent = pipeHandle;
    sData.type = TYPE_NONE;
    sData.sContentURI = contentURI;
    sData.streaming = OMX_FALSE;
    sData.abort = abort;
    sData.keepOpen = OMX_TRUE;

    if(OMX_ErrorNone != GetContentPipe(&sData))
        return TYPE_NONE;

    CPbyte buffer[DETECT_BUFFER_SIZE];
    fsl_osal_memset(buffer, 0, DETECT_BUFFER_SIZE);
    OMX_ERRORTYPE err = ReadContent(&sData, buffer, DETECT_BUFFER_SIZE);

    if(err == OMX_ErrorNone){
        for(OMX_U32 i=0; i<sizeof(TryFunc)/sizeof(TRYTYPEFUNC); i++) {
            if(OMX_ErrorNone == (*TryFunc[i])(&sData, buffer))
                break;
        }
    }
    else{
        sData.type = TYPE_NONE;
    }

    printf("GetContentTypeAndPipeHandle ok, type %d", sData.type);

    // workaround for FlakyNetworkTest
#if 0
    if(fsl_osal_strstr((fsl_osal_char*)contentURI, "http://localhost")) {
        sData.hPipe->Close(sData.hContent);
        *pipeHandle = NULL;
    }
#endif

    return sData.type;
}


typedef struct {
    const char *key;
    TRYTYPEFUNC TryFunc;
}TYPECONFIRM;

static TYPECONFIRM TypeConfirm[] = {
    {"parser.avi", TryAviType},
    {"parser.mp4", TryMp4Type},
    {"parser.mkv", TryMkvType},
    {"parser.asf", TryAsfType},
    {"parser.flv", TryFlvType},
    {"parser.rmvb", TryRmvbType},
    {"parser.aac", TryAACADIFType},
    {"parser.ogg", TryOggType},
    {"parser.flac", TryFlacType},
    {"parser.wav", TryWavType},
    {"parser.httplive", TryHttpliveType},
    {"parser.amr",TryAmrType},
    {"parser.mpg2", TryMpg2type},
    {"parser.mp3", TryMp3Type},
    {"parser.aac", TryAacType},
    {"parser.ac3",TryAc3Type},
    {"parser.ape",TryApeType}
};

OMX_STRING MediaTypeConformByContent(OMX_STRING contentURI, OMX_STRING role)
{
    MediaType type = TYPE_NONE;
    CONTENTDATA sData;
    CPresult ret = 0;
    OMX_STRING confirmed_role = NULL;

    OMX_Init();

    fsl_osal_memset(&sData, 0, sizeof(CONTENTDATA));
    sData.sContentURI = contentURI;
    if(OMX_ErrorNone != GetContentPipe(&sData))
        return NULL;

    sData.keepOpen = OMX_FALSE;

    ret = sData.hPipe->Open(&sData.hContent, contentURI, CP_AccessRead);
    if(ret != 0) {
        LOG_ERROR("Can't open content: %s\n", contentURI);
        return NULL;
    }

    CPbyte buffer[DETECT_BUFFER_SIZE];
    fsl_osal_memset(buffer, 0, DETECT_BUFFER_SIZE);
    ReadContent(&sData, buffer, DETECT_BUFFER_SIZE);

    sData.hPipe->Close(sData.hContent);

    for(OMX_U32 i=0; i<sizeof(TypeConfirm)/sizeof(TYPECONFIRM); i++) {
        const char *key = TypeConfirm[i].key;
        TRYTYPEFUNC func = TypeConfirm[i].TryFunc;
        if(fsl_osal_strcmp(role, key) == 0) {
            if(OMX_ErrorNone == (*func)(&sData, buffer)) {
                confirmed_role = role;
				break;
			}
        }
    }

    OMX_Deinit();

    return confirmed_role;
}


#define MPEG_SCAN_BUFFER_SIZE       (128*1024)

#define MPEGTS_HDR_SIZE             (4)

#define MPEGTS_FOUND_MIN_HEADERS    (4)
#define MPEGTS_FOUND_MAX_HEADERS    (10)
#define MPEGTS_MAX_PACKET_SIZE      (208)
#define MPEGTS_MIN_SYNC_SIZE        (MPEGTS_FOUND_MIN_HEADERS * MPEGTS_MAX_PACKET_SIZE)
#define MPEGTS_MAX_SYNC_SIZE        (MPEGTS_FOUND_MAX_HEADERS * MPEGTS_MAX_PACKET_SIZE)
#define MPEGTS_SCAN_LENGTH          (MPEGTS_MAX_SYNC_SIZE * 4)

/* Check for sync byte, error_indicator == 0 and packet has payload */
#define IS_MPEGTS_HEADER(data) (((data)[0] == 0x47) && \
                                (((data)[1] & 0x80) == 0x00) && \
                                (((data)[3] & 0x30) != 0x00))

CPbyte * Peek_data_from_buffer(CPbyte* pBuf, OMX_S32 Buf_size, OMX_S32 offset, OMX_S32 peek_size)
{
    CPbyte * pPtr = NULL;

    if (offset+peek_size < Buf_size) {
        pPtr = pBuf + offset;
    }

    return pPtr;
}

/* Search ahead at intervals of packet_size for MPEG-TS headers */
OMX_S32 mpeg_ts_probe_headers(CPbyte* pBuf, OMX_S32 Buf_size, OMX_S32 offset, OMX_S32 packet_size)
{
    /* We always enter this function having found at least one header already */
    OMX_S32 found = 1;
    CPbyte *data = NULL;

    //LOG_DEBUG ("MPEG-TS packets of size %u \r\n", packet_size);
    while (found < MPEGTS_FOUND_MAX_HEADERS) {
        offset += packet_size;

        data = Peek_data_from_buffer(pBuf, Buf_size, offset, MPEGTS_HDR_SIZE);
        if (data == NULL || !IS_MPEGTS_HEADER(data))
            return found;

        found++;
        //LOG_DEBUG ("MPEG-TS sync %ld at offset %ld \r\n", found, offset);
    }

    return found;
}

OMX_ERRORTYPE TryMpegTsType(CPbyte * pBuf, OMX_S32 buf_size)
{
    /* TS packet sizes to test: normal, DVHS packet size and
    * FEC with 16 or 20 byte codes packet size. */
    OMX_U16 pack_sizes[] = { 188, 192, 204, 208 };
    CPbyte *data = NULL;
    OMX_U32 size = 0;
    OMX_U64 skipped = 0;
    OMX_S32 found = 0;

    while (skipped < MPEGTS_SCAN_LENGTH) {
        if (size < MPEGTS_HDR_SIZE) {
            data = Peek_data_from_buffer (pBuf, buf_size, skipped, MPEGTS_MIN_SYNC_SIZE);
            if (!data)
                break;
            size = MPEGTS_MIN_SYNC_SIZE;
        }

        /* Have at least MPEGTS_HDR_SIZE bytes at this point */
        if (IS_MPEGTS_HEADER (data)) {
            //LOG_DEBUG ("possible mpeg-ts sync at offset %lld", skipped);
            for (OMX_S32 p = 0; p < 4; p++) {
                /* Probe ahead at size pack_sizes[p] */
                found = mpeg_ts_probe_headers (pBuf, buf_size, skipped, pack_sizes[p]);
                if (found >= MPEGTS_FOUND_MIN_HEADERS) {
                    return OMX_ErrorNone;
                }
            }
        }
        data++;
        skipped++;
        size--;
    }

    return OMX_ErrorUndefined;
}

#define MPEG_PES_ID_MIN         0xBD
#define MPEG_PES_ID_MAX         0xFF
#define MPEG_PACK_ID            0XBA
#define MPEG_SYS_ID             0xBB
#define IS_MPEG_SC_PREFIX(data) ( (((char *)(data))[0] == 0x00) &&  \
                                (((char *)(data))[1] == 0x00) &&  \
                                (((char *)(data))[2] == 0x01) )

#define MPEG2_MAX_PROBE_SIZE    (131072)  /* 128 * 1024 = 128kB should be 64 packs of the most common 2kB pack size. */

#define MPEG2_MIN_SYS_HEADERS   (2)
#define MPEG2_MAX_SYS_HEADERS   (5)

/*
 *  MPEG-1 Pack Header                                 bit
 *  pack_start_code                                     32
 *  '0010'                                               4
 *  system_clock_reference_base [32..30]                 3
 *  marker_bit (the value is  '1')                       1
 *  system_clock_reference_base [29..15]                15
 *  marker_bit                                           1
 *  system_clock_reference_base [14..0]                 15
 *  marker_bit                                           1
 *  marker_bit                                           1
 *  program_mux_rate                                    22
 *  marker_bit                                           1
 *  if (nextbits() = = system_header_start_code) {
 *      system_header ()
 *  }
 */


/*
 *  MPEG-2 Pack Header                                 bit
 *  pack_start_code                                     32
 *  '01'                                                 2
 *  system_clock_reference_base [32..30]                 3
 *  marker_bit (the value is  '1')                       1
 *  system_clock_reference_base [29..15]                15
 *  marker_bit                                           1
 *  system_clock_reference_base [14..0]                 15
 *  marker_bit                                           1
 *  system_clock_reference_extension                     9
 *  marker_bit                                           1
 *  program_mux_rate                                    22
 *  marker_bit                                           1
 *  marker_bit                                           1
 *  reserved                                             5
 *  pack_stuffing_length                                 3
 *  for (i = 0; i < pack_stuffing_length; i++) {
 *       stuffing_byte                                   8
 *  }
 *  if (nextbits() = = system_header_start_code) {
 *      system_header ()
 *  }
 */

OMX_ERRORTYPE CheckMpegPackHeader(CPbyte * data, OMX_U32 len, OMX_U32 * packSize)
{
    // check the rest bytes after pack start code, the pack start code should already be check as valid.
    OMX_U32 valid = 0;

    if (len < 16)
        return OMX_ErrorUndefined;

    if ((data[4] & 0xF0) == 0x20) {
        // check MPEG-1 Pack Header
        if ((data[4] & data[6] & data[8] & data[11] & 0x01) && (data[9] & 0x80))
            valid = IS_MPEG_SC_PREFIX(data + 12);

        if (packSize && valid)
            *packSize = 12;

    } else if ((data[4] & 0xC0) == 0x40) {
        // check MPEG-2 Pack Header
        if ((data[4] & data[6] & data[8] & 0x04) && (data[9] & 0x01) && (data[12] & 0x03) == 0x03) {
            OMX_U32 staffingLen = data[13] & 0x07;
            if (len > 18 + staffingLen)
                valid = IS_MPEG_SC_PREFIX(data + 14 + staffingLen);

            if (packSize && valid)
                *packSize = 14 + staffingLen;
        }
    }

    return (valid ? OMX_ErrorNone : OMX_ErrorUndefined);
}

/*
 *  PES Packet                                     bit
 *  packet_start_code_prefix                        24
 *  directory_stream_id                              8
 *  PES_packet_length                               16
 *  if (nextbits() == packet_start_code_prefix) {
 *      pes_packet()
 *  }
 */

OMX_ERRORTYPE CheckMpegPESHeader(CPbyte * data, OMX_U32 len, OMX_U32 * packSize)
{
    // check the rest bytes after packet_start_code, the packet_start_code should already be check as valid.
    OMX_U32 valid = 0;
    OMX_U32 PESLen;

    if (len < 6)
        return OMX_ErrorUndefined;

    PESLen = (data[4] << 8) | data[5];

    if (PESLen > 0) {
        if (len > PESLen + 10)
            valid = IS_MPEG_SC_PREFIX(data + 6 + PESLen);
        else
            valid = 1; // data len is not enough, suppose this is a valid pes header
    }

    if (packSize && valid)
        *packSize = 6 + PESLen;

    return (valid ? OMX_ErrorNone : OMX_ErrorUndefined);
}

/*
 *  System Header                                     bit
 *  system_header_start_code                           32
 *  header_length                                      16
 *  marker_bit                                          1
 *  rate_bound                                         22
 *  marker_bit                                          1
 *  audio_bound                                         6
 *  fixed_flag                                          1
 *  CSPS_flag                                           1
 *  system_audio_lock_flag                              1
 *  system_video_lock_flag                              1
 *  marker_bit                                          1
 *  video_bound                                         5
 *  packet_rate_restriction_flag                        1
 *  reserved_bits                                       7
 *  while (nextbits () == '1') {
 *      stream_id                                       8
 *      '11'                                            2
 *      P-STD_buffer_bound_scale                        1
 *      P-STD_buffer_size_bound                        13
 *  }
 */

OMX_ERRORTYPE CheckMpegSysHeader(CPbyte * data, OMX_U32 len, OMX_U32 * packSize)
{
    // check the rest bytes after system_header_start_code, the system_header_start_code should already be check as valid.
    OMX_U32 valid = 0;
    OMX_U32 sysHeaderLen;

    if (len < 6)
        return OMX_ErrorUndefined;

    sysHeaderLen = (data[4] << 8) | data[5];

    if (sysHeaderLen >= 6 && len >= sysHeaderLen + 10)
        valid = IS_MPEG_SC_PREFIX(data + sysHeaderLen + 6);

    if (packSize && valid)
        *packSize = 6 + sysHeaderLen;

    return (valid ? OMX_ErrorNone : OMX_ErrorUndefined);
}

/*
 * Estimate Probability of Identify Random Data As MPEG System Stream
 * 1. Check start code(32 bit): (1/2)^32
 * 2. Search MPEG2_MIN_SYS_HEADERS headers at least: (1/2)^32^(MPEG2_MIN_SYS_HEADERS+1)
 * 3. Probability of identify random data in MPEG2_MAX_PROBE_SIZE bytes:
 *        (MPEG2_MAX_PROBE_SIZE - 4*(MPEG2_MIN_SYS_HEADERS+1))*8/((1/2)^32^(MPEG2_MIN_SYS_HEADERS+1))
 * 4. MPEG2_MAX_PROBE_SIZE is 131072, MPEG2_MIN_SYS_HEADERS is 2, so probability is:
 *        (131072 - 4*(2+1))*8/((1/2)^32^3)
 *    the value is about the (1/2)^76
 * 5. Since we also check marker bit and header length, the actual probability is smaller than this value.
 */
OMX_ERRORTYPE TryMpegSysType(CPbyte * pBuf, OMX_U32 bufSize)
{
    CPbyte *data = NULL, *end = NULL;
    OMX_U32 packCnt = 0;
    OMX_U32 pesCnt = 0;
    OMX_U32 packSize;
    OMX_U32 len = MPEG2_MAX_PROBE_SIZE;
    OMX_BOOL continuous = OMX_FALSE;

    while (!data) {
        if (bufSize > len + 5)
            data = pBuf;
        else if (len < 4096 + 32)
            break;
        else
            len -= 4096;
    }

    if (!data)
        return OMX_ErrorUndefined;

    end = data + len;

    while (data + 4 < end){
        if (IS_MPEG_SC_PREFIX(data)) {
            if (OMX_FALSE == continuous)
                pesCnt = packCnt = 0; // only count continuous pes_headers/pack_headers

            packSize = 0;
            if (MPEG_PACK_ID == data[3]) {
                if (OMX_ErrorNone == CheckMpegPackHeader(data, end - data, &packSize))
                    packCnt++;
            } else if (MPEG_SYS_ID == data[3]) {
                if (OMX_ErrorNone == CheckMpegSysHeader(data, end - data, &packSize))
                    packCnt++;
            } else if (data[3] >= MPEG_PES_ID_MIN) {
                if (OMX_ErrorNone == CheckMpegPESHeader(data, end - data, &packSize)) {
                    pesCnt++;
                }
            }

            if (packSize > 0) {
                data += packSize;
                continuous = OMX_TRUE;
                if (pesCnt > 0 && (packCnt + pesCnt) > MPEG2_MAX_SYS_HEADERS)
                    return OMX_ErrorNone;
                else
                    continue;
            }
        }
        continuous = OMX_FALSE;
        data++;
    }
    // check pes_headers/pack_headers, pack headers are optional
    if (pesCnt > 0 && (packCnt + pesCnt) >= MPEG2_MIN_SYS_HEADERS)
        return OMX_ErrorNone;

    return OMX_ErrorUndefined;
};

OMX_ERRORTYPE CheckMPEGType(CONTENTDATA *pData)
{
    OMX_ERRORTYPE ret = OMX_ErrorFormatNotDetected;
    CPbyte *buffer = NULL;
    OMX_S32 buffer_size;
    CPint64 pos = 0, posEnd = 0, nSeekPos = 0, file_size;
#ifdef MPEG_PERF
    timeval start, stop;
	long timeused;
#endif

    buffer_size = MPEG_SCAN_BUFFER_SIZE;
    buffer = (CPbyte*)FSL_MALLOC(buffer_size * sizeof(CPbyte));
    if(buffer == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto bail;
    }
    fsl_osal_memset(buffer, 0, buffer_size*sizeof(CPbyte));

    if(pData->keepOpen == OMX_FALSE){
        if(0 != pData->hPipe->Open(&(pData->hContent), pData->sContentURI, CP_AccessRead)) {
            LOG_ERROR("Can't open content: %s\n", (char*)(pData->sContentURI));
            ret = OMX_ErrorUndefined;
            goto bail;
        }
    }
    else
        pData->hPipe->SetPosition(pData->hContent, 0, CP_OriginBegin);

    if(0 != pData->hPipe->GetPosition(pData->hContent, &pos)) {
        ret = OMX_ErrorUndefined;
        goto bail;
    }
    if (0 != pData->hPipe->SetPosition(pData->hContent, 0, CP_OriginEnd)) {
        ret = OMX_ErrorUndefined;
        goto bail;
    }
    if (0 != pData->hPipe->GetPosition(pData->hContent, &file_size)) {
        ret = OMX_ErrorUndefined;
        goto bail;
    }
    if (0 != pData->hPipe->SetPosition(pData->hContent,pos, CP_OriginBegin)) {
        ret = OMX_ErrorUndefined;
        goto bail;
    }

    if (buffer_size > file_size)
        buffer_size = file_size; // for small size test content

	printf ("file size: %lld, buffer size : %ld \r\n", file_size, buffer_size);

#ifdef MPEG_PERF
    gettimeofday(&start, NULL);
#endif
    ReadContent(pData, buffer, buffer_size);
#ifdef MPEG_PERF
    gettimeofday(&stop, NULL);
    timeused = 1000000 * ( stop.tv_sec - start.tv_sec ) + stop.tv_usec - start.tv_usec;
    printf("ReadContent time %d us \r\n", timeused);
#endif

    // Scan Buffer for MPEG_TS type
    if (OMX_ErrorNone == TryMpegTsType(buffer, buffer_size))
    {
        ret = OMX_ErrorNone;
        printf("it is a MPEG-TS content \r\n");
        goto bail;
    }

    // Scan buffer for MPEG_SYS type
    if (OMX_ErrorNone == TryMpegSysType(buffer, buffer_size))
    {
        ret =  OMX_ErrorNone;
        printf("it is a MPEG2 content \r\n");
        goto bail;
    }

bail:
    if(pData->keepOpen == OMX_FALSE)
        pData->hPipe->Close(pData->hContent);

    FSL_FREE(buffer);

    return ret;
}


/* File EOF */
