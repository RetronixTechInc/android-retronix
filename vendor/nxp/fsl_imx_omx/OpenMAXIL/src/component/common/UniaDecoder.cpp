/**
 *  Copyright (c) 2009-2015, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "UniaDecoder.h"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_LOG
#define LOG_LOG printf
#endif
#define MAX_PROFILE_ERROR_COUNT 1500 //about 1 second decoding time, 30 seconds' audio data length
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
UniaDecoder::UniaDecoder()
{
    nPorts = AUDIO_FILTER_PORT_NUMBER;
    nPushModeInputLen = 2048;
    bInContext = OMX_FALSE;
    nRingBufferScale = RING_BUFFER_SCALE;

    //these values must be reset by subclass
    role_cnt = 0;
    fsl_osal_memset(role,0,sizeof(role));
    codingType = OMX_AUDIO_CodingUnused;
    outputPortBufferSize = 0;
    decoderLibName = NULL;

    //optional value
    frameInput = OMX_FALSE;
    optionaDecoderlLibName = NULL;

    LOG_DEBUG("UniaDecoder::UniaDecoder");

}
OMX_ERRORTYPE UniaDecoder::InitComponent()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOG_DEBUG("UniaDecoder::InitComponent");


    memOps.Calloc = appLocalCalloc;
    memOps.Malloc = appLocalMalloc;
    memOps.Free= appLocalFree;
    memOps.ReAlloc= appLocalReAlloc;

    errorCount = 0;
    inputFrameCount = 0;
    consumeFrameCount= 0;
    profileErrorCount = 0;
    bSendEvent = OMX_FALSE;
    IDecoder = NULL;
    libMgr = NULL;
    hLib = NULL;

    if(decoderLibName == NULL || outputPortBufferSize == 0 || nPushModeInputLen == 0){
        LOG_ERROR("audio decoder param not inited!");
        return OMX_ErrorInsufficientResources;
    }
    LOG_DEBUG("nPushModeInputLen=%d,outputPortBufferSize=%d,codingType=%d"\
        ,nPushModeInputLen,outputPortBufferSize,codingType);
    ret = InitPort();

    if(ret != OMX_ErrorNone){
        return ret;
    }

    libMgr = FSL_NEW(ShareLibarayMgr, ());

    if(libMgr == NULL){
        ret = OMX_ErrorInsufficientResources;
        return ret;
    }

    ret = CreateDecoderInterface();

    return ret;
}
OMX_ERRORTYPE UniaDecoder::InitPort()
{

    OMX_ERRORTYPE ret = OMX_ErrorNone;

    do{
        LOG_DEBUG("UniaDec::InitPort");

        OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

        OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
        sPortDef.nPortIndex = AUDIO_FILTER_INPUT_PORT;
        sPortDef.eDir = OMX_DirInput;
        sPortDef.eDomain = OMX_PortDomainAudio;
        sPortDef.format.audio.eEncoding = (OMX_AUDIO_CODINGTYPE)codingType;
        sPortDef.bPopulated = OMX_FALSE;
        sPortDef.bEnabled = OMX_TRUE;
        sPortDef.nBufferCountMin = 1;
        sPortDef.nBufferCountActual = 3;
        sPortDef.nBufferSize = 32768;

        ret = ports[AUDIO_FILTER_INPUT_PORT]->SetPortDefinition(&sPortDef);

        if(ret != OMX_ErrorNone) {
            break;
        }

        sPortDef.nPortIndex = AUDIO_FILTER_OUTPUT_PORT;
        sPortDef.eDir = OMX_DirOutput;
        sPortDef.eDomain = OMX_PortDomainAudio;
        sPortDef.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
        sPortDef.bPopulated = OMX_FALSE;
        sPortDef.bEnabled = OMX_TRUE;
        sPortDef.nBufferCountMin = 1;
        sPortDef.nBufferCountActual = 3;
        sPortDef.nBufferSize = outputPortBufferSize;

        ret = ports[AUDIO_FILTER_OUTPUT_PORT]->SetPortDefinition(&sPortDef);

        if(ret != OMX_ErrorNone) {
            break;
        }

        OMX_INIT_STRUCT(&PcmMode, OMX_AUDIO_PARAM_PCMMODETYPE);

        PcmMode.nPortIndex = AUDIO_FILTER_OUTPUT_PORT;
        PcmMode.nChannels = 2;
        PcmMode.nSamplingRate = 44100;
        PcmMode.nBitPerSample = 16;
        PcmMode.bInterleaved = OMX_TRUE;
        PcmMode.eNumData = OMX_NumericalDataSigned;
        PcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
        PcmMode.eEndian = OMX_EndianLittle;
        PcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelNone;

    }while(0);

    if(ret != OMX_ErrorNone){
        LOG_ERROR("UniaDec::InitPort failed ret=%d\n",ret);
    }
    return ret;
}
OMX_ERRORTYPE UniaDecoder::DeInitComponent()
{
    if (libMgr) {

        if (hLib)
            libMgr->unload(hLib);
        hLib = NULL;
        FSL_FREE(IDecoder);
        FSL_DELETE(libMgr);
    }
    LOG_DEBUG("UniaDec::DeInitComponent inputFrameCount=%d,consumeFrameCount=%d",inputFrameCount,consumeFrameCount);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE UniaDecoder::AudioFilterInstanceInit()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    AUDIOFORMAT format = FORMAT_UNKNOW;
    OMX_BOOL hwBased = OMX_FALSE;

    do{

        codecConfig.buf = NULL;
        codecConfig.size = 0;
        errorCount = 0;
        profileErrorCount = 0;
        uniaHandle = NULL;

        printf("SetupDecoder %s \n", IDecoder->GetVersionInfo());

        ret = UniaDecoderGetDecoderProp(&format, &hwBased);

        if (ret == OMX_ErrorNone && IDecoder->CreateDecoderPlus != NULL && hwBased)
            uniaHandle = IDecoder->CreateDecoderPlus(&memOps, format);
        else
            uniaHandle = IDecoder->CreateDecoder(&memOps);

        if(ret == OMX_ErrorNotImplemented)
            ret = OMX_ErrorNone;

        if(uniaHandle == NULL){
            ret = OMX_ErrorNotImplemented;
            break;
        }

        //todo: query decoder buffer size for

        SetParameterToDecoder();
    }while(0);

    if(ret != OMX_ErrorNone)
        LOG_ERROR("UniaDec::AudioDecoderBaseInstanceInit ret=%d",ret);

    return ret;
}
OMX_ERRORTYPE UniaDecoder::CreateDecoderInterface()
{
    OMX_S32 ret = OMX_ErrorNone;

    do{

        tUniACodecQueryInterface  myQueryInterface;

        if(decoderLibName == NULL){
            ret = OMX_ErrorInsufficientResources;
            break;
        }
        if(optionaDecoderlLibName){
            hLib = libMgr->load((fsl_osal_char *)optionaDecoderlLibName);
            LOG_DEBUG("Audio Decoder library name 1: %s\n", optionaDecoderlLibName);
        }

        if(hLib == NULL){
            hLib = libMgr->load((fsl_osal_char *)decoderLibName);
            LOG_DEBUG("Audio Decoder library name 2: %s\n", decoderLibName);
        }

        if(NULL == hLib){
            LOG_ERROR("Fail to open Decoder library for %s\n", role);
            ret = OMX_ErrorComponentNotFound;
            break;
        }

        IDecoder = (UniaDecInterface *)FSL_MALLOC(sizeof(UniaDecInterface));
        if(IDecoder == NULL){
            ret = OMX_ErrorUndefined;
            break;
        }
        fsl_osal_memset(IDecoder,0 ,sizeof(UniaDecInterface));

        myQueryInterface = (tUniACodecQueryInterface)libMgr->getSymbol(hLib, (fsl_osal_char *)"UniACodecQueryInterface");
        if(NULL == myQueryInterface)
        {
            LOG_ERROR("Fail to query decoder interface\n");
            ret = OMX_ErrorNotImplemented;
            break;
        }

        ret = myQueryInterface(ACODEC_API_GET_VERSION_INFO, (void **)&IDecoder->GetVersionInfo);

        ret |= myQueryInterface(ACODEC_API_CREATE_CODEC, (void **)&IDecoder->CreateDecoder);
        ret |= myQueryInterface(ACODEC_API_DELETE_CODEC, (void **)&IDecoder->DeleteDecoder);
        ret |= myQueryInterface(ACODEC_API_RESET_CODEC, (void **)&IDecoder->ResetDecoder);
        ret |= myQueryInterface(ACODEC_API_SET_PARAMETER, (void **)&IDecoder->SetParameter);
        ret |= myQueryInterface(ACODEC_API_GET_PARAMETER, (void **)&IDecoder->GetParameter);
        ret |= myQueryInterface(ACODEC_API_DEC_FRAME, (void **)&IDecoder->DecodeFrame);
        ret |= myQueryInterface(ACODEC_API_GET_LAST_ERROR, (void **)&IDecoder->GetLastError);

        if(ret != OMX_ErrorNone){
            LOG_ERROR("Fail to query decoder API\n");
            ret = OMX_ErrorNotImplemented;
            break;
        }

        ret = myQueryInterface(ACODEC_API_CREATE_CODEC_PLUS, (void **)&IDecoder->CreateDecoderPlus);
        if (ret != OMX_ErrorNone){
            LOG_DEBUG("ACODEC_API_CREATE_CODEC_PLUS is not implemented\n");
            ret = OMX_ErrorNone;
        }

    }while(0);

    if(ret != OMX_ErrorNone){
        LOG_ERROR("UniaDecoder::CreateDecoderInterface ret=%d",ret);
        if (hLib)
            libMgr->unload(hLib);
        hLib = NULL;
        FSL_FREE(IDecoder);
        FSL_DELETE(libMgr);
    }

    return (OMX_ERRORTYPE)ret;
}
OMX_ERRORTYPE UniaDecoder::AudioFilterInstanceDeInit(){

    LOG_DEBUG("UniaDecoder::AudioDecoderBaseInstanceDeInit");
    FSL_FREE(codecConfig.buf);
    codecConfig.buf = NULL;

    if(IDecoder != NULL && uniaHandle != NULL){
        IDecoder->DeleteDecoder(uniaHandle);
        uniaHandle = NULL;
    }

    return OMX_ErrorNone;
}
OMX_ERRORTYPE UniaDecoder::AudioFilterCodecInit()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_S32 value = 0;
    //set stream type here for aac type after got from frame header
    if(OMX_ErrorNone == UniaDecoderGetParameter(UNIA_STREAM_TYPE,&value)){
        LOG_DEBUG("AudioFilterCodecInit UNIA_STREAM_TYPE=%d",value);
        IDecoder->SetParameter(uniaHandle,UNIA_STREAM_TYPE,(UniACodecParameter*)&value);
    }
    return ret;
}
OMX_ERRORTYPE UniaDecoder::SetParameterToDecoder()
{
    OMX_S32 value;

    for ( OMX_U32 i = UNIA_SAMPLERATE; i < UNIA_STREAM_TYPE; i++){
        if(i == UNIA_CODEC_DATA)
            continue;

        value = 0;
        if(OMX_ErrorNone == UniaDecoderGetParameter((UA_ParaType)i,&value)){
            IDecoder->SetParameter(uniaHandle,(UA_ParaType)i,(UniACodecParameter*)&value);
        }
    }

    //set output format for channel layout
    fsl_osal_memset(&channelTable,0,sizeof(CHAN_TABLE));
    if(OMX_ErrorNone == UniaDecoderGetParameter(UNIA_CHAN_MAP_TABLE,(OMX_S32*)&channelTable)){
        IDecoder->SetParameter(uniaHandle,UNIA_CHAN_MAP_TABLE,(UniACodecParameter*)&channelTable);
    }

    value = 0;
    if(OMX_ErrorNone == UniaDecoderGetParameter(UNIA_WMA_BlOCKALIGN,&value)){
        IDecoder->SetParameter(uniaHandle,UNIA_WMA_BlOCKALIGN,(UniACodecParameter*)&value);
    }

    value = 0;
    if(OMX_ErrorNone == UniaDecoderGetParameter(UNIA_WMA_VERSION,&value)){
        IDecoder->SetParameter(uniaHandle,UNIA_WMA_VERSION,(UniACodecParameter*)&value);
    }

    value = 0;
    if(OMX_ErrorNone == UniaDecoderGetParameter(UNIA_RA_FRAME_BITS,&value)){
        IDecoder->SetParameter(uniaHandle,UNIA_RA_FRAME_BITS,(UniACodecParameter*)&value);
    }

    LOG_DEBUG("SetParameterToDecoder");
    return OMX_ErrorNone;
}
OMX_ERRORTYPE UniaDecoder::ResetParameterWhenOutputChange()
{
    UniAcodecOutputPCMFormat outputValue;
    OMX_S32 value = 0;

    if(ACODEC_SUCCESS == IDecoder->GetParameter(uniaHandle,UNIA_OUTPUT_PCM_FORMAT,(UniACodecParameter*)&outputValue)){
        if(outputValue.samplerate == 0 || outputValue.channels == 0 || outputValue.depth == 0){
            LOG_ERROR("ResetParameterWhenOutputChange HAS 0 sample rate=%d,channel=%d,depth=%d",
                outputValue.samplerate,outputValue.channels,outputValue.depth);
            return OMX_ErrorBadParameter;
        }

        if(PcmMode.nSamplingRate != outputValue.samplerate || PcmMode.nChannels != outputValue.channels){
            bSendEvent = OMX_TRUE;
            LOG_DEBUG("bSendEvent 1");
        }else if(!bSendFirstPortSettingChanged){
            bSendFirstPortSettingChanged = OMX_TRUE;
            bSendEvent = OMX_FALSE;
            LOG_DEBUG("do not send event");
        }else{
            LOG_DEBUG("bSendEvent 2");
            bSendEvent = OMX_TRUE;
        }

        PcmMode.nSamplingRate = outputValue.samplerate;
        PcmMode.nChannels = outputValue.channels;
        nOutputBitPerSample = PcmMode.nBitPerSample = outputValue.depth;
        PcmMode.bInterleaved = (OMX_BOOL)outputValue.interleave;

        #ifndef OMX_STEREO_OUTPUT
        if(PcmMode.nChannels > 2){
            MapOutputLayoutChannel(&outputValue);
        }
        #endif

        LOG_DEBUG("OUTPUT CHANGED channel=%d,layout=%d,%d,%d,%d,%d,%d,%d,%d",PcmMode.nChannels,
            PcmMode.eChannelMapping[0],PcmMode.eChannelMapping[1],PcmMode.eChannelMapping[2],
            PcmMode.eChannelMapping[3],PcmMode.eChannelMapping[4],PcmMode.eChannelMapping[5],
            PcmMode.eChannelMapping[6],PcmMode.eChannelMapping[7]);
    }

    value = 0;
    if(ACODEC_SUCCESS == IDecoder->GetParameter(uniaHandle,UNIA_SAMPLERATE,(UniACodecParameter*)&value)){
        UniaDecoderSetParameter(UNIA_SAMPLERATE,value);
    }

    value = 0;
    if(ACODEC_SUCCESS == IDecoder->GetParameter(uniaHandle,UNIA_CHANNEL,(UniACodecParameter*)&value)){
        UniaDecoderSetParameter(UNIA_CHANNEL,value);
    }

    value = 0;
    if(ACODEC_SUCCESS == IDecoder->GetParameter(uniaHandle,UNIA_BITRATE,(UniACodecParameter*)&value)){
        UniaDecoderSetParameter(UNIA_BITRATE,value);
    }

    value = 0;
    if(ACODEC_SUCCESS == IDecoder->GetParameter(uniaHandle,UNIA_DEPTH,(UniACodecParameter*)&value)){
        if(value > 0)
            UniaDecoderSetParameter(UNIA_DEPTH,value);
    }
    LOG_DEBUG("ResetParameterWhenOutputChange nSamplingRate=%d,nChannels=%d",PcmMode.nSamplingRate,PcmMode.nChannels);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE UniaDecoder::MapOutputLayoutChannel(UniAcodecOutputPCMFormat * outputValue)
{
    OMX_U32 nChannels = outputValue->channels;
    OMX_U32 i = 0;

    if(nChannels == 0 || nChannels > UA_CHANNEL_MAX){
        return OMX_ErrorBadParameter;
    }

    fsl_osal_memset(&PcmMode.eChannelMapping[0],0 ,OMX_AUDIO_MAXCHANNELS*sizeof(OMX_AUDIO_CHANNELTYPE));

    for( i = 0; i < nChannels; i++){

        switch(outputValue->layout[i]){
            case UA_CHANNEL_FRONT_LEFT:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelLF;
                break;
            case UA_CHANNEL_FRONT_RIGHT:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelRF;
                break;
            case UA_CHANNEL_REAR_CENTER:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelCS;
                break;
            case UA_CHANNEL_REAR_LEFT:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelLR;
                break;
            case UA_CHANNEL_REAR_RIGHT:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelRR;
                break;
            case UA_CHANNEL_LFE:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelLFE;
                break;
            case UA_CHANNEL_FRONT_CENTER:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelCF;
                break;
            case UA_CHANNEL_FRONT_LEFT_CENTER:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelLF;
                break;
            case UA_CHANNEL_FRONT_RIGHT_CENTER:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelRF;
                break;
            case UA_CHANNEL_SIDE_LEFT:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelLS;
                break;
            case UA_CHANNEL_SIDE_RIGHT:
                PcmMode.eChannelMapping[i] = OMX_AUDIO_ChannelRS;
                break;
            default:
                break;
            }

    }

    return OMX_ErrorNone;
}
AUDIO_FILTERRETURNTYPE UniaDecoder::AudioFilterFrame()
{
    AUDIO_FILTERRETURNTYPE ret = AUDIO_FILTER_SUCCESS;
    OMX_S32 decoderRet = ACODEC_SUCCESS;
    OMX_U32 nActuralLen = 0;
    //OMX_U32 InputOffsetBegin = 0;
    OMX_U32 consumeLen = 0;
    OMX_U32 InputLen = 0;;
    OMX_U32 InputOffset = 0;
    OMX_U8* pBuffer = NULL;
    UniaDecFrameInfo FrameInfo;
    fsl_osal_memset(&FrameInfo, 0, sizeof(UniaDecFrameInfo));

    AudioRingBuffer.BufferGet(&pBuffer, nPushModeInputLen, &InputLen);

    if(ePlayMode == DEC_STREAM_MODE && OMX_ErrorNone == UniaDecoderParseFrame(pBuffer,InputLen,&FrameInfo)){
        if(FrameInfo.bGotOneFrame){
            LOG_LOG("got one frame,size=%d,next=%d,header=%d"
                ,FrameInfo.nFrameSize,FrameInfo.nNextSize,FrameInfo.nHeaderSize);
            InputLen = FrameInfo.nFrameSize;
            InputOffset = 0;
            AudioRingBuffer.BufferGet(&pBuffer, InputLen, &nActuralLen);

            if(FrameInfo.nNextSize > 0)
                nRequiredSize = FrameInfo.nNextSize+DEFAULT_REQUIRED_SIZE;
            else
                nRequiredSize = 0;

            if(nActuralLen < InputLen){
                 LOG_ERROR("nActuralLen=%d,input=%d",nActuralLen,InputLen);
                return ret;
            }
        }else{
            nRequiredSize = FrameInfo.nFrameSize+DEFAULT_REQUIRED_SIZE;
            LOG_LOG("can't get one frame,size=%d",FrameInfo.nFrameSize);
            return ret;
        }
    }else if(frameInput && InputLen != 0){
        OMX_U32 nFrameLen;
        nFrameLen = TS_Manager.GetFrameLen();
        if (nFrameLen == 0)
            nFrameLen = nPushModeInputLen;

        AudioRingBuffer.BufferGet(&pBuffer, nFrameLen, &InputLen);
    }

    //InputOffsetBegin = InputOffset;
    if(pBuffer && InputLen > 0){
        decoderRet = IDecoder->DecodeFrame(uniaHandle,pBuffer,InputLen,
            &InputOffset,&pOutBufferHdr->pBuffer,&nActuralLen);
        if(!frameInput){
            AudioRingBuffer.BufferConsumered(InputOffset);
        }
        inputFrameCount += InputOffset;
    }else{
        decoderRet = IDecoder->DecodeFrame(uniaHandle,NULL,0,&InputOffset,&pOutBufferHdr->pBuffer,&nActuralLen);
    }
    LOG_LOG("decoderRet=%d,InputLen=%d,InputOffset=%d,nActuralLen=%d",decoderRet,InputLen,InputOffset,nActuralLen);
    if(decoderRet == ACODEC_SUCCESS || decoderRet == ACODEC_CAPIBILITY_CHANGE){

        if((decoderRet & ACODEC_CAPIBILITY_CHANGE) && OMX_ErrorNone == ResetParameterWhenOutputChange()){
            if(nActuralLen != 0){
                TS_PerFrame = (OMX_S64)nActuralLen*8*OMX_TICKS_PER_SECOND/PcmMode.nChannels \
                    /PcmMode.nSamplingRate/nOutputBitPerSample;//PcmMode.nBitPerSample;
            }

            if(bSendEvent){
                LOG_DEBUG("send output port changed event");
                SendEvent(OMX_EventPortSettingsChanged, AUDIO_FILTER_OUTPUT_PORT, 0, NULL);
            }
        }
        pOutBufferHdr->nOffset = 0;
        pOutBufferHdr->nFilledLen = nActuralLen;

        TS_PerFrame = (OMX_S64)pOutBufferHdr->nFilledLen*8*OMX_TICKS_PER_SECOND/PcmMode.nChannels \
                /PcmMode.nSamplingRate/nOutputBitPerSample;//PcmMode.nBitPerSample;

        IDecoder->GetParameter(uniaHandle,UNIA_CONSUMED_LENGTH,(UniACodecParameter*)&consumeLen);

        TS_Manager.Consumered(consumeLen);
        LOG_LOG("AudioTime.TS_Increase TS_PerFrame=%lld,consumeLen=%d",TS_PerFrame,consumeLen);
        TS_Manager.TS_SetIncrease(TS_PerFrame);
        consumeFrameCount += consumeLen;
        if(frameInput){
            //more than one frames could be in a frame buffer, so consume one frame's length.
            AudioRingBuffer.BufferConsumered(consumeLen);
        }

        if(nActuralLen == 0){
            ret = AUDIO_FILTER_NEEDMORE;
        }

        LOG_LOG("Decoder outputLength=%d,pOutBufferHdr->nTimeStamp=%lld",\
            pOutBufferHdr->nFilledLen,pOutBufferHdr->nTimeStamp);
        LOG_LOG("OUTPUT = %x%x%x%x",pOutBufferHdr->pBuffer[0],pOutBufferHdr->pBuffer[1],pOutBufferHdr->pBuffer[2],pOutBufferHdr->pBuffer[3]);

        #if 0
        FILE * pfile;
        pfile = fopen("/data/data/pcm","ab");
        if(pfile){
            fwrite(pOutBufferHdr->pBuffer,1,pOutBufferHdr->nFilledLen,pfile);
            fclose(pfile);
        }
        #endif
        if(nActuralLen != 0){
            profileErrorCount = 0;
        }
    }else if(decoderRet == ACODEC_END_OF_STREAM){
        pOutBufferHdr->nOffset = 0;
        pOutBufferHdr->nFilledLen = 0;
        LOG_DEBUG("ACODEC_END_OF_STREAM");
        ret = AUDIO_FILTER_EOS;
    }else if(decoderRet == ACODEC_NOT_ENOUGH_DATA){
        ret = AUDIO_FILTER_NEEDMORE;
    }else if((decoderRet == ACODEC_ERROR_STREAM || decoderRet == ACODEC_ERR_UNKNOWN) && pBuffer){
        IDecoder->GetParameter(uniaHandle,UNIA_CONSUMED_LENGTH,(UniACodecParameter*)&consumeLen);
        IDecoder->ResetDecoder(uniaHandle);
        if(frameInput){
            AudioRingBuffer.BufferConsumered(InputOffset);
        }
        TS_Manager.Consumered(InputOffset);
        consumeFrameCount += InputOffset;
        LOG_DEBUG("ERROR CONSUME LEN=%d,getlen=%d",consumeLen,InputOffset);
        ret = AUDIO_FILTER_FAILURE;
        errorCount ++;
        LOG_DEBUG("ACODEC_ERROR_STREAM no more data");
    }else if((decoderRet == ACODEC_ERROR_STREAM || decoderRet == ACODEC_ERR_UNKNOWN) && !pBuffer){
        pOutBufferHdr->nOffset = 0;
        pOutBufferHdr->nFilledLen = 0;
        LOG_DEBUG("SET TO EOS");
        ret = AUDIO_FILTER_EOS;
    }else if(decoderRet == ACODEC_PROFILE_NOT_SUPPORT){
        pOutBufferHdr->nOffset = 0;
        pOutBufferHdr->nFilledLen = 0;
        LOG_DEBUG("ACODEC_PROFILE_NOT_SUPPORT");
        ret = AUDIO_FILTER_EOS;
        profileErrorCount++;
    }else if(decoderRet == ACODEC_INIT_ERR){
        pOutBufferHdr->nOffset = 0;
        pOutBufferHdr->nFilledLen = 0;
        LOG_DEBUG("ACODEC RESULT ACODEC_INIT_ERR!");
        ret = AUDIO_FILTER_FATAL_ERROR;
    }else if(ACODEC_PARA_ERROR == decoderRet){
        pOutBufferHdr->nOffset = 0;
        pOutBufferHdr->nFilledLen = 0;
        if(OMX_ErrorNone != UniaDecoderCheckParameter()){
            IDecoder->ResetDecoder(uniaHandle);
            ret = AUDIO_FILTER_FATAL_ERROR;
            LOG_ERROR("ACODEC_PARA_ERROR check failed");
        }
    }else{
        pOutBufferHdr->nOffset = 0;
        pOutBufferHdr->nFilledLen = 0;
        LOG_LOG("error decoderRet=%d",decoderRet);
        ret = AUDIO_FILTER_FAILURE;
        errorCount ++;
    }

    //for test usage
    if(errorCount > 500){
        LOG_DEBUG("Unia Decoder error count > 500 ***!!!");
        errorCount = 0;
    }

    //check if count of profile error reaches the limited.
    if(profileErrorCount > MAX_PROFILE_ERROR_COUNT){
        ret = AUDIO_FILTER_FATAL_ERROR;
        profileErrorCount = 0;
        LOG_DEBUG("return AUDIO_FILTER_FATAL_ERROR instead of ACODEC_PROFILE_NOT_SUPPORT");
    }

    return ret;
}
OMX_ERRORTYPE UniaDecoder::AudioFilterReset()
{
    OMX_S32 ret = ACODEC_SUCCESS;

    if(IDecoder == NULL || uniaHandle == NULL)
        return OMX_ErrorUndefined;

    ret = IDecoder->ResetDecoder(uniaHandle);
    LOG_LOG("UniaDec::AudioDecoderBaseReset ret=%d",ret);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE UniaDecoder::AudioFilterCheckCodecConfig()
{
    OMX_S32 ret = OMX_ErrorNone;
    if(IDecoder == NULL || uniaHandle == NULL)
        return OMX_ErrorUndefined;

    if (codecConfig.buf != NULL) {
        codecConfig.buf = (char *)FSL_REALLOC(codecConfig.buf, \
            codecConfig.size + pInBufferHdr->nFilledLen);
        if (codecConfig.buf == NULL)
        {
            LOG_ERROR("Can't get memory.\n");
            return OMX_ErrorInsufficientResources;
        }
        fsl_osal_memcpy(codecConfig.buf + codecConfig.size, pInBufferHdr->pBuffer, pInBufferHdr->nFilledLen);
        codecConfig.size += pInBufferHdr->nFilledLen;

    } else {
         codecConfig.buf = (char *)FSL_MALLOC(pInBufferHdr->nFilledLen);
        if (codecConfig.buf == NULL)
        {
            LOG_ERROR("Can't get memory.\n");
            return OMX_ErrorInsufficientResources;
        }
        fsl_osal_memcpy(codecConfig.buf, pInBufferHdr->pBuffer, pInBufferHdr->nFilledLen);
        codecConfig.size = pInBufferHdr->nFilledLen;
    }

    ret = IDecoder->SetParameter(uniaHandle,UNIA_CODEC_DATA,(UniACodecParameter*)&codecConfig);

    LOG_DEBUG("UniaDec::AudioDecoderBaseCheckCodecConfig nFilledLen=%d,ret=%d",pInBufferHdr->nFilledLen,ret);
    pInBufferHdr->nFilledLen = 0;

    return (OMX_ERRORTYPE)ret;
}
OMX_ERRORTYPE UniaDecoder::UniaDecoderCheckParameter()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    return ret;
}

OMX_ERRORTYPE UniaDecoder::PassParamToOutputWithoutDecode()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_S32 decoderRet = ACODEC_SUCCESS;
    UniAcodecOutputPCMFormat outputValue;

    decoderRet = IDecoder->GetParameter(uniaHandle,UNIA_OUTPUT_PCM_FORMAT,(UniACodecParameter*)&outputValue);
    if(ACODEC_SUCCESS != decoderRet || outputValue.samplerate == 0 || PcmMode.nSamplingRate == 0) {
        return OMX_ErrorBadParameter;
    }

    PcmMode.nSamplingRate = outputValue.samplerate;
    PcmMode.nChannels = outputValue.channels;

    return ret;
}
OMX_ERRORTYPE UniaDecoder::UniaDecoderParseFrame(OMX_U8* pBuffer,OMX_U32 len,UniaDecFrameInfo *info)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE UniaDecoder::UniaDecoderGetDecoderProp(AUDIOFORMAT *formatType, OMX_BOOL *isHwBased)
{
    return OMX_ErrorNotImplemented;
}

void UniaDecoder::SetDecLibName(const char * name)
{
    if (name != NULL) {
        decoderLibName = name;
        optionaDecoderlLibName = NULL;    //reset optionaDecoderlLibName to avoid load optionaDecoder
    }
}


