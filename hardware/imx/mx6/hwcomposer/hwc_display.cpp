/*
 * Copyright (C) 2009-2015 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>
#include <utils/threads.h>
#include <hardware/hwcomposer.h>
#include <hardware_legacy/uevent.h>
#include <utils/StrongPointer.h>

#include <linux/mxcfb.h>
#include <linux/ioctl.h>
#include <EGL/egl.h>
#include "gralloc_priv.h"
#include "hwc_context.h"
#include "hwc_vsync.h"

static int hwc_judge_display_state(struct hwc_context_t* ctx)
{
    char fb_path[HWC_PATH_LENGTH];
    char tmp[HWC_PATH_LENGTH];
    char value[HWC_STRING_LENGTH];
    FILE *fp;
    int dispid = 0;

    for (int i = 0; i < HWC_MAX_FB; i++) {
        if(dispid >= HWC_NUM_PHYSICAL_DISPLAY_TYPES) {
            ALOGW("system can't support more than %d devices", dispid);
            break;
        }

        displayInfo *pInfo = &ctx->mDispInfo[dispid];
        memset(fb_path, 0, sizeof(fb_path));
        snprintf(fb_path, HWC_PATH_LENGTH, HWC_FB_SYS"%d", i);
        //check the fb device exist.
        if (!(fp = fopen(fb_path, "r"))) {
            ALOGW("open %s failed", fb_path);
            continue;
        }
        fclose(fp);

        //check if it is a real device
        snprintf(tmp, sizeof(tmp), "%s/name", fb_path);
        if (!(fp = fopen(tmp, "r"))) {
            ALOGW("open %s failed", tmp);
            continue;
        }
        memset(value, 0, sizeof(value));
        if (!fgets(value, sizeof(value), fp)) {
            ALOGE("Unable to read fb%d name %s", i, tmp);
            fclose(fp);
            continue;
        }
        if (strstr(value, "FG")) {
            ALOGI("fb%d is overlay device", i);
            fclose(fp);
            continue;
        }
        fclose(fp);

        //read fb device name
        snprintf(tmp, sizeof(tmp), "%s/fsl_disp_dev_property", fb_path);
        if (!(fp = fopen(tmp, "r"))) {
            ALOGI("open %s failed", tmp);
            //continue;
            //make default type to ldb.
            pInfo->type = HWC_DISPLAY_LDB;
            pInfo->connected = true;
        }
        else {
            memset(value, 0, sizeof(value));
            if (!fgets(value, sizeof(value), fp)) {
                ALOGI("read %s failed", tmp);
                fclose(fp);
                continue;
                //make default type to ldb.
                //pInfo->type = HWC_DISPLAY_LDB;
                //pInfo->connected = true;
            }
            else if (strstr(value, "hdmi")) {
                ALOGI("fb%d is %s device", i, value);
                pInfo->type = HWC_DISPLAY_HDMI;
            }
            else if (strstr(value, "dvi")) {
                ALOGI("fb%d is %s device", i, value);
                pInfo->type = HWC_DISPLAY_DVI;
            }
            else {
                ALOGI("fb%d is %s device", i, value);
                pInfo->type = HWC_DISPLAY_LDB;
                pInfo->connected = true;
            }
            fclose(fp);
        }

        pInfo->fb_num = i;
        if(pInfo->type != HWC_DISPLAY_LDB) {
            //judge connected device state
            snprintf(tmp, sizeof(tmp), "%s/disp_dev/cable_state", fb_path);
            if (!(fp = fopen(tmp, "r"))) {
                ALOGI("open %s failed", tmp);
                //make default to false.
                pInfo->connected = false;
            }
            else {
                memset(value,  0, sizeof(value));
                if (!fgets(value, sizeof(value), fp)) {
                    ALOGI("read %s failed", tmp);
                    //make default to false.
                    pInfo->connected = false;
                }
                else if (strstr(value, "plugin")) {
                    ALOGI("fb%d device %s", i, value);
                    pInfo->connected = true;
                }
                else {
                    ALOGI("fb%d device %s", i, value);
                    pInfo->connected = false;
                }
                fclose(fp);
            }
        }

        //allow primary display plug-out then plug-in.
        if (dispid == 0 && pInfo->connected == false) {
            pInfo->connected = true;
            ctx->m_vsync_thread->setFakeVSync(true);
        }
        dispid ++;
    }

    return 0;
}

int hwc_get_framebuffer_info(displayInfo *pInfo)
{
    char fb_path[HWC_PATH_LENGTH];
    int refreshRate;

    memset(fb_path, 0, sizeof(fb_path));
    snprintf(fb_path, HWC_PATH_LENGTH, HWC_FB_PATH"%d", pInfo->fb_num);
    if (pInfo->fd > 0) {
        close(pInfo->fd);
        pInfo->fd = 0;
    }

    pInfo->fd = open(fb_path, O_RDWR);
    if(pInfo->fd < 0) {
        ALOGE("open %s failed", fb_path);
        return BAD_VALUE;
    }

    struct fb_var_screeninfo info;
    if (ioctl(pInfo->fd, FBIOGET_VSCREENINFO, &info) == -1) {
        ALOGE("FBIOGET_VSCREENINFO ioctl failed: %s", strerror(errno));
        close(pInfo->fd);
        return BAD_VALUE;
    }

    refreshRate = 1000000000000LLU /
        (
         uint64_t( info.upper_margin + info.lower_margin + info.yres + info.vsync_len)
         * ( info.left_margin  + info.right_margin + info.xres + info.hsync_len)
         * info.pixclock
        );

    if (refreshRate == 0) {
        ALOGW("invalid refresh rate, assuming 60 Hz");
        refreshRate = 60;
    }

    if (int(info.width) <= 0 || int(info.height) <= 0) {
        // the driver doesn't return that information
        // default to 160 dpi
        info.width  = ((info.xres * 25.4f)/160.0f + 0.5f);
        info.height = ((info.yres * 25.4f)/160.0f + 0.5f);
    }

    pInfo->xres = info.xres;
    pInfo->yres = info.yres;
    pInfo->xdpi = 1000 * (info.xres * 25.4f) / info.width;
    pInfo->ydpi = 1000 * (info.yres * 25.4f) / info.height;
    pInfo->vsync_period  = 1000000000 / refreshRate;
    pInfo->blank  = 0;
    pInfo->format = (info.bits_per_pixel == 32)
                         ? ((info.red.offset == 0) ? HAL_PIXEL_FORMAT_RGBA_8888 :
                            HAL_PIXEL_FORMAT_BGRA_8888)
                         : HAL_PIXEL_FORMAT_RGB_565;

    ALOGV("using\n"
          "xres         = %d px\n"
          "yres         = %d px\n"
          "width        = %d mm (%f dpi)\n"
          "height       = %d mm (%f dpi)\n"
          "refresh rate = %d Hz\n"
          "format       = %d\n",
          info.xres, info.yres, info.width, pInfo->xdpi / 1000.0,
          info.height, pInfo->ydpi / 1000.0, refreshRate,
          pInfo->format);

    return NO_ERROR;
}

int hwc_get_display_info(struct hwc_context_t* ctx)
{
    int err = 0;
    int dispid = 0;

    hwc_judge_display_state(ctx);
    for(dispid=0; dispid<HWC_NUM_PHYSICAL_DISPLAY_TYPES; dispid++) {
        displayInfo *pInfo = &ctx->mDispInfo[dispid];
        if(pInfo->connected) {
            err = hwc_get_framebuffer_info(pInfo);
            if (!err && ctx->m_hwc_ops) {
                ctx->m_hwc_ops->setDisplayInfo(ctx->m_hwc_ops, dispid,
                            pInfo->xres, pInfo->yres, pInfo->connected);
            }
        }
    }

    return err;
}

int hwc_get_display_fbid(struct hwc_context_t* ctx, int disp_type)
{
    int fbid = -1;
    int dispid = 0;
    for(dispid=0; dispid<HWC_NUM_PHYSICAL_DISPLAY_TYPES; dispid++) {
        displayInfo *pInfo = &ctx->mDispInfo[dispid];
        if(pInfo->type == disp_type) {
            fbid = pInfo->fb_num;
            break;
        }
    }

    return fbid;
}

int hwc_get_display_dispid(struct hwc_context_t* ctx, int disp_type)
{
    int dispid = 0;
    for(dispid=0; dispid<HWC_NUM_PHYSICAL_DISPLAY_TYPES; dispid++) {
        displayInfo *pInfo = &ctx->mDispInfo[dispid];
        if(pInfo->type == disp_type) {
            return dispid;
        }
    }

    return dispid;
}

