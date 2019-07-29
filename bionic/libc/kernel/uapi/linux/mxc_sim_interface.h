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
#ifndef UAPI_MXC_SIM_INTERFACE_H
#define UAPI_MXC_SIM_INTERFACE_H
#define SIM_ATR_LENGTH_MAX 32
typedef struct {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned int size;
  unsigned char * atr_buffer;
  int errval;
} sim_atr_t;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_PROTOCOL_T0 1
#define SIM_PROTOCOL_T1 2
#define SIM_XFER_TYPE_TPDU 1
#define SIM_XFER_TYPE_PTS 2
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
typedef struct {
  unsigned int wwt;
  unsigned int cwt;
  unsigned int bwt;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned int bgt;
  unsigned int cgt;
} sim_timing_t;
typedef struct {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned char * xmt_buffer;
  int xmt_length;
  int timeout;
  int errval;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
} sim_xmt_t;
typedef struct {
  unsigned char * rcv_buffer;
  int rcv_length;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  int timeout;
  int errval;
} sim_rcv_t;
typedef struct {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned char di;
  unsigned char fi;
} sim_baud_t;
#define SIM_POWER_OFF (0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_POWER_ON (1)
#define SIM_PRESENT_REMOVED (0)
#define SIM_PRESENT_DETECTED (1)
#define SIM_PRESENT_OPERATIONAL (2)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_OK (0)
#define SIM_ERROR_CWT (1 << 0)
#define SIM_ERROR_BWT (1 << 1)
#define SIM_ERROR_PARITY (1 << 2)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_ERROR_INVALID_TS (1 << 3)
#define SIM_ERROR_FRAME (1 << 4)
#define SIM_ERROR_ATR_TIMEROUT (1 << 5)
#define SIM_ERROR_NACK_THRESHOLD (1 << 6)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_ERROR_BGT (1 << 7)
#define SIM_ERROR_ATR_DELAY (1 << 8)
#define SIM_E_ACCESS (1)
#define SIM_E_TPDUSHORT (2)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_E_PTSEMPTY (3)
#define SIM_E_INVALIDXFERTYPE (4)
#define SIM_E_INVALIDXMTLENGTH (5)
#define SIM_E_INVALIDRCVLENGTH (6)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_E_NACK (7)
#define SIM_E_TIMEOUT (8)
#define SIM_E_NOCARD (9)
#define SIM_E_PARAM_FI_INVALID (10)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_E_PARAM_DI_INVALID (11)
#define SIM_E_PARAM_FBYD_WITHFRACTION (12)
#define SIM_E_PARAM_FBYD_NOTDIVBY8OR12 (13)
#define SIM_E_PARAM_DIVISOR_RANGE (14)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_E_MALLOC (15)
#define SIM_E_IRQ (16)
#define SIM_E_POWERED_ON (17)
#define SIM_E_POWERED_OFF (18)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_IOCTL_BASE (0xc0)
#define SIM_IOCTL_GET_PRESENSE _IOR(SIM_IOCTL_BASE, 1, int)
#define SIM_IOCTL_GET_ATR _IOR(SIM_IOCTL_BASE, 2, sim_atr_t)
#define SIM_IOCTL_XMT _IOR(SIM_IOCTL_BASE, 3, sim_xmt_t)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_IOCTL_RCV _IOR(SIM_IOCTL_BASE, 4, sim_rcv_t)
#define SIM_IOCTL_ACTIVATE _IO(SIM_IOCTL_BASE, 5)
#define SIM_IOCTL_DEACTIVATE _IO(SIM_IOCTL_BASE, 6)
#define SIM_IOCTL_WARM_RESET _IO(SIM_IOCTL_BASE, 7)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_IOCTL_COLD_RESET _IO(SIM_IOCTL_BASE, 8)
#define SIM_IOCTL_CARD_LOCK _IO(SIM_IOCTL_BASE, 9)
#define SIM_IOCTL_CARD_EJECT _IO(SIM_IOCTL_BASE, 10)
#define SIM_IOCTL_SET_PROTOCOL _IOR(SIM_IOCTL_BASE, 11, unsigned int)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define SIM_IOCTL_SET_TIMING _IOR(SIM_IOCTL_BASE, 12, sim_timing_t)
#define SIM_IOCTL_SET_BAUD _IOR(SIM_IOCTL_BASE, 13, sim_baud_t)
#define SIM_IOCTL_WAIT _IOR(SIM_IOCTL_BASE, 14, unsigned int)
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
