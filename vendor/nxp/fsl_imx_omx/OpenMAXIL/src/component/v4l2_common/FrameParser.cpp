/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "FrameParser.h"
//#undef LOG_DEBUG
//#define LOG_DEBUG printf

OMX_ERRORTYPE FrameParser::Create(OMX_VIDEO_CODINGTYPE type)
{
    format = type;
    bHeaderParsed = OMX_FALSE;
    bAvcc = OMX_FALSE;
    bHevc = OMX_FALSE;
    nNalLength=0;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE FrameParser::Reset(OMX_VIDEO_CODINGTYPE type)
{
    format = type;

    bAvcc = OMX_FALSE;
    bHevc = OMX_FALSE;
    bHeaderParsed = OMX_FALSE;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE FrameParser::Parse(OMX_U8*buf,OMX_U32* size)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    switch(format){
    case OMX_VIDEO_CodingAVC:
        LOG_DEBUG("Parse H264 BEGIN\n");
        ret = ParseH264(buf,size);
        LOG_DEBUG("Parse H264 END\n");
        break;
    case OMX_VIDEO_CodingHEVC:
        LOG_DEBUG("Parse HEVC BEGIN\n");
        ret = ParseHEVC(buf,size);
        LOG_DEBUG("Parse HEVC END\n");
        break;
    default:
        ret = OMX_ErrorNone;
        break;
    }
    return ret;
}
OMX_ERRORTYPE FrameParser::ParseH264(OMX_U8*buf,OMX_U32* size)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(!bAvcc){
        if(buf[0]==1){
            nNalLength=(buf[4]&0x3)+1;
            bAvcc = OMX_TRUE;
            LOG_DEBUG("Parse H264 bAvcc found len=%d\n",nNalLength);
        }
    }

    if(!bAvcc)
        return ret;

    //do not modify the buffer we have changed before
    if(buf[0] == 0x0 && buf[1] == 0x0 && buf[2] == 0x0 && buf[3] == 0x1){
        //byte-stream format
        return OMX_ErrorNone;
    }

    if(!bHeaderParsed){
        LOG_DEBUG("Parse H264 ParseAvccHeader BEGIN\n");
        ret = ParseAvccHeader(buf,size);
        if(ret == OMX_ErrorNone)
            bHeaderParsed = OMX_TRUE;

        return ret;
    }
    ret = ParseNalFrame(buf,size);

    return ret;
}
OMX_ERRORTYPE FrameParser::ParseHEVC(OMX_U8*buf,OMX_U32* size)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    if(!bHevc){
        if(buf[0]==1){
            nNalLength=(buf[21]&0x3)+1;
            bHevc = OMX_TRUE;
        }
    }

    if(!bHevc)
        return ret;
        
    //do not modify the buffer we have changed before
    if(buf[0] == 0x0 && buf[1] == 0x0 && buf[2] == 0x0 && buf[3] == 0x1){
        //byte-stream format
        return OMX_ErrorNone;
    }

    if(!bHeaderParsed){
        ret = ParseHevcHeader(buf,size);
        if(ret == OMX_ErrorNone)
            bHeaderParsed = OMX_TRUE;

        return ret;
    }
    ret = ParseNalFrame(buf,size);

    return ret;

}

