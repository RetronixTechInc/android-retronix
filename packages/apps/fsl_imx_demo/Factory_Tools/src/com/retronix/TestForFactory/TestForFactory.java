package com.retronix.TestForFactory;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;

public class TestForFactory extends Activity {
    
	private Button mTestAllStorage,mTestColor,mTestKeypad,mFinishTest;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        
        setContentView(R.layout.main);
        
    	mTestAllStorage = (Button)findViewById(R.id.allstorage);
        mTestKeypad = (Button)findViewById(R.id.keypad);
        mTestColor = (Button)findViewById(R.id.color);
        mFinishTest = (Button)findViewById(R.id.close);
        
        mTestAllStorage.setOnClickListener(new Button.OnClickListener(){
			@Override
			public void onClick(View arg0) {
				Intent video = new Intent(TestForFactory.this,TestVideo.class);
		        startActivity(video);
			}
        });  
                
        mTestKeypad.setOnClickListener(new Button.OnClickListener(){
			@Override
			public void onClick(View arg0) {
				Intent keypad = new Intent(TestForFactory.this,CheckResult.class);
		        startActivity(keypad);
			}
        });  
        
        mTestColor.setOnClickListener(new Button.OnClickListener(){
			@Override
			public void onClick(View arg0) {
				Intent color = new Intent(TestForFactory.this,TestColor.class);
		        startActivity(color);
			}      	
        });
        mFinishTest.setOnClickListener(new Button.OnClickListener(){
			@Override
			public void onClick(View arg0) {
				Intent finish = new Intent(TestForFactory.this,TestFinish.class);
		        startActivity(finish);
		        android.os.Process.killProcess(android.os.Process.myPid());
			}      	
        });
    }

	@Override
	protected void onDestroy() {
		super.onDestroy();
	}

}