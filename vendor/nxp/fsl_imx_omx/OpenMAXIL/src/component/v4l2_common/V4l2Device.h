/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#ifndef ZPUDEV_H
#define ZPUDEV_H

#include "ComponentBase.h"

typedef enum{
V4L2_DEV_TYPE_DECODER = 0,
V4L2_DEV_TYPE_ENCODER,
V4L2_DEV_TYPE_ISI,
}V4l2DEV_TYPE;

#define V4L2_DEV_POLL_RET_NONE 0
#define V4L2_DEV_POLL_RET_EVENT_RC 1
#define V4L2_DEV_POLL_RET_EVENT_EOS 2
#define V4L2_DEV_POLL_RET_OUTPUT 4
#define V4L2_DEV_POLL_RET_CAPTURE 8
#define V4L2_DEV_POLL_RET_EVENT_SKIP 0x10


typedef struct {
    OMX_S32 nBitRate;/*unit: bps*/
    OMX_S32 nBitRateMode;
    OMX_S32 nGOPSize;
    OMX_S32 nH264_i_qp;
    OMX_S32 nH264_p_qp;
    OMX_S32 nH264_min_qp;
    OMX_S32 nH264_max_qp;
    OMX_S32 nMpeg4_i_qp;
    OMX_S32 nMpeg4_p_qp;
    OMX_S32 nIntraFreshNum;//V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB
    OMX_S32 nProfile;
    OMX_S32 nLevel;
} V4l2EncInputParam;

class V4l2Dev{
public:
    OMX_S32 Open(V4l2DEV_TYPE type, OMX_U8* name);
    OMX_ERRORTYPE Close(OMX_S32 fd);
    OMX_ERRORTYPE LookupNode(V4l2DEV_TYPE type, OMX_U8* name);
    OMX_BOOL isV4lBufferTypeSupported(OMX_S32 fd,V4l2DEV_TYPE dec_type, OMX_U32 v4l2_buf_type);
    OMX_ERRORTYPE SetEncoderParam(OMX_S32 fd,V4l2EncInputParam *param);
    OMX_ERRORTYPE SetH264EncoderProfileAndLevel(OMX_S32 fd, OMX_U32 profile, OMX_U32 level);
    OMX_ERRORTYPE SetEncoderRotMode(OMX_S32 fd,int nRotAngle);
    OMX_ERRORTYPE StopDecoder(OMX_S32 fd);
    OMX_ERRORTYPE StopEncoder(OMX_S32 fd);
    OMX_S32 Poll(OMX_S32 fd);
    OMX_S32 DqEvent(OMX_S32 fd);
    OMX_ERRORTYPE GetFrameAlignment(OMX_S32 fd, OMX_U32 format, OMX_U32 *width, OMX_U32 *height);
    OMX_ERRORTYPE SetEncoderFps(OMX_S32 fd, OMX_U32 framerate);

private:
    OMX_ERRORTYPE SetCtrl(OMX_S32 fd,OMX_U32 id, OMX_S32 value);

    OMX_U32 nCaps;
};
#endif

