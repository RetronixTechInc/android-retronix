/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H
#include "ComponentBase.h"
#include "Mem.h"

#define FRAME_PARSER_PADDING_SIZE (32)
class FrameParser{
public:
    OMX_ERRORTYPE Create(OMX_VIDEO_CODINGTYPE type);
    OMX_ERRORTYPE Reset(OMX_VIDEO_CODINGTYPE type);
    OMX_ERRORTYPE Parse(OMX_U8*buf,OMX_U32 *size);
private:
    OMX_U32 format;
    OMX_ERRORTYPE ParseH264(OMX_U8*buf,OMX_U32* size);
    OMX_ERRORTYPE ParseAvccHeader(OMX_U8*buf,OMX_U32* size);

    OMX_ERRORTYPE ParseHEVC(OMX_U8*buf,OMX_U32* size);
    OMX_ERRORTYPE ParseHevcHeader(OMX_U8*buf,OMX_U32* size);
    OMX_ERRORTYPE ParseNalFrame(OMX_U8*buf,OMX_U32* size);
    OMX_U32 parseNALSize(OMX_U8 *data);

    OMX_BOOL bAvcc;
    OMX_BOOL bHevc;

    OMX_BOOL bHeaderParsed;
    OMX_U32 nNalLength;
    
};
#endif

