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
import android.os.Environment;
import android.provider.DocumentsContract;
import android.content.Context;
public class ExternalStorageDocumentParsePath implements IParsePath{

	@Override
	public String parseUrl(Context context, Uri uri) {
		final String docId = DocumentsContract.getDocumentId(uri);
		final String[] split = docId.split(":");
		final String type = split[0];
		String storageDefinition;

		if("primary".equalsIgnoreCase(type)){
			return Environment.getExternalStorageDirectory() + "/" + split[1];
		} else {
			if(Environment.isExternalStorageRemovable()){
				return System.getenv("EXTERNAL_STORAGE") + "/" + split[1];
			} else{
				return  "/storage/" + type + "/" + split[1];
			}
		}
	}
}
