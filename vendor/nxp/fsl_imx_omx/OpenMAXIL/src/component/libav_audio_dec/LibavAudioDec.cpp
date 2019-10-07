/**
 *  Copyright (c) 2014, Freescale Semiconductor Inc.,
 *  Copyright 2017 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "LibavAudioDec.h"

#define LIBAV_COMP_NAME_ADPCMDEC "OMX.Freescale.std.audio_decoder.adpcm.sw-based"
#define LIBAV_COMP_NAME_G711DEC "OMX.Freescale.std.audio_decoder.g711.sw-based"
#define LIBAV_COMP_NAME_OPUSDEC "OMX.Freescale.std.audio_decoder.opus.sw-based"
#define LIBAV_COMP_NAME_APEDEC "OMX.Freescale.std.audio_decoder.ape.sw-based"

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_WARNING
#define LOG_WARNING printf
#endif

#define LIBAV_APE_AUDIO_PUSH_MODE_LEN (1<<20)
#define LIBAV_OUTPUT_PORT_BUFFER_SIZE (128<<10)


static void libav_log_callback (void *ptr, int level, const char *fmt, va_list vl)
{
    LogLevel log_level;

    switch (level) {
        case AV_LOG_QUIET:
            log_level = LOG_LEVEL_NONE;
            break;
        case AV_LOG_ERROR:
            log_level = LOG_LEVEL_ERROR;
            break;
        case AV_LOG_INFO:
            log_level = LOG_LEVEL_INFO;
            break;
        case AV_LOG_DEBUG:
            log_level = LOG_LEVEL_DEBUG;
            break;
        default:
            log_level = LOG_LEVEL_INFO;
            break;
    }

    LOG2(log_level, fmt, vl);
}

OMX_ERRORTYPE  LibavAudioDec::GetCodecID()
{
    switch ((OMX_U32)CodingType) {
        case OMX_AUDIO_CodingG711:
        {
            if (InputMode.PcmMode.ePCMMode == OMX_AUDIO_PCMModeALaw)
                codecID = AV_CODEC_ID_PCM_ALAW;
            else if (InputMode.PcmMode.ePCMMode == OMX_AUDIO_PCMModeMULaw)
                codecID = AV_CODEC_ID_PCM_MULAW;
        }
            break;
        case OMX_AUDIO_CodingADPCM:
        {
            if (InputMode.AdpcmMode.CodecID == ADPCM_MS)
                codecID = AV_CODEC_ID_ADPCM_MS;
            else if (InputMode.AdpcmMode.CodecID == ADPCM_IMA_WAV)
                codecID = AV_CODEC_ID_ADPCM_IMA_WAV;
        }
            break;

        case OMX_AUDIO_CodingOPUS:
            codecID = AV_CODEC_ID_OPUS;
            break;
        case OMX_AUDIO_CodingAPE:
            codecID = AV_CODEC_ID_APE;
            break;
        default:
            return OMX_ErrorUndefined;
    }
    return OMX_ErrorNone;
}

LibavAudioDec::LibavAudioDec()
{
    ComponentVersion.s.nVersionMajor = 0x1;
    ComponentVersion.s.nVersionMinor = 0x1;
    ComponentVersion.s.nRevision = 0x2;
    ComponentVersion.s.nStep = 0x0;
    role_cnt = 1;
    role[0] = (OMX_STRING)"audio_decoder.adpcm";
    bInContext = OMX_FALSE;
    nPorts = AUDIO_FILTER_PORT_NUMBER;
    nPushModeInputLen = 4096;
    nRingBufferScale = RING_BUFFER_SCALE;
    CodingType = OMX_AUDIO_CodingUnused;
    codecID = AV_CODEC_ID_NONE;
    codecContext = NULL;
    frame = NULL;
    avr = NULL;
    errorCnt = 0;
    codecDataSize = 0;
    bNeedInput = OMX_TRUE;
    nInputSize = 0;
    ILibavDec = NULL;
    libavDecMgr = NULL;
}

OMX_ERRORTYPE LibavAudioDec::InitComponent()
{
    /*set default definition*/
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    sPortDef.eDir = OMX_DirInput;
    sPortDef.eDomain = OMX_PortDomainAudio;
    sPortDef.format.audio.eEncoding = CodingType;
    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_TRUE;
    sPortDef.nBufferCountMin = 1;
    sPortDef.nBufferCountActual = 3;
    sPortDef.nBufferSize = 32768;
    ret = ports[AUDIO_FILTER_INPUT_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
        LOG_ERROR("Set port definition for port[%d] failed.\n", AUDIO_FILTER_INPUT_PORT);
        return ret;
    }

    sPortDef.nPortIndex = AUDIO_FILTER_OUTPUT_PORT;
    sPortDef.eDir = OMX_DirOutput;
    sPortDef.eDomain = OMX_PortDomainAudio;
    sPortDef.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    sPortDef.bPopulated = OMX_FALSE;
    sPortDef.bEnabled = OMX_TRUE;
    sPortDef.nBufferCountMin = 1;
    sPortDef.nBufferCountActual = 3;
    sPortDef.nBufferSize = LIBAV_OUTPUT_PORT_BUFFER_SIZE;
    ret = ports[AUDIO_FILTER_OUTPUT_PORT]->SetPortDefinition(&sPortDef);
    if(ret != OMX_ErrorNone) {
	LOG_ERROR("Set port definition for port[%d] failed.\n", 0);
	return ret;
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

    libavDecMgr = FSL_NEW(LibavDecInterfaceMgr, ());

    if(libavDecMgr == NULL)
        return OMX_ErrorInsufficientResources;

    ILibavDec = (LibavDecInterface*)FSL_MALLOC(sizeof(LibavDecInterface));
    if (!ILibavDec)
        return OMX_ErrorInsufficientResources;

    fsl_osal_memset(ILibavDec, 0, sizeof(LibavDecInterface));

	return ret;
}

