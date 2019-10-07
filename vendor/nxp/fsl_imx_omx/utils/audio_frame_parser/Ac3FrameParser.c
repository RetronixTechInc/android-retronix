/**
 *  Copyright (c) 2010-2012, Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 *
 */

#include "Ac3FrameParser.h"
#define NFSCOD      3   /* # defined sample rates */
#define NDATARATE   38  /* # defined data rates */
#define AC3D_FRAME_SIZE 1536
//these tables got from http://rmworkshop.com/dvd_info/related_info/ac3hdr.html
static const fsl_osal_s16 ac3_frame_size[NFSCOD][NDATARATE] =
{   {   64, 64, 80, 80, 96, 96, 112, 112,
    128, 128, 160, 160, 192, 192, 224, 224,
    256, 256, 320, 320, 384, 384, 448, 448,
    512, 512, 640, 640, 768, 768, 896, 896,
    1024, 1024, 1152, 1152, 1280, 1280 },
{   69, 70, 87, 88, 104, 105, 121, 122,
    139, 140, 174, 175, 208, 209, 243, 244,
    278, 279, 348, 349, 417, 418, 487, 488,
    557, 558, 696, 697, 835, 836, 975, 976,
    1114, 1115, 1253, 1254, 1393, 1394 },
{   96, 96, 120, 120, 144, 144, 168, 168,
    192, 192, 240, 240, 288, 288, 336, 336,
    384, 384, 480, 480, 576, 576, 672, 672,
    768, 768, 960, 960, 1152, 1152, 1344, 1344,
    1536, 1536, 1728, 1728, 1920, 1920 }
    };
//these tables got from http://rmworkshop.com/dvd_info/related_info/ac3hdr.html
static const fsl_osal_s32 ac3_sampling_rate[NFSCOD] = {48000,44100,32000};
static const fsl_osal_s32 ac3_channel[] = {2,1,2,3,3,4,4,5};

static efsl_osal_bool IsAC3FrameHeader(fsl_osal_u8 * pHeader,FRAME_INFO * Info)
{
    int fscod = 0;
    int frmsizecod;
    int bsid;
    int bsmode;
    int acmod;
    int lfeonOffset = 0;
    int lfeon = 0;
    int samplerate;
    int framesize;
    efsl_osal_bool bigEndian = E_FSL_OSAL_FALSE;

    if(pHeader == NULL || Info == NULL)
        return E_FSL_OSAL_FALSE;

    if ((pHeader[0] == 0x0b && pHeader[1] == 0x77) || (pHeader[0] == 0x77 && pHeader[1] == 0x0b)){
        ;
    }else{
        return E_FSL_OSAL_FALSE;
    }

    if(pHeader[0] == 0x0b && pHeader[1] == 0x77){
        fscod = pHeader[4] >> 6;
        frmsizecod = pHeader[4] & 0x3f;
        acmod = pHeader[6] >> 5;
        bigEndian = E_FSL_OSAL_TRUE;
    }
    else if(pHeader[0] == 0x77 && pHeader[1] == 0x0b){
        fscod = pHeader[5] >> 6;
        frmsizecod = pHeader[5] & 0x3f;
        acmod = pHeader[7] >> 5;
    }

    if (fscod>=NFSCOD || frmsizecod>=NDATARATE){
        return E_FSL_OSAL_FALSE;
    }

    samplerate = ac3_sampling_rate[fscod];
    framesize = 2 * ac3_frame_size[fscod][frmsizecod];

    if ((acmod & 0x1) && (acmod != 0x1)){
        lfeonOffset += 2;
    }
    if (acmod & 0x4){
        lfeonOffset += 2;
    }
    if (acmod == 0x2){
        lfeonOffset += 2;
    }

    if(lfeonOffset <= 5){
        lfeon = (pHeader[bigEndian?6:7] >> (4 - lfeonOffset)) & 0x01;
    }else{
        lfeon = (pHeader[bigEndian?7:6] >> 6)& 0x01;
    }

    Info->frm_size = framesize;
    Info->sampling_rate = samplerate;
    Info->b_rate = (framesize<<3)*samplerate/AC3D_FRAME_SIZE/1000;
    Info->sample_per_fr = AC3D_FRAME_SIZE;
    Info->channels = ac3_channel[acmod] + lfeon;
    return E_FSL_OSAL_TRUE;
}

AFP_RETURN Ac3CheckFrame(AUDIO_FRAME_INFO *pFrameInfo, fsl_osal_u8 *pBuffer, fsl_osal_u32 nBufferLen)
{
    return CheckFrame(pFrameInfo,pBuffer,nBufferLen,AC3_FRAME_HEAD_SIZE,IsAC3FrameHeader);
}
