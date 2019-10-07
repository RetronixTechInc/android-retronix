/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2018 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "fsl_debug_console.h"
#include "fsl_lpi2c_freertos.h"

#include "srtm_dispatcher.h"
#include "srtm_peercore.h"
#include "srtm_message.h"
#include "srtm_auto_service.h"
#include "srtm_rpmsg_endpoint.h"
#include "srtm_mu_endpoint.h"
#include "srtm_i2c_service.h"

#include "app_srtm.h"
#include "automotive.h"
#include "board.h"
#include "fsl_mu.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
typedef enum
{
    APP_SRTM_StateRun = 0x0U,
    APP_SRTM_StateLinkedUp,
    APP_SRTM_StateReboot,
    APP_SRTM_StateShutdown,
} app_srtm_state_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static srtm_status_t APP_SRTM_I2C_Read(srtm_i2c_adapter_t adapter,
                                       uint32_t base_addr,
                                       srtm_i2c_type_t type,
                                       uint16_t slaveAddr,
                                       uint8_t *buf,
                                       uint8_t len,
                                       uint16_t flags);

static srtm_status_t APP_SRTM_I2C_Write(srtm_i2c_adapter_t adapter,
                                        uint32_t base_addr,
                                        srtm_i2c_type_t type,
                                        uint16_t slaveAddr,
                                        uint8_t *buf,
                                        uint8_t len,
                                        uint16_t flags);

static srtm_status_t APP_SRTM_I2C_SwitchChannel(srtm_i2c_adapter_t adapter,
                                                uint32_t base_addr,
                                                srtm_i2c_type_t type,
                                                uint16_t slaveAddr,
                                                srtm_i2c_switch_channel channel);
static srtm_status_t APP_SRTM_Auto_RegisterEvent(srtm_auto_adapter_t adapter,
                                                 uint32_t clientId,
                                                 uint8_t *reqData,
                                                 uint32_t reqLen,
                                                 uint8_t *respData,
                                                 uint32_t respLen);
static srtm_status_t APP_SRTM_Auto_UnregisterEvent(srtm_auto_adapter_t adapter,
                                                   uint32_t clientId,
                                                   uint8_t *reqData,
                                                   uint32_t reqLen,
                                                   uint8_t *respData,
                                                   uint32_t respLen);
static srtm_status_t APP_SRTM_Auto_Control(srtm_auto_adapter_t adapter,
                                           uint32_t clientId,
                                           uint8_t *reqData,
                                           uint32_t reqLen,
                                           uint8_t *respData,
                                           uint32_t respLen);
static srtm_status_t APP_SRTM_Auto_PowerReport(srtm_auto_adapter_t adapter,
                                               uint32_t clientId,
                                               uint8_t *reqData,
                                               uint32_t reqLen,
                                               uint8_t *respData,
                                               uint32_t respLen);
static srtm_status_t APP_SRTM_Auto_GetInfo(srtm_auto_adapter_t adapter,
                                           uint32_t clientId,
                                           uint8_t *reqData,
                                           uint32_t reqLen,
                                           uint8_t *respData,
                                           uint32_t respLen);
