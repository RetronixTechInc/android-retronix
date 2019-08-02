package com.example.usbupdate;

import android.app.Activity;
import android.util.Log;
import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.content.ContentResolver;
import android.provider.Settings;
import android.content.Intent;
import android.os.UserHandle;
import android.os.SystemProperties;
import com.android.internal.app.LocalePicker;
import android.app.AlarmManager;
import android.widget.Toast;
import android.content.ComponentName;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.util.Locale;
import java.util.ArrayList;

/**
 * 读取 interrupt.ini
 * @author tom
 *
 */
public class FileUpdate {
	private static final String LOG_TAG = "Tom===FileUpdate";
	private static final boolean DEBUG = false;

	private static String result = "null";
	private	String slanguage = "null" ;
	private	String scountry = "null" ;
	
	Context mContext = null; 
	
    /* construct */
    public FileUpdate(Context context) {
        mContext = context;
    }

	public boolean read(){
		if (DEBUG) Log.d(LOG_TAG, "read");
		int icontrol = rtxcontrol_parse("/mnt/udisk/usbupdate/rtxcontrol");
		if( (icontrol & 0x02) == 0x02)
		{
			rtxbuild_parse("/mnt/udisk/usbupdate/rtxbuild.prop");
		}
		
		if( icontrol != 0)
		{
			if(result.equals("null"))
			{
				result = "Setting configure update success!" ;
			}
			Toast.makeText(mContext, result, Toast.LENGTH_LONG).show();
		}
		return true;
	}

	private int rtxcontrol_parse(String sf)
	{
		int ret = 0 ;
		
		try{
			File f = new File(sf);
			if(!f.exists()){      
				return 0;
			}
			  
			FileInputStream fis = new FileInputStream(f);
			InputStreamReader rx = new InputStreamReader(fis, "GB2312");
			BufferedReader dis = new BufferedReader(rx);
			
			String a;
			
			while(true){
				if (DEBUG) Log.d(LOG_TAG , "rtxcontrol_parse" ); 
				a = dis.readLine();      
				if (DEBUG) Log.d(LOG_TAG , "rtxcontrol_parse a=" + a ); 

				if(a == null) break; //no more
				a = a.trim();  
				if(a.startsWith("#") || a.length() < 5){
					continue;
				}
				if (DEBUG) Log.d(LOG_TAG , "rtxcontrol_parse a.length()=" + a.length() ); 				
				String[] b = a.split("=");
				int ivalue = 0 ;
				String svalue = "null" ;

				if(b.length==2){
					if (DEBUG) Log.d(LOG_TAG , "rtxcontrol_parse b[0]=" + b[0] + "; b[1]=" + b[1] );  
					if(b[0].compareTo("rtxbuild") == 0){
						ivalue = Integer.parseInt(b[1]);
						if (ivalue == 1)
						{
							ret |= 0x02 ;
						}
					}
					else if(b[0].compareTo("defaulthome") == 0){
						svalue = b[1].trim();
						setDefaultHome(svalue) ;
						ret |= 0x04  ;
					}
				}
			}
			
			if(dis != null){
				dis.close();
			}
			if(rx != null){
				rx.close();
			}
			if(fis != null){
				fis.close();
			}
		}
		catch(Exception ex){
			ex.printStackTrace();
			result = "parse rtxcontrol file fail!" ;
			return 0x01 ;
		}
		return ret ;
	}

