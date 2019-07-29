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

import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.provider.DocumentsContract;

public class PathUtils {

	public static String getPath(final Context context, final Uri uri) {

		IParsePath parsePath = null;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT && DocumentsContract.isDocumentUri(context, uri)) {

			if ("com.android.externalstorage.documents".equals(uri.getAuthority())){
				parsePath = new ExternalStorageDocumentParsePath();
			}
			else{
				//Other content provider uri operation
			}
		} else{
            if ("media".equals(uri.getAuthority())) {
                parsePath = new MediaStoreParsePath();
            }
		}
		return parsePath != null ? parsePath.parseUrl(context, uri) : new DefaultParsePath().parseUrl(context, uri);
	}
}
