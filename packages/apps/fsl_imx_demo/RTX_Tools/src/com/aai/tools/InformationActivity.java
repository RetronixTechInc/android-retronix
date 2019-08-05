/*
 * Copyright (C) 2008 The Android Open Source Project
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

package com.aai.tools;

import android.os.Bundle;
import android.preference.PreferenceActivity;
import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import android.util.Log;

import android.text.TextUtils;
import java.io.InputStreamReader;
import java.io.DataInputStream;
import java.net.NetworkInterface;
import java.util.Enumeration;
import java.net.InetAddress;
import java.net.Inet4Address;
import java.net.SocketException;

import android.net.wifi.WifiManager;
import android.net.wifi.WifiInfo;

import android.provider.Settings;
import android.os.SystemClock;
import android.preference.Preference;
import android.preference.PreferenceScreen;
import android.widget.Toast;
import android.content.Intent;
import android.view.KeyEvent;

public class InformationActivity extends PreferenceActivity {
    private static final String LOG_TAG = "Information";
    private static final boolean DEBUG = false;

    private static final String KEY_ETHMAC = "eth_mac";
    private static final String KEY_IP = "eth_ip";

    long[] mHits = new long[3];
    Toast mDevHitToast;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (DEBUG) Log.d(LOG_TAG, "onCreate");
        this.addPreferencesFromResource(R.layout.information);

        String wifiMAC = getMacAddress("/sys/class/net/wlan0/address");
        String ethMAC = getMacAddress("/sys/class/net/eth0/address");
        findPreference("eth_mac").setSummary(!TextUtils.isEmpty(ethMAC) ? ethMAC  : "Unknown");
        findPreference("eth_ip").setSummary(getEthIP());
        findPreference("wifi_mac").setSummary(!TextUtils.isEmpty(wifiMAC) ? wifiMAC  : "Unknown");
        findPreference("wifi_ip").setSummary(getWifiIP());
        findPreference("mcu_version").setSummary(getDevinfo("cat /sys/devices/platform/mcu_efm.0/mcu_version"));

        //getPreferenceScreen().removePreference(findPreference("device_name"));
    }

    public String getMacAddress(String path){
    	try {
    	StringBuffer fileData = new StringBuffer(1000);
    	BufferedReader reader = new BufferedReader(new FileReader(path));
    	char[] buf = new char[1024];
    	int numRead=0;
    	
			while((numRead=reader.read(buf)) != -1){
				String readData = String.valueOf(buf, 0, numRead);
				fileData.append(readData);
			}
			reader.close();
	    	return fileData.toString().toUpperCase().substring(0, 17);
		} catch (IOException e) {
			e.printStackTrace();
		}
    	return null;
    }

    private String getEthIP(){
    	try {
    		NetworkInterface ni = NetworkInterface.getByName("eth0");
    		Enumeration<InetAddress> addresses = ni.getInetAddresses();
            while (addresses.hasMoreElements()) {
                InetAddress addr = addresses.nextElement();
                if (addr instanceof Inet4Address) {
                    return addr.getHostAddress().toString();
                }
            }
	    } 
	    catch (SocketException ex){}
	    catch (NullPointerException ex){}
	    return getString(R.string.ip_unknown);
    }
    
    private String getWifiIP(){
    	WifiManager wifiManager = (WifiManager) getSystemService(WIFI_SERVICE);
    	WifiInfo wifiInfo = wifiManager.getConnectionInfo();
    	int ip = wifiInfo.getIpAddress();
    	String ipString = String.format(
    			   "%d.%d.%d.%d",
    			   (ip & 0xff),
    			   (ip >> 8 & 0xff),
    			   (ip >> 16 & 0xff),
    			   (ip >> 24 & 0xff));
    	return ipString;
    }
    
    private String getDevinfo(String cmd){
    	Process p;
    	try {
    		p = Runtime.getRuntime().exec(cmd);		
    		BufferedReader br=new BufferedReader(new InputStreamReader(p.getInputStream ()));	
    		String mSerialNumber = br.readLine();	
	    	br.close();    	
	    	p.destroy();    	
	    	return mSerialNumber;
    	} 
    	catch (IOException e) { }		
    	catch (NumberFormatException e) { }		
    	return getString(R.string.mcu_unknown);
    }
    
    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
    	if (DEBUG) Log.d(LOG_TAG, "onPreferenceTreeClick");
        if (preference.getKey().equals(KEY_ETHMAC)) {
            System.arraycopy(mHits, 1, mHits, 0, mHits.length-1);
            mHits[mHits.length-1] = SystemClock.uptimeMillis();
            if (mHits[0] >= (SystemClock.uptimeMillis()-500)) {
                /*Intent intent = new Intent(Intent.ACTION_MAIN);
                intent.setClassName("android", com.android.internal.app.PlatLogoActivity.class.getName());
                try {
                    startActivity(intent);
                } catch (Exception e) {
                    Log.e(LOG_TAG, "Unable to start activity " + intent.toString());
                }*/
            	callfunc("IOTest");
            }
        }else if (preference.getKey().equals(KEY_IP)) {
            System.arraycopy(mHits, 1, mHits, 0, mHits.length-1);
            mHits[mHits.length-1] = SystemClock.uptimeMillis();
            if (mHits[0] >= (SystemClock.uptimeMillis()-500)) {
                /*Intent intent = new Intent(Intent.ACTION_MAIN);
                intent.setClassName("android", com.android.internal.app.PlatLogoActivity.class.getName());
                try {
                    startActivity(intent);
                } catch (Exception e) {
                    Log.e(LOG_TAG, "Unable to start activity " + intent.toString());
                }*/
            	callfunc("StableTest");
            }
        }
        return super.onPreferenceTreeClick(preferenceScreen, preference);
    }
    
	private void callfunc(String item)
    {
			if(DEBUG) Log.d(LOG_TAG,"Tom-------test send android.intent.MPToolaction.send");
			Intent _t = new Intent("android.intent.MPToolaction.send");;
			_t.putExtra("arg0", "I love RTX"); 				//magic package
			
			if("IOTest".equals(item)){
				_t.putExtra("arg1", "IOTest");	 	//call function
				_t.putExtra("arg2", "");	// function arg
			}else if("StableTest".equals(item)){
				_t.putExtra("arg1", "StableTest");	//call function
				_t.putExtra("arg2", "");			// function arg
			}
			
			InformationActivity.this.sendBroadcast(_t);

	};


    

}
