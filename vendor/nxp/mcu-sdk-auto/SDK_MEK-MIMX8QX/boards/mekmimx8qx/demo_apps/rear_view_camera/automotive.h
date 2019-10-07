/*
 * Copyright (c) 2017-2018, NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _AUTOMOTIVE_H_
#define _AUTOMOTIVE_H_

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"


#include "rpmsg_lite.h"
#include "srtm_auto_service.h"

#define MAX_VEHICLE_STATE_MSG_QUEUE          10
#define MAX_AP_POWER_MSG_QUEUE               3
#define MAX_GET_INFO_MSG_QUEUE               3
#define MAX_CONTROL_MSG_QUEUE                6

#define AUTO_UI_EVENT_BIT_STARTUP       (1 << 0)
#define AUTO_UI_EVENT_BIT_STOP          (1 << 1)
#define AUTO_UI_EVENT_BIT_CAMERA        (1 << 2)
#define AUTO_UI_EVENT_BIT_HVAC          (1 << 3)
#define AUTO_UI_EVENT_BIT_PROGRESS      (1 << 4)
#define AUTO_UI_EVENT_BITS_NORMAL       (AUTO_UI_EVENT_BIT_HVAC | AUTO_UI_EVENT_BIT_PROGRESS)
#define AUTO_UI_EVENT_BITS_ALL          (AUTO_UI_EVENT_BIT_STARTUP | AUTO_UI_EVENT_BIT_STOP | AUTO_UI_EVENT_BIT_CAMERA | AUTO_UI_EVENT_BIT_HVAC | AUTO_UI_EVENT_BIT_PROGRESS)

#define UI_RESOURCE_BIT_NONE                 (1 << 0)
#define UI_RESOURCE_BIT_DISPLAY              (1 << 1)
#define UI_RESOURCE_BIT_CAMERA               (1 << 2)
#define UI_RESOURCE_BITS_ALL                 (UI_RESOURCE_BIT_DISPLAY | UI_RESOURCE_BIT_CAMERA)

#define AP_STATUS_REBOOT                 (1 << 0)
#define AP_STATUS_POWER_OFF              (1 << 1)
#define AP_STATUS_MASKS                  (AP_STATUS_REBOOT | AP_STATUS_POWER_OFF)

#define ANDROID_SRTM_CLIENT_ID                0x00
#define APP_MS2TICK(ms) ((ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS)

enum _VEHICLE_STATE_TYPE
{
    STATE_TYPE_AC_ON,    /* B0:off/on */
    STATE_TYPE_AUTO_ON,  /* B0:off/on */
    STATE_TYPE_AC_TEMP,  /* B0:ac index(left,right), B1:temperature */
    STATE_TYPE_FAN_SPEED,/* B0:fan speed*/
    STATE_TYPE_FAN_DIRECTION,/* B0:fan direction*/
    STATE_TYPE_RECIRC_ON,/* B0:recirc off/on */
    STATE_TYPE_HEATER,   /* B0:heater index(left/right), B1:level(0~3) */
    STATE_TYPE_DEFROST,  /* B0:glass index, B1:off/on, */
    STATE_TYPE_HVAC_POWER_ON,/* B0:off/on */
    STATE_TYPE_MUTE,     /* B0:off/on */
    STATE_TYPE_VOLUME,   /* B0:value(0~255) */
    STATE_TYPE_DOOR,     /* B0:door index(1~4), B1:close/open/lock/unlock,   */

    STATE_TYPE_RVC,      /* B0:camera index(0:all, 1~4:index), B1:on/off */
    STATE_TYPE_LIGHT,    /* B0:light index, B1:level */
    STATE_TYPE_GEAR,     /* B0:gear index */
    STATE_TYPE_TURN_SIGNAL, /* B0:none/left/right */

    STATE_TYPE_ANDROID,  /* B0:registered/unregistered, B1:partition/reason */
    STATE_TYPE_AP_POWER, /* B0:AP power state */
};

enum _CONTROL_UNIT_TYPE
{
    CTRL_TYPE_AC_ON,    /* B0:off/on */
    CTRL_TYPE_AUTO_ON,  /* B0:off/on */
    CTRL_TYPE_AC_TEMP,  /* B0:ac index(left,right), B1:temperature */
    CTRL_TYPE_FAN_SPEED,/* B0:fan speed*/
    CTRL_TYPE_FAN_DIRECTION,/* B0:fan direction*/
    CTRL_TYPE_RECIRC_ON,/* B0:recirc off/on */
    CTRL_TYPE_HEATER,   /* B0:heater index(left/right), B1:level(0~3) */
    CTRL_TYPE_DEFROST,  /* B0:glass index, B1:off/on, */
    CTRL_TYPE_HVAC_POWER_ON,/* B0:off/on */
    CTRL_TYPE_MUTE,     /* B0:off/on */
    CTRL_TYPE_VOLUME,   /* B0:value(0~255) */
    CTRL_TYPE_DOOR,     /* B0:door index(1~4), B1:close/open/lock/unlock,   */

