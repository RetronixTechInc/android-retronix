/**
 *  Copyright 2018 NXP
 *  All Rights Reserved.
 *
 *  The following programs are the sole property of Freescale Semiconductor Inc.,
 *  and contain its proprietary and confidential information.
 */

#include "V4l2Device.h"

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>

#if 0
#undef LOG_DEBUG
#define LOG_DEBUG printf
#undef LOG_LOG
#define LOG_LOG printf
#endif
#define VPU_DEC_NODE "/dev/video12"
#define VPU_ENC_NODE "/dev/video13"
#define V4L2_EVENT_SKIP 0x8

OMX_ERRORTYPE V4l2Dev::LookupNode(V4l2DEV_TYPE type, OMX_U8* name)
{
    int index = 2;
    OMX_BOOL bGot = OMX_FALSE;
    OMX_S32 fd = -1;
    struct v4l2_capability cap;

    //open device node directly to save search time.
    if(type == V4L2_DEV_TYPE_DECODER){
         strcpy((char *)name, VPU_DEC_NODE );
         return OMX_ErrorNone;
    }else if(type == V4L2_DEV_TYPE_ENCODER){
         strcpy((char *)name, VPU_ENC_NODE );
         return OMX_ErrorNone;
    }

    while(index < 20){
        sprintf((char*)name, "/dev/video%d", index);

        fd = open ((char*)name, O_RDWR);
        if(fd < 0){
            LOG_DEBUG("open index %d failed\n",index);
            index ++;
            continue;
        }
        if (ioctl (fd, VIDIOC_QUERYCAP, &cap) < 0) {
            close(fd);
            LOG_DEBUG("VIDIOC_QUERYCAP %d failed\n",index);
            index ++;
            continue;
        }
        LOG_DEBUG("index %d name=%s\n",index,(char*)cap.driver);

        if(type == V4L2_DEV_TYPE_DECODER || type == V4L2_DEV_TYPE_ENCODER){
            if(NULL == strstr((char*)cap.driver, "vpu")){
                close(fd);
                index ++;
                continue;
            }
        }
        if(type == V4L2_DEV_TYPE_ISI){
            if(NULL == strstr((char*)cap.driver, "mxc-isi")){
                close(fd);
                index ++;
                continue;
            }
        }

        if (!((cap.capabilities & (V4L2_CAP_VIDEO_M2M |
                        V4L2_CAP_VIDEO_M2M_MPLANE)) ||
                ((cap.capabilities &
                        (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)) &&
                    (cap.capabilities &
                        (V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_VIDEO_OUTPUT_MPLANE))))){
            close(fd);
            index ++;
            continue;
        }
        #if 0
        {
            bGot = OMX_TRUE;
            close(fd);
            break;
        }
        #endif
        LOG_DEBUG("index %d \n",index);
        if((isV4lBufferTypeSupported(fd,type,V4L2_BUF_TYPE_VIDEO_OUTPUT)||
            isV4lBufferTypeSupported(fd,type,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) &&
            (isV4lBufferTypeSupported(fd,type,V4L2_BUF_TYPE_VIDEO_CAPTURE)||
            isV4lBufferTypeSupported(fd,type,V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))){
            bGot = OMX_TRUE;
            close(fd);
            LOG_DEBUG("get device %s \n",name);
            break;
        }
        close(fd);
        index ++;
    }

    
    if(bGot){
        return OMX_ErrorNone;
    }else{
        return OMX_ErrorNoMore;
    }
    
}

OMX_BOOL V4l2Dev::isV4lBufferTypeSupported(OMX_S32 fd,V4l2DEV_TYPE dec_type, OMX_U32 v4l2_buf_type )
{
    OMX_U32 i = 0;
    OMX_BOOL bGot = OMX_FALSE;
    struct v4l2_fmtdesc sFmt;

    while(OMX_TRUE){
        sFmt.index = i;
        sFmt.type = v4l2_buf_type;
        if(ioctl(fd,VIDIOC_ENUM_FMT,&sFmt) < 0)
            break;

        i++;
        if(v4l2_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT || v4l2_buf_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE){
            if(dec_type == V4L2_DEV_TYPE_DECODER && (sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = OMX_TRUE;
                break;
            }else if(dec_type == V4L2_DEV_TYPE_ENCODER && !(sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = OMX_TRUE;
                break;
            }else if(dec_type == V4L2_DEV_TYPE_ISI && !(sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = OMX_TRUE;
                break;
            }
        }else if(v4l2_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE || v4l2_buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE){
            if(dec_type == V4L2_DEV_TYPE_DECODER && !(sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = OMX_TRUE;
                break;
            }else if(dec_type == V4L2_DEV_TYPE_ENCODER && (sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = OMX_TRUE;
                break;
            }else if(dec_type == V4L2_DEV_TYPE_ISI && !(sFmt.flags & V4L2_FMT_FLAG_COMPRESSED)){
                bGot = OMX_TRUE;
                break;
            }
        }
    }

    return bGot;

}

OMX_S32 V4l2Dev::Open(V4l2DEV_TYPE type, OMX_U8* name)
{
    OMX_S32 fd = -1;

    fd = open ((char*)name, O_RDWR);
    if(fd > 0){
        struct v4l2_event_subscription  sub;
        fsl_osal_memset(&sub, 0, sizeof(struct v4l2_event_subscription));

        sub.type = V4L2_EVENT_SOURCE_CHANGE;
        ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

        sub.type = V4L2_EVENT_EOS;
        ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

        if(type == V4L2_DEV_TYPE_DECODER){
            sub.type = V4L2_EVENT_SKIP;
            ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
        }

    }
    return fd;
}
OMX_ERRORTYPE V4l2Dev::Close(OMX_S32 fd)
{
    if(fd >= 0)
        close(fd);

    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dev::SetEncoderParam(OMX_S32 fd,V4l2EncInputParam *param)
{
    OMX_S32 ret = OMX_ErrorNone;

    if(param == NULL)
        return OMX_ErrorBadParameter;

    LOG_DEBUG("SetEncoderParam nBitRate=%d\n",param->nBitRate);
    LOG_DEBUG("SetEncoderParam nGOPSize=%d\n",param->nGOPSize);
    LOG_DEBUG("SetEncoderParam nIntraFreshNum=%d\n",param->nIntraFreshNum);
    if(param->nBitRate > 0){
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_BITRATE_MODE,param->nBitRateMode);
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_BITRATE,param->nBitRate);
    }

    if(param->nGOPSize > 0)
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_GOP_SIZE,param->nGOPSize);
    if(param->nH264_i_qp > 0)
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP,param->nH264_i_qp);
    if(param->nH264_p_qp > 0)
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP,param->nH264_p_qp);
    if(param->nH264_min_qp > 0)
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_H264_MIN_QP,param->nH264_min_qp);
    if(param->nH264_max_qp > 0)
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_H264_MAX_QP,param->nH264_max_qp);
    if(param->nMpeg4_i_qp > 0)
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP,param->nMpeg4_i_qp);
    if(param->nMpeg4_p_qp > 0)
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP,param->nMpeg4_p_qp);
    if(param->nIntraFreshNum > 0)
        ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB,param->nIntraFreshNum);

    return (OMX_ERRORTYPE)ret;
}
OMX_ERRORTYPE V4l2Dev::SetH264EncoderProfileAndLevel(OMX_S32 fd, OMX_U32 profile, OMX_U32 level)
{
    OMX_S32 ret = OMX_ErrorNone;
    OMX_S32 v4l2_profile = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
    OMX_S32 v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_1_0;

    if(fd <0)
        return OMX_ErrorBadParameter;

    switch(profile){
        case OMX_VIDEO_AVCProfileBaseline:
            v4l2_profile = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
            break;
        case OMX_VIDEO_AVCProfileMain:
            v4l2_profile = V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;
            break;
        case OMX_VIDEO_AVCProfileHigh:
            v4l2_profile = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
            break;
        default:
            break;
    }

    switch (level) {
        case OMX_VIDEO_AVCLevel1:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_1_0;
            break;
        case OMX_VIDEO_AVCLevel1b:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_1B;
            break;
        case OMX_VIDEO_AVCLevel11:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_1_1;
            break;
        case OMX_VIDEO_AVCLevel12:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_1_2;
            break;
        case OMX_VIDEO_AVCLevel13:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_1_3;
            break;
        case OMX_VIDEO_AVCLevel2:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_2_0;
            break;
        case OMX_VIDEO_AVCLevel21:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_2_1;
            break;
        case OMX_VIDEO_AVCLevel22:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_2_2;
            break;
        case OMX_VIDEO_AVCLevel3:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_3_0;
            break;
        case OMX_VIDEO_AVCLevel31:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_3_1;
            break;
        case OMX_VIDEO_AVCLevel32:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_3_2;
            break;
        case OMX_VIDEO_AVCLevel4:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
            break;
        case OMX_VIDEO_AVCLevel41:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_4_1;
            break;
        case OMX_VIDEO_AVCLevel42:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_4_2;
            break;
        case OMX_VIDEO_AVCLevel5:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_5_0;
            break;
        case OMX_VIDEO_AVCLevel51:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
            break;
        case OMX_VIDEO_AVCLevel52:
        case OMX_VIDEO_AVCLevelMax:
            v4l2_level = V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
            break;
        default:
            break;
    }


    ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_H264_PROFILE, v4l2_profile);
    ret |= SetCtrl(fd,V4L2_CID_MPEG_VIDEO_H264_LEVEL, v4l2_level);
    LOG_DEBUG("set profile=%d,level=%d,ret=%d",v4l2_profile,v4l2_level,ret);
    return (OMX_ERRORTYPE)ret;
}

