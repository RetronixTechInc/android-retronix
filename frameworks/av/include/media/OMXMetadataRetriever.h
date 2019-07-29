/*
**
** Copyright (C) 2008 The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* Copyright 2009-2015 Freescale Semiconductor, Inc. */

#ifndef ANDROID_OMXMETADATARETRIEVER_H
#define ANDROID_OMXMETADATARETRIEVER_H

#include <media/MediaMetadataRetrieverInterface.h>
#include <binder/IInterface.h>
#include <utils/List.h>
#include <utils/String8.h>
#include <utils/KeyedVector.h>

#include <gui/Surface.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>

namespace android {

class OMXMetadataRetriever : public MediaMetadataRetrieverInterface
{
public:
                        OMXMetadataRetriever(int nMediaType = 0);
    virtual             ~OMXMetadataRetriever();

    virtual status_t    setDataSource(const char *url);
    virtual status_t    setDataSource(const sp<DataSource>& source);
    virtual status_t    setDataSource(const sp<IMediaHTTPService> &httpService, const char *url, const KeyedVector<String8, String8> *headers = NULL);
    virtual status_t    setDataSource(int fd, int64_t offset, int64_t length);
    virtual status_t    setMode(int mode);
    virtual status_t    getMode(int* mode) const;
    virtual VideoFrame* captureFrame(int64_t timeUs, int option);
    virtual VideoFrame* getFrameAtTime(int64_t timeUs, int option);
    virtual MediaAlbumArt* extractAlbumArt();
    virtual const char* extractMetadata(int keyCode);

private:
    void*               MetadataExtractor;
    bool mParsedMetaData;
	MediaAlbumArt *mAlbumArt;
    void ExtractMetadata();
    KeyedVector<int, String8> mMetaData;
    int                 mSharedFd;
    int                 mMediaType;  
    char                contentURI[128];
    sp<SurfaceComposerClient>           session;
    sp<SurfaceControl>                  surfaceControl;
    sp<Surface>                         surface;
    bool sessionCreated;
};

}; // namespace android

#endif // ANDROID_OMXMETADATARETRIEVER_H
