/*
 * Copyright 2018 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"
#include "queue.h"

#include "fsl_debug_console.h"
#include "fsl_camera_receiver.h"
#include "fsl_isi_camera_adapter.h"
#include "fsl_camera_device.h"
#include "fsl_dpu.h"
#include "fsl_irqsteer.h"
#include "pin_mux.h"
#include "fsl_intmux.h"

#include "board.h"
#include "isi_config.h"
#include "isi_example.h"
#include "app_srtm.h"

#include "automotive.h"
#include "app_display.h"

#include "nxp_logo.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define APP_CAMERA_BUFFER_COUNT 3
#define APP_GRAPH_BUFFER_COUNT 2

#define APP_MAKE_COLOR(red, green, blue) ((((uint16_t)(red)) << 11U) | (((uint16_t)(green)) << 5U) | ((uint16_t)(blue)))
#define APP_COLOR_BLUE APP_MAKE_COLOR(0, 0, 0x1F)
#define APP_COLOR_GREEN APP_MAKE_COLOR(0, 0x3F, 0)
#define APP_COLOR_RED APP_MAKE_COLOR(0x1F, 0, 0)
#define APP_COLOR_YELLOW APP_MAKE_COLOR(0x1F, 0x3F, 0)
#define APP_COLOR_CYAN APP_MAKE_COLOR(0, 0x3F, 0x1F)
#define APP_COLOR_MAGENTA APP_MAKE_COLOR(0x1F, 0, 0x1F)
#define APP_COLOR_BLACK APP_MAKE_COLOR(0, 0, 0)
#define APP_COLOR_WHITE APP_MAKE_COLOR(0x1F, 0x3F, 0x1F)
#define APP_COLOR_SILVER APP_MAKE_COLOR(0x18, 0x30, 0x18)
#define APP_COLOR_GRAY APP_MAKE_COLOR(0x10, 0x20, 0x10)

#define APP_MAKE_COLOR32(alpha, red, green, blue) \
    ((((uint32_t)(alpha)) << 24U) | (((uint32_t)(red)) << 16U) | (((uint32_t)(green)) << 8U) | ((uint32_t)(blue)))
#define APP_COLOR_RED32 APP_MAKE_COLOR32(0xFF, 0xFF, 0, 0)
#define APP_COLOR_GREEN32 APP_MAKE_COLOR32(0xFF, 0, 0xFF, 0)
#define APP_COLOR_YELLOW32 APP_MAKE_COLOR32(0xFF, 0xFF, 0xFF, 0)

#define APP_PEN_WIDTH (8U)

#define APP_INVALID_PARTITION (0xFF)

/*******************************************************************************
 * Variables
 ******************************************************************************/
static volatile uint32_t s_percentage;
static volatile bool s_showLoadProgress;
static volatile bool s_displayInit;
static volatile bool s_cameraInUse;
static TimerHandle_t s_timer;
static uint32_t s_graphIndex;
static sc_rm_pt_t android_pt = APP_INVALID_PARTITION;

/* RGB565 */
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_graphBuffer[APP_GRAPH_BUFFER_COUNT][SDK_SIZEALIGN(
                                  APP_CAMERA_HEIGHT * APP_CAMERA_WIDTH * 2, APP_FB_ALIGN_BYTE)],
                              APP_FB_ALIGN_BYTE);
/* RGB565 */
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t s_cameraBuffer[APP_CAMERA_BUFFER_COUNT][SDK_SIZEALIGN(
                                  APP_CAMERA_HEIGHT * APP_CAMERA_WIDTH * 2, APP_FB_ALIGN_BYTE)],
                              APP_FB_ALIGN_BYTE);
/* ARGB8888 */
AT_NONCACHEABLE_SECTION_ALIGN(
    static uint8_t s_overlayBuffer[SDK_SIZEALIGN(APP_FRAME_HEIGHT * APP_FRAME_WIDTH * 4, APP_FB_ALIGN_BYTE)],
    APP_FB_ALIGN_BYTE);