OMX_ERRORTYPE LibavAudioDec::DeInitComponent()
{
    if (ILibavDec) {
        libavDecMgr->DeleteLibavDecInterfaces(ILibavDec);
        FSL_DELETE(ILibavDec);
    }

    if(libavDecMgr)
        FSL_FREE(libavDecMgr);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavAudioDec::GetCodecContext()
{
    switch ((OMX_U32)CodingType) {
        case OMX_AUDIO_CodingG711:
            codecContext->channels = InputMode.PcmMode.nChannels;
            codecContext->sample_rate = InputMode.PcmMode.nSamplingRate;
            codecContext->bits_per_coded_sample = InputMode.PcmMode.nBitPerSample;
            break;

        case OMX_AUDIO_CodingADPCM:
            codecContext->channels = InputMode.AdpcmMode.nChannels;
            codecContext->sample_rate = InputMode.AdpcmMode.nSampleRate;
            codecContext->bits_per_coded_sample = InputMode.AdpcmMode.nBitPerSample;
            codecContext->block_align = InputMode.AdpcmMode.nBlockAlign;
            LOG_DEBUG("\t channels: %d \t sample_rate: %d \t bits_per_sample: %d \t block_align: %d \tcodecID: %x\n", \
                                           InputMode.AdpcmMode.nChannels, InputMode.AdpcmMode.nSampleRate, \
                                           InputMode.AdpcmMode.nBitPerSample, InputMode.AdpcmMode.nBlockAlign, codecID);
            break;

        case OMX_AUDIO_CodingOPUS:
            codecContext->channels = InputMode.OpusMode.nChannels;
            codecContext->sample_rate = InputMode.OpusMode.nSampleRate;
            break;
        case OMX_AUDIO_CodingAPE:
            codecContext->channels = InputMode.ApeMode.nChannels;
            codecContext->sample_rate = InputMode.ApeMode.nSampleRate;
            codecContext->bits_per_coded_sample = InputMode.ApeMode.nBitPerSample;
            codecContext->extradata = codecData;
            codecContext->extradata_size = codecDataSize;
            break;

        default:
            break;

    }
    LOG_DEBUG("GetCodecContext\n sample rate %d channels %d bit per sample %d block align %d\n", \
                                                             codecContext->sample_rate, \
                                                             codecContext->channels, \
                                                             codecContext->bits_per_coded_sample, \
                                                             codecContext->block_align);

	return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavAudioDec::AudioFilterInstanceInit()
{
    OMX_S32 ret = 0;

    ret = libavDecMgr->CreateLibavDecInterfaces(ILibavDec);
    if (ret)
        return OMX_ErrorUndefined;

    ILibavDec->AvLogSetLevel(AV_LOG_DEBUG);
    ILibavDec->AvLogSetCallback(libav_log_callback);

    ILibavDec->AvcodecRegisterAll();

    if (OMX_ErrorNone != GetCodecID())
        return OMX_ErrorUndefined;
	if (codecID == AV_CODEC_ID_NONE)
		return OMX_ErrorUndefined;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavAudioDec::AudioFilterCodecInit()
{
    AVCodec* codec;

	codec =  ILibavDec->AvcodecFindDecoder(codecID);
    if(!codec){
        LOG_ERROR("find decoder fail, codecID %x" , codecID);
        return OMX_ErrorUndefined;
    }

    codecContext = ILibavDec->AvcodecAllocContext3(codec);
    if(!codecContext){
        LOG_ERROR("alloc context fail");
        return OMX_ErrorUndefined;
    }

    GetCodecContext();
    if(ILibavDec->AvcodecOpen2(codecContext, codec, NULL) < 0){
        LOG_ERROR(" %x codec open fail", codecID);
        return OMX_ErrorUndefined;
    }

    frame = ILibavDec->AvFrameAlloc();
    if(frame == NULL){
        return OMX_ErrorInsufficientResources;
    }

    nInputSize = 0;
    bNeedInput = OMX_TRUE;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavAudioDec::AudioFilterInstanceDeInit()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(codecContext) {
        ILibavDec->AvcodecClose(codecContext);
        ILibavDec->AvFree(codecContext);
        codecContext = NULL;
    }
	if(frame) {
        ILibavDec->AvFrameFree(&frame);
        frame = NULL;
    }
    if(avr) {
        ILibavDec->SwrClose(avr);
        ILibavDec->SwrFree(&avr);
        avr = NULL;
    }

    errorCnt = 0;
    return ret;
}

OMX_ERRORTYPE LibavAudioDec::AudioFilterGetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch ((OMX_U32)nParamIndex) {
        case OMX_IndexParamStandardComponentRole:
            fsl_osal_strcpy((OMX_STRING)((OMX_PARAM_COMPONENTROLETYPE*) \
                        pComponentParameterStructure)->cRole,(OMX_STRING)cRole);
            break;
        case OMX_IndexParamAudioAdpcm:
        {
	        OMX_AUDIO_PARAM_ADPCMMODETYPE *pAdpcmMode;
            pAdpcmMode = (OMX_AUDIO_PARAM_ADPCMMODETYPE*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pAdpcmMode, OMX_AUDIO_PARAM_ADPCMMODETYPE, ret);

            if(ret != OMX_ErrorNone)
                break;

            fsl_osal_memcpy(pAdpcmMode, &(InputMode.AdpcmMode), sizeof(OMX_AUDIO_PARAM_ADPCMMODETYPE));
            CodingType = OMX_AUDIO_CodingADPCM;
            fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_ADPCMDEC);
        }
            break;
        case OMX_IndexParamAudioOpus:
        {
            OMX_AUDIO_PARAM_OPUSTYPE *pOpusMode;
            pOpusMode = (OMX_AUDIO_PARAM_OPUSTYPE*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pOpusMode, OMX_AUDIO_PARAM_OPUSTYPE, ret);
            if(ret != OMX_ErrorNone)
                break;
            fsl_osal_memcpy(pOpusMode, &(InputMode.OpusMode), sizeof(OMX_AUDIO_PARAM_OPUSTYPE));
            CodingType = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingOPUS;
            fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_OPUSDEC);
        }
            break;
        case OMX_IndexParamAudioApe:
        {
            OMX_AUDIO_PARAM_APETYPE *pApeMode;
            pApeMode = (OMX_AUDIO_PARAM_APETYPE*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pApeMode, OMX_AUDIO_PARAM_APETYPE, ret);
            if(ret != OMX_ErrorNone)
                break;
            fsl_osal_memcpy(pApeMode, &(InputMode.ApeMode), sizeof(OMX_AUDIO_PARAM_APETYPE));
            CodingType = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingAPE;
            fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_APEDEC);
        }
            break;
        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}

