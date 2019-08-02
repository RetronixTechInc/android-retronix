/*
/* Copyright 2012 Freescale Semiconductor, Inc.
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

package com.example.receive_tools;

import java.net.MalformedURLException;
import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.*;
import android.widget.Toast;
import java.security.GeneralSecurityException;
import java.io.*;
import android.os.RecoverySystem;
import android.os.PowerManager;
import android.os.IPowerManager;
import android.os.PowerManager.WakeLock;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.RemoteException;
import java.util.StringTokenizer;

// Controller of OTA Activity
public class OtaAppActivity extends Activity {
		
    private static final String LOG_TAG = "OtaAppActivity";
    private static final boolean DEBUG = false;

    Context mContext; 
	WakeLock mWakelock;
	PowerManager pm ;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (DEBUG) Log.d(this.toString(), "onCreate");  
		mContext = getBaseContext();
		pm = (PowerManager)mContext.getSystemService(Context.POWER_SERVICE);
		mWakelock = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, "OTA Wakelock");
    }
    
    @Override
    public void onStart() {
    	super.onStart();
    	if (DEBUG) Log.d(this.toString(), "onStart");
    }
    
    @Override
    public void onRestart() {
    	super.onRestart();
    	if (DEBUG) Log.d(this.toString(), "onRestart");
    }
    
    
    @Override
    public void onPause() {
    	super.onPause();
    	if (DEBUG) Log.d(this.toString(), "onPause");
    }
    
    @Override
    public void onStop() {
    	super.onStop();
    	if (DEBUG) Log.d(this.toString(), "onStop");
    }

    private void ShowMessage(String str){
    	Toast.makeText(OtaAppActivity.this, str, Toast.LENGTH_LONG).show();
    }
		
    public String updatesystem( String arg2 ) {
        if (DEBUG) Log.d(this.toString(), "updatesystem");     
        if (compareLocalVersion( arg2 )) {
        	return startInstallUpgradePackage ( arg2 ) ;
        }else{
        	return "latest" ;
        }
    }
	
	// return true if needs to upgrade
	private boolean compareLocalVersion( String updatepackage ) {
        boolean upgrade = false ;

        if (DEBUG) Log.d(this.toString(), "compareLocalVersion");     
		String DeviceID = Build.ID;
		
		String DeviceName = DeviceID.substring(0, 4);
		float Deviceversion ;
		if ( isNumeric(DeviceID.substring(4, 8) )){
			Deviceversion = new Float(DeviceID.substring(4, 8));
		}else{
			return upgrade ;
		}
		
		int filename_start = updatepackage.indexOf("/update-") ;
    	String filename = new String(updatepackage.substring(filename_start+8));
		String UpdatefileName = filename.substring(0, 4);
		float Updatefileversion ;		
		if ( isNumeric(filename.substring(4, 8) )){
			Updatefileversion = new Float(filename.substring(4, 8));
		}else{
			return upgrade ;
		}
		
        if (DeviceName.equals(filename))
        {
            upgrade = Updatefileversion > Deviceversion;
        }
           
		if (DEBUG) Log.d(LOG_TAG, "DeviceName=" + DeviceName + ";  Deviceversion=" + Deviceversion);    
        if (DEBUG) Log.d(LOG_TAG, "UpdatefileName=" + UpdatefileName +  ";  Updatefileversion=" + Updatefileversion);    
			
		return upgrade;
	}

	public String startInstallUpgradePackage( String updatepackage ) {
        if (DEBUG) Log.d(this.toString(), "startInstallUpgradePackage : verify Package : " + updatepackage);     
		File recoveryFile = new File(updatepackage);
		if(!recoveryFile.canRead()) {
    		return "file-fail";
    	}

		// first verify package
         try {
        	 RecoverySystem.verifyPackage(recoveryFile, recoveryVerifyListener, null);
         } catch (IOException e1) {
        	 return "signed-fail";
         } catch (GeneralSecurityException e1) {
        	 return "signed-fail";
         }

         //if (DEBUG) Log.d(this.toString(), "startInstallUpgradePackage : install Package: " + updatepackage);     
         // then install package
        /* try {
        	 mWakelock.acquire();
             if (DEBUG) Log.d(this.toString(), "startInstallUpgradePackage : 0000000000000000001");  
      	     RecoverySystem.installPackage(this, recoveryFile);
        } catch (IOException e) {
      	   // TODO Auto-generated catch block
        	 return "signed-fail";
         } catch (SecurityException e){
        	 return "signed-fail";
         } finally {
        	 mWakelock.release();
         }
         // cannot reach here...
         if (DEBUG) Log.d(this.toString(), "startInstallUpgradePackage : 0000000000000000002");   
         */  
         return "pass" ;
	}
	
	RecoverySystem.ProgressListener recoveryVerifyListener = new RecoverySystem.ProgressListener() {
		public void onProgress(int progress) {
			if (DEBUG) Log.d(this.toString(), "verify progress" + progress);
		}
	};

    private boolean isNumeric(String ipaddr) {
  	  
        //  Check if the string is valid
        
        if ( ipaddr == null || ipaddr.length() != 4 )
          return false;
          
        //  Check the address string, should be n.n.n.n format
        
        StringTokenizer token = new StringTokenizer(ipaddr,".");
        if ( token.countTokens() != 2)
          return false;

        while ( token.hasMoreTokens()) {
          
          //  Get the current token and convert to an integer value
          
          String ipNum = token.nextToken();
          
          try {
            int ipVal = Integer.valueOf(ipNum).intValue();
            if ( ipVal < 0 || ipVal > 99)
              return false;
          }
          catch (NumberFormatException ex) {
            return false;
          }
        }
        
        //  Looks like a valid IP address
        
        return true;
      }


}