extern camera_device_handle_t cameraDevice;
extern camera_receiver_handle_t cameraReceiver;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void APP_TimerCallback(TimerHandle_t timer);
static bool APP_AutoRegisterHandler(uint32_t clientId,
                                    bool registered,
                                    void *param,
                                    uint8_t *reqData,
                                    uint32_t reqLen,
                                    uint8_t *respData,
                                    uint32_t respLen);

/*******************************************************************************
 * Code
 ******************************************************************************/
void BOARD_InitHardware(void)
{
    sc_ipc_t ipc;

    ipc = BOARD_InitRpc();
    BOARD_InitPins(ipc);
    BOARD_BootClockRUN();
    BOARD_InitMemory();
    BOARD_InitDebugConsole();

    /* Power up the MU used for RPMSG */
    if (sc_pm_set_resource_power_mode(ipc, SC_R_MU_6B, SC_PM_PW_MODE_ON) != SC_ERR_NONE)
    {
        PRINTF("Error: Failed to power on MU6B\r\n");
    }
    if (sc_pm_set_resource_power_mode(ipc, SC_R_MU_7B, SC_PM_PW_MODE_ON) != SC_ERR_NONE)
    {
        PRINTF("Error: Failed to power on MU7B\r\n");
    }
    if (sc_pm_set_resource_power_mode(ipc, SC_R_MU_12B, SC_PM_PW_MODE_ON) != SC_ERR_NONE)
    {
        PRINTF("Error: Failed to power on MU12B\r\n");
    }

    if (sc_pm_set_resource_power_mode(ipc, SC_R_M4_1_INTMUX, SC_PM_PW_MODE_ON) != SC_ERR_NONE)
    {
        PRINTF("Error: Failed to power on M4_1_INTMUX\r\n");
    }
    if (sc_pm_set_resource_power_mode(ipc, SC_R_IRQSTR_M4_1, SC_PM_PW_MODE_ON) != SC_ERR_NONE)
    {
        PRINTF("Error: Failed to power on IRQSTEER!\r\n");
    }

    /* Power the I2C module */
    if (sc_pm_set_resource_power_mode(ipc, SC_R_M4_1_I2C, SC_PM_PW_MODE_ON) != SC_ERR_NONE)
    {
        PRINTF("Error: Failed to enable lpi2c");
    }
    /* Set LPI2C clock */
    if (CLOCK_SetIpFreq(kCLOCK_M4_1_Lpi2c, SC_133MHZ) == 0)
    {
        PRINTF("Error: Failed to set LPI2C frequency\r\n");
    }

    INTMUX_Init(CM4_1__INTMUX);
    IRQSTEER_Init(IRQSTEER);
    IRQSTEER_EnableInterrupt(IRQSTEER, BOARD_UART_IRQ);

    platform_scu_mu_init();
    SOC_BootupAPCore();
}

static void APP_DrawLogo(uint16_t *buf, uint32_t x, uint32_t y)
{
    uint32_t i, j;
    uint16_t *pix = (uint16_t *)NXP_logo;

    for (i = 0; i < NXP_LOGO_HEIGHT; i++)
    {
        for (j = 0; j < NXP_LOGO_WIDTH; j++)
        {
            *(buf + (y + i) * APP_CAMERA_WIDTH + x + j) = *pix++;
        }
    }
}

static void APP_DrawRectangle(uint16_t *buf, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t color)
{
    uint32_t i, j;

    for (i = 0; i < h; i++)
    {
        for (j = 0; j < w; j++)
        {
            *(buf + (y + i) * APP_CAMERA_WIDTH + x + j) = color;
        }
    }
}

