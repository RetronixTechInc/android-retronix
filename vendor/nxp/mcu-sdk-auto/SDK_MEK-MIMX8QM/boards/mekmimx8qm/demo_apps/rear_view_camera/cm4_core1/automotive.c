/*
 * Copyright 2018 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "fsl_debug_console.h"
#include "board.h"
#include "automotive.h"
#include "app_srtm.h"


#define APP_REAR_VIEW_CAMERA_TASK_PRIO              (4U)
#define APP_AP_POWER_MONITOR_TASK_PRIO              (3U)
#define APP_VEHICLE_STATE_MONITOR_TASK_PRIO         (3U)/*dispatcher priority should higher than vehicle_state_monitor_task's*/
#define APP_AP_CONTROL_TASK_PRIO                    (4U)
#define APP_VIRTUAL_CONTROL_CONSOLE_TASK_PRIO       (2U)
#define APP_HVAC_UI_EVENT_TASK_PRIO                 (4U)

EventGroupHandle_t g_xAutoUiEvent = NULL;
EventGroupHandle_t g_xUIResourceBits = NULL;
EventGroupHandle_t g_xAPStatus = NULL;
/* message queue to contain vehicle states*/
QueueHandle_t xVStateQueue = NULL;
/* message queue for ap_power_monitor_task to process register, unregister, power_report event*/
QueueHandle_t xAPPowerQueue = NULL;
/* message queue for ap_control_task to process SRTM control request*/
QueueHandle_t xControlMsgQueue = NULL;

extern void BOARD_InitHardware(void);
extern void rear_view_camera_task(void *pvParameters);
static void ap_power_monitor_task(void *pvParameters);
static void vehicle_state_monitor_task(void *pvParameters);
static void ap_control_task(void *pvParameters);
static void hvac_ui_event_process_task(void *pvParameters);
static void virtual_control_console_task(void *pvParameters);

/*******************************************************************************
 * Code
 ******************************************************************************/
int main(void)
{
    BOARD_InitHardware();
    APP_SRTM_Init();

    g_xAutoUiEvent = xEventGroupCreate();
    assert(g_xAutoUiEvent);
    g_xUIResourceBits = xEventGroupCreate();
    assert(g_xUIResourceBits);
    g_xAPStatus = xEventGroupCreate();
    assert(g_xAPStatus);

    xAPPowerQueue = xQueueCreate(MAX_AP_POWER_MSG_QUEUE, sizeof(ap_power_state_t));
    assert(xAPPowerQueue);

    xVStateQueue = xQueueCreate(MAX_VEHICLE_STATE_MSG_QUEUE, sizeof(vehicle_state_t));
    assert(xVStateQueue);

    xControlMsgQueue = xQueueCreate(MAX_CONTROL_MSG_QUEUE, sizeof(control_unit_t));
    assert(xControlMsgQueue);

    if (xTaskCreate(rear_view_camera_task, "RearViewCamera", 512U, NULL, APP_REAR_VIEW_CAMERA_TASK_PRIO, NULL) != pdPASS)
    {
        PRINTF("RearViewCamera Task creation failed!.\r\n");
    }
    if (xTaskCreate(ap_power_monitor_task, "APPowerMonitorr", 512U, NULL, APP_AP_POWER_MONITOR_TASK_PRIO, NULL) != pdPASS)
    {
        PRINTF("APPowerMonitorr Task creation failed!.\r\n");
    }
    if (xTaskCreate(vehicle_state_monitor_task, "VehicleMonitor", 512U, NULL, APP_VEHICLE_STATE_MONITOR_TASK_PRIO, NULL) != pdPASS)
    {
        PRINTF("VehicleMonitor Task creation failed!.\r\n");
    }
    if (xTaskCreate(ap_control_task, "APControl", 512U, NULL, APP_AP_CONTROL_TASK_PRIO, NULL) != pdPASS)
    {
        PRINTF("AP control task creation failed!.\r\n");
    }
    if (xTaskCreate(hvac_ui_event_process_task, "HVAC_UI", 512U, NULL, APP_HVAC_UI_EVENT_TASK_PRIO, NULL) != pdPASS)
    {
        PRINTF("HVAC UI task creation failed!.\r\n");
    }
    if (xTaskCreate(virtual_control_console_task, "Cosole", 512U, NULL, APP_VIRTUAL_CONTROL_CONSOLE_TASK_PRIO, NULL) != pdPASS)
    {
        PRINTF("Console Task creation failed!.\r\n");
    }
    vTaskStartScheduler();

    /* Application should never reach this point. */
    for (;;)
    {
    }
}

