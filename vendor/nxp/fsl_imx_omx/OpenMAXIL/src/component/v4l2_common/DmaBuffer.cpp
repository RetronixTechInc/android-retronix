/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */
#include "DmaBuffer.h"
#include "IonAllocator.h"
#include "PlatformResourceMgrItf.h"
#include <sys/mman.h>

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#endif
#define Align(ptr,align)    (((OMX_U32)(ptr)+(align)-1)/(align)*(align))

DmaBuffer::DmaBuffer()
{
    nBufferCnt = 0;
    nBufferSize = 0;
    //pIonAllocator = NULL;
    pFreeQueue = NULL;
    nOutBufferSize = 0;
}

OMX_ERRORTYPE DmaBuffer::Create(OMX_U32 num_plane)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    pFreeQueue = FSL_NEW(Queue, ());
    if(pFreeQueue->Create(MAX_BUFFER_CNT, sizeof(DmaBufferHdr *), E_FSL_OSAL_TRUE)!= QUEUE_SUCCESS){
        ret = OMX_ErrorUndefined;
        return ret;
    }

    fsl_osal_memset(&sDmaBufferMap, 0, sizeof(DmaBufferHdr )*MAX_BUFFER_CNT);
    nPlanesCnt = num_plane;
    LOG_DEBUG("DmaBuffer::Create SUCCESS");

    return ret;
}
OMX_ERRORTYPE DmaBuffer::Allocate(OMX_U32 cnt, OMX_U32 *size,  OMX_COLOR_FORMATTYPE format)
{
    int ret = 0;
    if(pFreeQueue == NULL)
        return OMX_ErrorInsufficientResources;

    if(cnt > MAX_BUFFER_CNT)
        cnt = MAX_BUFFER_CNT;

    if(size == 0)
        return OMX_ErrorBadParameter;

    if(nPlanesCnt != 2 && format != OMX_COLOR_FormatYUV420SemiPlanar){
        return OMX_ErrorNotImplemented;
    }

    fsl::IonAllocator * pIonAllocator = fsl::IonAllocator::getInstance();

    for(OMX_U32 i = 0; i < cnt; i++){

        DmaBufferHdr *pBuf = &sDmaBufferMap[i];
        sDmaBufferMap[i].num_of_plane = nPlanesCnt;

        for(OMX_U32 j = 0; j < nPlanesCnt; j++){
            int fd = 0;
            uint64_t phys_addr = 0;
            uint64_t virt_addr = 0;

            if(size[j] == 0)
                continue;

            fd = pIonAllocator->allocMemory(size[j], ION_MEM_ALIGN, fsl::MFLAGS_CONTIGUOUS);

            if(fd <= 0){
                LOG_ERROR("DmaBuffer allocate failed j=%d,size=%d",j,size[j]);
                return OMX_ErrorUndefined;
            }

            ret = pIonAllocator->getPhys(fd, size[j], phys_addr);
            if(ret != 0){
                LOG_ERROR("DmaBuffer getPhys failed");
                return OMX_ErrorUndefined;
            }

            ret = pIonAllocator->getVaddrs(fd, size[j], virt_addr);
            if(ret != 0){
                LOG_ERROR("DmaBuffer getVaddrs failed");
                return OMX_ErrorUndefined;
            }

            sDmaBufferMap[i].plane[j].fd = fd;
            sDmaBufferMap[i].plane[j].phys = phys_addr;
            sDmaBufferMap[i].plane[j].vaddr = virt_addr;
            sDmaBufferMap[i].plane[j].size = size[j];
            LOG_DEBUG("DmaBuffer allocate fd=%d phys_addr=%llx vaddr=%llx\n",fd,phys_addr, virt_addr);
        }
  
        nBufferCnt ++;
        pFreeQueue->Add( &pBuf);
    }
    nBufferSize = size[0] + size[1] + size[2];
    LOG_DEBUG("DmaBuffer Allocate END");
    return OMX_ErrorNone;
}
OMX_ERRORTYPE DmaBuffer::AllocateForOutput(OMX_U32 size, OMX_PTR *outPtr)
{
    int ret = 0;

    if(size == 0)
        return OMX_ErrorBadParameter;

    int fd = 0;
    uint64_t phys_addr = 0;
    uint64_t virt_addr = 0;
    OMX_PTR dummy=(OMX_U8*)0x01;

    fsl::IonAllocator * pIonAllocator = fsl::IonAllocator::getInstance();

    fd = pIonAllocator->allocMemory(size, ION_MEM_ALIGN, fsl::MFLAGS_CONTIGUOUS);

    if(fd <= 0){
        LOG_ERROR("DmaBuffer allocate failed 1,size=%d",size);
        return OMX_ErrorInsufficientResources;
    }

    ret = pIonAllocator->getPhys(fd, size, phys_addr);

    ret = pIonAllocator->getVaddrs(fd, size, virt_addr);

    if(phys_addr != 0 && virt_addr != 0){
        AddHwBuffer((OMX_PTR)phys_addr, (OMX_PTR)virt_addr);
        ModifyFdAndAddr((OMX_PTR)virt_addr, fd, dummy);
    }
    LOG_DEBUG("AllocateForOutput ret=%d, fd=%d addr=%x, phys=%x",ret,fd, (int)virt_addr, (int)phys_addr);
    *outPtr = (OMX_PTR)(virt_addr & 0xFFFFFFFF);
    nOutBufferSize = size;

    return OMX_ErrorNone;

}