static void APP_DrawLoadingFrame(uint32_t index, uint32_t percent)
{
    uint32_t width = APP_CAMERA_WIDTH * 3 / 4;

    assert(percent <= 100);

    if (s_showLoadProgress)
    {
        APP_DrawLogo((uint16_t *)s_graphBuffer[index], APP_CAMERA_WIDTH / 8, APP_CAMERA_HEIGHT / 8);
        APP_DrawRectangle((uint16_t *)s_graphBuffer[index], APP_CAMERA_WIDTH / 8, APP_CAMERA_HEIGHT / 2, width, 64,
                          APP_COLOR_WHITE);

        APP_DrawRectangle((uint16_t *)s_graphBuffer[index], APP_CAMERA_WIDTH / 8 + 8, APP_CAMERA_HEIGHT / 2 + 8,
                          (width - 16) * percent / 100, 48, APP_COLOR_BLUE);
    }
    else
    {
        /* Fill black */
        APP_DrawRectangle((uint16_t *)s_graphBuffer[index], APP_CAMERA_WIDTH / 8, APP_CAMERA_HEIGHT / 2, width, 64,
                          APP_COLOR_BLACK);
    }
}

static void APP_DrawLineX(int32_t sx, int32_t sy, int32_t ex, int32_t ey, uint32_t color)
{
    int32_t dx, dy;
    int32_t stepY;
    int32_t x, y;
    int32_t delta;
    uint32_t *buf = (uint32_t *)s_overlayBuffer;

    dx = ex - sx;
    dy = ey - sy;

    if (dy >= 0)
    {
        stepY = 1;
    }
    else
    {
        stepY = -1;
        dy    = -dy; /* delta need to be positive */
    }

    y     = sy;
    delta = 2 * dy - dx;

    for (x = sx; x <= ex; x++)
    {
        if (x >= 0 && x < APP_FRAME_WIDTH && y >= 0 && y < APP_FRAME_HEIGHT)
        {
            buf[y * APP_FRAME_WIDTH + x] = color;
        }
        if (delta > 0)
        {
            y += stepY;
            delta = delta - 2 * dx;
        }
        delta += 2 * dy;
    }
}

static void APP_DrawLineY(int32_t sx, int32_t sy, int32_t ex, int32_t ey, uint32_t color)
{
    int32_t dx, dy;
    int32_t stepX;
    int32_t x, y;
    int32_t delta;
    uint32_t *buf = (uint32_t *)s_overlayBuffer;

    dx = ex - sx;
    dy = ey - sy;

    if (dx >= 0)
    {
        stepX = 1;
    }
    else
    {
        stepX = -1;
        dx    = -dx; /* delta need to be positive */
    }

    x     = sx;
    delta = 2 * dx - dy;

    for (y = sy; y <= ey; y++)
    {
        if (x >= 0 && x < APP_FRAME_WIDTH && y >= 0 && y < APP_FRAME_HEIGHT)
        {
            buf[y * APP_FRAME_WIDTH + x] = color;
        }
        if (delta > 0)
        {
            x += stepX;
            delta = delta - 2 * dy;
        }
        delta += 2 * dx;
    }
}

static void APP_DrawLine(int32_t startX, int32_t startY, int32_t endX, int32_t endY, uint32_t penWidth, uint32_t color)
{
    int32_t dx, dy;
    /* Parameter x, y is permillage value on the screen, need to convert to absolute coordinates */
    int32_t sx = startX * APP_FRAME_WIDTH / 1000;
    int32_t sy = startY * APP_FRAME_HEIGHT / 1000;
    int32_t ex = endX * APP_FRAME_WIDTH / 1000;
    int32_t ey = endY * APP_FRAME_HEIGHT / 1000;
    uint32_t i;

    dx = sx > ex ? sx - ex : ex - sx;
    dy = sy > ey ? sy - ey : ey - sy;
    if (dy < dx)
    {
        if (sx > ex)
        {
            for (i = 0; i < penWidth; i++)
            {
                APP_DrawLineX(ex, ey + i - penWidth / 2, sx, sy + i - penWidth / 2, color);
            }
        }
        else
        {
            for (i = 0; i < penWidth; i++)
            {
                APP_DrawLineX(sx, sy + i - penWidth / 2, ex, ey + i - penWidth / 2, color);
            }
        }
    }
    else
    {
        if (sy > ey)
        {
            for (i = 0; i < penWidth; i++)
            {
                APP_DrawLineY(ex + i - penWidth / 2, ey, sx + i - penWidth / 2, sy, color);
            }
        }
        else
        {
            for (i = 0; i < penWidth; i++)
            {
                APP_DrawLineY(sx + i - penWidth / 2, sy, ex + i - penWidth / 2, ey, color);
            }
        }
    }
}