static void ap_power_monitor_task(void *pvParameters)
{
    ap_power_state_t ap_power;
    uint8_t reqData[SRTM_USER_DATA_LENGTH];

    APP_SRTM_StartCommunication();

    while (true)
    {
        if (xQueueReceive(xAPPowerQueue, &ap_power, portMAX_DELAY))
        {
            switch (ap_power.powerState)
            {
            case AP_POWER_REPORT_BOOT_COMPLETE:
                /* TODO: send BOOT_REASON to android, boot reason need to get from other module*/
                reqData[0] = BOOT_REASON_USER_POWER_ON;
                AUTO_SRTM_SendCommand(SRTM_AUTO_CMD_BOOT_REASON, reqData, SRTM_USER_DATA_LENGTH, (TickType_t)10);
                break;
            case AP_POWER_REPORT_DEEP_SLEEP_ENTRY:
                break;
            case AP_POWER_REPORT_DEEP_SLEEP_EXIT:
                break;
            case AP_POWER_REPORT_SHUTDOWN_POSTPONE:
                break;
            case AP_POWER_REPORT_SHUTDOWN_START:
                break;
            case AP_POWER_REPORT_DISPLAY_OFF:
                break;
            case AP_POWER_REPORT_DISPLAY_ON:
                break;
            default:
                break;
            }
        }
    }
}

