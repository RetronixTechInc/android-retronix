/*
 * Copyright 2018 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _ISI_CONFIG_H_
#define _ISI_CONFIG_H_

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*${macro:start}*/
/* Whether use external convertor such as MIPI2HDMI convertor (ADV7535) */
#ifndef APP_DISPLAY_EXTERNAL_CONVERTOR
#define APP_DISPLAY_EXTERNAL_CONVERTOR 1
#endif

/* Use the LVDS interface. */
#define DPU_EXAMPLE_DI DPU_DI_LVDS
/* Use MIPI CSI2. */
#define ISI_EXAMPLE_CI ISI_MIPI_CSI2
/*USE MAX9286 Camera*/
#define CAMERA_DEVICE CAMERA_DEVICE_MAX9286
/*${macro:end}*/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
/*${prototype:start}*/
/*${prototype:end}*/
#endif /* _ISI_CONFIG_H_ */
