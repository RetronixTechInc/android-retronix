package com.example.receive_tools;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.os.IPowerManager;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.os.SystemClock;
import android.content.ComponentName;
import android.os.RecoverySystem;
import android.os.RemoteException;
import java.io.*;
import android.os.Handler;

public class BootReceiver extends BroadcastReceiver{

	private static final String LOG_TAG = "Tom===receive_tools";
	private static final boolean DEBUG = true;
	private String retstring ;
	private static final String MAPPER_DISABLE_PROP_NAME = "persist.sys.mapper.disable";
	private EthernetActivity ethernetcontroll;
	private OtaAppActivity otaappactivity;
    private static String ALIVE_NAME ;
    private static String START_PACKAGE_NAME ;
    private static String START_PACKAGE_CLASS ;
    private static int alive_check_time = 10000 ;
    private static int alive_action = 0 ;
    private static int died_count = 0 ;
    private Context mContext ;
    private static String mem_free_size = "200000" ;
    private int inputnum = 0 ;

    /*	Start Service */
	public void onReceive(Context context, Intent intent) {
		String mIntent = intent.getAction();
		Intent _t = new Intent("android.intent.AAIaction.receive");
		_t.putExtra("arg0", "We love RTX");
		mContext = context ;
        
        Log.d(LOG_TAG, "1234567890");
		
		if(mIntent.equals("android.intent.action.BOOT_COMPLETED")){
            Log.d(LOG_TAG, "android.intent.action.BOOT_COMPLETED");
			McuControlActivity.mcufunction("sg","0");
		}
		else if(mIntent.equals("android.intent.AAIaction.send")){
			String arg0 = intent.getStringExtra("arg0");
			String arg1 = intent.getStringExtra("arg1");
			String arg2 = intent.getStringExtra("arg2");
			String arg3 = intent.getStringExtra("arg3");
			Log.d(LOG_TAG, "arg0 :" + arg0 + "; arg1:" + arg1 + "; arg2:" + arg2 + "; arg3:" + arg3);
			if("I love RTX".equals(arg0)){
				if("poweroff".equals(arg1)){
					//EthernetManager mEthManager = new EthernetManager(context);			
					//ethernetcontroll = new EthernetActivity(mEthManager);
					//ethernetcontroll.enablewakeonlan();
					_t.putExtra("arg1", "poweroff");
					_t.putExtra("arg2", "pass");					
					shutdownsystem();
				}else if("setwakeuptime".equals(arg1)){
					_t.putExtra("arg1", arg1);
					if(arg2.length() == 19){
						retstring = McuControlActivity.mcufunction("sa",arg2);
						if("Fail".equals(retstring)){
							_t.putExtra("arg2", "ng");	
						}else{
							_t.putExtra("arg2", retstring);
						}
					}else{
						_t.putExtra("arg2", "ng");
					}
				}else if("getwakeuptime".equals(arg1)){
					retstring = McuControlActivity.mcufunction("a",arg2);
					_t.putExtra("arg1", arg1);
					_t.putExtra("arg2", retstring);					
				}else if("disableinput".equals(arg1)){
					_t.putExtra("arg1", arg1);
					if(isNumeric(arg2)){
						int inputtype = Integer.valueOf(arg2);
						if(inputtype < 256){
							SystemProperties.set(MAPPER_DISABLE_PROP_NAME, arg2);
							_t.putExtra("arg2", "pass");					
						}else{
							_t.putExtra("arg2", "nemeric too big");
						}
					}else{
						_t.putExtra("arg2", "need nemeric");						
					}
				}else if("getinputdisable".equals(arg1)){
					String input_type = SystemProperties.get(MAPPER_DISABLE_PROP_NAME, "48");
					_t.putExtra("arg1", arg1);
					_t.putExtra("arg2", input_type);					
				}else if("setethernet".equals(arg1)){
					_t.putExtra("arg1", arg1);
					//EthernetManager mEthManager = new EthernetManager(context);				
					//ethernetcontroll = new EthernetActivity(mEthManager);
					//retstring = ethernetcontroll.ethernetset(arg2);
					retstring = "pass";
					if("Fail".equals(retstring)){
						_t.putExtra("arg2", "ng");	
					}else{
						_t.putExtra("arg2", retstring);
					}
				}else if("getethernet".equals(arg1)){
					_t.putExtra("arg1", arg1);
					//EthernetManager mEthManager = new EthernetManager(context);				
					//ethernetcontroll = new EthernetActivity(mEthManager);
					//retstring = ethernetcontroll.ethernetget();
					retstring = "pass";
					if("Fail".equals(retstring)){
						_t.putExtra("arg2", "ng");	
					}else{
						_t.putExtra("arg2", retstring);
					}
				}else if("killprocess".equals(arg1) || "pidsize".equals(arg1) ){
					if("mediaserver".equals(arg2))
					{
						retstring = KillprocessActivity.killfunction("/system/bin/mediaserver",arg3,mem_free_size);
					}
					else
					{
						retstring = KillprocessActivity.killfunction(arg2,arg3,mem_free_size);
					}
					_t.putExtra("arg1", arg1);
					_t.putExtra("arg2", retstring);					
				}else if("updatesystem".equals(arg1)){
					_t.putExtra("arg1", arg1);
				    //Intent i = new Intent(context , OtaAppActivity.class);
				    //i.setClassName("com.fsl.android.ota", "com.fsl.android.ota.OtaAppActivity");  
					//i.setAction("android.intent.action.MAIN");
					//i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
				    //context.startActivity(i);
					//_t.putExtra("arg2", "pass");
					
					if( arg2.length() > 7 ){
						otaappactivity = new OtaAppActivity();
						retstring = otaappactivity.updatesystem(arg2);
						if ("pass".equals(retstring)){
							File recoveryFile = new File(arg2);
							try {
								RecoverySystem.installPackage(context, recoveryFile);
							} catch (IOException e) {
							}
							_t.putExtra("arg2", "pass");
						}else{
							// version is latest, file-fail or signed-fail
							_t.putExtra("arg2", retstring);
						}
					}else{
						_t.putExtra("arg2", "ng");
					}				
				}else if("pidalive".equals(arg1)){
					_t.putExtra("arg1", arg1);
					if("add".equals(arg3)){
						if( alive_action == 0 ){
							String alive_arg[] = new String[] { };
							alive_arg = arg2.split(";");
							if ( alive_arg.length >= 3 ){
								ALIVE_NAME = alive_arg[0] ;
								START_PACKAGE_NAME = alive_arg[1] ;
								START_PACKAGE_CLASS = START_PACKAGE_NAME + "." + alive_arg[2] ;
								died_count = 0 ;
								_Handler.post(run);
								Log.v(LOG_TAG, "check_pidalive--monitor=" + ALIVE_NAME + ";START_PACKAGE_CLASS=" + START_PACKAGE_CLASS);        
								_t.putExtra("arg2", "pass");
								alive_action = 1;
							}else{
								if("tomlai.project2".equals(arg2))
								{
									ALIVE_NAME = "org." + arg2 ;
									START_PACKAGE_NAME = ALIVE_NAME ;
									START_PACKAGE_CLASS = START_PACKAGE_NAME + ".main" ;
									Log.v(LOG_TAG, "check_pidalive-----monitor=" + ALIVE_NAME);
									died_count = 0 ;
									_Handler.post(run);
									_t.putExtra("arg2", "pass");
									alive_action = 1;
								}
								else
								{
									_t.putExtra("arg2", "ng");
								}																		
							}
						}else{
							_t.putExtra("arg2", "ng");
						}
					}else if("del".equals(arg3)){
					  _Handler.removeCallbacks(run);
					  Log.v(LOG_TAG, "check_pidalive-----=removeCallbacks");        
					  alive_action = 0 ;
					  died_count = 0 ;
					  _t.putExtra("arg2", "pass");					
					  retstring = McuControlActivity.mcufunction("sg","0");
					  if(! "success".equals(retstring)){
						 McuControlActivity.mcufunction("sg","0");
					  }
					}else{
					  _t.putExtra("arg2", "ng");											
					}
				}else if("alivetime".equals(arg1)){
					_t.putExtra("arg1", arg1);
					if(isNumeric(arg2)){
						inputnum = Integer.valueOf(arg2);
						if(inputnum > 1000){
							alive_check_time = inputnum;	
						}else{
							alive_check_time = 1000;
						}
						_t.putExtra("arg2", "pass");	
					}else{
						_t.putExtra("arg2", "need nemeric");						
					}
				}else{
					_t.putExtra("arg1", "not support");
					_t.putExtra("arg2", "ng");					
				}
			}
		}else{
			_t.putExtra("arg1", "ng");
			_t.putExtra("arg2", "ng");
		}
		
		context.sendBroadcast(_t);
	}

