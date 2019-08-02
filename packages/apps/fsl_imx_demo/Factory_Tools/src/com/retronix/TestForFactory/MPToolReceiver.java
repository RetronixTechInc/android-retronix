package com.retronix.TestForFactory;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class MPToolReceiver extends BroadcastReceiver{

	private static final String LOG_TAG = "BootReceiver";
	private static final boolean DEBUG = true;
	
	/*	Start Service */
	public void onReceive(Context context, Intent intent) {
		String mIntent = intent.getAction();
		Intent _t = new Intent("android.intent.MPToolaction.receive");
		_t.putExtra("arg0", "We love RTX");

		if(mIntent.equals("android.intent.MPToolaction.send")){
		    if (DEBUG) Log.d(LOG_TAG, "onReceive Tom******************: android.intent.MPToolaction.send");	
			String arg0 = intent.getStringExtra("arg0");
			String arg1 = intent.getStringExtra("arg1");
			String arg2 = intent.getStringExtra("arg2");
			Log.d(LOG_TAG, "Tom-------arg0 :" + arg0 + "; arg1:" + arg1 + "; arg2:" + arg2);
			if("I love RTX".equals(arg0)){
				if("IOTest".equals(arg1)){
					_t.putExtra("arg1", arg1);
					_t.putExtra("arg2", "pass");					
		            Intent newIntent = new Intent(context, TestVideo.class);
			        newIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK); //If activity is not launched in Activity environment, this flag is mandatory to set
			        context.startActivity(newIntent);
				}else if("StableTest".equals(arg1)){
					_t.putExtra("arg1", arg1);
					_t.putExtra("arg2", "pass");					
		            Intent newIntent = new Intent(context, CheckResult.class);
			        newIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK); //If activity is not launched in Activity environment, this flag is mandatory to set
			        context.startActivity(newIntent);
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

}