static void APP_DrawReverseFrame(void)
{
    /* x, y is permillage value */
    APP_DrawLine(0, 1000, 83, 667, APP_PEN_WIDTH, APP_COLOR_RED32);
    APP_DrawLine(1000, 1000, 917, 667, APP_PEN_WIDTH, APP_COLOR_RED32);
    APP_DrawLine(42, 833, 125, 833, APP_PEN_WIDTH, APP_COLOR_RED32);
    APP_DrawLine(958, 833, 875, 833, APP_PEN_WIDTH, APP_COLOR_RED32);
    APP_DrawLine(83, 667, 167, 333, APP_PEN_WIDTH, APP_COLOR_YELLOW32);
    APP_DrawLine(917, 667, 833, 333, APP_PEN_WIDTH, APP_COLOR_YELLOW32);
    APP_DrawLine(125, 500, 210, 500, APP_PEN_WIDTH, APP_COLOR_YELLOW32);
    APP_DrawLine(875, 500, 790, 500, APP_PEN_WIDTH, APP_COLOR_YELLOW32);
    APP_DrawLine(167, 333, 250, 0, APP_PEN_WIDTH, APP_COLOR_GREEN32);
    APP_DrawLine(833, 333, 750, 0, APP_PEN_WIDTH, APP_COLOR_GREEN32);
    APP_DrawLine(210, 167, 293, 167, APP_PEN_WIDTH, APP_COLOR_GREEN32);
    APP_DrawLine(790, 167, 707, 167, APP_PEN_WIDTH, APP_COLOR_GREEN32);
}

/*
 * The callback function is called when the display controller updated the use
 * the new frame buffer. The previous active frame buffer address is returned
 * so that it could be submited to the camera buffer queue.
 */
static void APP_DisplayFrameDoneCallback(uint32_t frameBuffer)
{
    if (s_cameraInUse && frameBuffer > (uint32_t)s_cameraBuffer[0] &&
        frameBuffer < (uint32_t)s_cameraBuffer[APP_CAMERA_BUFFER_COUNT])
    {
        CAMERA_RECEIVER_SubmitEmptyBuffer(&cameraReceiver, frameBuffer);
    }
}

static void APP_TimerCallback(TimerHandle_t timer)
{
    if (++s_percentage == 100)
    {
        s_percentage = 0;
    }

    if (s_displayInit && !s_cameraInUse)
    {
        APP_DrawLoadingFrame(s_graphIndex, s_percentage);
        if (!APP_IsDisplayFramePending())
        {
            APP_SetDisplayFrameBuffer((uint32_t)s_graphBuffer[s_graphIndex]);
            s_graphIndex = (s_graphIndex + 1) % APP_GRAPH_BUFFER_COUNT;
        }
    }
}

static void APP_CameraFrameDoneCallback(camera_receiver_handle_t *handle, status_t status, void *userData)
{
    uint32_t fullCameraBufferAddr;
    status_t st;

    if (status == kStatus_Success)
    {
        st = CAMERA_RECEIVER_GetFullBuffer(handle, &fullCameraBufferAddr);
        if (st != kStatus_Success)
        {
            assert(false);
        }

        if (APP_IsDisplayFramePending() || !s_cameraInUse)
        {
            /* Last captured picture is not displayed, discard this one */
            CAMERA_RECEIVER_SubmitEmptyBuffer(handle, fullCameraBufferAddr);
        }
        else
        {
            /* Pass the full frame buffer to display controller to show. */
            APP_SetDisplayFrameBuffer(fullCameraBufferAddr);
        }
    }
}

static void APP_AutoPassResource(void)
{
    if (android_pt != APP_INVALID_PARTITION)
    {
        SOC_AssignDisplayCamera(android_pt);
    }
}

static bool APP_AutoRegisterHandler(uint32_t clientId,
                                    bool registered,
                                    void *param,
                                    uint8_t *reqData,
                                    uint32_t reqLen,
                                    uint8_t *respData,
                                    uint32_t respLen)
{
    if (registered)
    {
        assert(reqData && reqLen >= 1);
        android_pt = reqData[0];
    }
    else
    {
        android_pt = APP_INVALID_PARTITION; /* Invalid android partition */
    }

    return true;
}