OMX_ERRORTYPE V4l2Dev::SetEncoderRotMode(OMX_S32 fd,int nRotAngle)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_S32 value= 1;
    struct v4l2_control ctl = { 0 ,0};
    if(90 == nRotAngle|| 270 == nRotAngle)
        ret = SetCtrl(fd,V4L2_CID_HFLIP,value);
    else if(0 == nRotAngle || 180 == nRotAngle)
        ret = SetCtrl(fd,V4L2_CID_VFLIP,value);
    else
        return OMX_ErrorBadParameter;
    LOG_DEBUG("SetEncoderPrama SetEncoderRotMode=%d\n",nRotAngle);
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dev::SetCtrl(OMX_S32 fd,OMX_U32 id, OMX_S32 value)
{
    int ret = 0;
    struct v4l2_control ctl = { 0,0 };
    ctl.id = id;
    ctl.value = value;
    ret = ioctl(fd, VIDIOC_S_CTRL, &ctl);
    if(ret < 0)
        return OMX_ErrorUndefined;
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dev::StopDecoder(OMX_S32 fd)
{
    int ret = 0;
    struct v4l2_decoder_cmd cmd;
    fsl_osal_memset(&cmd, 0, sizeof(struct v4l2_decoder_cmd));

    cmd.cmd = V4L2_DEC_CMD_STOP;
    cmd.flags = V4L2_DEC_CMD_STOP_IMMEDIATELY;

    ret = ioctl(fd, VIDIOC_DECODER_CMD, &cmd);
    if(ret < 0)
        return OMX_ErrorUndefined;

    LOG_DEBUG("V4l2Dev::StopDecoder SUCCESS\n");
    return OMX_ErrorNone;
}
OMX_ERRORTYPE V4l2Dev::StopEncoder(OMX_S32 fd)
{
    int ret = 0;

    struct v4l2_encoder_cmd cmd;
    fsl_osal_memset(&cmd, 0, sizeof(struct v4l2_encoder_cmd));

    cmd.cmd = V4L2_ENC_CMD_STOP;
    cmd.flags = V4L2_ENC_CMD_STOP_AT_GOP_END;
    ret = ioctl(fd, VIDIOC_ENCODER_CMD, &cmd);

    if(ret < 0){
        LOG_DEBUG("V4l2Dev::StopEncoder FAILED\n");
        return OMX_ErrorUndefined;
    }

    LOG_DEBUG("V4l2Dev::StopEncoder SUCCESS\n");
    return OMX_ErrorNone;
}
OMX_S32 V4l2Dev::Poll(OMX_S32 fd)
{
    OMX_U32 ret = V4L2_DEV_POLL_RET_NONE;
    int r;
    struct pollfd pfd;
    struct timespec ts;
    ts.tv_sec = 1;
    ts.tv_nsec = 0;

    pfd.fd = fd;
    pfd.events = POLLERR | POLLNVAL | POLLHUP;
    pfd.revents = 0;

    pfd.events |= POLLOUT | POLLPRI | POLLWRNORM;
    pfd.events |= POLLIN | POLLRDNORM;

    LOG_LOG("Poll BEGIN\n",this);
    r = ppoll (&pfd, 1, &ts, NULL);

    if(r < 0)
        ret = -1;
    else if(r == 0){
        ret = V4L2_DEV_POLL_RET_NONE;
        LOG_LOG("Poll END V4L2_DEV_POLL_RET_NONE\n");
    }else{

        if(pfd.revents & POLLPRI){
            LOG_LOG("[%p]POLLPRI \n",this);
            ret |= DqEvent(fd);
            return ret;
        }

        if(pfd.revents & POLLERR){
            //char tembuf[1];
            //read (mFd, tembuf, 1);
            ret = V4L2_DEV_POLL_RET_NONE;
            //sleep 10 ms to avoid running too fast
            fsl_osal_sleep(10000);
            return ret;
        }
        LOG_LOG("Poll events=%x,pfd.revents=%x\n",pfd.events,pfd.revents);

        if((pfd.revents & POLLIN) || (pfd.revents & POLLRDNORM)){
            ret |= V4L2_DEV_POLL_RET_CAPTURE;
        }
        if((pfd.revents & POLLOUT) || (pfd.revents & POLLWRNORM)){
            ret |= V4L2_DEV_POLL_RET_OUTPUT;
        }
    }

    LOG_LOG("Poll END,ret=%x\n",ret);
    return ret;
}
OMX_S32 V4l2Dev::DqEvent(OMX_S32 fd)
{
    int result = 0;
    OMX_U32 ret = 0;
    struct v4l2_event event;
    fsl_osal_memset(&event, 0, sizeof(struct v4l2_event));
    LOG_DEBUG("CALL VIDIOC_DQEVENT");
    result = ioctl(fd, VIDIOC_DQEVENT, &event);
    if(result == 0){
        LOG_DEBUG("VIDIOC_DQEVENT type=%d",event.type);
        switch(event.type){
            case V4L2_EVENT_SOURCE_CHANGE:
                if(event.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION)
                    ret |= V4L2_DEV_POLL_RET_EVENT_RC;
                LOG_DEBUG("[%p]ProcessBuffer V4L2OBJECT_RESOLUTION_CHANGED\n",this);
                break;
            case V4L2_EVENT_EOS:
                ret |= V4L2_DEV_POLL_RET_EVENT_EOS;
                LOG_DEBUG("[%p]DqEvent OMX_BUFFERFLAG_EOS\n",this);
                break;
            case V4L2_EVENT_SKIP:
                ret |= V4L2_DEV_POLL_RET_EVENT_SKIP;
                LOG_DEBUG("[%p]DqEvent V4L2_DEV_POLL_RET_EVENT_SKIP\n",this);
                break;
            default:
                break;
        }
    }

    return ret;
}


OMX_ERRORTYPE V4l2Dev::GetFrameAlignment(OMX_S32 fd, OMX_U32 format, OMX_U32 *width, OMX_U32 *height)
{
    if(width == NULL || height == NULL || fd < 0 || 0 == format)
        return OMX_ErrorBadParameter;

    OMX_U32 ret = 0;
    struct v4l2_frmsizeenum frm;
    OMX_U32 min_width = 0;
    OMX_U32 max_width = 0;
    OMX_U32 step_width = 0;

    OMX_U32 min_height = 0;
    OMX_U32 max_height = 0;
    OMX_U32 step_height = 0;

    fsl_osal_memset(&frm, 0, sizeof(struct v4l2_frmsizeenum));
    frm.index = 0;
    frm.type = V4L2_FRMSIZE_TYPE_STEPWISE;
    frm.pixel_format = format;

    ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frm);
    if(0 == ret){
        min_width = frm.stepwise.min_width;
        max_width = frm.stepwise.max_width;
        step_width = frm.stepwise.step_width;
        min_height = frm.stepwise.min_height;
        max_height = frm.stepwise.max_height;
        step_height = frm.stepwise.step_height;

        *width = step_width;
        *height = step_height;
        LOG_DEBUG("GetFrameAlignment step_width=%d,step_height=%d",step_width,step_height);
        return OMX_ErrorNone;
    }

    return OMX_ErrorUndefined;
}
OMX_ERRORTYPE V4l2Dev::SetEncoderFps(OMX_S32 fd, OMX_U32 framerate)
{
    struct v4l2_streamparm parm;
    int ret = 0;

    if (fd < 0 || 0 == framerate)
        return OMX_ErrorBadParameter;

    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = framerate;
    LOG_DEBUG("set frame rate =%d",framerate);
    ret = ioctl(fd, VIDIOC_S_PARM, &parm);
    if (ret) {
        LOG_ERROR("SetEncoderFps fail\n");
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