static uint8_t android_registered_flag;
static volatile uint32_t shared_resource_free = false;
static volatile uint32_t hvac_ui_busy = false;
static void vehicle_state_monitor_task(void *pvParameters)
{
    EventBits_t resBits;
    vehicle_state_t stateMsg;
    uint8_t reqData[SRTM_USER_DATA_LENGTH];
    static uint32_t pre_gear_state = AUTO_GEAR_NONE;
    TaskHandle_t regHandle;

    android_registered_flag = 0;
    while (true)
    {
        if (xQueueReceive(xVStateQueue, &stateMsg, portMAX_DELAY))
        {
            PRINTF("vehicle_state_monitor_task get:type=%d\r\n",stateMsg.type);
            switch (stateMsg.type)
            {
            case STATE_TYPE_ANDROID:
                android_registered_flag = stateMsg.reg.enroll;/* 1:registered, 0:unregistered */
                regHandle = stateMsg.reg.handle;
                PRINTF("srtm register:flag=%d,handle=%x\r\n",android_registered_flag, regHandle);
                if (android_registered_flag)
                {
                    if (!shared_resource_free)
                    {
                        /* check if shared resource in use, NOT clear bit, NOT waitforall, NOT block here if no bit set*/
                        resBits = xEventGroupWaitBits(g_xUIResourceBits, UI_RESOURCE_BITS_ALL, pdFALSE, pdFALSE, (TickType_t)0);
                        if (!(resBits & UI_RESOURCE_BIT_CAMERA))
                        {
                            /* camera resource is not occupied, stop display and release it*/
                            xEventGroupSetBits(g_xAutoUiEvent, AUTO_UI_EVENT_BIT_STOP);
                            resBits = xEventGroupWaitBits(g_xUIResourceBits, UI_RESOURCE_BIT_NONE, pdTRUE, pdFALSE, portMAX_DELAY);/*wait for resource release*/
                            if (resBits & UI_RESOURCE_BIT_NONE)
                            {
                                /* send command to android that no shared resource in use */
                                xTaskNotify(regHandle, SHARED_RESOURCE_FREE, eSetValueWithOverwrite);
                                shared_resource_free = true;
                            }
                        }
                        else  /* shared resource is in use, cannot free now */
                        {
                            xTaskNotify(regHandle, SHARED_RESOURCE_OCCUPIED, eSetValueWithOverwrite);
                        }
                    }
                    else
                    {
                        /*register twice, should not happen*/
                        PRINTF("srtm auto service already registered, should not register again!\r\n");
                        xTaskNotify(regHandle, SHARED_RESOURCE_FREE, eSetValueWithOverwrite);
                    }
                }
                else
                {
                    /* unregister android client, M4 continue to take over display */
                    resBits = xEventGroupWaitBits(g_xAPStatus, AP_STATUS_MASKS, pdTRUE, pdFALSE, APP_MS2TICK(10000));/*clear bit, NOT waitforall, block 10000ms*/
                    if (resBits & AP_STATUS_MASKS)
                    {
                        PRINTF("AP reboot or power off\r\n");
                        APP_SRTM_Reboot();
                        if (pre_gear_state == AUTO_GEAR_REVERSE)
                            xEventGroupSetBits(g_xAutoUiEvent, AUTO_UI_EVENT_BIT_STARTUP | AUTO_UI_EVENT_BIT_CAMERA);
                        else
                            xEventGroupSetBits(g_xAutoUiEvent, AUTO_UI_EVENT_BIT_STARTUP | AUTO_UI_EVENT_BIT_PROGRESS);/* TODO: change progress bar to hvac ui*/

                        shared_resource_free = false;
                    }
                    else
                    {
                         PRINTF("Wait for AP reboot or power off interrupt timeout!\r\n");
                    }
                }
                break;
            case STATE_TYPE_AP_POWER:
                PRINTF("AP Power change: reboot or power off\r\n");
                APP_SRTM_Reboot();
                if (pre_gear_state == AUTO_GEAR_REVERSE)
                    xEventGroupSetBits(g_xAutoUiEvent, AUTO_UI_EVENT_BIT_STARTUP | AUTO_UI_EVENT_BIT_CAMERA);
                else
                    xEventGroupSetBits(g_xAutoUiEvent, AUTO_UI_EVENT_BIT_STARTUP | AUTO_UI_EVENT_BIT_PROGRESS);/* TODO: change progress bar to hvac ui*/

                shared_resource_free = false;
                android_registered_flag = false;
                break;
            case STATE_TYPE_GEAR:
                if (pre_gear_state == stateMsg.value)
                    break;

                switch (stateMsg.value)
                {
                case AUTO_GEAR_REVERSE:
                    if (android_registered_flag)
                    {
                        reqData[0] = STATE_TYPE_GEAR;
                        reqData[1] = 0x00;
                        reqData[2] = AUTO_GEAR_REVERSE;
                        reqData[3] = 0x00;
                        reqData[4] = 0x00;
                        reqData[5] = 0x00;
                        AUTO_SRTM_SendCommand(SRTM_AUTO_CMD_VEHICLE_STATE, reqData, SRTM_USER_DATA_LENGTH, (TickType_t)10);
                    }
                    else
                    {
                        xEventGroupSetBits(g_xAutoUiEvent, AUTO_UI_EVENT_BIT_STARTUP | AUTO_UI_EVENT_BIT_CAMERA);
                    }
                    break;
                case AUTO_GEAR_DRIVE:
                case AUTO_GEAR_PARKING:
                case AUTO_GEAR_NEUTRAL:
                case AUTO_GEAR_FIRST:
                case AUTO_GEAR_SECOND:
                case AUTO_GEAR_SPORT:
                case AUTO_GEAR_MANUAL_1:
                case AUTO_GEAR_MANUAL_2:
                case AUTO_GEAR_MANUAL_3:
                case AUTO_GEAR_MANUAL_4:
                case AUTO_GEAR_MANUAL_5:
                case AUTO_GEAR_MANUAL_6:
                    if (android_registered_flag)
                    {
                        if (!shared_resource_free)
                        {
                            xEventGroupSetBits(g_xAutoUiEvent, AUTO_UI_EVENT_BIT_STOP);
                            resBits = xEventGroupWaitBits(g_xUIResourceBits, UI_RESOURCE_BIT_NONE, pdTRUE, pdFALSE, portMAX_DELAY);/*wait for resource release*/
                            if (resBits & UI_RESOURCE_BIT_NONE)
                            {
                                shared_resource_free = true;
                            }
                        }
                        reqData[0] = STATE_TYPE_GEAR;
                        reqData[1] = 0x00;
                        reqData[2] = stateMsg.value;
                        reqData[3] = 0x00;
                        reqData[4] = 0x00;
                        reqData[5] = 0x00;
                        AUTO_SRTM_SendCommand(SRTM_AUTO_CMD_VEHICLE_STATE, reqData, SRTM_USER_DATA_LENGTH, (TickType_t)10);
                    }
                    else
                    {
                        xEventGroupSetBits(g_xAutoUiEvent, AUTO_UI_EVENT_BIT_STARTUP | AUTO_UI_EVENT_BIT_PROGRESS);/*TODO: switch to HVAC UI*/
                    }
                    break;
                default:
                    break;
                }

                pre_gear_state = stateMsg.value;
                break;
            case STATE_TYPE_AC_ON:
            case STATE_TYPE_AUTO_ON:
            case STATE_TYPE_AC_TEMP:
            case STATE_TYPE_FAN_SPEED:
            case STATE_TYPE_FAN_DIRECTION:
            case STATE_TYPE_RECIRC_ON:
            case STATE_TYPE_HEATER:
            case STATE_TYPE_DEFROST:
            case STATE_TYPE_HVAC_POWER_ON:
            case STATE_TYPE_MUTE:
            case STATE_TYPE_VOLUME:
            case STATE_TYPE_DOOR:
            case STATE_TYPE_TURN_SIGNAL:
                if (android_registered_flag)
                {
                    reqData[0] = (uint8_t)(stateMsg.type & 0xff);
                    reqData[1] = (uint8_t)((stateMsg.type >> 8) & 0xff);
                    reqData[2] = (uint8_t)(stateMsg.value & 0xff);
                    reqData[3] = (uint8_t)((stateMsg.value >> 8) & 0xff);
                    reqData[4] = (uint8_t)((stateMsg.value >> 16) & 0xff);
                    reqData[5] = (uint8_t)((stateMsg.value >> 24) & 0xff);
                    reqData[SRTM_USER_DATA_LENGTH - 1] = stateMsg.index;
                    AUTO_SRTM_SendCommand(SRTM_AUTO_CMD_VEHICLE_STATE, reqData, SRTM_USER_DATA_LENGTH, (TickType_t)10);
                }
                break;
            default:
                break;
            }
        }
    }
}

