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
#ifndef _MXC_MLB_UAPI_H
#define _MXC_MLB_UAPI_H
#define MLB_DBG_RUNTIME _IO('S', 0x09)
#define MLB_SET_FPS _IOW('S', 0x10, unsigned int)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MLB_GET_VER _IOR('S', 0x11, unsigned long)
#define MLB_SET_DEVADDR _IOR('S', 0x12, unsigned char)
#define MLB_CHAN_SETADDR _IOW('S', 0x13, unsigned int)
#define MLB_CHAN_STARTUP _IO('S', 0x14)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MLB_CHAN_SHUTDOWN _IO('S', 0x15)
#define MLB_CHAN_GETEVENT _IOR('S', 0x16, unsigned long)
#define MLB_SET_ISOC_BLKSIZE_188 _IO('S', 0x17)
#define MLB_SET_ISOC_BLKSIZE_196 _IO('S', 0x18)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define MLB_SET_SYNC_QUAD _IOW('S', 0x19, unsigned int)
#define MLB_IRQ_ENABLE _IO('S', 0x20)
#define MLB_IRQ_DISABLE _IO('S', 0x21)
enum {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  MLB_EVT_TX_PROTO_ERR_CUR = 1 << 0,
  MLB_EVT_TX_BRK_DETECT_CUR = 1 << 1,
  MLB_EVT_TX_PROTO_ERR_PREV = 1 << 8,
  MLB_EVT_TX_BRK_DETECT_PREV = 1 << 9,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  MLB_EVT_RX_PROTO_ERR_CUR = 1 << 16,
  MLB_EVT_RX_BRK_DETECT_CUR = 1 << 17,
  MLB_EVT_RX_PROTO_ERR_PREV = 1 << 24,
  MLB_EVT_RX_BRK_DETECT_PREV = 1 << 25,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#endif