OMX_ERRORTYPE FrameParser::ParseAvccHeader(OMX_U8*buf,OMX_U32 *size)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_U8* pSPS;
    OMX_U8* pPPS;
    OMX_S32 spsSize, ppsSize;
    OMX_U32 numOfSPS,numOfPPS;
    OMX_U8* p=buf;
    OMX_U8 * pTemp = NULL;
    OMX_S32 bufferLen;
    OMX_S32 targetLen;
    OMX_U32 i = 0;
    OMX_U8 * pDec;
    OMX_S32 offset = 0;

    if(buf == NULL || size == NULL || *size < 8)
        return OMX_ErrorBadParameter;
    bufferLen = *size;
    LOG_DEBUG("Parse H264 ParseAvccHeader 0 size=%d\n",bufferLen);

    /* according to IEC 14496-15 4.1.5.1.1*/
    /* [0]: version */
    /* [1]: profile */
    /* [2]: profile compat */
    /* [3]: level */
    /* [4]: 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
    /* [5]: 3 bits reserved (111) + 5 bits number of sps (00001) */
    /*16bits: sps_size*/                          
    /*sps data*/
    /*number of pps*/
    /*16bits: pps_size*/
    /*pps data */

    numOfSPS = p[5] & 0x1F;

    p += 6;
    bufferLen -= 6;

    //got target buffer size, do not copy buffer now.

    for( i = 0; i < numOfSPS; i++){
        if(bufferLen < 2)
            goto bail;

        spsSize = p[0] << 8 | p[1];
        p += 2;

        if(spsSize > bufferLen)
            goto bail;

        p += (OMX_U32)spsSize;
        bufferLen -= spsSize;
        offset += 4+spsSize;
    }

    if(bufferLen < 1)
        goto bail;

    numOfPPS = *p;
    p++;
    bufferLen --;

    for(i = 0; i < numOfPPS; i++){
        if(bufferLen < 2)
            goto bail;

        ppsSize=(p[0]<<8)|p[1];
        p += 2;
        bufferLen -= 2;

        if(ppsSize > bufferLen)
            goto bail;

        p += (OMX_U32)ppsSize;
        bufferLen -= ppsSize;
        offset += 4+ppsSize;
    }

    targetLen = offset;

    pTemp = (OMX_U8*)FSL_MALLOC(targetLen);
    if(pTemp == NULL)
        goto bail;
    LOG_DEBUG("Parse H264 ParseAvccHeader 1 targetLen=%d\n",targetLen);

    p = buf+6;
    bufferLen = *size - 6;
    offset = 0;

    for( i = 0; i < numOfSPS; i++){
        if(bufferLen < 2)
            goto bail;

        spsSize = p[0] << 8 | p[1];
        p += 2;

        if(spsSize > bufferLen)
            goto bail;

        pTemp[offset]=0x0;
        pTemp[offset+1]=0x0;
        pTemp[offset+2]=0x0;
        pTemp[offset+3]=0x01;
        offset += 4;

        fsl_osal_memcpy(&pTemp[offset], p, spsSize);

        p += spsSize;
        bufferLen -= spsSize;
        offset += spsSize;
    }

    if(bufferLen < 1)
        goto bail;

    numOfPPS = *p;
    p++;
    bufferLen --;

    LOG_DEBUG("Parse H264 ParseAvccHeader 1 sps end\n");

    for(i = 0; i < numOfPPS; i++){
        if(bufferLen < 2)
            goto bail;

        ppsSize=(p[0]<<8)|p[1];
        p += 2;
        bufferLen -= 2;

        if(ppsSize > bufferLen)
            goto bail;

        pTemp[offset]=0x0;
        pTemp[offset+1]=0x0;
        pTemp[offset+2]=0x0;
        pTemp[offset+3]=0x01;
        offset += 4;

        fsl_osal_memcpy(&pTemp[offset], p, ppsSize);

        p += ppsSize;
        bufferLen -= ppsSize;
        offset += ppsSize;
    }

    fsl_osal_memcpy(buf,pTemp,targetLen);
    *size = targetLen;
    FSL_FREE(pTemp);

    LOG_DEBUG("Parse H264 ParseAvccHeader targetLen=%d\n",targetLen);
    return OMX_ErrorNone;