static void hvac_ui_event_process_task(void *pvParameters)
{
    /* TODO: add HVAC UI initialization*/
    while (true)
    {
        /* TODO: process HVAC UI touch event and update UI*/
        vTaskDelay( 100000 );
    }
}

static void ap_control_task(void *pvParameters)
{
    control_unit_t ctrl;
    float *ptemp, temp;
    int temp_int1, temp_int2;

    while (true)
    {
        if (xQueueReceive(xControlMsgQueue, &ctrl, portMAX_DELAY))
        {
            switch (ctrl.type)
            {
            case CTRL_TYPE_AC_ON:
                PRINTF("Android control: AC_ON, %s\r\n", ctrl.value[0] ? "on" : "off");
                break;
            case CTRL_TYPE_AUTO_ON:
                PRINTF("Android control: AUTO_ON, %s\r\n", ctrl.value[0] ? "on" : "off");
                break;
            case CTRL_TYPE_AC_TEMP:
                ptemp = (float*)&ctrl.value;
                temp = *ptemp;
                temp_int1 = (uint32_t)(temp*1000);
                temp_int1 = temp_int1 / 1000;
                temp_int2 = (uint32_t)(temp*1000);
                temp_int2 = temp_int2 % 1000;
                PRINTF("Android control: AC_TEMP, index=%d, temp=%d.%d\r\n", ctrl.value[4], temp_int1, temp_int2);
                break;
            case CTRL_TYPE_FAN_SPEED:
                PRINTF("Android control: FAN_SPEED, 0x%x\r\n", ctrl.value[0]);
                break;
            case CTRL_TYPE_FAN_DIRECTION:
                PRINTF("Android control: FAN_DIRECTION, 0x%x\r\n", ctrl.value[0]);
                break;
            case CTRL_TYPE_RECIRC_ON:
                PRINTF("Android control: RECIRC_ON, %s\r\n", ctrl.value[0] ? "on" : "off");
                break;
            case CTRL_TYPE_HEATER:
                PRINTF("Android control: HEATER, index=%d, level=%d\r\n", ctrl.value[4], ctrl.value[0]);
                break;
            case CTRL_TYPE_DEFROST:
                PRINTF("Android control: DEFROST, index=%d, %s\r\n", ctrl.value[4], ctrl.value[0] ? "on" : "off");
                break;
            case CTRL_TYPE_HVAC_POWER_ON:
                PRINTF("Android control: HVAC_POWER_ON, %s\r\n", ctrl.value[0] ? "on" : "off");
                break;
            case CTRL_TYPE_MUTE:
                PRINTF("Android control: MUTE, %s\r\n", ctrl.value[0] ? "on" : "off");
                break;
            case CTRL_TYPE_VOLUME:
                PRINTF("Android control: VOLUME, vol=%d\r\n", ctrl.value[0]);
                break;
            case CTRL_TYPE_DOOR:
                PRINTF("Android control: DOOR, index=%d, state=%d\r\n", ctrl.value[4], ctrl.value[0]);
                break;
            case CTRL_TYPE_ALARM:
                break;
            default:
                break;
            }
        }
    }
}



