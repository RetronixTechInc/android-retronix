/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _UAPI_IMX_DRM_H_
#define _UAPI_IMX_DRM_H_
#include "drm.h"
#ifdef __cplusplus
extern "C" {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
struct drm_imx_dpu_frame_info {
  __u32 width;
  __u32 height;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 x_offset;
  __u32 y_offset;
  __u32 stride;
  __u32 format;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 modifier;
  __u64 baddr;
  __u64 uv_addr;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_IMX_DPU_SET_CMDLIST 0x00
#define DRM_IMX_DPU_WAIT 0x01
#define DRM_IMX_DPU_GET_PARAM 0x02
#define DRM_IOCTL_IMX_DPU_SET_CMDLIST DRM_IOWR(DRM_COMMAND_BASE + DRM_IMX_DPU_SET_CMDLIST, struct drm_imx_dpu_set_cmdlist)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DRM_IOCTL_IMX_DPU_WAIT DRM_IOWR(DRM_COMMAND_BASE + DRM_IMX_DPU_WAIT, struct drm_imx_dpu_wait)
#define DRM_IOCTL_IMX_DPU_GET_PARAM DRM_IOWR(DRM_COMMAND_BASE + DRM_IMX_DPU_GET_PARAM, enum drm_imx_dpu_param)
struct drm_imx_dpu_set_cmdlist {
  __u64 cmd;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 cmd_nr;
  __u64 user_data;
};
struct drm_imx_dpu_wait {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u64 user_data;
};
enum drm_imx_dpu_param {
  DRM_IMX_MAX_DPUS,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#ifdef __cplusplus
}
#endif
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */

