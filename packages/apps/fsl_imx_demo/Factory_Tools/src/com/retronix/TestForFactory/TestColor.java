package com.retronix.TestForFactory;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
//import android.view.Display;
//import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
//import android.view.ViewGroup.LayoutParams;
//import android.widget.Button;
import android.widget.LinearLayout;
//import android.widget.PopupWindow;
import android.widget.RelativeLayout;

public class TestColor extends Activity {

	private LinearLayout mLinearLayout;
	public enum ColorEnum{White,Black,Red,Green,Blue};
	private ColorEnum mShowColor;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
				
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
		
		setContentView(R.layout.colorlayout);
		
		init();

		mLinearLayout.setOnClickListener(new RelativeLayout.OnClickListener(){
			@Override
			public void onClick(View arg0) {
				
				switch(mShowColor)
				{
					case White:
						mShowColor = ColorEnum.Black;
						mLinearLayout.setBackgroundColor(Color.rgb(255, 255, 255));
						break;
					case Black:
						mShowColor = ColorEnum.Red;
						mLinearLayout.setBackgroundColor(Color.rgb(0, 0, 0));
						break;
					case Red:
						mShowColor = ColorEnum.Green;
						mLinearLayout.setBackgroundColor(Color.rgb(255, 0, 0));
						break;
					case Green:
						mShowColor = ColorEnum.Blue;
						mLinearLayout.setBackgroundColor(Color.rgb(0, 255, 0));
						break;
					case Blue:
						mShowColor = ColorEnum.White;
						mLinearLayout.setBackgroundColor(Color.rgb(0, 0, 255));
						break;
				}
			}
		});
	}
	
	private void init(){
		
		mLinearLayout = (LinearLayout)findViewById(R.id.layout);
		mLinearLayout.setBackgroundColor(Color.WHITE);
				
		mShowColor = ColorEnum.Black;
	}

}
