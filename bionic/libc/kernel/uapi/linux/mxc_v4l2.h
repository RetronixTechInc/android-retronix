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
#ifndef __ASM_ARCH_MXC_V4L2_H__
#define __ASM_ARCH_MXC_V4L2_H__
#define V4L2_CID_MXC_ROT (V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_MXC_FLASH (V4L2_CID_PRIVATE_BASE + 1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define V4L2_CID_MXC_VF_ROT (V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_MXC_MOTION (V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_MXC_SWITCH_CAM (V4L2_CID_PRIVATE_BASE + 6)
#define V4L2_MXC_ROTATE_NONE 0
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define V4L2_MXC_ROTATE_VERT_FLIP 1
#define V4L2_MXC_ROTATE_HORIZ_FLIP 2
#define V4L2_MXC_ROTATE_180 3
#define V4L2_MXC_ROTATE_90_RIGHT 4
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define V4L2_MXC_ROTATE_90_RIGHT_VFLIP 5
#define V4L2_MXC_ROTATE_90_RIGHT_HFLIP 6
#define V4L2_MXC_ROTATE_90_LEFT 7
struct v4l2_mxc_offset {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  uint32_t u_offset;
  uint32_t v_offset;
};
struct v4l2_mxc_dest_crop {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  __u32 type;
  struct v4l2_mxc_offset offset;
};
#define VIDIOC_S_INPUT_CROP _IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct v4l2_crop)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define VIDIOC_G_INPUT_CROP _IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct v4l2_crop)
#define VIDIOC_S_DEST_CROP _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct v4l2_mxc_dest_crop)
#endif
