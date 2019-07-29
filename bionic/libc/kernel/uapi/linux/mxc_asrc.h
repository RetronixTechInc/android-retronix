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
#ifndef __MXC_ASRC_UAPI_H__
#define __MXC_ASRC_UAPI_H__
#define ASRC_IOC_MAGIC 'C'
#define ASRC_REQ_PAIR _IOWR(ASRC_IOC_MAGIC, 0, struct asrc_req)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ASRC_CONFIG_PAIR _IOWR(ASRC_IOC_MAGIC, 1, struct asrc_config)
#define ASRC_RELEASE_PAIR _IOW(ASRC_IOC_MAGIC, 2, enum asrc_pair_index)
#define ASRC_CONVERT _IOW(ASRC_IOC_MAGIC, 3, struct asrc_convert_buffer)
#define ASRC_START_CONV _IOW(ASRC_IOC_MAGIC, 4, enum asrc_pair_index)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define ASRC_STOP_CONV _IOW(ASRC_IOC_MAGIC, 5, enum asrc_pair_index)
#define ASRC_STATUS _IOW(ASRC_IOC_MAGIC, 6, struct asrc_status_flags)
#define ASRC_FLUSH _IOW(ASRC_IOC_MAGIC, 7, enum asrc_pair_index)
enum asrc_pair_index {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  ASRC_INVALID_PAIR = - 1,
  ASRC_PAIR_A = 0,
  ASRC_PAIR_B = 1,
  ASRC_PAIR_C = 2,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define ASRC_PAIR_MAX_NUM (ASRC_PAIR_C + 1)
enum asrc_inclk {
  INCLK_NONE = 0x03,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  INCLK_ESAI_RX = 0x00,
  INCLK_SSI1_RX = 0x01,
  INCLK_SSI2_RX = 0x02,
  INCLK_SSI3_RX = 0x07,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  INCLK_SPDIF_RX = 0x04,
  INCLK_MLB_CLK = 0x05,
  INCLK_PAD = 0x06,
  INCLK_ESAI_TX = 0x08,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  INCLK_SSI1_TX = 0x09,
  INCLK_SSI2_TX = 0x0a,
  INCLK_SSI3_TX = 0x0b,
  INCLK_SPDIF_TX = 0x0c,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  INCLK_ASRCK1_CLK = 0x0f,
};
enum asrc_outclk {
  OUTCLK_NONE = 0x03,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  OUTCLK_ESAI_TX = 0x00,
  OUTCLK_SSI1_TX = 0x01,
  OUTCLK_SSI2_TX = 0x02,
  OUTCLK_SSI3_TX = 0x07,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  OUTCLK_SPDIF_TX = 0x04,
  OUTCLK_MLB_CLK = 0x05,
  OUTCLK_PAD = 0x06,
  OUTCLK_ESAI_RX = 0x08,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  OUTCLK_SSI1_RX = 0x09,
  OUTCLK_SSI2_RX = 0x0a,
  OUTCLK_SSI3_RX = 0x0b,
  OUTCLK_SPDIF_RX = 0x0c,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  OUTCLK_ASRCK1_CLK = 0x0f,
};
enum asrc_word_width {
  ASRC_WIDTH_24_BIT = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  ASRC_WIDTH_16_BIT = 1,
  ASRC_WIDTH_8_BIT = 2,
};
struct asrc_config {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  enum asrc_pair_index pair;
  unsigned int channel_num;
  unsigned int buffer_num;
  unsigned int dma_buffer_size;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned int input_sample_rate;
  unsigned int output_sample_rate;
  enum asrc_word_width input_word_width;
  enum asrc_word_width output_word_width;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  enum asrc_inclk inclk;
  enum asrc_outclk outclk;
};
struct asrc_req {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned int chn_num;
  enum asrc_pair_index index;
};
struct asrc_querybuf {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned int buffer_index;
  unsigned int input_length;
  unsigned int output_length;
  unsigned long input_offset;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  unsigned long output_offset;
};
struct asrc_convert_buffer {
  void * input_buffer_vaddr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  void * output_buffer_vaddr;
  unsigned int input_buffer_length;
  unsigned int output_buffer_length;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct asrc_status_flags {
  enum asrc_pair_index index;
  unsigned int overload_error;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum asrc_error_status {
  ASRC_TASK_Q_OVERLOAD = 0x01,
  ASRC_OUTPUT_TASK_OVERLOAD = 0x02,
  ASRC_INPUT_TASK_OVERLOAD = 0x04,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
  ASRC_OUTPUT_BUFFER_OVERFLOW = 0x08,
  ASRC_INPUT_BUFFER_UNDERRUN = 0x10,
};
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