    private void shutdownsystem(){
		IPowerManager power=IPowerManager.Stub.asInterface(ServiceManager.getService("power"));
		if(power != null) {
			try {
				/**
				 * frameworks/base/core/java/android/os/IPowerManager.aidl
				 * void shutdown(boolean confirm, boolean wait);
				 */
				power.goToSleep(SystemClock.uptimeMillis(), PowerManager.GO_TO_SLEEP_REASON_APPLICATION, 0); 
				power.shutdown(false,false);
			} catch (RemoteException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
    }

    private static boolean isNumeric(String str){
    	   for(int i=str.length();--i>=0;){
    	      int chr=str.charAt(i);
    	      if(chr<48 || chr>57)
    	         return false;
    	   }
    	   return true;
    	}
    

	Handler _Handler = new Handler();
    Runnable run = new Runnable() {
		@Override
		public void run() {
			// TODO Auto-generated method stub
			if( alive_action == 1 ){
				_Handler.postDelayed(run, alive_check_time);
				check_pidalive();
			}
		}
	};
	
	private void check_pidalive(){		
		String retstring; 
		McuControlActivity.mcufunction("sg","120");
		retstring = KillprocessActivity.alivefunction(ALIVE_NAME);
		if( "died".equals(retstring) ){
			died_count++ ;
		}else if( "alive".equals(retstring) ){
			died_count = 0 ;
		}
		if (DEBUG) Log.v(LOG_TAG, "retstring=" + retstring + "; died_count=" + died_count);        
		if( died_count >= 3 ){
			died_count = 0 ;
			Log.v(LOG_TAG, "check_pidalive-----startActivity=" + START_PACKAGE_NAME);
		    Intent i = new Intent();
		    i.setClassName(START_PACKAGE_NAME, START_PACKAGE_CLASS);  
	        i.setAction("android.intent.action.MAIN"); //MyActivity action defined in AndroidManifest.xml
	        i.addCategory("android.intent.category.LAUNCHER");//MyActivity category defined in AndroidManifest.xml
			i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
			mContext.startActivity(i);			
		}
	}

}