OMX_ERRORTYPE DmaBuffer::Get(DmaBufferHdr **bufHdlr)
{
    if(bufHdlr == NULL)
        return OMX_ErrorBadParameter;

    if(pFreeQueue->Size() > 0){
        pFreeQueue->Get(bufHdlr);
        return OMX_ErrorNone;
    }else{
        *bufHdlr = NULL;
        return OMX_ErrorNoMore;
    }
}
OMX_ERRORTYPE DmaBuffer::Add(DmaBufferHdr *bufHdlr)
{
    if(bufHdlr == NULL)
        return OMX_ErrorBadParameter;

    pFreeQueue->Add(&bufHdlr);
    LOG_DEBUG("DmaBuffer::Add size=%d",pFreeQueue->Size());

    return OMX_ErrorNone;
}
OMX_ERRORTYPE DmaBuffer::FreeAll()
{
    LOG_DEBUG("DmaBuffer FreeAll BEGIN");
    DmaBufferHdr *bufHdlr = NULL;

    for(OMX_U32 i = 0; i < MAX_BUFFER_CNT; i++){
        for(OMX_U32 j = 0; j < sDmaBufferMap[i].num_of_plane; j++){
            if(sDmaBufferMap[i].plane[j].fd > 0){
                munmap((void*)sDmaBufferMap[i].plane[j].vaddr, sDmaBufferMap[i].plane[j].size);
                LOG_DEBUG("DmaBuffer Free fd=%d",sDmaBufferMap[i].plane[j].fd);
                close(sDmaBufferMap[i].plane[j].fd);
                sDmaBufferMap[i].plane[j].fd = 0;
            }
        }
    }

    while(pFreeQueue->Size() > 0){
        pFreeQueue->Get(&bufHdlr);
    }

    nBufferCnt = 0;

    return OMX_ErrorNone;
}
OMX_ERRORTYPE DmaBuffer::Free(DmaBufferHdr *buf)
{
    OMX_BOOL bFind = OMX_FALSE;
    if(buf == NULL)
        return OMX_ErrorNone;

    for(OMX_U32 i = 0; i < MAX_BUFFER_CNT; i++){
        if(buf->plane[0].fd > 0 && sDmaBufferMap[i].plane[0].fd == buf->plane[0].fd){
            munmap((void*)sDmaBufferMap[i].plane[0].vaddr, sDmaBufferMap[i].plane[0].size);
            close(sDmaBufferMap[i].plane[0].fd);
            sDmaBufferMap[i].plane[0].fd = 0;
            if(sDmaBufferMap[i].plane[1].fd > 0){
                munmap((void*)sDmaBufferMap[i].plane[1].vaddr, sDmaBufferMap[i].plane[1].size);
                close(sDmaBufferMap[i].plane[1].fd);
                sDmaBufferMap[i].plane[1].fd = 0;
            }
            bFind = OMX_TRUE;
            break;
        }
    }

    if(!bFind)
        LOG_ERROR("DmaBuffer::Free do not find the index");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE DmaBuffer::Flush()
{
    return OMX_ErrorNone;
}

DmaBuffer::~DmaBuffer()
{

    if(pFreeQueue != NULL){
        pFreeQueue->Free();
        FSL_DELETE(pFreeQueue);
    }

    LOG_DEBUG("DmaBuffer::~DmaBuffer()");
#if 0
    for(OMX_U32 i = 0; i < MAX_BUFFER_CNT; i++){
        if(sDmaBufferMap[i].plane[0].fd > 0){
            LOG_ERROR("sDmaBufferMap index=%d,fd=%d did not free",i,sDmaBufferMap[i].plane[0].fd);
        }
    }
#endif
}
OMX_ERRORTYPE DmaBuffer::Free(OMX_PTR buf)
{
    OMX_S32 fd = -1;
    OMX_PTR addr=NULL;
    OMX_PTR phys_addr = NULL;

    if(buf == NULL)
        return OMX_ErrorBadParameter;

    if(OMX_ErrorNone == GetFdAndAddr(buf,&fd,(OMX_PTR*)&addr) && fd > 0){
        if(nOutBufferSize > 0)
            munmap((void*)buf, nOutBufferSize);
        LOG_DEBUG("DmaBuffer::Free close fd=%d",fd);
        close(fd);
    }

    RemoveHwBuffer(buf);
    return OMX_ErrorNone;
}


