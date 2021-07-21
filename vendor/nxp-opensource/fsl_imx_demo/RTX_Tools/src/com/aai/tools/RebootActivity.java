package com.aai.tools;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.PowerManager;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.util.Log;

public class RebootActivity extends Activity{

    private static final String LOG_TAG = "RebootActivity";
    private static final boolean DEBUG = false;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		// TODO Auto-generated method stub
		super.onCreate(savedInstanceState);
        if (DEBUG) Log.d(LOG_TAG, "onCreate");
        
        AlertDialog.Builder dialog = new AlertDialog.Builder(RebootActivity.this);
        dialog.setTitle(R.string.reboot_title);
        dialog.setMessage(R.string.reboot_msg);
        dialog.setPositiveButton(R.string.dialog_sure,
            new DialogInterface.OnClickListener(){
                public void onClick(DialogInterface arg0, int arg1){
            		//Test intent to call URL
            		rebootsystem();
                }
            });
        dialog.setNegativeButton(R.string.dialog_cancel,
                new DialogInterface.OnClickListener(){
                    public void onClick(DialogInterface arg0, int arg1){
                    }
                });
        dialog.show();       
	}

    private void rebootsystem(){
		PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);

	    pm.reboot("null");
	    //pm.reboot("recovery");
//		Intent intent = new Intent(Intent.ACTION_REBOOT);
//		intent.putExtra("nowait", 1);
//		intent.putExtra("interval", 1);
//		intent.putExtra("window", 0);
//		sendBroadcast(intent);
//	    finish();
    }
    
	//@Override
	//protected void onDestroy() {
		// TODO Auto-generated method stub
		//super.onDestroy();
		//android.os.Process.killProcess(android.os.Process.myPid());
	//}

}
