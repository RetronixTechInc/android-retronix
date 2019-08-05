package com.aai.tools;

import com.aai.tools.AAI_Tools;

import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.util.Log;
import android.view.Surface;
import android.view.IWindowManager;
import android.os.ServiceManager;
import android.os.RemoteException;
import android.content.ContentResolver;
import android.preference.CheckBoxPreference;
import android.preference.PreferenceScreen;

import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import android.os.SystemProperties;

public class ScreenOutputActivity extends PreferenceActivity implements Preference.OnPreferenceChangeListener{
    private static final String LOG_TAG = "ScreenOutput";
    private static final boolean DEBUG = false;

    private static final String KEY_SCREEN_ORIENTATION = "orientation";
    private static final String KEY_SCREEN_PORT = "screenport";
    private static final String KEY_SCREEN_RESOLUTION = "resolution";
    private static final String ID_AUDIO_FIXED = "audio_analog";
    private static final String ID_NAVIGATIONBAR = "hide_navigationbar";
    
    private ListPreference mScreenOrientation;
    private ListPreference mScreenPort;
    private ListPreference mScreenResolution;
	private CheckBoxPreference cbAudio_fixed;
	private CheckBoxPreference cbHide_navigationbar;
	
	private static final String FIXED_AUDIO_PROP_NAME = "persist.sys.fixed.audio";
	private static final String HIDE_NAVIGATIONBAR_PROP_NAME = "persist.sys.hide.navigationbar";
	
    private static final String SCREEN_PORT_KEYWORD = "set_display = run ";
    private static final String SCREEN_RESOLUTION_KEYWORD = "def_video = ";
    
    private String[] argget = { "rtx_setenv", "--list"};
    private String[] argset = { "rtx_setenv", "--set", "def_video=1024x768@60"};
    
    private IWindowManager mWindowManager;

    @Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		this.addPreferencesFromResource(R.layout.screenoutput);
        if (DEBUG) Log.d(LOG_TAG, "onCreate");
		
        mWindowManager = IWindowManager.Stub.asInterface(ServiceManager.getService("window"));

