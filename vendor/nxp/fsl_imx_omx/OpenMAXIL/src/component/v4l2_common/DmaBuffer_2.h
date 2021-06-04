/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef DMABUFFER_2_H
#define DMABUFFER_2_H

#define MAX_PLANE_CNT   (3)

typedef struct {
    int fd;
    uint64_t phys;
    uint64_t vaddr;
    uint32_t size;
}DmaBufferPlane;

//dma buffer flags:
#define DMA_BUF_EOS         (0x1)
#define DMA_BUF_INTERLACED  (0x2)
#define DMA_BUF_SYNC        (0x4)

typedef struct {
    OMX_U32 num_of_plane;
    DmaBufferPlane plane[MAX_PLANE_CNT];
    OMX_U32 flag;
    OMX_BOOL bReadyForProcess;
}DmaBufferHdr;

#endif
