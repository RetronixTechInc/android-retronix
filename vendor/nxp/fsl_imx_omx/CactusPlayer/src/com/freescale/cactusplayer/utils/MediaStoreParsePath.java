/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
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

package com.freescale.cactusplayer.utils;

import android.net.Uri;
import android.content.Context;
import android.provider.MediaStore;
import android.database.Cursor;
public class MediaStoreParsePath implements IParsePath{

    @Override
    public String parseUrl(Context context, Uri uri) {
        Cursor cursor = context.getContentResolver().query(uri, new String[]{MediaStore.Video.Media.DATA},
                null, null, null);
        int column = cursor.getColumnIndexOrThrow(MediaStore.Video.Media.DATA);
        cursor.moveToFirst();
        String path = cursor.getString(column);
        return path;
    }
}