        init(KEY_SCREEN_ORIENTATION);
		init(KEY_SCREEN_PORT);
		init(KEY_SCREEN_RESOLUTION);	
		init(ID_AUDIO_FIXED);	
		init(ID_NAVIGATIONBAR);	
		
	}

    private void init(String arg){
        if (DEBUG) Log.d(LOG_TAG, "init");
        
    	if(KEY_SCREEN_ORIENTATION.equals(arg)){
    		mScreenOrientation = (ListPreference) findPreference(KEY_SCREEN_ORIENTATION);
    		mScreenOrientation.setValue("0");
			ContentResolver cv = getContentResolver(); 
	    	String str_accelerometer_rotation = android.provider.Settings.System.getString(cv,android.provider.Settings.System.ACCELEROMETER_ROTATION); 
	    	String str_user_rotation = android.provider.Settings.System.getString(cv,android.provider.Settings.System.USER_ROTATION); 
	    	if("1".equals(str_accelerometer_rotation)){
	        	mScreenOrientation.setValue("0");
	    	} else if(str_user_rotation == null) { 
	    		mScreenOrientation.setValue("0");
	    	} else if(! "".equals(str_user_rotation)) { 
	            int user_rotation_value = Integer.parseInt((String) str_user_rotation);	    	
    	       switch(user_rotation_value){
	           	case 1:
	           		mScreenOrientation.setValue("90");
		            break;
		        case 2:
		        	mScreenOrientation.setValue("180");
		        	break;
		        case 3:
		        	mScreenOrientation.setValue("270");
		        	break;
		        default:
		        	mScreenOrientation.setValue("0");
		        	break;
    	       }
	    	}

    		mScreenOrientation.setOnPreferenceChangeListener(this);
    	}
    	else if(KEY_SCREEN_PORT.equals(arg)){
    		mScreenPort = (ListPreference) findPreference(KEY_SCREEN_PORT);
    		String port = AAI_Tools.sSuCommand(argget);
            int index_port = port.indexOf(SCREEN_PORT_KEYWORD) + SCREEN_PORT_KEYWORD.length();

            if(index_port > 20){
            	String port_name = new String(port.substring(index_port, index_port+3));

            	if ("vga".equals(port_name)) {
            		mScreenPort.setValue("0");
            	} else if("hdm".equals(port_name)){
            		mScreenPort.setValue("1");
            	} else if("dua".equals(port_name)){
            		mScreenPort.setValue("2");
            	} else{
            		mScreenPort.setValue("0");    			
            	}
            } else {
        		mScreenPort.setValue("0");    			            	
            }
    		mScreenPort.setOnPreferenceChangeListener(this);
    	}
    	else if(KEY_SCREEN_RESOLUTION.equals(arg)){
    		mScreenResolution = (ListPreference) findPreference(KEY_SCREEN_RESOLUTION);
    		String resolution = AAI_Tools.sSuCommand(argget);
            int index_resolution_start = resolution.indexOf(SCREEN_RESOLUTION_KEYWORD) + SCREEN_RESOLUTION_KEYWORD.length();
        	String resolution_start = new String(resolution.substring(index_resolution_start));
            int index_resolution_end = resolution_start.indexOf("@");
    	    if (DEBUG) Log.d(LOG_TAG, "index_resolution = " + index_resolution_end);

            if(index_resolution_start > 20){
            	String resolution_name = new String(resolution.substring(index_resolution_start, index_resolution_start + index_resolution_end));
        	    if (DEBUG) Log.d(LOG_TAG, "resolution_name = " + resolution_name);
            	if ("640x480".equals(resolution_name)) {
            		mScreenResolution.setValue("1");
            	} else if("800x600".equals(resolution_name)){
            		mScreenResolution.setValue("1");
            	} else if("1024x768".equals(resolution_name)){
            		mScreenResolution.setValue("2");
            	} else if("1280x720".equals(resolution_name)){
            		mScreenResolution.setValue("3");
            	} else if("1280x800".equals(resolution_name)){
            		mScreenResolution.setValue("3");
            	} else if("1366x768".equals(resolution_name)){
            		mScreenResolution.setValue("4");
            	} else if("1920x1080".equals(resolution_name)){
            		mScreenResolution.setValue("5");
            	} else{
            		mScreenResolution.setValue("2");    			
            	}
            } else {
            	mScreenResolution.setValue("2");    			            	
            }
    		mScreenResolution.setOnPreferenceChangeListener(this);
    	}
    	else if(ID_AUDIO_FIXED.equals(arg)){
			cbAudio_fixed = (CheckBoxPreference) findPreference(ID_AUDIO_FIXED);
			cbAudio_fixed.setChecked("1".equals(SystemProperties.get(FIXED_AUDIO_PROP_NAME,"1")));
    	}
    	else if(ID_NAVIGATIONBAR.equals(arg)){
			cbHide_navigationbar = (CheckBoxPreference) findPreference(ID_NAVIGATIONBAR);
			cbHide_navigationbar.setChecked("1".equals(SystemProperties.get(HIDE_NAVIGATIONBAR_PROP_NAME,"1")));
    	}
    }
    
    @Override
    public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
		if (DEBUG) Log.d(LOG_TAG, "onPreferenceTreeClick=" + preference);
		
    	if (preference == cbAudio_fixed) {
    		boolean value = cbAudio_fixed.isChecked();
    		if(value == true){
    			SystemProperties.set(FIXED_AUDIO_PROP_NAME,"1");
    		}else{
    			SystemProperties.set(FIXED_AUDIO_PROP_NAME,"0");
    		}
    	}
        else if (preference == cbHide_navigationbar) {
    		boolean value = cbHide_navigationbar.isChecked();
    		if(value == true){
    			SystemProperties.set(HIDE_NAVIGATIONBAR_PROP_NAME,"1");
    		}else{
    			SystemProperties.set(HIDE_NAVIGATIONBAR_PROP_NAME,"0");
    		}
    	}

        return true;
    }
    
	@Override
	public boolean onPreferenceChange(Preference arg0, Object arg1) {
		// TODO Auto-generated method stub
		final String key = arg0.getKey();
        int value = Integer.parseInt((String) arg1);
	    if (DEBUG) Log.d(LOG_TAG, "onPreferenceChange = " + key + " : " + value);
	
		if (KEY_SCREEN_ORIENTATION.equals(key)) {
	       switch(value){
	        case 0:
				try {
	                    mWindowManager.freezeRotation(Surface.ROTATION_0);
	            } catch (RemoteException e) {
	            }
	            break;
	        case 90:
				try {
	                    mWindowManager.freezeRotation(Surface.ROTATION_90);
	            } catch (RemoteException e) {
	            }
	            break;
	        case 180:
				try {
	                    mWindowManager.freezeRotation(Surface.ROTATION_180);
	            } catch (RemoteException e) {
	            }
	            break;
	        case 270:
				try {
	                    mWindowManager.freezeRotation(Surface.ROTATION_270);
	            } catch (RemoteException e) {
	            }
	            break;            
	        case 360:
				try {
	                    mWindowManager.thawRotation();
	            } catch (RemoteException e) {
	            }
	            break;
	        default:
				try {
	                    mWindowManager.freezeRotation(Surface.ROTATION_0);
	                    mWindowManager.thawRotation();
	            } catch (RemoteException e) {
	            }
	            break;
	        }
		}
		
		if (KEY_SCREEN_PORT.equals(key)) {
		       switch(value){
		        case 0:
		        	argset[2] = "set_display=run vga"; 
            		break;
		        case 1:
		        	argset[2] = "set_display=run hdmi"; 
		            break;
		        case 2:
		        	argset[2] = "set_display=run dual-hdmi"; 
		       		/*AAI_Tools.sSuCommand(argset);
            		String resolution_check = mScreenResolution.getValue();
            		int resolution_int = Integer.valueOf(resolution_check);
            		if (resolution_int > 2 && resolution_int < 5) {
            			argset[2] = "def_video=1024x768@60"; 
    		       		AAI_Tools.sSuCommand(argset);
                		mScreenResolution.setValue("2");						
					}*/
		            break;
		        default:
		        	argset[2] = "set_display=run vga"; 
		            break;
		        }
       		AAI_Tools.sSuCommand(argset);
			}
			
		if (KEY_SCREEN_RESOLUTION.equals(key)) {
			if(port_check(value)){
		       switch(value){
		        case 0:
            		argset[2] = "def_video=1024x768@60"; 
            		break;
		        case 1:
            		argset[2] = "def_video=800x600@60"; 
		            break;
		        case 2:
            		argset[2] = "def_video=1024x768@60"; 
		            break;
		        case 3:
            		argset[2] = "def_video=1280x720@60"; 
		            break;
		        case 4:
            		argset[2] = "def_video=1366x768@60"; 
		            break;
		        case 5:
            		argset[2] = "def_video=1920x1080@60"; 
		            break;
		        default:
            		argset[2] = "def_video=1024x768@60"; 
		            break;
		        }
		       AAI_Tools.sSuCommand(argset);
			}
		}
		return true;
	}

    private boolean port_check(int value){
    	if ( value > 2 && value < 5 ){
    		String port_check = mScreenPort.getValue();
			if ("2".equals(port_check)) {
        		argset[2] = "def_video=1024x768@60"; 
        		AAI_Tools.sSuCommand(argset);
    			mScreenResolution.setValue("2");
    			return false;
			}
    	}
		return true;
    }

}