#define MAX_COMMAND_STR_LENGTH      30
#define CMD_RET_USAGE               (-1)
#define MAX_COMMAND_PARAMS          5

int do_help(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[]);
int do_version(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[]);
int do_gear_state(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[]);
int do_turn_signal(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[]);
int do_vehicle_state_report(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[]);

struct cmd_tbl_s cmd_tables[] =
{
    {"?", 2, do_help, "alias for 'help'", ""},
    {"help", 2, do_help, "print command description/usage", ""},
    {"version", 1, do_version, "print srtm version, build time", ""},
    {"report", 3, do_vehicle_state_report, "send vehicle state to android",
                                "format: report subcmd [index] value\r\navailable sub command:\r\n\t"
                                        "ac_on val\r\n\t"
                                        "auto_on val\r\n\t"
                                        "ac_temp val idx\r\n\t"
                                        "fan_speed val\r\n\t"
                                        "fan_direction val\r\n\t"
                                        "recirc_on val\r\n\t"
                                        "heater val idx\r\n\t"
                                        "defrost val idx\r\n\t"
                                        "hvac_power val\r\n\t"
                                        "mute val\r\n\t"
                                        "volume val\r\n\t"
                                        "door val idx"},
    {"gear", 1, do_gear_state, "send gear state to android",
                               "report current gear,"
                                 "format: gear index\r\n\treverse: gear 2\r\n\tdrive: gear 4"},
    {"turn", 1, do_turn_signal, "send turn signal(none/left/right) to android",
                               "turn none/left/right:\r\n\tturn 0/1/2"},
};

#define CMD_TABLE_ITEM_CNT      (sizeof(cmd_tables)/sizeof(cmd_tbl_t))

int do_help(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[])
{
    int i;
    if (argc > cmd_tbl->maxargs)
    {
        PRINTF("%s - %s\r\n%s\r\n", cmd_tbl->name, cmd_tbl->usage, cmd_tbl->help);
        return 1;
    }
    else if (argc == cmd_tbl->maxargs)
    {
        for (i=0; i<CMD_TABLE_ITEM_CNT; i++)
        {
            if (!strcmp(cmd_tables[i].name, argv[1]))
            {
                PRINTF("%s\t - %s\r\n", cmd_tables[i].name, cmd_tables[i].usage);
                PRINTF("%s\r\n", cmd_tables[i].help);
                break;
            }
        }
        if (i == CMD_TABLE_ITEM_CNT)
        {
            PRINTF("%s: Unknown command '%s' - try 'help'\r\n", cmd_tbl->name, argv[1]);
        }
    }
    else
    {
        for (i=0; i<CMD_TABLE_ITEM_CNT; i++)
        {
            PRINTF("%s\t - %s\r\n", cmd_tables[i].name, cmd_tables[i].usage);
        }
    }
    return 0;
}
int do_version(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[])
{
    if (argc >= 2)
        return CMD_RET_USAGE;

    PRINTF("SRTM version %s\r\n", RL_VERSION);
    PRINTF("Build Time: %s--%s \r\n", __DATE__, __TIME__);

    return 0;
}
int do_gear_state(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[])
{
    vehicle_state_t vstate;
    uint32_t gear_idx;
    char *endptr;
    if (argc != 2)
        return CMD_RET_USAGE;

    gear_idx = strtol(argv[1], &endptr, 0);

    vstate.type = STATE_TYPE_GEAR;
    vstate.value = gear_idx;
    xQueueSend(xVStateQueue, (void *)&vstate, (TickType_t)0);

    return 0;
}