static void APP_InitCamera(void)
{
    status_t status;
    const camera_config_t cameraConfig = {
        .pixelFormat                = kVIDEO_PixelFormatYUYV,
        .bytesPerPixel              = 2,
        .resolution                 = FSL_VIDEO_RESOLUTION(APP_CAMERA_WIDTH, APP_CAMERA_HEIGHT),
        .frameBufferLinePitch_Bytes = APP_CAMERA_WIDTH * 2,
#if (ISI_EXAMPLE_CI == ISI_MIPI_CSI2)
        .interface = kCAMERA_InterfaceMIPI,
#elif (APP_CI_PI_MODE == CI_PI_MODE_GATE_CLOCK)
        .interface   = kCAMERA_InterfaceGatedClock,
#elif (APP_CI_PI_MODE == CI_PI_MODE_NON_GATE_CLOCK)
        .interface = kCAMERA_InterfaceNonGatedClock,
#else
        .interface = kCAMERA_InterfaceCCIR656,
#endif
        .controlFlags = APP_CAMERA_CONTROL_FLAGS,
        .framePerSec  = APP_CAMERA_FRAME_RATE,
#if (ISI_EXAMPLE_CI == ISI_MIPI_CSI2)
        .mipiChannel = APP_MIPI_CSI_VC,
        .csiLanes    = APP_MIPI_CSI_LANES,
#else
        .mipiChannel = 0,
        .csiLanes    = 0,
#endif
    };

    /* The camera input pixel format is YUV422, the ISI could convert to RGB565. */
    isi_ext_config_t isiExtConfig = {
        .outputBytesPerPixel   = 2,
        .outputPixelFormat     = kVIDEO_PixelFormatRGB565,
        .outputFrameResolution = FSL_VIDEO_RESOLUTION(APP_CAMERA_WIDTH, APP_CAMERA_HEIGHT),
        .flags                 = kCAMERA_ISI_FlipHorizontal,
    };
    status = CAMERA_RECEIVER_InitExt(&cameraReceiver, &cameraConfig, &isiExtConfig, APP_CameraFrameDoneCallback, NULL);

    if (kStatus_Success != status)
    {
        PRINTF("ISI initialize failed.\r\n");
        assert(false);
        return;
    }

    /*
     * Submit the empty buffer to camera buffer queue.
     * The first buffer is used by the display. So submit from the second buffer.
     */
    for (uint32_t i = 0; i < APP_CAMERA_BUFFER_COUNT; i++)
    {
        CAMERA_RECEIVER_SubmitEmptyBuffer(&cameraReceiver, (uint32_t)(s_cameraBuffer[i]));
    }

    CAMERA_RECEIVER_Start(&cameraReceiver);

    APP_InitCameraInterface();

    status = CAMERA_DEVICE_Init(&cameraDevice, &cameraConfig);

    if (kStatus_Success != status)
    {
        PRINTF("Camera initialize failed.\r\n");
        assert(false);
        return;
    }
}

static void APP_StartCamera(void)
{
    APP_PrepareCamera();
    APP_SetIsiPermission((uint32_t)s_cameraBuffer, (((uint32_t)(s_cameraBuffer)) + sizeof(s_cameraBuffer) - 1U));
    APP_InitCamera();
    CAMERA_DEVICE_Start(&cameraDevice);
}

static void APP_StopCamera(void)
{
    CAMERA_RECEIVER_Stop(&cameraReceiver);
    CAMERA_RECEIVER_Deinit(&cameraReceiver);

    APP_DeinitCameraInterface();

    CAMERA_DEVICE_Stop(&cameraDevice);
    CAMERA_DEVICE_Deinit(&cameraDevice);

    APP_UnsetIsiPermission();
    APP_UnprepareCamera();
}

