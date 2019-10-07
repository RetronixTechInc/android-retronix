/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "FrameConverter.h"

//#undef LOG_DEBUG
//#define LOG_DEBUG printf

OMX_ERRORTYPE FrameConverter::Create(OMX_VIDEO_CODINGTYPE format)
{
    if(format != OMX_VIDEO_CodingAVC)
        return OMX_ErrorUndefined;
    nFormat = format;
    pSpsPps = NULL;
    nLen = 0;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE FrameConverter::ConvertToCodecData(
    OMX_U8* pInData, OMX_U32 nSize, OMX_U8* pOutData,OMX_U32*nOutSize,OMX_U32 * nConsumeLen)
{
    OMX_U8* pPre=pInData;
    OMX_U32 spsSize=0, ppsSize=0;
    OMX_U8 *sps=NULL, *pps=NULL;
    OMX_U8* pNext=NULL;
    OMX_U8 naltype;
    OMX_S32 length=(OMX_S32)nSize;
    OMX_U8* pTemp=NULL,*pFilled=NULL;
    OMX_U32 skipLen=0;

    if(pInData == NULL || pOutData == NULL || nOutSize == NULL)
        return OMX_ErrorBadParameter;

    /*search boundary of sps and pps */
    if(OMX_FALSE==FindStartCode(pInData,length,&pPre)){
        goto search_finish;
    }

    pPre+=4; //skip 4 bytes of startcode
    length-=(pPre-pInData);

    if(length<=0){
        goto search_finish;
    }

    while(1){
        int size;

        if(FindStartCode(pPre,length,&pNext)){
            size=pNext-pPre;
        }
        else{
            size=length; //last nal
        }
        naltype=pPre[0] & 0x1f;
        LOG_DEBUG("find one nal, type: 0x%x, size: %d \r\n",naltype,size);
        if (naltype==7) { /* SPS */
            sps=pPre;
            spsSize=size;
        }
        else if (naltype==8) { /* PPS */
            pps= pPre;
            ppsSize=size;
        }else if(naltype == 0x0c){//skip coda padding bytes
            skipLen += 4+size;
        }
        
        if(pNext==NULL){
            goto search_finish;
        }
        pNext+=4;
        length-=(pNext-pPre);
        if(length<=0){
        	goto search_finish;
        }
        pPre=pNext;
    }
search_finish:
    if((sps==NULL)||(pps==NULL)){
        return OMX_ErrorUndefined;
    }

    pFilled=pOutData;
    pFilled[0]=1;		/* version */
    pFilled[1]=sps[1];	/* profile */
    pFilled[2]=sps[2];	/* profile compat */
    pFilled[3]=sps[3];	/* level */
    pFilled[4]=0xFF;	/* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
    pFilled[5]=0xE1;	/* 3 bits reserved (111) + 5 bits number of sps (00001) */
    pFilled+=6;

    pFilled[0]=(spsSize>>8)&0xFF; /*sps size*/
    pFilled[1]=spsSize&0xFF;
    pFilled+=2;
    fsl_osal_memcpy(pFilled,sps,spsSize); /*sps data*/
    pFilled+=spsSize;

    pFilled[0]=1;		/* number of pps */
    pFilled++;
    pFilled[0]=(ppsSize>>8)&0xFF;	/*pps size*/
    pFilled[1]=ppsSize&0xFF;
    pFilled+=2;
    fsl_osal_memcpy(pFilled,pps,ppsSize); /*pps data*/
    *nOutSize=6+2+spsSize+1+2+ppsSize;
    *nConsumeLen = 4+4+spsSize+ppsSize+skipLen;

    FSL_FREE(pSpsPps);
    if(nSize > (*nConsumeLen))
        nLen = (*nConsumeLen);
    else
        return OMX_ErrorOverflow;
    pSpsPps = (OMX_U8*)fsl_osal_malloc_new(nLen);
    fsl_osal_memcpy(pSpsPps,pInData,nLen); /*sps data*/

    return OMX_ErrorNone;
}
OMX_ERRORTYPE FrameConverter::CheckSpsPps(OMX_U8* pInData, OMX_U32 nSize, OMX_U32* nConsumeLen)
{
    OMX_U8* pPre=pInData;
    OMX_U32 spsSize=0, ppsSize=0;
    OMX_U8 *sps=NULL, *pps=NULL;
    OMX_U8* pNext=NULL;
    OMX_U8 naltype;
    OMX_S32 length=(OMX_S32)nSize;
    OMX_U8* pTemp=NULL,*pFilled=NULL;
    OMX_U32 skipLen=0;

    if(pInData == NULL || nConsumeLen == NULL)
        return OMX_ErrorBadParameter;

    /*search boundary of sps and pps */
    if(OMX_FALSE==FindStartCode(pInData,length,&pPre)){
        goto search_finish;
    }

    pPre+=4; //skip 4 bytes of startcode
    length-=(pPre-pInData);

    if(length<=0){
        goto search_finish;
    }

    while(1){
        int size;

        if(FindStartCode(pPre,length,&pNext)){
            size=pNext-pPre;
        }
        else{
            size=length; //last nal
        }
        naltype=pPre[0] & 0x1f;
        LOG_DEBUG("find one nal, type: 0x%x, size: %d \r\n",naltype,size);
        if (naltype==7) { /* SPS */
            sps=pPre;
            spsSize=size;
        }
        else if (naltype==8) { /* PPS */
            pps= pPre;
            ppsSize=size;
        }else if(naltype == 0x0c){//skip coda padding bytes
            skipLen += 4+size;
        }

        if(pNext==NULL){
            goto search_finish;
        }
        pNext+=4;
        length-=(pNext-pPre);
        if(length<=0){
            goto search_finish;
        }
        pPre=pNext;
    }

search_finish:
    if((sps==NULL)||(pps==NULL)){
        return OMX_ErrorUndefined;
    }

    *nConsumeLen = 4+4+spsSize+ppsSize+skipLen;

    FSL_FREE(pSpsPps);
    if(nSize > (*nConsumeLen))
        nLen =(*nConsumeLen);
    else{
        return OMX_ErrorOverflow;
    }

    pSpsPps = (OMX_U8*)fsl_osal_malloc_new(nLen);
    fsl_osal_memcpy(pSpsPps,pInData,nLen); /*sps data*/
    LOG_DEBUG("size=%d,consume=%d,nLen=%d",nSize,*nConsumeLen,nLen);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE FrameConverter::ConvertToData(OMX_U8* pData, OMX_U32 nSize)
{
    /*we will replace the 'start code'(00000001) with 'nal size'(4bytes), and the buffer length no changed*/
    OMX_U8* pPre=pData;
    OMX_S32 length=(OMX_S32)nSize;
    OMX_U32 nalSize=0;
    OMX_U32 outSize=0;
    OMX_U32 i=0;
    OMX_U8* pNext=NULL;
    OMX_U8 naltype;

    if(!FindStartCode(pData,length,&pPre)){
        goto finish;
    }
    pPre+=4; //skip 4 bytes of startcode
    length-=(pPre-pData);
    while(1){

        if(FindStartCode(pPre,length,&pNext)){
            nalSize=pNext-pPre;
        }
        else{
            nalSize=length; //last nal
        }
        naltype=pPre[0] & 0x1f;
        LOG_DEBUG("find one nal, type: 0x%x, size: %d \r\n",naltype,nalSize);

        pPre[-4]=(nalSize>>24)&0xFF;
        pPre[-3]=(nalSize>>16)&0xFF;
        pPre[-2]=(nalSize>>8)&0xFF;
        pPre[-1]=(nalSize)&0xFF;

        i++;
        outSize+=nalSize+4;
        LOG_DEBUG("ConvertToData find one nal, size: %d \r\n",nalSize);

        if(pNext==NULL){
            goto finish;
        }
        pNext+=4;
        length-=(pNext-pPre);
        pPre=pNext;
    }
finish:
    return OMX_ErrorNone;
}
OMX_ERRORTYPE FrameConverter::GetSpsPpsPtr(OMX_U8** pData,OMX_U32 *outLen)
{
    if(pData == NULL || outLen == NULL)
        return OMX_ErrorBadParameter;

    *pData = pSpsPps;
    *outLen = nLen;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE FrameConverter::Destroy()
{
    FSL_FREE(pSpsPps);
    nLen = 0;
    return OMX_ErrorNone;
}
OMX_BOOL FrameConverter::FindStartCode(OMX_U8* pData, OMX_U32 nSize,OMX_U8** ppStart)
{
    #define AVC_START_CODE 0x00000001
    OMX_U32 startcode=0xFFFFFFFF;
    OMX_U8* p=pData;
    OMX_U8* pEnd=pData+nSize;
    
    if(nSize < 4){
        *ppStart=NULL;
        return OMX_FALSE;
    }
    while(p<pEnd){
        startcode=(startcode<<8)|p[0];
        if(AVC_START_CODE==startcode){
            break;
        }
        p++;
    }
    if(p>=pEnd){
        *ppStart=NULL;
        return OMX_FALSE;
    }
    *ppStart=p-3;
    return OMX_TRUE;
}