int do_turn_signal(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[])
{
    vehicle_state_t vstate;
    uint32_t state;
    char *endptr;
    if (argc != 2)
        return CMD_RET_USAGE;

    state = strtol(argv[1], &endptr, 0);

    vstate.type = STATE_TYPE_TURN_SIGNAL;
    vstate.value = state;
    xQueueSend(xVStateQueue, (void *)&vstate, (TickType_t)0);

    return 0;
}

hvac_cmd_map_t hvac_cmd_map[] =
{
    {"ac_on", STATE_TYPE_AC_ON, 1},
    {"auto_on", STATE_TYPE_AUTO_ON, 1},
    {"ac_temp", STATE_TYPE_AC_TEMP, 2},
    {"fan_speed", STATE_TYPE_FAN_SPEED, 1},
    {"fan_direction", STATE_TYPE_FAN_DIRECTION, 1},
    {"recirc_on", STATE_TYPE_RECIRC_ON, 1},
    {"heater", STATE_TYPE_HEATER, 2},
    {"defrost", STATE_TYPE_DEFROST, 2},
    {"hvac_power", STATE_TYPE_HVAC_POWER_ON, 1},
    {"mute", STATE_TYPE_MUTE, 1},
    {"volume", STATE_TYPE_VOLUME, 1},
    {"door", STATE_TYPE_DOOR, 2},
};
#define HVAC_CMD_MAP_CNT        (sizeof(hvac_cmd_map)/sizeof(hvac_cmd_map_t))

int do_vehicle_state_report(struct cmd_tbl_s *cmd_tbl, int argc, char * const argv[])
{
    int i;
    vehicle_state_t vstate = {0};
    uint8_t num;
    char *endptr;
    float tempf;
    if (argc < 3)
        return CMD_RET_USAGE;

    for (i=0; i<HVAC_CMD_MAP_CNT; i++)
    {
        if (!strcmp(hvac_cmd_map[i].name, argv[1]))
        {
            if (argc != hvac_cmd_map[i].num_args + 2)
            {
                PRINTF("report: %s, number of parameters not match,require %d params!\r\n", argv[1], hvac_cmd_map[i].num_args);
                return CMD_RET_USAGE;
            }
            break;
        }
    }
    if (i == HVAC_CMD_MAP_CNT)
    {
        PRINTF("report: cannot find sub command: %s\r\n", argv[1]);
        return CMD_RET_USAGE;
    }

    num = hvac_cmd_map[i].num_args;
    vstate.type = hvac_cmd_map[i].id;
    if (strchr(argv[2], '.')) {
        tempf = strtof(argv[2], &endptr);
        vstate.value = *(uint32_t*)&tempf;
    } else
        vstate.value = strtol(argv[2], &endptr, 0);

    if (num == 2)
        vstate.index = strtol(argv[3], &endptr, 0);
    else
        vstate.index = 0;

    PRINTF("report: subcmd=%s, id=%d, ", argv[1], vstate.type);
    for (i=0; i<num; i++)
        PRINTF("params[%d]=%s, ", i, argv[2+i]);
    PRINTF("\r\n");

    xQueueSend(xVStateQueue, (void *)&vstate, (TickType_t)0);

    return 0;
}

