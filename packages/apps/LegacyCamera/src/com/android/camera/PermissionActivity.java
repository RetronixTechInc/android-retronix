/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
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
package com.android.camera;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.widget.Toast;
import android.content.Intent;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.util.Log;
import com.android.camera.R;
import com.android.camera.Camera;

public class PermissionActivity extends Activity {


	private int mNumPermissionsToRequest = 0;
	private boolean mShouldRequestLocationPermission = false;
	private boolean mFlagHasLocationPermission = true;
	private boolean mShouldRequestMessagePermission = false;
	private boolean mFlagHasMessagePermission = true;
	private int mIndexPermissionRequestLocation = 0;
	private int mIndexPermissionRequestMessage = 0;
	private static final int PERMISSION_REQUEST_CODE = 0;
	private static final String TAG = "Camera PermissionActivity";

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.permissions);
		checkPermission();
	}

	private void checkPermission(){

		//Request Location Permission
		if(checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION)
				!= PackageManager.PERMISSION_GRANTED){
			mNumPermissionsToRequest++;
			mShouldRequestLocationPermission  = true;
		}else{
			mFlagHasLocationPermission  = true;
		}

		
		//Request Message Permission
		if(checkSelfPermission(Manifest.permission.READ_SMS)
				!= PackageManager.PERMISSION_GRANTED){
			mNumPermissionsToRequest++;
			mShouldRequestMessagePermission  = true;
		}else{
			mFlagHasMessagePermission  = true;
		}

		String[] permissionToRequest = new String[mNumPermissionsToRequest];
		int permissionRequestIndex = 0;

		if(mShouldRequestLocationPermission){
			permissionToRequest[permissionRequestIndex] = Manifest.permission.ACCESS_FINE_LOCATION;
			mIndexPermissionRequestLocation= permissionRequestIndex;
			permissionRequestIndex++;
		}
		
		if(mShouldRequestMessagePermission){
			permissionToRequest[permissionRequestIndex] = Manifest.permission.READ_SMS;
			mIndexPermissionRequestMessage= permissionRequestIndex;
			permissionRequestIndex++;
		}
		
		if(permissionToRequest.length > 0){
			requestPermissions(permissionToRequest, PERMISSION_REQUEST_CODE);
		}else{
			Intent enterCamera = new Intent(this, Camera.class);
			startActivity(enterCamera);
			finish();
		}
	}

	@Override
	public void onRequestPermissionsResult(int requestCode,
			String permissions[], int[] grantResults) {
		switch (requestCode) {
		case PERMISSION_REQUEST_CODE:
			if (grantResults.length > 0
					&& grantResults[0] == PackageManager.PERMISSION_GRANTED) {
				Log.v(TAG, "Grant permission successfully");
			} else {
				Log.v(TAG, "Grant permission unsuccessfully");
			}
			break;
		default:
			break;
		}
		enterCamera();
	}

	private void enterCamera(){
		Intent enterCamera = new Intent(this, Camera.class);
		startActivity(enterCamera);
		finish();
	}

}