	private boolean rtxbuild_parse(String sf)
	{
		try{
			File f = new File(sf);
			if(!f.exists()){      
				return false;
			}
			
			FileInputStream fis = new FileInputStream(f);
			InputStreamReader rx = new InputStreamReader(fis, "GB2312");
			BufferedReader dis = new BufferedReader(rx);
			
			String a;
			
			while(true){
				a = dis.readLine();      

				if(a == null) break; //no more
				a = a.trim();  

				if(a.startsWith("#") || a.length() < 5){
					continue;
				}
				String[] b = a.split("=");
				int ivalue = 0 ;
				String svalue = "null" ;

				if(b.length==2){
					if (DEBUG) Log.d(LOG_TAG , "rtxbuild_parse b[0]=" + b[0] + "; b[1]=" + b[1] );  
					if(b[0].compareTo("ro.rtx.musicvalue") == 0){
						ivalue = Integer.parseInt(b[1]);
						setMusicVolume(ivalue) ;
					}
					else if(b[0].compareTo("ro.debuggable") == 0){
						ivalue = Integer.parseInt(b[1]);
						setADBstatus(ivalue) ;
					}
					else if(b[0].compareTo("ro.rtx.touchsound") == 0){
						ivalue = Integer.parseInt(b[1]); 
						setTouchSound(ivalue) ;
					}
					else if(b[0].compareTo("ro.rtx.brightness") == 0){
						ivalue = Integer.parseInt(b[1]);
						setBrightness(ivalue) ;
					}
					else if(b[0].compareTo("ro.rtx.airpalne") == 0){
						ivalue = Integer.parseInt(b[1]);
						setAirplanemode(ivalue) ;
					}
					else if(b[0].compareTo("ro.rtx.spellcheck") == 0){
						ivalue = Integer.parseInt(b[1]);
						setSpellCheck(ivalue) ;
					}
					else if(b[0].compareTo("persist.sys.language") == 0){
						slanguage = b[1].trim();
						//SystemProperties.set("persist.sys.language", slanguage);
						if(scountry.length() == 2)
						{						
							setLanguage();
						}
					}
					else if(b[0].compareTo("persist.sys.country") == 0){
						scountry = b[1].trim();
						//SystemProperties.set("persist.sys.country", scountry);
						if(slanguage.length() == 2)
						{
							setLanguage();
						}
					}
					else if(b[0].compareTo("persist.sys.timezone") == 0){
						setTimezone(b[1].trim()) ;
					}
					else if(b[0].compareTo("persist.sys.keyboard") == 0){
						svalue = b[1].trim();
						SystemProperties.set("persist.sys.keyboard", svalue);
					}
				}
			}
			
			if(dis != null){
				dis.close();
			}
			if(rx != null){
				rx.close();
			}
			if(fis != null){
				fis.close();
			}
		}
		catch(Exception ex){
			ex.printStackTrace();
			result = "parse rtxbuild file fail!" + ex ;
			return false;
		}
		return true;
	}
	
