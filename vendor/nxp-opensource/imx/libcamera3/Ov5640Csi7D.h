/*
 * Copyright 2018 NXP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _OV5640_CSI_7D_H
#define _OV5640_CSI_7D_H

#include "Camera.h"
#include "MMAPStream.h"

class Ov5640Csi7D : public Camera
{
 public:
    Ov5640Csi7D(int32_t id, int32_t facing, int32_t orientation, char* path);
    ~Ov5640Csi7D();

    virtual status_t initSensorStaticData();
    virtual PixelFormat getPreviewPixelFormat();

    virtual int getCaptureMode(int width, int height);

 private:
    class OvStream : public MMAPStream {
     public:
        OvStream(Camera* device) : MMAPStream(device) {}
        virtual ~OvStream() {}

        // configure device.
        virtual int32_t onDeviceConfigureLocked();
    };
};

#endif
