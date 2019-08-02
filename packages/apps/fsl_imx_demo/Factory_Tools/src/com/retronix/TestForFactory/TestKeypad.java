package com.retronix.TestForFactory;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.content.Intent;
import android.os.Bundle;
import android.os.CountDownTimer;
import android.view.Display;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;

public class TestKeypad extends Activity{

	private LinearLayout mLinearLayout;
	private PopupWindow menu;
	private int screenWidth,screenHeigh;
	private View view;
	private TextView mKeypadLabel,mTextView;
	private boolean mSendMessage,mIsback,mIsMenu,mIsVolumeDown,mIsVolumeUp;
	private final static char TRUE = (char)0x11;
	private final static char FALSE = (char)0x10;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
		
		setContentView(R.layout.keypadlayout);

		init();
		
		mKeypadLabel.setText("Press keypad : ");
		
		mLinearLayout.setOnClickListener(new RelativeLayout.OnClickListener(){
			@Override
			public void onClick(View arg0) {
				menu.setAnimationStyle(R.style.pop_bootpm_anim_style);
				menu.setBackgroundDrawable(TestKeypad.this.getResources().getDrawable(R.drawable.videoback));
				menu.showAtLocation(mLinearLayout, Gravity.CENTER,screenWidth,screenHeigh);
				clear();
				
				Button keypadfinish = (Button)view.findViewById(R.id.keypadfinish);
				keypadfinish.setOnClickListener(new Button.OnClickListener() {
					@Override
					public void onClick(View v) {
						menu.dismiss();
						TestKeypad.this.finish();
					}
				});
			}
		});
		
		new CountDownTimer(60000,1000){
            @Override
            public void onFinish() {
            	if(!mSendMessage){
            		Intent WaringIntent = new Intent("send_keypad_message");
            		char menu,vol_up,vol_down,back;
            		if(mIsMenu) menu = TRUE;
            		else menu = FALSE;
            		if(mIsVolumeUp) vol_up = TRUE;
            		else vol_up = FALSE;
            		if(mIsVolumeDown) vol_down = TRUE;
            		else vol_down = FALSE;
            		if(mIsback) back = TRUE;
            		else back = FALSE;
            		WaringIntent.putExtra("menu", menu);
            		WaringIntent.putExtra("vol_up", vol_up);
            		WaringIntent.putExtra("vol_down", vol_down);
            		WaringIntent.putExtra("back", back);
            		TestKeypad.this.sendBroadcast(WaringIntent);
            	}
            }
            @Override
            public void onTick(long millisUntilFinished) {
            	if(!mSendMessage){
	            	if(mIsback && mIsMenu && mIsVolumeDown && mIsVolumeUp){
	            		Intent WaringIntent = new Intent("send_keypad_message");
	            		WaringIntent.putExtra("menu", TRUE);
	            		WaringIntent.putExtra("vol_up", TRUE);
	            		WaringIntent.putExtra("vol_down", TRUE);
	            		WaringIntent.putExtra("back", TRUE);
	            		TestKeypad.this.sendBroadcast(WaringIntent);
	            		mSendMessage = true;
	            	}
            	}
            }
        }.start();
	}
	
	private void init(){
		Display display = this.getWindowManager().getDefaultDisplay();
		screenWidth = display.getWidth();
		screenHeigh = display.getHeight();	
		mLinearLayout = (LinearLayout)findViewById(R.id.keypadlayout);
		view = this.getLayoutInflater().inflate(R.layout.keypad, null);
		menu = new PopupWindow(view);
		menu.setWidth(LayoutParams.WRAP_CONTENT);
		menu.setHeight(LayoutParams.WRAP_CONTENT);
		menu.setFocusable(true);
		
		mKeypadLabel = (TextView)findViewById(R.id.keypadlabel);
		mTextView = (TextView)findViewById(R.id.keypadtextview);
		
		mSendMessage = false;
		mIsback = false;
		mIsMenu = false;
		mIsVolumeDown = false;
		mIsVolumeUp = false;
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if(keyCode == KeyEvent.KEYCODE_BACK){
			clear();
			mTextView.setText("back");
			mIsback = true;
		}
		else if(keyCode == KeyEvent.KEYCODE_POWER){
			clear();
			mTextView.setText("power");
		}
		else if(keyCode == KeyEvent.KEYCODE_MENU){
			clear();
			mTextView.setText("menu");
			mIsMenu = true;
		}
		else if(keyCode == KeyEvent.KEYCODE_VOLUME_DOWN){
			clear();
			mTextView.setText("volume down");
			mIsVolumeDown = true;
		}
		else if(keyCode == KeyEvent.KEYCODE_VOLUME_UP){
			clear();
			mTextView.setText("volume up");
			mIsVolumeUp = true;
		}
		return true;
	}

	private void clear(){
		mTextView.setText("");
	}

}