OMX_ERRORTYPE  LibavAudioDec::SetRoleFormat(OMX_STRING role)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if(fsl_osal_strcmp(role, "audio_decoder.adpcm") == 0) {
        CodingType = OMX_AUDIO_CodingADPCM;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_ADPCMDEC);
        OMX_INIT_STRUCT(&(InputMode.PcmMode), OMX_AUDIO_PARAM_PCMMODETYPE);
    }
	else if(fsl_osal_strcmp(role, "audio_decoder.g711") == 0) {
        CodingType = OMX_AUDIO_CodingG711;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_G711DEC);
        OMX_INIT_STRUCT(&(InputMode.AdpcmMode), OMX_AUDIO_PARAM_ADPCMMODETYPE)
    }
    else if(fsl_osal_strcmp(role, "audio_decoder.opus") == 0)
    {
        CodingType = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingOPUS;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_OPUSDEC);
        OMX_INIT_STRUCT(&(InputMode.OpusMode), OMX_AUDIO_PARAM_OPUSTYPE)
    }
    else if(fsl_osal_strcmp(role, "audio_decoder.ape") == 0)
    {
        CodingType = (OMX_AUDIO_CODINGTYPE)OMX_AUDIO_CodingAPE;
        fsl_osal_strcpy((fsl_osal_char*)name, LIBAV_COMP_NAME_APEDEC);
        nPushModeInputLen = LIBAV_APE_AUDIO_PUSH_MODE_LEN;
        OMX_INIT_STRUCT(&(InputMode.ApeMode), OMX_AUDIO_PARAM_APETYPE)
    }
	else {
        CodingType = OMX_AUDIO_CodingUnused;
        codecID = AV_CODEC_ID_NONE;
        LOG_ERROR("%s: failure: unknow role: %s \r\n",__FUNCTION__,role);
        return OMX_ErrorUndefined;
    }

    ret = CheckPortFmt();
	if(ret != OMX_ErrorNone)
		return ret;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavAudioDec::AudioFilterSetParameter(
        OMX_INDEXTYPE nParamIndex,
        OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch((int)nParamIndex){
        case OMX_IndexParamStandardComponentRole:
        {
            fsl_osal_strcpy( (fsl_osal_char *)cRole,(fsl_osal_char *) \
            ((OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure)->cRole);
            if (OMX_ErrorNone != SetRoleFormat((OMX_STRING)cRole)) {
                LOG_DEBUG("SetRoleFormat return error.\n");
                return OMX_ErrorBadParameter;
            }
        }
            break;

        case OMX_IndexParamAudioAdpcm:
        {
	        OMX_AUDIO_PARAM_ADPCMMODETYPE *pAdpcmMode;
            pAdpcmMode = (OMX_AUDIO_PARAM_ADPCMMODETYPE*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pAdpcmMode, OMX_AUDIO_PARAM_ADPCMMODETYPE, ret);

            if(ret != OMX_ErrorNone)
                break;

            fsl_osal_memcpy(&(InputMode.AdpcmMode), pAdpcmMode, sizeof(OMX_AUDIO_PARAM_ADPCMMODETYPE));

            if (PcmMode.nChannels != InputMode.AdpcmMode.nChannels || \
		            PcmMode.nSamplingRate != InputMode.AdpcmMode.nSampleRate || \
		            PcmMode.nBitPerSample != InputMode.AdpcmMode.nBitPerSample) {
                PcmMode.nChannels = InputMode.AdpcmMode.nChannels;
                PcmMode.nSamplingRate = InputMode.AdpcmMode.nSampleRate;
            }

            if (InputMode.AdpcmMode.nBlockAlign > 0)
                nPushModeInputLen = InputMode.AdpcmMode.nBlockAlign;
        }
            break;

        case OMX_IndexParamAudioOpus:
        {
            OMX_AUDIO_PARAM_OPUSTYPE *pOpusMode;
            pOpusMode = (OMX_AUDIO_PARAM_OPUSTYPE*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pOpusMode, OMX_AUDIO_PARAM_OPUSTYPE, ret);
            if(ret != OMX_ErrorNone)
                break;
            fsl_osal_memcpy(&(InputMode.OpusMode), pOpusMode, sizeof(OMX_AUDIO_PARAM_OPUSTYPE));
            if (PcmMode.nChannels != InputMode.OpusMode.nChannels || PcmMode.nSamplingRate != InputMode.OpusMode.nSampleRate) {
                PcmMode.nChannels = InputMode.OpusMode.nChannels;
                PcmMode.nSamplingRate = InputMode.OpusMode.nSampleRate;
            }
        }
            break;
        case OMX_IndexParamAudioApe:
        {
            OMX_AUDIO_PARAM_APETYPE *pApeMode;
            pApeMode = (OMX_AUDIO_PARAM_APETYPE*)pComponentParameterStructure;
            OMX_CHECK_STRUCT(pApeMode, OMX_AUDIO_PARAM_APETYPE, ret);
            if(ret != OMX_ErrorNone)
                break;
            fsl_osal_memcpy(&(InputMode.ApeMode), pApeMode, sizeof(OMX_AUDIO_PARAM_APETYPE));
            if (PcmMode.nChannels != InputMode.ApeMode.nChannels || PcmMode.nSamplingRate != InputMode.ApeMode.nSampleRate) {
                PcmMode.nChannels = InputMode.ApeMode.nChannels;
                PcmMode.nSamplingRate = InputMode.ApeMode.nSampleRate;
            }
        }
            break;

        default:
            ret = OMX_ErrorUnsupportedIndex;
            break;
    }

    return ret;
}

OMX_ERRORTYPE LibavAudioDec::AudioFilterSetParameterPCM()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

	InputMode.PcmMode.nChannels = PcmMode.nChannels;
	InputMode.PcmMode.nSamplingRate = PcmMode.nSamplingRate;
	InputMode.PcmMode.nBitPerSample = PcmMode.nBitPerSample;
	InputMode.PcmMode.ePCMMode = PcmMode.ePCMMode;

	PcmMode.nPortIndex = AUDIO_FILTER_OUTPUT_PORT;
	PcmMode.nBitPerSample = 16;
	PcmMode.bInterleaved = OMX_TRUE;
	PcmMode.eNumData = OMX_NumericalDataSigned;
	PcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
	PcmMode.eEndian = OMX_EndianLittle;
	PcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelNone;

    return ret;
}