int console_run_command(const char *cmd)
{
    char cmdbuf[MAX_COMMAND_STR_LENGTH];
    const char *str = cmd;
    int argc = 0;
    char *argv[MAX_COMMAND_PARAMS];
    int len;
    bool check_start_flag;
    bool check_end_flag;
    int result;
    int i;

    for (i=0; i<(MAX_COMMAND_STR_LENGTH-1) && str; i++)
        cmdbuf[i] = *str++;

    len = strlen(cmdbuf);
    check_start_flag = true;
    check_end_flag = false;
    for (i=0; i<len; i++)
    {
        if (check_start_flag && (cmdbuf[i] != ' '))
        {
            argv[argc++] = &cmdbuf[i];
            if (argc >= MAX_COMMAND_PARAMS)
                break;
            check_start_flag = false;
            check_end_flag = true;
        }
        else if (check_end_flag && (cmdbuf[i] == ' '))
        {
            cmdbuf[i] = '\0';
            check_start_flag = true;
            check_end_flag = false;
        }
    }

    PRINTF("parse cmd: argc=%d, ", argc);
    for (i=0; i<argc; i++)
        PRINTF("argv[%d]=%s, ", i, argv[i]);
    PRINTF("\r\n");

    for (i=0; i<CMD_TABLE_ITEM_CNT; i++)
        if (!strcmp(cmd_tables[i].name, argv[0])) /* find the command */
            break;
    if (i == CMD_TABLE_ITEM_CNT)
    {
        PRINTF("Unknown command '%s' - try 'help'\r\n", argv[0]);
        return 1;
    }

    result = cmd_tables[i].cmd(&cmd_tables[i], argc, argv);
    if (result == CMD_RET_USAGE)
    {
        argv[1] = argv[0];
        argv[0] = "help";
        argc = 2;
        cmd_tables[1].cmd(&cmd_tables[1], argc, argv);/* print help log*/
    }

    return 0;
}
/*! @brief character backspace ASCII value */
#define APP_CONSOLE_BACKSPACE       0x08
#define MAX_HISTORY_NUM             10
#define PROMPT                      "=>"
static uint8_t console_strings[MAX_HISTORY_NUM + 1][MAX_COMMAND_STR_LENGTH];/* reserve one string for current input*/
static int history_index;
static bool history_buff_full;
static int hist_index_head;   /* the latest stored string item index*/
static int hist_index_store;  /* the next index that input string will store*/
static int hist_index_tail;   /* the lastest stored string item index*/
static bool edit_flag;