	/* 預設HOME設定 */
	private void setDefaultHome(String svalue) {
        PackageManager mPm = mContext.getPackageManager();
        ArrayList<ResolveInfo> homeActivities = new ArrayList<ResolveInfo>();
        ComponentName currentDefaultHome  = mPm.getHomeActivities(homeActivities);
        if(currentDefaultHome != null)
        {
        	if (DEBUG)
        		Log.d(LOG_TAG , "currentDefaultHome=" + currentDefaultHome + "; getPackageName=" + currentDefaultHome.getPackageName() );  
        }
        ComponentName[] mHomeComponentSet = new ComponentName[homeActivities.size()];
        ComponentName setactivityName = null ;
        
        for (int i = 0; i < homeActivities.size(); i++) {
            final ResolveInfo candidate = homeActivities.get(i);
            final ActivityInfo info = candidate.activityInfo;
            ComponentName activityName = new ComponentName(info.packageName, info.name);
            mHomeComponentSet[i] = activityName;
            if (DEBUG) Log.d(LOG_TAG , "info.packageName=" + info.packageName ); 
            if (DEBUG) Log.d(LOG_TAG , "info.name=" + info.name ); 
            if (DEBUG) Log.d(LOG_TAG , "activityName=" + activityName ); 
            if (info.packageName.equals(svalue)) {
            	setactivityName = activityName;
            }
        }

        IntentFilter mHomeFilter = new IntentFilter(Intent.ACTION_MAIN);
        mHomeFilter.addCategory(Intent.CATEGORY_HOME);
        mHomeFilter.addCategory(Intent.CATEGORY_DEFAULT);
        if (setactivityName != null)
        {
        mPm.replacePreferredActivity(mHomeFilter, IntentFilter.MATCH_CATEGORY_EMPTY,
                mHomeComponentSet, setactivityName);
        }
        else
        {
        	result += "set Default Home fail : can't find package name " + svalue + ";\n";        	
        }
        return ;
	}
	
	
	/* 聲音設定值 */
	private void setMusicVolume(int ivalue) throws Exception {
		AudioManager audioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
		int max_ivalue = audioManager.getStreamMaxVolume(AudioManager.STREAM_MUSIC);
		if (DEBUG) Log.d(LOG_TAG, "max_ivalue=" + max_ivalue + "; user_set_value=" + ivalue );      
		if (ivalue > max_ivalue) {
			audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, max_ivalue, 0);
		}
		else
		{
			audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, ivalue, 0);			
		}
	}
	
	/* setTimezone 設定 */
	private void setTimezone(String tzId) {
		if(tzId != null)
		{
			ContentResolver cv = mContext.getContentResolver(); 
			Settings.Global.putInt(cv, Settings.Global.AUTO_TIME_ZONE, 0);
			AlarmManager alarm = (AlarmManager) mContext.getSystemService(Context.ALARM_SERVICE);
	        alarm.setTimeZone(tzId);
		}
		else
		{
			result += "ADB debug set fail : value not't 0 or 1;\n" ;
		}
	}
	
	/* adb debug設定值 */
	private void setADBstatus(int ivalue) {
		if(ivalue == 0 || ivalue == 1)
		{
			ContentResolver cv = mContext.getContentResolver(); 
			Settings.Global.putInt(cv, Settings.Global.ADB_ENABLED, ivalue);
		}
		else
		{
			result += "ADB debug set fail : value not't 0 or 1;\n" ;
		}
	}
	
	/* Touch sound 設定值 */
	private void setTouchSound(int ivalue) {
		if(ivalue == 0 || ivalue == 1)
		{
			ContentResolver cv = mContext.getContentResolver(); 
			Settings.System.putInt(cv, Settings.System.SOUND_EFFECTS_ENABLED, ivalue);
		}
		else
		{
			result += "TouchSound set fail : value not't 0 or 1;\n" ;
		}
	}
	
	/* brightness 設定值 */
	private void setBrightness(int ivalue) {
		if(ivalue >= 20 && ivalue <= 255)
		{
			ContentResolver cv = mContext.getContentResolver(); 
			Settings.System.putInt(cv, Settings.System.SCREEN_BRIGHTNESS, ivalue);
		}
		else
		{
			result += "Brightness set fail : value out of range(0~255);\n" ;
		}
	}
	
	/* Airplanemode 設定值 */
	private void setAirplanemode(int ivalue) {
		if(ivalue == 0 || ivalue == 1)
		{
			ContentResolver cv = mContext.getContentResolver(); 
			Settings.Global.putInt(cv, Settings.Global.AIRPLANE_MODE_ON, ivalue);
	        // Post the intent
	        Intent intent = new Intent(Intent.ACTION_AIRPLANE_MODE_CHANGED);
	        intent.putExtra("state", ivalue==1 ? true : false);
	        mContext.sendBroadcastAsUser(intent, UserHandle.ALL);
		}
		else
		{
			result += "Airplanemode set fail : value not't 0 or 1;\n" ;
		}
	}
	
	/* setSpellCheck 設定值 */
	private void setSpellCheck(int ivalue) {
		if(ivalue == 0 || ivalue == 1)
		{
	        if (DEBUG) Log.d(LOG_TAG , "setSpellCheck ivalue=" + ivalue );  
			ContentResolver cv = mContext.getContentResolver(); 
			Settings.Secure.putInt(cv, Settings.Secure.SPELL_CHECKER_ENABLED, ivalue);
		}
		else
		{
			result += "SpellCheckmode set fail : value not't 0 or 1;\n" ;
		}
	}

	/* setLanguage 設定值 */
	private void setLanguage() {
		Locale llocal = new Locale(slanguage, scountry);
        LocalePicker llocpick = new LocalePicker();
        llocpick.updateLocale(llocal);
        if (DEBUG) Log.d(LOG_TAG , "setLanguage locale=" + llocal );  
        
		/*if (DEBUG) Log.d(LOG_TAG , "current getDefault=" + mContext.getResources().getConfiguration().locale.getDefault() );  
		if (DEBUG) Log.d(LOG_TAG , "current getDisplayLanguage=" + mContext.getResources().getConfiguration().locale.getDisplayLanguage() );  
		if (DEBUG) Log.d(LOG_TAG , "current getCountry=" + mContext.getResources().getConfiguration().locale.getCountry() );  
		if (DEBUG) Log.d(LOG_TAG , "current getDisplayCountry=" + mContext.getResources().getConfiguration().locale.getDisplayCountry() );  
		if (DEBUG) Log.d(LOG_TAG , "current getDisplayName=" + mContext.getResources().getConfiguration().locale.getDisplayName(new Locale("en", "US")) );  
		if (DEBUG) Log.d(LOG_TAG , "current getLanguage=" + mContext.getResources().getConfiguration().locale.getLanguage() );  
		
		int i = 0 ;
		Locale alocal[] = mContext.getResources().getConfiguration().locale.getAvailableLocales();
		if (DEBUG) Log.d(LOG_TAG , "current alocal length=" + alocal.length );  
		for( i = 0; i < alocal.length; i++)
		{
			Log.d(LOG_TAG , "current locale=" + alocal[i] );  
		}
		*/
	}

}