OMX_ERRORTYPE LibavAudioDec::AVConvertSample(OMX_U32 *len)
{
    OMX_U32 data_size;
    OMX_S32 out_samples, out_size;
    int out_linesize;
    enum AVSampleFormat out_fmt = AV_SAMPLE_FMT_S16;
    OMX_U32 osize = ILibavDec->AvGetBytesPerSample(out_fmt);
    OMX_U32 nb_samples = frame->nb_samples;
    OMX_U8 *out_buf;

    if (avr)
        ILibavDec->SwrClose(avr);
    else {
        avr = ILibavDec->SwrAlloc();
        if (!avr) {
            LOG_DEBUG("SwrContext allocate failed!\n");
            return OMX_ErrorUndefined;
        }
    }

    data_size = ILibavDec->AvSamplesGetBufferSize(NULL, codecContext->channels, frame->nb_samples, \
                                                 codecContext->sample_fmt, 1);

    ILibavDec->AvOptSetInt(avr, "in_channel_layout",  frame->channel_layout, 0);
    ILibavDec->AvOptSetInt(avr, "in_sample_fmt",      frame->format,         0);
    ILibavDec->AvOptSetInt(avr, "in_sample_rate",     frame->sample_rate,    0);
    ILibavDec->AvOptSetInt(avr, "out_channel_layout", frame->channel_layout, 0);
    ILibavDec->AvOptSetInt(avr, "out_sample_fmt",     out_fmt,     0);
    ILibavDec->AvOptSetInt(avr, "out_sample_rate",    frame->sample_rate,    0);

    if (ILibavDec->SwrInit(avr) < 0) {
        LOG_DEBUG("error initializing swresample!\n");
        return OMX_ErrorUndefined;
    }

    out_size = ILibavDec->AvSamplesGetBufferSize(&out_linesize, codecContext->channels, nb_samples, out_fmt, 0);

    out_samples = ILibavDec->SwrConvert(avr, &(pOutBufferHdr->pBuffer), nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
    if (out_samples < 0) {
        LOG_DEBUG("error convert swresample!\n");
        return OMX_ErrorUndefined;
    }
    data_size = out_samples * osize * codecContext->channels;
    *len = data_size;

    return OMX_ErrorNone;
}

AUDIO_FILTERRETURNTYPE LibavAudioDec::AudioFilterFrame()
{
    AUDIO_FILTERRETURNTYPE ret = AUDIO_FILTER_SUCCESS;
    OMX_U8 *pBuffer = NULL;
    OMX_U32 nConsumeLen = 0, data_size = 0, nchannel = 0, nFrameLen = 0;
    int nOutSize = 0;
    AVPacket pkt;
    int err = 0;

    if (bNeedInput) {
        AudioRingBuffer.BufferGet(&pBuffer, nPushModeInputLen, &nInputSize);
        if (!nInputSize){
            LOG_ERROR("AudioRingBuffer.BufferGet failed.\n");
            pOutBufferHdr->nOffset = 0;
            pOutBufferHdr->nFilledLen = 0;
            return AUDIO_FILTER_EOS;
        }else if (OMX_AUDIO_CodingOPUS == CodingType || OMX_AUDIO_CodingAPE == CodingType){
            //opus & ape decoder only can decode a packet each time, and it needs to know the packet size, aka, the nFrameLen.
            nFrameLen = TS_Manager.GetFrameLen();
            if (0 == nFrameLen) {
                if(errorCnt++ > 1000)
                    return AUDIO_FILTER_FATAL_ERROR; // keep from dead loop
                TS_Manager.Consumered(0);
                return ret;
            }
            else if (nFrameLen != nInputSize) {
                AudioRingBuffer.BufferGet(&pBuffer, nFrameLen, &nInputSize);
            }
        }

        ILibavDec->AvInitPacket(&pkt);
        pkt.data = (uint8_t*)pBuffer;
        pkt.size = nInputSize;

        err = ILibavDec->AvcodecSendPacket(codecContext, &pkt);
        if (err < 0 && err != AVERROR_EOF) {
            if(errorCnt++ > 1000)
                return AUDIO_FILTER_FATAL_ERROR;
            else
                ret = AUDIO_FILTER_FAILURE;
        }
    }

    //  Unlike with older APIs, the packet is always fully consumed, and if it contains multiple frames,
    //  it will require you to call avcodec_receive_frame() multiple times afterwards before you can send a new packet.
    err = ILibavDec->AvcodecReceiveFrame(codecContext, frame);
    if (err >= 0) {
        data_size = ILibavDec->AvSamplesGetBufferSize(NULL, codecContext->channels, frame->nb_samples, codecContext->sample_fmt, 1);
        bNeedInput = OMX_FALSE;
    }
    else if (err == AVERROR(EAGAIN)) {
        nConsumeLen = nInputSize;
        nInputSize = 0;
        bNeedInput = OMX_TRUE;
    }
    else if (err < 0) {
        if(errorCnt++ > 1000)
            return AUDIO_FILTER_FATAL_ERROR;
        else
            return AUDIO_FILTER_FAILURE;
    }

    LOG_DEBUG("ch: %d sampleRate: %d BitPerSample: %d\n", PcmMode.nChannels, \
		PcmMode.nSamplingRate, PcmMode.nBitPerSample);

    /* AV sample convert*/
    AVConvertSample(&data_size);
    LOG_DEBUG("\t data_size: %d \t nConsumeLen: %d \t nFrameLen: %d \t err %d\n", data_size, nConsumeLen, nFrameLen, err);
#if 0
    FILE * pfile;
    pfile = fopen("/data/ape_in","ab");
    if(pfile){
        fwrite(pOutBufferHdr->pBuffer,1,data_size,pfile);
        fclose(pfile);
    }
#endif

    pOutBufferHdr->nFilledLen = data_size;
    AudioRingBuffer.BufferConsumered(nConsumeLen);

    TS_PerFrame = (OMX_S64)pOutBufferHdr->nFilledLen * OMX_TICKS_PER_SECOND * 8 \
		           / PcmMode.nChannels / PcmMode.nBitPerSample / PcmMode.nSamplingRate;

    TS_Manager.Consumered(nConsumeLen);
    pOutBufferHdr->nOffset = 0;
    TS_Manager.TS_SetIncrease(TS_PerFrame);
    errorCnt = 0;

    return ret;
}

OMX_ERRORTYPE LibavAudioDec::AudioFilterReset()
{
    AudioFilterInstanceDeInit();
    AudioFilterCodecInit();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavAudioDec::CheckPortFmt()
{
    /*change input and outport CodeingType*/
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;

    OMX_INIT_STRUCT(&sPortDef, OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nPortIndex = AUDIO_FILTER_INPUT_PORT;
    ports[AUDIO_FILTER_INPUT_PORT]->GetPortDefinition(&sPortDef);
    sPortDef.format.audio.eEncoding = CodingType;
    ports[AUDIO_FILTER_INPUT_PORT]->SetPortDefinition(&sPortDef);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE LibavAudioDec::AudioFilterCheckCodecConfig()
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    switch((OMX_U32)CodingType) {
        case OMX_AUDIO_CodingAPE:
            fsl_osal_memcpy(codecData, pInBufferHdr->pBuffer, pInBufferHdr->nFilledLen);
            codecDataSize = pInBufferHdr->nFilledLen;
            pInBufferHdr->nFilledLen = 0;
            break;
        default:
            break;
    }

    return ret;
}


/**< C style functions to expose entry point for the shared library */
extern "C" {
    OMX_ERRORTYPE LibavAudioDecInit(OMX_IN OMX_HANDLETYPE pHandle)
    {
        OMX_ERRORTYPE ret = OMX_ErrorNone;
        LibavAudioDec *obj = NULL;
        ComponentBase *base = NULL;

        obj = FSL_NEW(LibavAudioDec, ());
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
