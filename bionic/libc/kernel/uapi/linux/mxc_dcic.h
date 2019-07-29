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
#ifndef __ASM_ARCH_MXC_DCIC_H__
#define __ASM_ARCH_MXC_DCIC_H__
#define DCIC_IOC_ALLOC_ROI_NUM _IO('D', 10)
#define DCIC_IOC_FREE_ROI_NUM _IO('D', 11)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define DCIC_IOC_CONFIG_DCIC _IO('D', 12)
#define DCIC_IOC_CONFIG_ROI _IO('D', 13)
#define DCIC_IOC_GET_RESULT _IO('D', 14)
struct roi_params {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned int roi_n;
  unsigned int ref_sig;
  unsigned int start_y;
  unsigned int start_x;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned int end_y;
  unsigned int end_x;
  char freeze;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#endif
