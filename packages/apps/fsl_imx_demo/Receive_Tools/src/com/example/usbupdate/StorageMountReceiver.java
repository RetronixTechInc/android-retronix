package com.example.usbupdate;

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
import android.os.Build;

import java.io.*;

import android.os.Handler;

public class StorageMountReceiver extends BroadcastReceiver{

	private static final String LOG_TAG = "Tom===StorageMountReceiver";
	private static final boolean DEBUG = false;

    private Context mContext ;
    private FileUpdate objFile = null;
	private static final String PROP_USB_UPDATE = "persist.sys.usb.update";

	private static String DEVICPROJECTNAME = "";
	private static float DEVICEVERSION = -1;
    
    /*	Start Service */
	public void onReceive(Context context, Intent intent) {
		String mIntent = intent.getAction();
		mContext = context ;

        if (mIntent.equals("android.intent.action.MEDIA_MOUNTED")) 
        {
        	if (DEBUG) Log.d(LOG_TAG, mContext.getClass() + "; mIntent=" + mIntent);   
            getSystemVersion();  
			objFile = new FileUpdate(mContext);
			objFile.read();
            if(!getFileDir(intent.getData().getPath()))
            {
                SystemProperties.set(PROP_USB_UPDATE, "true");
            }
        }
	}

	private boolean getFileDir(String filePath)
	{
        boolean bret = false;
        
		File f=new File(filePath);  
		File[] files = null ;
		if (f.canRead())
		{
			files=f.listFiles();
		}

		if(files != null)
		{
            for(int i=0;i<files.length;i++)
            {
              File file=files[i];
              if (file.getName().endsWith(".zip") || file.getName().endsWith(".rtx"))
              {
                if (DEBUG) Log.d(LOG_TAG, "file.getName()=" + file.getName());  
                try{
                    if(writeRecovery(file.getPath())){
                        bret = true;
                        break;
                    }
                } catch (IOException e) {

                }
              }
            }
		}
        return bret;
	}
    
    private boolean writeRecovery(String sfile) throws IOException {
		if (DEBUG) Log.d(LOG_TAG, "sfile=" + sfile);
        
        if(sfile.contains("/emulated") || !sfile.contains("/update-R207"))
        {
            return false;
        }

		File file = new File(sfile); 
		
		String path_abs = "/mnt/udisk/";
		String file_name = file.getName();
		File recoveryFile = new File(path_abs+file_name);
		
		if(file.getName().endsWith(".rtx"))
		{
			if(compareSystemVersion(sfile))
			{
				RecoverySystem.installPackage(mContext, recoveryFile);
				return true;
			}
		}
		else if(file.getName().endsWith(".zip"))
		{
            if(upatefileverify(sfile))
            {
                RecoverySystem.installPackage(mContext, recoveryFile);
                return true;
            }
		}
		
	    return false;
	}
    
    private boolean compareSystemVersion(String path) {
		File systemzip = new File(path);
		FileInputStream _zipinput;
		try {
			_zipinput = new FileInputStream(systemzip);
			byte[] head = new byte[16];
			_zipinput.read(head);
			_zipinput.close();		
			
			/////////////////////////check force update/////////////////////////// 
			String filehead = new String(head);
			if (DEBUG) Log.d(LOG_TAG, "filehead=" + filehead);  
     
			String forceupdate = filehead.substring(0,3);
			String fileProjectname = filehead.substring(8,12);
			String fileVersion = filehead.substring(12,16);

	       
	       if (DEBUG) Log.d(LOG_TAG, "fileProjectname=" + fileProjectname);        
	       if (DEBUG) Log.d(LOG_TAG, "fileVersion=" + fileVersion);        
	       if(forceupdate.equals("RTX")){
	       	//if( fileVersion > DEVICEVERSION){
	       		return true;
	       	//}
	       }else{
	       	if(fileProjectname.equals(DEVICPROJECTNAME)){
	       		if( fileVersion.equals(DEVICEVERSION))
						return true;
	       	}		
	       }
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		} catch (NumberFormatException e) {
			e.printStackTrace();
		}
		return false;
	}
	
	RecoverySystem.ProgressListener recoveryVerifyListener = new RecoverySystem.ProgressListener() {
		public void onProgress(int progress) {
			Log.d(LOG_TAG, "verify progress" + progress);
		}
	};
	
	private boolean upatefileverify(String path)
	{
		boolean bret = false;
		
		File recoveryFile = new File(path);
		  try {
        	 //mWakelock.acquire();
        	 RecoverySystem.verifyPackage(recoveryFile, recoveryVerifyListener, null);
        	 bret = true;
         } catch (Exception e1) {
        	 e1.printStackTrace();
        	 return bret;
         } finally {
        	 //mWakelock.release();
         }
         
         return bret;
		
	}
    
    public void getSystemVersion(){
    	String systemversion = Build.ID;
 
		if(systemversion.length() > 7)
		{
			DEVICPROJECTNAME = systemversion.substring(0, 4);
			String sVer = systemversion.substring(4, 8) ;
			boolean bVer = isNumeric(sVer);
			if(bVer)
			{
				try{
					DEVICEVERSION = new Float(sVer);
				} catch (Exception e) { 
					e.printStackTrace(); 
				}
			}
		}
		if (DEBUG) Log.d(LOG_TAG, "getSystemVersion=" + DEVICEVERSION);    
    }
    
    private static boolean isNumeric(String str){
		if(str == null)
		{
			return false;
		}
		
	   for(int i=str.length();--i>=0;){
	      int chr=str.charAt(i);
	      if(chr<46 || chr>57 || chr==47)
	         return false;
	   }
	   
	   return true;
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
    
	Handler _Handler = new Handler();
    Runnable run = new Runnable() {
		@Override
		public void run() {
			// TODO Auto-generated method stub
			//	_Handler.postDelayed(run, 10000);
				check_pidalive();
		}
	};
	
	private void check_pidalive(){		
	}

}