bail:
    FSL_FREE(pTemp);
    LOG_DEBUG("Parse H264 ParseAvccHeader error\n");
    return OMX_ErrorBadParameter;
}
OMX_ERRORTYPE FrameParser::ParseNalFrame(OMX_U8*buf,OMX_U32* size)
{
    OMX_S32 nLeftSize = *size;
    OMX_U32 nAddedSize = 0;
    OMX_U8 * p = buf;
    OMX_U32 nOffset = 0;
    OMX_U32 nDataSize = 0;

    while(nLeftSize > 0){
        if(nLeftSize < (OMX_S32)nNalLength)
            break;

        nDataSize = parseNALSize(p);

        if((OMX_U32)nLeftSize < nNalLength + nDataSize)
            break;

        if(nAddedSize + 3 > FRAME_PARSER_PADDING_SIZE)
            break;

        fsl_osal_memmove(p+4,p+nNalLength,nLeftSize);
        p[0] = p[1] = p[2] = 0x0;
        p[3] = 0x1;
        p += 4+nDataSize;
        nLeftSize -= nNalLength+nDataSize;
        nAddedSize += 4-nNalLength;
    }

    if(nLeftSize == 0){
        *size += nAddedSize;
        return OMX_ErrorNone;
    }else
        return OMX_ErrorBadParameter;

}
OMX_U32 FrameParser::parseNALSize(OMX_U8 *data) {
    switch (nNalLength) {
        case 1:
            return *data;
        case 2:
            return ((data[0] << 8) | data[1]);
        case 3:
            return ((data[0] << 16) | (data[1] << 8) | data[2]);
        case 4:
            return ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
    }

    return 0;
}
OMX_ERRORTYPE FrameParser::ParseHevcHeader(OMX_U8*buf,OMX_U32* size)
{
    OMX_U32 i, j, k;
    OMX_U32 info_size=0;
    OMX_U8 *data;
    OMX_U8 * ptr;
    OMX_S32 leftSize;
    OMX_U8 lengthSizeMinusOne;
    OMX_U32 originLen = 0;
    OMX_U32 newLen = 0;
    OMX_PTR newCodecData = NULL;

    OMX_U32 blockNum;

    OMX_U32 nalNum;
    OMX_U16 NALLength;

    if(buf == NULL || size == 0)
        return OMX_ErrorBadParameter;

    originLen = *size;

    if(originLen < 23)
        return OMX_ErrorBadParameter;

    ptr = buf;

    blockNum = ptr[22];

    data = ptr+23;
    leftSize = originLen - 23;

    //get target codec data len
    for (i = 0; i < blockNum; i++) {
        data += 1;
        leftSize -= 1;

        // Num of nals
        nalNum = (data[0] << 8) + data[1];
        data += 2;
        leftSize -= 2;

        for (j = 0;j < nalNum;j++) {
            if (leftSize < 2) {
                break;
            }

            NALLength = (data[0] << 8) + data[1];
            LOG_DEBUG("ParseCodecInfo NALLength=%d",NALLength);
            data += 2;
            leftSize -= 2;

            if (leftSize < NALLength) {
                break;
            }

            //start code needs 4 bytes
            newLen += 4+NALLength;

            data += NALLength;
            leftSize -= NALLength;

        }
    }

    newCodecData = (OMX_U8*)FSL_MALLOC(newLen);
    if(newCodecData ==NULL)
        return OMX_ErrorUndefined;

    data=(OMX_U8 *)newCodecData;
    fsl_osal_memset(data,0,newLen);

    ptr = (OMX_U8 *)buf+23;
    leftSize = originLen - 23;
    for (i = 0; i < blockNum; i++) {
        ptr += 1;
        leftSize -= 1;

        // Num of nals
        nalNum = (ptr[0] << 8) + ptr[1];
        ptr += 2;
        leftSize -= 2;

        for (j = 0;j < nalNum;j++) {
            if (leftSize < 2) {
                break;
            }

            NALLength = (ptr[0] << 8) + ptr[1];
            ptr += 2;
            leftSize -= 2;

            if (leftSize < NALLength) {
                break;
            }

            //add start code
            *data=0x0;
            *(data+1)=0x0;
            *(data+2)=0x0;
            *(data+3)=0x01;

            //as start code needs 4 bytes, move 2 bytes for start code, the other 2 bytes uses the nal length
            fsl_osal_memcpy(data+4, ptr, NALLength);

            data += 4+ NALLength;

            ptr += NALLength;
            leftSize -= NALLength;

        }
    }
    
    fsl_osal_memcpy(buf, newCodecData, newLen);
    FSL_FREE(newCodecData);
    * size = newLen;

    return OMX_ErrorNone;

}