static void APP_SRTM_ResetServices(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/
static srtm_dispatcher_t disp;
static srtm_peercore_t core;
static srtm_service_t autoService;
static srtm_service_t i2cService;
static SemaphoreHandle_t monSig;
static volatile app_srtm_state_t srtmState;
static struct rpmsg_lite_instance *rpmsgHandle;
static app_rpmsg_monitor_t rpmsgMonitor;
static void *rpmsgMonitorParam;
static TimerHandle_t linkupTimer;

static struct _srtm_auto_adapter autoAdapter = {.registerEvent   = APP_SRTM_Auto_RegisterEvent,
                                                .unregisterEvent = APP_SRTM_Auto_UnregisterEvent,
                                                .control         = APP_SRTM_Auto_Control,
                                                .powerReport     = APP_SRTM_Auto_PowerReport,
                                                .getInfo         = APP_SRTM_Auto_GetInfo};

static struct _i2c_bus qx_i2c_buses[] = {{.bus_id         = 1,
                                          .base_addr      = ADMA__LPI2C1_BASE,
                                          .type           = SRTM_I2C_TYPE_LPI2C,
                                          .switch_idx     = I2C_SWITCH_NONE,
                                          .switch_channel = SRTM_I2C_SWITCH_CHANNEL_UNSPECIFIED},
                                         {.bus_id         = 5,
                                          .base_addr      = CM4__LPI2C_BASE,
                                          .type           = SRTM_I2C_TYPE_LPI2C,
                                          .switch_idx     = I2C_SWITCH_NONE,
                                          .switch_channel = SRTM_I2C_SWITCH_CHANNEL_UNSPECIFIED},
                                         {.bus_id         = 12,
                                          .base_addr      = ADMA__LPI2C1_BASE,
                                          .type           = SRTM_I2C_TYPE_LPI2C,
                                          .switch_idx     = 0,
                                          .switch_channel = SRTM_I2C_SWITCH_CHANNEL0},
                                         {.bus_id         = 13,
                                          .base_addr      = ADMA__LPI2C1_BASE,
                                          .type           = SRTM_I2C_TYPE_LPI2C,
                                          .switch_idx     = 0,
                                          .switch_channel = SRTM_I2C_SWITCH_CHANNEL1},
                                         {.bus_id         = 14,
                                          .base_addr      = ADMA__LPI2C1_BASE,
                                          .type           = SRTM_I2C_TYPE_LPI2C,
                                          .switch_idx     = 0,
                                          .switch_channel = SRTM_I2C_SWITCH_CHANNEL2},
                                         {.bus_id         = 15,
                                          .base_addr      = ADMA__LPI2C1_BASE,
                                          .type           = SRTM_I2C_TYPE_LPI2C,
                                          .switch_idx     = 0,
                                          .switch_channel = SRTM_I2C_SWITCH_CHANNEL3}};

static struct _i2c_switch qx_i2c_switchs[] = {
    {.slaveAddr = EXAMPLE_I2C_SWITCH_ADDR, .cur_channel = SRTM_I2C_SWITCH_CHANNEL_UNSPECIFIED}};

static struct _srtm_i2c_adapter qx_i2c_adapter = {.read          = APP_SRTM_I2C_Read,
                                                  .write         = APP_SRTM_I2C_Write,
                                                  .switchchannel = APP_SRTM_I2C_SwitchChannel,
                                                  .bus_structure = {
                                                      .buses      = qx_i2c_buses,
                                                      .bus_num    = sizeof(qx_i2c_buses) / sizeof(struct _i2c_bus),
                                                      .switches   = qx_i2c_switchs,
                                                      .switch_num = sizeof(qx_i2c_switchs) / sizeof(struct _i2c_switch),
                                                  }};

static app_auto_register_handler_t appRegHandler;
static void *appRegParam;
/*******************************************************************************
 * Code
 ******************************************************************************/
static void APP_SRTM_SetRemoteReady(uint32_t ready)
{
    MU_SetFlagsNonBlocking(APP_KERNEL_MU, ready);
}

static void APP_SRTM_PollLinkup(srtm_dispatcher_t dispatcher, void *param1, void *param2)
{
    if (srtmState == APP_SRTM_StateRun)
    {
        if (rpmsg_lite_is_link_up(rpmsgHandle))
        {
            srtmState = APP_SRTM_StateLinkedUp;
            /*
             * A core has sent the kick, deassert the handshake
             */
            APP_SRTM_SetRemoteReady(0);
            xSemaphoreGive(monSig);
        }
        else
        {
            /* Start timer to poll linkup status. */
            xTimerStart(linkupTimer, portMAX_DELAY);
        }
    }
}

static void APP_LinkupTimerCallback(TimerHandle_t xTimer)
{
    srtm_procedure_t proc = SRTM_Procedure_Create(APP_SRTM_PollLinkup, NULL, NULL);

    if (proc)
    {
        SRTM_Dispatcher_PostProc(disp, proc);
    }
}

static void APP_SRTM_NotifyPeerCoreReady(struct rpmsg_lite_instance *rpmsgHandle, bool ready)
{
    if (rpmsgMonitor)
    {
        rpmsgMonitor(rpmsgHandle, ready, rpmsgMonitorParam);
    }
}

static void APP_SRTM_Linkup(void)
{
    srtm_channel_t chan;
    srtm_rpmsg_endpoint_config_t rpmsgConfig;

    APP_SRTM_NotifyPeerCoreReady(rpmsgHandle, true);

    /* Common RPMsg channel config */
    rpmsgConfig.localAddr   = RL_ADDR_ANY;
    rpmsgConfig.peerAddr    = RL_ADDR_ANY;
    rpmsgConfig.rpmsgHandle = rpmsgHandle;

    /* Create and add SRTM AUTO channel to peer core */
    rpmsgConfig.epName = APP_SRTM_AUTO_CHANNEL_NAME;
    chan               = SRTM_RPMsgEndpoint_Create(&rpmsgConfig);
    SRTM_PeerCore_AddChannel(core, chan);

    /* Create and add SRTM I2C channel to peer core*/
    rpmsgConfig.epName = APP_SRTM_I2C_CHANNEL_NAME;
    chan               = SRTM_RPMsgEndpoint_Create(&rpmsgConfig);
    SRTM_PeerCore_AddChannel(core, chan);

    PRINTF("app_srtm: AUTO and I2C service registered\r\n");
    SRTM_Dispatcher_AddPeerCore(disp, core);
}

static void APP_SRTM_InitPeerCore(void)
{
    srtm_channel_t chan;
    srtm_mu_endpoint_config_t muConfig;

    rpmsgHandle = rpmsg_lite_remote_init((void *)RPMSG_LITE_SRTM_SHMEM_BASE, RPMSG_LITE_SRTM_LINK_ID, RL_NO_FLAGS);
    assert(rpmsgHandle);

    /*
     * SRTM will inform peer core that it is ready to receive the first kick
     */
    APP_SRTM_SetRemoteReady(1);

    if (rpmsg_lite_is_link_up(rpmsgHandle))
    {
        /* If resume context has already linked up, don't need to announce channel again. */
        APP_SRTM_Linkup();
    }
    else
    {
        /* Start timer to poll linkup status. */
        xTimerStart(linkupTimer, portMAX_DELAY);
    }

    /* Create SRTM peer core */
    core = SRTM_PeerCore_Create(PEER_CORE_ID);

    /* Set peer core state to activated */
    SRTM_PeerCore_SetState(core, SRTM_PeerCore_State_Activated);

    muConfig.mu          = APP_UBOOT_MU;
    muConfig.mu_nvic_irq = APP_UBOOT_MU_IRQ;
    chan                 = SRTM_MUEndpoint_Create(&muConfig);
    SRTM_PeerCore_AddChannel(core, chan);

    SRTM_Dispatcher_AddPeerCore(disp, core);
}

static void APP_SRTM_ResetServices(void)
{
    SRTM_I2CService_Reset(i2cService, core);
    SRTM_AutoService_Reset(autoService, core);
}

static void APP_SRTM_DeinitPeerCore(void)
{
    /* Stop linkupTimer if it's started. */
    xTimerStop(linkupTimer, portMAX_DELAY);

    if (core)
    {
        /* Notify application for the peer core disconnection. */
        APP_SRTM_NotifyPeerCoreReady(rpmsgHandle, false);
        /* Need to let services know peer core is now down. */
        APP_SRTM_ResetServices();

        SRTM_Dispatcher_RemovePeerCore(disp, core);
        SRTM_PeerCore_Destroy(core);
        core = NULL;
    }

    if (rpmsgHandle)
    {
        rpmsg_lite_deinit(rpmsgHandle);
        rpmsgHandle = NULL;
    }
}

static srtm_status_t APP_SRTM_Auto_RegisterEvent(srtm_auto_adapter_t adapter,
                                                 uint32_t clientId,
                                                 uint8_t *reqData,
                                                 uint32_t reqLen,
                                                 uint8_t *respData,
                                                 uint32_t respLen)
{
    srtm_status_t status;
    vehicle_state_t stateMsg;
    TaskHandle_t curTaskHandle;
    uint32_t ulNotifiedValue;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    if (appRegHandler)
        if (!appRegHandler(clientId, true, appRegParam, reqData, reqLen, respData, respLen))
            return SRTM_Status_Error;

    curTaskHandle = xTaskGetCurrentTaskHandle();
    stateMsg.type = STATE_TYPE_ANDROID;
    stateMsg.reg.enroll = 0x01;
    stateMsg.reg.handle = curTaskHandle;
    if (xQueueSend(xVStateQueue, (void *)&stateMsg, (TickType_t)0))
    {
	    /* wait for response(current resource state) from other task*/
        xTaskNotifyWait( 0x00, 0xffffffff, &ulNotifiedValue, (TickType_t)10);
        respData[0] = (uint8_t)ulNotifiedValue;
        respData[1] = 0;
        PRINTF("Receive register command, partition=%d, respState=%d!\r\n", reqData[0], respData[0]);
        status = SRTM_Status_Success;
    }
    else
    {
        PRINTF("Receive register command, send to process queue failed!\r\n");
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t APP_SRTM_Auto_UnregisterEvent(srtm_auto_adapter_t adapter,
                                                   uint32_t clientId,
                                                   uint8_t *reqData,
                                                   uint32_t reqLen,
                                                   uint8_t *respData,
                                                   uint32_t respLen)
{
    vehicle_state_t stateMsg;
    srtm_status_t status;
    TaskHandle_t curTaskHandle;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    if (appRegHandler)
        if (!appRegHandler(clientId, false, appRegParam, reqData, reqLen, respData, respLen))
            return SRTM_Status_Error;

    curTaskHandle = xTaskGetCurrentTaskHandle();
    stateMsg.type = STATE_TYPE_ANDROID;
    stateMsg.reg.enroll = 0x00;
    stateMsg.reg.handle = curTaskHandle;
    if (xQueueSend(xVStateQueue, (void *)&stateMsg, (TickType_t)0))
    {
        status = SRTM_Status_Success;
        PRINTF("Unregister the client: %x with reason: %d successfully\r\n", clientId, reqData[0]);
    }
    else
    {
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t APP_SRTM_Auto_Control(srtm_auto_adapter_t adapter,
                                           uint32_t clientId,
                                           uint8_t *reqData,
                                           uint32_t reqLen,
                                           uint8_t *respData,
                                           uint32_t respLen)
{
    int i;
    srtm_status_t status;
    control_unit_t ctrl;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    ctrl.type = (reqData[1] << 8) | reqData[0];
    ctrl.timeout = (reqData[5] << 24) | (reqData[4] << 16) | (reqData[3] << 8) | reqData[2];
    for (i=0; i<SRTM_USER_DATA_LENGTH - 6; i++)
        ctrl.value[i] = reqData[6+i];

    PRINTF("Receive control command, cmd=%x,to=%d,p[0]=%d,p[1]=%d,p[2]=%d,p[3]=%d,p[4]=%d\r\n",
           ctrl.type, ctrl.timeout, ctrl.value[0], ctrl.value[1],ctrl.value[2],
           ctrl.value[3],ctrl.value[4]);
    if (xQueueSend(xControlMsgQueue, (void *)&ctrl, (TickType_t)0))
    {
        status = SRTM_Status_Success;
    }
    else
    {
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t APP_SRTM_Auto_PowerReport(srtm_auto_adapter_t adapter,
                                               uint32_t clientId,
                                               uint8_t *reqData,
                                               uint32_t reqLen,
                                               uint8_t *respData,
                                               uint32_t respLen)
{
    srtm_status_t status;
    ap_power_state_t ap_report;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    ap_report.powerState = (reqData[1] << 8) | reqData[0];
    ap_report.time_postpone = (reqData[3] << 24) | (reqData[2] << 16) | (reqData[1] << 8) | reqData[0];

    PRINTF("Receive power report, state=%x,time_postpone=%d\r\n", ap_report.powerState, ap_report.time_postpone);
    if (xQueueSend(xAPPowerQueue, (void *)&ap_report, (TickType_t)0))
    {
        status = SRTM_Status_Success;
        PRINTF("AP power report: %d\r\n", ap_report.powerState);
    }
    else
    {
        status = SRTM_Status_Error;
    }

    return status;
}

static srtm_status_t APP_SRTM_Auto_GetInfo(srtm_auto_adapter_t adapter,
                                           uint32_t clientId,
                                           uint8_t *reqData,
                                           uint32_t reqLen,
                                           uint8_t *respData,
                                           uint32_t respLen)
{
    uint16_t infoIndex;

    if (ANDROID_SRTM_CLIENT_ID != clientId)
        return SRTM_Status_InvalidParameter;

    infoIndex = (reqData[1] << 8) | reqData[0];
    PRINTF("Receive get info, idx=%d\r\n", infoIndex);
    /* TODO: search the infoIndex data and fill the respData */
    if (infoIndex == VEHICLE_UNIQUE_ID)
    {
        respData[0] = reqData[0];
        respData[1] = reqData[1];
        respData[2] = 0x12;
        respData[3] = 0x34;
        respData[4] = 0x56;
        respData[5] = 0x78;
        respData[6] = 0xab;
        respData[7] = 0xcd;
        respData[8] = 0xef;
        respData[9] = 0xff;
    }

    return SRTM_Status_Success;
}

static void APP_SRTM_InitI2CDevice(void)
{
    lpi2c_master_config_t masterConfig;
    LPI2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Hz = EXAMPLE_IOEXP_LPI2C_BAUDRATE;
    LPI2C_MasterInit(EXAMPLE_IOEXP_LPI2C_MASTER, &masterConfig, I2C_SOURCE_CLOCK_FREQ_LPI2C1);

    LPI2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Hz = EXAMPLE_LPI2C_BAUDRATE;
    LPI2C_MasterInit(EXAMPLE_LPI2C, &masterConfig, I2C_SOURCE_CLOCK_FREQ_M4);
}

static void APP_SRTM_InitI2CService(void)
{
    APP_SRTM_InitI2CDevice();
    i2cService = SRTM_I2CService_Create(&qx_i2c_adapter);
    SRTM_Dispatcher_RegisterService(disp, i2cService);
}

static void APP_SRTM_InitAutoService(void)
{
    /* Create and register service */
    autoService = SRTM_AutoService_Create(&autoAdapter);
    SRTM_Dispatcher_RegisterService(disp, autoService);
}

static void APP_SRTM_InitServices(void)
{
    APP_SRTM_InitI2CService();
    APP_SRTM_InitAutoService();
}

static void SRTM_MonitorTask(void *pvParameters)
{
    app_srtm_state_t state = APP_SRTM_StateShutdown;

    /* Initialize services and add to dispatcher */
    APP_SRTM_InitServices();

    /* Start SRTM dispatcher */
    SRTM_Dispatcher_Start(disp);

    /* Monitor peer core state change */
    while (true)
    {
        xSemaphoreTake(monSig, portMAX_DELAY);

        if (state == srtmState)
        {
            continue;
        }

        switch (srtmState)
        {
            case APP_SRTM_StateRun:
                assert(state == APP_SRTM_StateShutdown);
                SRTM_Dispatcher_Stop(disp);
                APP_SRTM_InitPeerCore();
                SRTM_Dispatcher_Start(disp);
                state = APP_SRTM_StateRun;
                break;

            case APP_SRTM_StateLinkedUp:
                if (state == APP_SRTM_StateRun)
                {
                    SRTM_Dispatcher_Stop(disp);
                    APP_SRTM_Linkup();
                    SRTM_Dispatcher_Start(disp);
                }
                break;

            case APP_SRTM_StateReboot:
                assert(state == APP_SRTM_StateRun);

                PRINTF("Handle Peer Core Reboot\r\n");
                SRTM_Dispatcher_Stop(disp);
                /* Remove peer core from dispatcher */
                APP_SRTM_DeinitPeerCore();

                /* Restore srtmState to Run. */
                srtmState = APP_SRTM_StateRun;

                /* Initialize peer core and add to dispatcher */
                APP_SRTM_InitPeerCore();
                SRTM_Dispatcher_Start(disp);

                /* Do not need to change state. It's still Run. */
                break;

            default:
                assert(false);
                break;
        }
    }
}

static void SRTM_DispatcherTask(void *pvParameters)
{
    SRTM_Dispatcher_Run(disp);
}

static void APP_SRTM_InitPeriph(bool resume)
{
}

void APP_SRTM_Init(void)
{
    APP_SRTM_InitPeriph(false);

    monSig = xSemaphoreCreateBinary();
    assert(monSig);

    linkupTimer =
        xTimerCreate("Linkup", APP_MS2TICK(APP_LINKUP_TIMER_PERIOD_MS), pdFALSE, NULL, APP_LinkupTimerCallback);
    assert(linkupTimer);

    /* Create SRTM dispatcher */
    disp = SRTM_Dispatcher_Create();

    xTaskCreate(SRTM_MonitorTask, "SRTM monitor", 256U, NULL, APP_SRTM_MONITOR_TASK_PRIO, NULL);
    xTaskCreate(SRTM_DispatcherTask, "SRTM dispatcher", 512U, NULL, APP_SRTM_DISPATCHER_TASK_PRIO, NULL);
}

void APP_SRTM_StartCommunication(void)
{
    srtmState = APP_SRTM_StateRun;
    xSemaphoreGive(monSig);
}

void APP_SRTM_Reboot(void)
{
    srtmState = APP_SRTM_StateReboot;
    xSemaphoreGive(monSig);
}

void APP_SRTM_SetAutoRegisterHandler(app_auto_register_handler_t handler, void *param)
{
    appRegHandler = handler;
    appRegParam   = param;
}

bool APP_SRTM_SendAutoCommand(uint32_t clientId,
                              srtm_auto_cmd_t cmd,
                              uint8_t *cmdParam,
                              uint32_t paramLen,
                              uint8_t *result,
                              uint32_t resultLen,
                              uint32_t timeout)
{
    assert(autoService);

    PRINTF("Send command, cmd=%x,p[0]=%d,p[1]=%d,p[2]=%d,p[3]=%d,p[4]=%d,p[5]=%d\r\n", cmd,
           cmdParam[0],cmdParam[1],cmdParam[2],cmdParam[3],cmdParam[4],cmdParam[5]);
    if (SRTM_AutoService_SendCommand(autoService, clientId, cmd, cmdParam, paramLen, result, resultLen, timeout) ==
        SRTM_Status_Success)
    {
        return true;
    }

    return false;
}

void APP_SRTM_Suspend(void)
{
}

void APP_SRTM_Resume(bool resume)
{
    if (resume)
    {
    }
}

void APP_SRTM_SetRpmsgMonitor(app_rpmsg_monitor_t monitor, void *param)
{
    rpmsgMonitor      = monitor;
    rpmsgMonitorParam = param;
}

static srtm_status_t APP_SRTM_I2C_SwitchChannel(srtm_i2c_adapter_t adapter,
                                                uint32_t base_addr,
                                                srtm_i2c_type_t type,
                                                uint16_t slaveAddr,
                                                srtm_i2c_switch_channel channel)
{
    uint8_t txBuff[1];
    assert(channel < SRTM_I2C_SWITCH_CHANNEL_UNSPECIFIED);
    txBuff[0] = 1 << (uint8_t)channel;
    return adapter->write(adapter, base_addr, type, slaveAddr, txBuff, sizeof(txBuff),
                          SRTM_I2C_FLAG_NEED_STOP); // APP_SRTM_I2C_Write
}

static srtm_status_t APP_SRTM_I2C_Write(srtm_i2c_adapter_t adapter,
                                        uint32_t base_addr,
                                        srtm_i2c_type_t type,
                                        uint16_t slaveAddr,
                                        uint8_t *buf,
                                        uint8_t len,
                                        uint16_t flags)
{
    status_t retVal = kStatus_Fail;
    uint8_t needStop;

    needStop = (flags & SRTM_I2C_FLAG_NEED_STOP) ? 1 : 0;

    switch (type)
    {
        case SRTM_I2C_TYPE_LPI2C:
            retVal = BOARD_LPI2C_SendWithoutSubAddr((LPI2C_Type *)base_addr, slaveAddr, buf, len, needStop);
            break;
        default:
            break;
    }
    return (retVal == kStatus_Success) ? SRTM_Status_Success : SRTM_Status_TransferFailed;
}

static srtm_status_t APP_SRTM_I2C_Read(srtm_i2c_adapter_t adapter,
                                       uint32_t base_addr,
                                       srtm_i2c_type_t type,
                                       uint16_t slaveAddr,
                                       uint8_t *buf,
                                       uint8_t len,
                                       uint16_t flags)
{
    status_t retVal = kStatus_Fail;
    uint8_t needStop;

    needStop = (flags & SRTM_I2C_FLAG_NEED_STOP) ? 1 : 0;

    switch (type)
    {
        case SRTM_I2C_TYPE_LPI2C:
            retVal = BOARD_LPI2C_ReceiveWithoutSubAddr((LPI2C_Type *)base_addr, slaveAddr, buf, len, needStop);
            break;
        default:
            break;
    }
    return (retVal == kStatus_Success) ? SRTM_Status_Success : SRTM_Status_TransferFailed;
}

int LSIO_MU8_INT_B_IRQHandler(void)
{
    SRTM_MUEndpoint_Handler();

    return 0;
}

uint8_t APP_Read_I2C_Register(uint8_t busID, uint16_t slaveAddr, uint8_t regIndex)
{
    uint8_t value;
    SRTM_I2C_RequestBusWrite(i2cService, busID, slaveAddr, &regIndex, 1, 0);
    SRTM_I2C_RequestBusRead(i2cService, busID, slaveAddr, &value, 1);
    return value;
}

uint8_t APP_Write_I2C_Register(uint8_t busID, uint16_t slaveAddr, uint8_t regIndex, uint8_t value)
{
    uint8_t write_content[2];
    write_content[0] = regIndex;
    write_content[1] = value;
    SRTM_I2C_RequestBusWrite(i2cService, busID, slaveAddr, write_content, 2, 1);
    return value;
}

void APP_SRTM_HandlePeerReboot(void)
{
    if (srtmState != APP_SRTM_StateShutdown)
    {
        srtmState = APP_SRTM_StateReboot;
        xSemaphoreGive(monSig);
    }
}
