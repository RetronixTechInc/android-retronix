/*
 * Copyright 2017 NXP
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

#ifndef _OV5640_CSI_8MQ_H
#define _OV5640_CSI_8MQ_H

#include "Camera.h"
#include "USPStream.h"
#include "MMAPStream.h"

class Ov5640Csi8MQ : public Camera
{
public:
    Ov5640Csi8MQ(int32_t id, int32_t facing, int32_t orientation, char* path);
    ~Ov5640Csi8MQ();

    virtual status_t initSensorStaticData();
    virtual int getFps(int width, int height, int defValue);
    virtual PixelFormat getPreviewPixelFormat();

private:
    class OvStream : public MMAPStream {
    public:
        OvStream(Camera* device)
            : MMAPStream(device) {}
        virtual ~OvStream() {}
        virtual int32_t onDeviceConfigureLocked();
        virtual int32_t getCaptureMode(int width, int height);
    };
};

#endif
