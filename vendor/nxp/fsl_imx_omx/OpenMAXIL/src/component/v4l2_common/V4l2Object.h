/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef V4L2OBJECT_H
#define V4L2OBJECT_H


#include <linux/videodev2.h>

#include "ComponentBase.h"
#include "Mem.h"
#include <sys/ioctl.h>
#include <stdio.h>
#include "DmaBuffer_2.h"

#define V4L2OBJECT_SUCCESS 0x1
#define V4L2OBJECT_EOS 0x02
#define V4L2OBJECT_RESOLUTION_CHANGED 0x04
#define V4L2OBJECT_TIMEOUT 0x8

#define V4L2OBJECT_ERROR 0x100

#define V4L2_OBJECT_MAX_BUFFER_COUNT 32
#define V4L2_OBJECT_MAX_PLANES  3

#define V4L2_OBJECT_FLAG_KEYFRAME 1
#define V4L2_OBJECT_FLAG_METADATA_BUFFER 2
#define V4L2_OBJECT_FLAG_NATIVE_BUFFER 4
#define V4L2_OBJECT_FLAG_NO_AUTO_START 0x8


typedef struct {
    struct v4l2_buffer v4l2Buffer;
    struct v4l2_plane v4l2Plane[V4L2_OBJECT_MAX_PLANES];
    OMX_PTR mmap_addr[V4L2_OBJECT_MAX_PLANES];
    OMX_BUFFERHEADERTYPE *omxBuffer;
    DmaBufferHdr *dmaBuffer;
    OMX_U8 *vaddr;
    OMX_S32 fd[V4L2_OBJECT_MAX_PLANES];
} V4l2ObjectMap;

typedef struct {
    OMX_U32 width;
    OMX_U32 height;
    OMX_U32 format;
    OMX_U32 stride;
    OMX_U32 plane_num;//if plane_num is 1 when call set format from v4l2 filter, then v4l2 object will adjust the size for multi planes
    OMX_U32 bufferSize[V4L2_OBJECT_MAX_PLANES];
}V4l2ObjectFormat;

class V4l2Object {
public:
    OMX_ERRORTYPE Create(OMX_S32 dev,enum v4l2_buf_type type,OMX_U32 plane_num);
    OMX_ERRORTYPE ReleaseResource();

    OMX_ERRORTYPE SetFormat(V4l2ObjectFormat* pFormat);
    OMX_ERRORTYPE GetFormat(V4l2ObjectFormat* pFormat);
    OMX_ERRORTYPE SetCrop(OMX_CONFIG_RECTTYPE* sCrop);
    OMX_ERRORTYPE GetCrop(OMX_CONFIG_RECTTYPE* sCrop);
    OMX_ERRORTYPE GetMinimumBufferCount(OMX_U32 *num);//get minimum required buffer count for capture object
    OMX_ERRORTYPE SetBufferCount(OMX_U32 count, enum v4l2_memory type,OMX_U32 planes_num);
    OMX_BOOL HasEmptyBuffer();
    OMX_BOOL HasOutputBuffer();
    OMX_BOOL HasBuffer();

    OMX_ERRORTYPE SetBuffer(OMX_BUFFERHEADERTYPE * inputBuffer,OMX_U32 flags);
    OMX_U32 ProcessBuffer(OMX_U32 flags);
    OMX_ERRORTYPE GetOutputBuffer(OMX_BUFFERHEADERTYPE ** bufHdlr);

    OMX_PTR AllocateBuffer(OMX_U32 nSize);//map buffer
    OMX_ERRORTYPE FreeBuffer(OMX_PTR buffer);//call unmap
    OMX_ERRORTYPE Flush();

    OMX_ERRORTYPE GetBuffer(OMX_BUFFERHEADERTYPE ** bufHdlr);
    OMX_BOOL isMMap();
    OMX_ERRORTYPE Start();
    OMX_ERRORTYPE Stop();

    OMX_ERRORTYPE SetBuffer(DmaBufferHdr * inputBuffer,OMX_U32 flags);
    OMX_ERRORTYPE GetOutputBuffer(DmaBufferHdr ** bufHdlr);
    OMX_ERRORTYPE GetBuffer(DmaBufferHdr ** bufHdlr);

private:

    OMX_S32 mFd;
    enum v4l2_buf_type eBufferType;

    OMX_U32 eIOType;
    OMX_U32 nPlanesNum;
    
    OMX_U32 nBufferCount;
    OMX_U32 nBufferNum; //for allocate buffer count

    OMX_BOOL bBufferRequired;
    OMX_BOOL bMultiPlane;
    OMX_BOOL bRunning;

    OMX_U32 nBufferSize[3];
    Queue* pQueue;
    Queue* pOutQueue;
    List<struct v4l2_fmtdesc> formatList;


    V4l2ObjectMap * pBufferMap;
    OMX_U8* pIndex[V4L2_OBJECT_MAX_BUFFER_COUNT];

    fsl_osal_mutex sMutex;

    OMX_PTR pData;

    OMX_U32 nCnt;
    OMX_BOOL bChange;
    OMX_BOOL bMetadataBuffer;
    OMX_BOOL bProcessing;



    OMX_BOOL IsFormatSupported(OMX_U32 format);
    OMX_ERRORTYPE QueryFormat(OMX_U32 * num);
    
    OMX_ERRORTYPE BufferMap_Create(OMX_U32 count);

    OMX_ERRORTYPE BufferMap_Update(OMX_BUFFERHEADERTYPE *inputBuffer,OMX_U8* pBuf,OMX_U32 index);
    OMX_ERRORTYPE BufferMap_UpdateUserPtr(OMX_BUFFERHEADERTYPE *inputBuffer,OMX_U8* pBuf,OMX_U32 index);
    OMX_ERRORTYPE BufferMap_UpdateDmaBuf(OMX_BUFFERHEADERTYPE *inputBuffer,OMX_U8* pBuf,OMX_U32 index);

    OMX_ERRORTYPE BufferMap_UpdateDmaBuf(DmaBufferHdr *inputBuffer, OMX_U8* pBuf, OMX_U32 index);

    OMX_ERRORTYPE BufferIndex_FindIndex(OMX_U8* pBuf,OMX_U32 *index);
    OMX_ERRORTYPE BufferIndex_GetEmptyIndex(OMX_U32 *index);
    OMX_ERRORTYPE BufferIndex_AddIndex(OMX_U8* pBuf,OMX_U32 index);
    OMX_ERRORTYPE BufferIndex_RemoveIndex(OMX_U32 index);


    OMX_ERRORTYPE Select();
};
#endif

