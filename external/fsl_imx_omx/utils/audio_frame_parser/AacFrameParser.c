/**
 *  Copyright (c) 2010-2015, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "AacFrameParser.h"

static const fsl_osal_u32 aac_sampling_frequency[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
#define AAC_SAMPLERATE_TABLE_SIZE (fsl_osal_u32)(sizeof(aac_sampling_frequency)/sizeof(aac_sampling_frequency[0]))
#define AAC_FRAME_SIZE 1024
static efsl_osal_bool IsADTSFrameHeader(fsl_osal_u8 * pHeader,FRAME_INFO * Info)
{
    fsl_osal_s32 id;
    fsl_osal_s32 layer;
    fsl_osal_s32 rotection_abs = 0;
    fsl_osal_s32 profile;
    fsl_osal_s32 sampling_freq_idx;
    fsl_osal_s32 private_bit;
    fsl_osal_s32 channel_config;
    fsl_osal_s32 original_copy;
    fsl_osal_s32 home;
    fsl_osal_s32 copyright_id_bit;
    fsl_osal_s32 copyright_id_start;
    fsl_osal_s32 frame_length;
    fsl_osal_s32 adts_buffer_fullness;
    fsl_osal_s32 num_of_rdb;
    fsl_osal_s32 protection_abs;

    if(pHeader == NULL || Info == NULL)
        return E_FSL_OSAL_FALSE;

    if(pHeader[0] !=0xFF || (pHeader[1] & 0xF0) != 0xF0)
        return E_FSL_OSAL_FALSE;

    id = ((fsl_osal_s32)pHeader[1]&0x08)>>3;
    layer = ((fsl_osal_s32)pHeader[1]&0x06)>>1;
    protection_abs = (fsl_osal_s32)pHeader[1]&0x01;

    if(layer != 0){
        return E_FSL_OSAL_FALSE;
    }

    profile = ((fsl_osal_s32)pHeader[2]&0xC0) >> 6;
    //do not check the profile level, decoder will check it.
   // if(profile != 1){
    //    return E_FSL_OSAL_FALSE;
   // }

    sampling_freq_idx = ((fsl_osal_s32)pHeader[2]&0x3C) >> 2;
    if (sampling_freq_idx >= 0xc){
        return E_FSL_OSAL_FALSE;
    }

    private_bit = ((fsl_osal_s32)pHeader[2]&0x02) >> 1;
    channel_config = (((fsl_osal_s32)pHeader[2]&0x01) << 2) + (((fsl_osal_s32)pHeader[3]&0xC0) >> 6);
    original_copy = ((fsl_osal_s32)pHeader[3]&0x20) >> 5;
    home = ((fsl_osal_s32)pHeader[3]&0x10) >> 4;

    copyright_id_bit = ((fsl_osal_s32)pHeader[3]&0x08) >> 3;
    copyright_id_start = ((fsl_osal_s32)pHeader[3]&0x04) >> 2;
    frame_length = (((fsl_osal_s32)pHeader[3]&0x03) << 11) + (((fsl_osal_s32)pHeader[4]) << 3) + (((fsl_osal_s32)pHeader[5]&0xE0) >> 5);

    adts_buffer_fullness = (((fsl_osal_s32)pHeader[5]&0x1F) << 6) + (((fsl_osal_s32)pHeader[6]&0xFC) >> 2);
    num_of_rdb = ((fsl_osal_s32)pHeader[6]&0x03);

    Info->channels = channel_config;

    //ref to 13818-7AAC (2).pdf, Table 42 ¡ª Channel Configuration
    if(Info->channels == 7)
    {
        Info->channels = 8;
    }
    else if(Info->channels == 0) //if 0, should parsed in raw block, so just set 1
    {
        Info->channels = 1;
    }

    if((fsl_osal_u32)sampling_freq_idx < AAC_SAMPLERATE_TABLE_SIZE)
    {
        Info->sampling_rate = aac_sampling_frequency[sampling_freq_idx];
    }

    Info->sample_per_fr = AAC_FRAME_SIZE;
    Info->b_rate = (frame_length << 3) * Info->sampling_rate / Info->sample_per_fr; // / 1000;

    Info->frm_size = frame_length;

    if(1 == protection_abs)
        Info->header_size = 7;
    else
        Info->header_size = 9;

    return E_FSL_OSAL_TRUE;
}
AFP_RETURN AacCheckFrame(AUDIO_FRAME_INFO *pFrameInfo, fsl_osal_u8 *pBuffer, fsl_osal_u32 nBufferLen)
{
    return CheckFrame(pFrameInfo,pBuffer,nBufferLen,AAC_FRAME_HEAD_SIZE,IsADTSFrameHeader);
}