    CTRL_TYPE_ALARM,    /* B0:on/off/pause/resume, B1~B4:timeout(second)*/
};

enum _AUTOMOTIVE_GEAR
{
    AUTO_GEAR_NONE = 0U,      /*Not use, for inital state*/
    AUTO_GEAR_PARKING,
    AUTO_GEAR_REVERSE,
    AUTO_GEAR_NEUTRAL,
    AUTO_GEAR_DRIVE,
    AUTO_GEAR_FIRST,
    AUTO_GEAR_SECOND,
    AUTO_GEAR_SPORT,
    AUTO_GEAR_MANUAL_1,
    AUTO_GEAR_MANUAL_2,
    AUTO_GEAR_MANUAL_3,
    AUTO_GEAR_MANUAL_4,
    AUTO_GEAR_MANUAL_5,
    AUTO_GEAR_MANUAL_6,
};

struct _register/*Not payload of srtm protocol*/
{
    uint8_t enroll;/*0~1: unregister/unregister*/
    void *handle; /*private*/
};

typedef struct
{
    uint16_t type;
    union {
        struct _register reg;
        uint32_t value;
    };
    uint8_t index;
} vehicle_state_t;

typedef struct
{
    uint16_t type;
    uint32_t timeout;
    uint8_t value[5];
} control_unit_t;

enum _AP_POWER_REPORT
{
    /* following state should align with android side, android report this value*/
    AP_POWER_REPORT_BOOT_COMPLETE = 0U,
    AP_POWER_REPORT_DEEP_SLEEP_ENTRY,
    AP_POWER_REPORT_DEEP_SLEEP_EXIT,
    AP_POWER_REPORT_SHUTDOWN_POSTPONE,
    AP_POWER_REPORT_SHUTDOWN_START,
    AP_POWER_REPORT_DISPLAY_OFF,
    AP_POWER_REPORT_DISPLAY_ON,
};

typedef struct
{
    uint16_t powerState;
    uint32_t time_postpone;
} ap_power_state_t;

typedef struct _hvac_command_map
{
    char *name;
    uint16_t id;
    uint8_t num_args;
} hvac_cmd_map_t;

/* the device state response to Android's register request */
enum _M4_DEVICE_STATE
{
    SHARED_RESOURCE_FREE = 0U,
    SHARED_RESOURCE_OCCUPIED,
    USER_ACTIVITY_BUSY,
};

enum _VEHICLE_INFO
{
    VEHICLE_UNIQUE_ID,
};

enum _ANDROID_BOOT_REASON
{
    BOOT_REASON_USER_POWER_ON = 0U,
    BOOT_REASON_DOOR_OPEN,
    BOOT_REASON_DOOR_UNLOCK,
    BOOT_REASON_REMOTE_START,
    BOOT_REASON_TIMER,
};



/* message queue to contain vehicle states*/
extern QueueHandle_t xVStateQueue;
/* message queue for ap_power_monitor_task to process register, unregister, power_report event*/
extern QueueHandle_t xAPPowerQueue;
/* message queue for ap_control_task to process SRTM control request*/
extern QueueHandle_t xControlMsgQueue;

/* display control event bits*/
extern EventGroupHandle_t g_xAutoUiEvent;
/* display occupied resources bits */
extern EventGroupHandle_t g_xUIResourceBits;
/* AP core state change: reboot, power-off */
extern EventGroupHandle_t g_xAPStatus;

#define AUTO_SRTM_SendCommand(cmd, cmdParam, paramLen, timeout) \
        APP_SRTM_SendAutoCommand(ANDROID_SRTM_CLIENT_ID, cmd, cmdParam, paramLen, NULL, 0, timeout)


struct cmd_tbl_s {
    char *name;		/* Command Name			*/
    int  maxargs;	/* maximum number of arguments	*/
    int  (*cmd)(struct cmd_tbl_s *, int, char * const []);
    char *usage;		/* Usage message	(short)	*/
    char *help;		/* Help  message	(long)	*/
};
typedef struct cmd_tbl_s cmd_tbl_t;

#endif