static void virtual_control_console_task(void *pvParameters)
{
    status_t status;
    vehicle_state_t vstate;
    uint8_t *ptr;
    int str_index;
    history_index = 0;
    history_buff_full = 0;
    hist_index_head = 0;
    hist_index_tail = 0;
    hist_index_store = 0;
    uint8_t ctrl_char[8];
    int ctrl_char_index;
    uint8_t *hist_str;
    uint8_t str_buf[MAX_COMMAND_STR_LENGTH];
    int ctrl_char_len = 0;

    edit_flag = false;

    PRINTF("Rear View Camera Demo\r\n=====================\r\n");
    PRINTF("Build Time: %s--%s \r\n", __DATE__, __TIME__);

    vstate.type = STATE_TYPE_GEAR;
    vstate.value = AUTO_GEAR_DRIVE;
    xQueueSend(xVStateQueue, (void *)&vstate, (TickType_t)0);

    PRINTF("Input 'help' to get available commands\r\n");

    while (true)
    {
        PRINTF(PROMPT);
        str_index = 0;
        ctrl_char_index = 0;
        ctrl_char_len = 0;
        ptr = str_buf;
        while (true)
        {
            status = DbgConsole_TryGetchar(ptr);
            if (kStatus_Fail == status)
            {
                vTaskDelay(5);
                continue;
            }

            if (('\r' == *ptr) || ('\n' == *ptr))
            {
                *ptr = '\0';
                if (edit_flag && str_index > 0)
                {
                    hist_str = &console_strings[hist_index_store][0];
                    strcpy(hist_str, str_buf);
                    hist_index_head = hist_index_store;
                    hist_index_store++;/* point to next store index position */
                    if (hist_index_store >= MAX_HISTORY_NUM + 1)
                    {
                        hist_index_store = 0;
                        history_buff_full = true;
                    }
                    if (history_buff_full)
                    {
                        hist_index_tail = hist_index_store + 1;/*the hist_index_store index should be overlayed*/
                    }
                    history_index = hist_index_store;
                }
                PRINTF("\r\n");
                break;
            }

            edit_flag = true;
            if (ctrl_char_len > 0)
            {
                if (ctrl_char_len == 1)
                {
                    if ((*ptr == '[') || (*ptr == 'O'))
                        ctrl_char_len++;
                    else/* incorrect control char*/
                        ctrl_char_len = 0;
                }
                else if (ctrl_char_len == 2)
                {
                    switch (*ptr)
                    {
                    case 'A':/* Up arrow */
                        if (history_index != hist_index_tail)
                        {
                            if (history_index == 0)
                                history_index = MAX_HISTORY_NUM;
                            else
                                history_index--;
                            hist_str = &console_strings[history_index][0];
                            if (str_index > 0)
                                while (str_index--)
                                    PRINTF("\b \b");
                            PRINTF("%s", hist_str);
                            strcpy((char *)str_buf, hist_str);
                            str_index = strlen(hist_str);
                            ptr = &str_buf[str_index];
                            edit_flag = false;
                            if ((hist_index_head == 0) && (!history_buff_full))/* process only one history case*/
                                history_index = hist_index_store;
                        }
                        ctrl_char_len = 0;
                        break;
                    case 'B':/* Down arrow */
                        if (history_index != hist_index_store)
                        {
                            if (history_index == MAX_HISTORY_NUM)
                                history_index = 0;
                            else
                                history_index++;

                            if (str_index > 0)
                                while (str_index--)
                                    PRINTF("\b \b");
                            if (history_index != hist_index_store)
                            {
                                hist_str = &console_strings[history_index][0];
                                PRINTF("%s", hist_str);
                                strcpy(str_buf, hist_str);
                                str_index = strlen(hist_str);
                                ptr = &str_buf[str_index];
                            }
                            else
                            {
                                str_buf[0] = '\0';
                                str_index = 0;
                                ptr = str_buf;
                            }
                        }
                        else
                        {
                            str_buf[0] = '\0';
                            ptr = str_buf;
                            if (str_index > 0)
                                while (str_index--)
                                    PRINTF("\b \b");
                        }
                        edit_flag = false;
                        ctrl_char_len = 0;
                        break;
                    case 'C':/* Right arrow */
                    case 'D':/* Left arrow */
                    case 'F':
                    case 'H':
                        ctrl_char_len = 0;/* discard these control char*/
                        break;
                    case '1':
                    case '3':
                    case '4':
                    case '7':
                    case '8':
                        if (ctrl_char[0] == '[')/*checking following char*/
                            ctrl_char_len++;
                        else
                            ctrl_char_len = 0;/* discard this control char*/
                        break;
                    default:/* incorrect control char*/
                        ctrl_char_len = 0;
                        break;
                    }
                }
                else if (ctrl_char_len == 3)
                {
                    if (*ptr == '~')
                    {
                        switch (ctrl_char[1])
                        {
                        case '1':
                        case '3':
                        case '4':
                        case '7':
                        case '8':
                            ctrl_char_len = 0;/* discard this control char*/
                            break;
                        }
                    }
                    else
                    {
                        ctrl_char_len = 0;/* incorrect control char*/
                    }
                }
                if (ctrl_char_len > 0)
                {
                    ctrl_char[ctrl_char_index++] = *ptr;
                }
                else
                {
                    ctrl_char_index = 0;
                }
                continue;
            }

            if (0x1b == *ptr)
            {
                ctrl_char_len = 1;
            }
            else if (APP_CONSOLE_BACKSPACE == *ptr)
            {
                if (str_index >= 1)
                {
                    ptr = ptr - 1;
                    str_index = str_index - 1;
                    PRINTF("\b \b");
                }
            }

            else if ((*ptr >= ' ') && (*ptr <= '~'))
            {
                PRINTF("%c", *ptr);
                ptr++;
                str_index++;
                if (str_index >= MAX_COMMAND_STR_LENGTH - 1)
                {
                    *ptr = '\0';
                    break;
                }
            }
        }
        PRINTF("input str: %s\r\n", str_buf);
        if (str_buf[0] != '\0')
            console_run_command(str_buf);
    }
}