void rear_view_camera_task(void *pvParameters)
{
    EventBits_t eventBits;
    s_percentage = 0;
    s_showLoadProgress = 0;
    s_displayInit = 0;
    s_cameraInUse = 0;

    APP_SRTM_SetAutoRegisterHandler(APP_AutoRegisterHandler, NULL);

    s_timer = xTimerCreate("Periodic Timer", APP_MS2TICK(200), pdTRUE, NULL, APP_TimerCallback);// update loadProgress bar
    assert(s_timer);

    while (true)
    {
        eventBits = xEventGroupWaitBits(g_xAutoUiEvent, AUTO_UI_EVENT_BITS_ALL, pdTRUE, pdFALSE, portMAX_DELAY);
	PRINTF("rear_view_camera_task get event: 0x%x!\r\n", eventBits);
        if (eventBits & AUTO_UI_EVENT_BIT_STARTUP) /* switch to M4 UI */
        {
            if (!s_displayInit)
            {
                xEventGroupSetBits(g_xUIResourceBits, UI_RESOURCE_BIT_DISPLAY);
                xEventGroupClearBits(g_xUIResourceBits, UI_RESOURCE_BIT_NONE);
                APP_InitDisplay((uint32_t)s_graphBuffer[s_graphIndex], (uint32_t)s_overlayBuffer, APP_DisplayFrameDoneCallback);
                s_displayInit = true;
            }
        }

        if (eventBits & AUTO_UI_EVENT_BIT_CAMERA)
        {
            assert(s_displayInit);
            if (s_showLoadProgress)
            {
                xTimerStop(s_timer, portMAX_DELAY);
                s_showLoadProgress = false;
            }
            if (!s_cameraInUse)
            {
                xEventGroupSetBits(g_xUIResourceBits, UI_RESOURCE_BIT_CAMERA);
                APP_StartCamera();
                APP_DrawReverseFrame();
                s_cameraInUse = true;
            }
        }
        else if (eventBits & AUTO_UI_EVENT_BITS_NORMAL)
        {
            assert(s_displayInit);

            if (s_cameraInUse)
            {
                s_cameraInUse = false;
                APP_StopCamera();
                /* Clean overlay */
                memset(s_overlayBuffer, 0, sizeof(s_overlayBuffer));
                xEventGroupClearBits(g_xUIResourceBits, UI_RESOURCE_BIT_CAMERA);
            }

            if (eventBits & AUTO_UI_EVENT_BIT_HVAC)
            {
                if (s_showLoadProgress)
                {
                    xTimerStop(s_timer, portMAX_DELAY);
                    s_showLoadProgress = false;
                }
            }
            else if (eventBits & AUTO_UI_EVENT_BIT_PROGRESS)
            {
                /* display progress bar again. */
                s_showLoadProgress = true;
                xTimerStart(s_timer, portMAX_DELAY);
                APP_DrawLoadingFrame(s_graphIndex, s_percentage);
                APP_SetDisplayFrameBuffer((uint32_t)s_graphBuffer[s_graphIndex]);
                s_graphIndex = (s_graphIndex + 1) % APP_GRAPH_BUFFER_COUNT;
            }
        }

        if (eventBits & AUTO_UI_EVENT_BIT_STOP) /* switch to android UI, stop all display related */
        {
            if (s_cameraInUse)
            {
                s_cameraInUse = false;
                APP_StopCamera();
                memset(s_overlayBuffer, 0, sizeof(s_overlayBuffer));
                xEventGroupClearBits(g_xUIResourceBits, UI_RESOURCE_BIT_CAMERA);
            }
            if (s_showLoadProgress)
            {
                xTimerStop(s_timer, portMAX_DELAY);
                s_showLoadProgress = false;
            }
            if (s_displayInit)
            {
                s_displayInit = false;
                APP_DeinitDisplay();
                xEventGroupClearBits(g_xUIResourceBits, UI_RESOURCE_BIT_DISPLAY);
            }

            APP_AutoPassResource();
            xEventGroupSetBits(g_xUIResourceBits, UI_RESOURCE_BIT_NONE);
        }
    }
}

void vApplicationMallocFailedHook(void)
{
    PRINTF("Malloc Failed!!!\r\n");
}
