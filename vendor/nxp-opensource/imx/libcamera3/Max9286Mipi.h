/*
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc.
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

#ifndef _MAX9286_MIPI_H
#define _MAX9286_MIPI_H

#include "Camera.h"
#include "MMAPStream.h"

class Max9286Mipi : public Camera
{
public:
    Max9286Mipi(int32_t id, int32_t facing, int32_t orientation, char *path);
    ~Max9286Mipi();

    virtual status_t initSensorStaticData();
    virtual PixelFormat getPreviewPixelFormat();

private:
    class Max9286Stream : public MMAPStream
    {
    public:
        Max9286Stream(Camera *device) : MMAPStream(device, true) { mV4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE; }
        virtual ~Max9286Stream() {}

        // configure device.
        virtual int32_t onDeviceConfigureLocked();
    };
};

#endif
