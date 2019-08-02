package com.retronix.TestForFactory;

import android.app.Activity;
import android.os.Bundle;

public class TestFinish extends Activity {
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.finish);
		
		this.finish();
		this.onDestroy();
		//android.os.Process.killProcess(android.os.Process.myPid());
		System.exit(0);
	}
}
