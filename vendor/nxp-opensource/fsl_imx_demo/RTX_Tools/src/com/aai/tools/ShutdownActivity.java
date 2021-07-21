package com.aai.tools;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.util.Log;
import android.os.IPowerManager;
import android.os.RemoteException;
import android.os.ServiceManager;

public class ShutdownActivity extends Activity{

    private static final String LOG_TAG = "ShutdownActivity";
    private static final boolean DEBUG = false;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// TODO Auto-generated method stub
		super.onCreate(savedInstanceState);
        if (DEBUG) Log.d(LOG_TAG, "onCreate");
        
        AlertDialog.Builder dialog = new AlertDialog.Builder(ShutdownActivity.this);
        dialog.setTitle(R.string.shutdown_title);
        dialog.setMessage(R.string.shutdown_msg);
        dialog.setPositiveButton(R.string.dialog_sure,
            new DialogInterface.OnClickListener(){
                public void onClick(DialogInterface arg0, int arg1){
            		//Test intent to call URL
                	shutdownsystem();
                }
            });
        dialog.setNegativeButton(R.string.dialog_cancel,
                new DialogInterface.OnClickListener(){
                    public void onClick(DialogInterface arg0, int arg1){
                    }
                });
        dialog.show();       
	}

    private void shutdownsystem(){
		IPowerManager pm = IPowerManager.Stub.asInterface(ServiceManager.getService("power"));
		if(pm!=null) {
			try {
				/**
				 * frameworks/base/core/java/android/os/IPowerManager.aidl
				 * void shutdown(boolean confirm, String reason, boolean wait);
 				 * void reboot(boolean confirm, String reason, boolean wait);
				 */
				pm.shutdown(false, null, false);
			} catch (RemoteException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
    }
    
	//@Override
	//protected void onDestroy() {
		// TODO Auto-generated method stub
		//super.onDestroy();
		//android.os.Process.killProcess(android.os.Process.myPid());
	//}

}
