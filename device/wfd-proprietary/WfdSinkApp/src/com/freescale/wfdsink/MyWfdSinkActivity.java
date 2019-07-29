/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.freescale.wfdsink;

import java.util.ArrayList;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.View;
import android.widget.GridView;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.net.wifi.p2p.WifiP2pDevice;
import com.fsl.wfd.WfdSink;
import android.widget.BaseAdapter;
import java.util.List;
import android.view.LayoutInflater;
import android.content.Context;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.view.View.OnTouchListener;
import android.view.MotionEvent;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.UserHandle;
import android.content.Intent;
import android.net.wifi.p2p.WifiP2pManager;
import android.media.AudioManager;
import android.os.Bundle;
import com.fsl.wfd.SinkView;
import com.fsl.wfd.SinkActivityBase;

public class MyWfdSinkActivity extends SinkActivityBase{

	private static final String TAG = "MySinkActivity";
	private WfdSink mWfdSink;
	//Surface Member
	private boolean mStarted = false;
	private boolean mConnected = false;

	//UI Compoment Member
	private ImageButton mImageButton;
	private ArrayList<String> mSourcePeers = new ArrayList<String>();
	private GridView mGridView;
	private SinkView mSurfaceView;
	private PictureAdapter mPictureAdapter;
	private DisplayMetrics mDisplayMetrics;
	private boolean mButtonShow = false;
	private String mThisName;
	private TextView currentdevice;
	private TextView status;
	private ProgressBar progressBar;
	private LinearLayout layout_devicelist;
	private LinearLayout layout;
	private RelativeLayout relativelayout;
	private int navigation;
	private static View sink_main;
	private View divider_view;
	private boolean isExit;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
				WindowManager.LayoutParams.FLAG_FULLSCREEN);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		getWindow().getDecorView()
			.setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
					| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
	}

	@Override
	protected void initView() {

		//Root View
		sink_main = getLayoutInflater().from(this).inflate(R.layout.sink_main, null);
		setContentView(sink_main);
		mWfdSink = getWfdSink();

		mSurfaceView = (SinkView)findViewById(R.id.sink_preview);    
		mGridView = (GridView)findViewById(R.id.gridview);
		status = (TextView)findViewById(R.id.status);
		layout = (LinearLayout)findViewById(R.id.linearLayout1);
		relativelayout = (RelativeLayout)findViewById(R.id.relativelayout);
		layout_devicelist = (LinearLayout)findViewById(R.id.layout_device_list);
		progressBar = (ProgressBar)findViewById(R.id.progressBar);
		mPictureAdapter = new PictureAdapter(this);
		mDisplayMetrics = new DisplayMetrics();
		getWindowManager().getDefaultDisplay().getMetrics(mDisplayMetrics);

		mSurfaceView.setVisibility(View.INVISIBLE);
		mSurfaceView.setWfdSink(mWfdSink);
		mGridView.setVisibility(View.VISIBLE);
		mGridView.setAdapter(mPictureAdapter); 
		isExit = false;
	}

	@Override
	protected void onWifiDisabled() {
		Log.v(TAG, "onWifiDisabled");		
		wifiDialog();
	}

	@Override
	protected void onWifiEnabled() {
		Log.v(TAG, "onWifiEnabled");
		if(!isExit)
			startSearch();
	}

	@Override
	protected void handleDeviceChangeOnUiThread(WifiP2pDevice[] devices) {
		super.handleDeviceChangeOnUiThread(devices);		
		Log.v(TAG, "onDeviceListChange");
		mSourcePeers.clear();
		for (WifiP2pDevice device : devices) {
			mSourcePeers.add(device.deviceName);
		}
		mPictureAdapter.setSourcePeers(mSourcePeers);
		mGridView.setAdapter(mPictureAdapter);
		mGridView.postInvalidate();
	}

	@Override
	protected void handleSinkConnectOnUiThread(boolean connected) {
		super.handleSinkConnectOnUiThread(connected);
		Log.v(TAG, "---------handle Sink Connect----------");
		status.setText(R.string.wifi_status_connected);
		stopSearch();
		sink_main.setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
		handleConnected(connected);	
	}

	@Override
	protected void handleDeviceUpdateOnUiThread() {
		currentdevice = (TextView)findViewById(R.id.currentdevice);
		if (mThisName != mWfdSink.getDeviceName())
			mThisName = mWfdSink.getDeviceName();
		String ssid = getString(R.string.wifi_ssid) + ":";
		currentdevice.setText(ssid + mWfdSink.getDeviceName());
	}

	@Override
	protected void handleSinkVideoSizeChangeOnUiThread(int height, int width) {
	}

	@Override
	protected void onStartSearch() {
		super.onStartSearch();
		status.setText(R.string.wifi_status_searching);
	}

	@Override
	protected void onExit() {
		super.onExit();
		exitDialog();
	}

	@Override
	protected void handlRtspStartBeginOnUiThread() {
		super.handlRtspStartBeginOnUiThread();
	}
	@Override
	protected void handleRtspStartFinishedOnUiThread() {
		super.handleRtspStartFinishedOnUiThread();
	}
	@Override
	protected void handleRtspStoptBeginOnUiThread() {
		super.handleRtspStoptBeginOnUiThread();
	}

	@Override
	protected void onStop(){
		super.onStop();
		if (mStarted) {
			stopPlayer();
		}

		if (mConnected) {
			//           disconnectPeer();
			mConnected = false;
		}
		stopSearch();
		//        mWfdSink.setMiracastMode(WifiP2pManager.MIRACAST_DISABLED);
	}

	private void wifiDialog() {
		Dialog dialog = new AlertDialog.Builder(MyWfdSinkActivity.this)
			.setTitle(R.string.wifi_no_open).setMessage(R.string.wifi_cancel_exit).setIcon(R.drawable.ic_dialog)
			.setPositiveButton(R.string.wifi_confirm, new DialogInterface.OnClickListener() {

					@Override
					public void onClick(DialogInterface dialog, int which) {
					Intent intent = new Intent(android.provider.Settings.ACTION_WIFI_SETTINGS);
					intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
						Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED |
						Intent.FLAG_ACTIVITY_CLEAR_TOP);
					startActivityAsUser(intent, new
						UserHandle(UserHandle.USER_CURRENT));
					}
					}).setNegativeButton(R.string.wifi_cancel, new DialogInterface.OnClickListener() {

						@Override
						public void onClick(DialogInterface dialog, int which) {
							System.exit(-1);
						}
					}).create();
		dialog.getWindow().setType(WindowManager.LayoutParams.TYPE_SYSTEM_ALERT);
		dialog.show();
	}

	private void exitDialog() {
		Dialog dialog = new AlertDialog.Builder(MyWfdSinkActivity.this)
			.setTitle(R.string.program_exit)
			.setMessage(R.string.program_exit_sure)
			.setIcon(R.drawable.ic_dialog)
			.setPositiveButton(R.string.wifi_confirm, new DialogInterface.OnClickListener(){

					@Override
					public void onClick(DialogInterface arg0, int arg1) {

					isExit = true;
					stopSearch();
					MyWfdSinkActivity.this.finish();
					//			mWfdSink.dispose();
					}

					}).setNegativeButton(R.string.wifi_cancel, new DialogInterface.OnClickListener() {

						@Override
						public void onClick(DialogInterface dialog, int which) {
							sink_main.setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
						}
					}).create();
		dialog.show();
	}

	private void handleConnected(boolean connected) {
		mConnected = connected;
		Log.v(TAG, "handleConnected is " + connected );
		if (mConnected) {
			stopSearch();
			layout.setVisibility(View.GONE);
			layout_devicelist.setVisibility(View.GONE);
			mSurfaceView.setVisibility(View.VISIBLE);
			mGridView.setVisibility(View.GONE);
			mStarted = true;
			//            startPlayer();
			//			mWfdSink.setMiracastMode(WifiP2pManager.MIRACAST_SINK);
		}
		else {
			stopPlayer();
			//          mWfdSink.setMiracastMode(WifiP2pManager.MIRACAST_DISABLED);
		}
	}

	private void startPlayer() {
		//	 	if (mSurfaceHolder == null) {
		//			mSurfaceView.setWaitingForSurface(true);
		//			return;
		//		}
		//		if (mSurfaceHolder.getSurface() != null){
		//        	mWfdSink.startRtsp(mSurfaceHolder.getSurface());
		//      	 	mStarted = true;
		//		}
	}	

	private void stopPlayer() {
		Log.v(TAG, "MySinkActivity stopPlayer");
		status.setText(R.string.wifi_status_disconnected);
		//  	if (!mStarted) {
		//          return;
		//     }
		mSourcePeers.clear();
		mPictureAdapter.setSourcePeers(mSourcePeers);
		mGridView.setAdapter(mPictureAdapter);
		mGridView.postInvalidate();
		layout.setVisibility(View.VISIBLE);
		layout_devicelist.setVisibility(View.VISIBLE);
		mSurfaceView.setVisibility(View.INVISIBLE);
		mGridView.setVisibility(View.VISIBLE);
		mStarted = false;
		//  		mWfdSink.stopActive();
		mConnected = false;
		if (!isExit)
			startSearch();
	}

	//    private void disconnectPeer() {
	//        mWfdSink.disconnect();
	//    }

	//View Adapter
	public class ViewHolder {
		public TextView mTitle;
		public ImageView mImage;
	}

	public class PictureAdapter extends BaseAdapter {
		private List<String> mPeers;
		private LayoutInflater mInflater;

		public PictureAdapter(Context context) {
			mInflater = LayoutInflater.from(context);
		}

		public void setSourcePeers(List<String> sourcePeers) {
			Log.w(TAG, "setSourcePeers");
			mPeers = sourcePeers;
		}

		@Override
		public int getCount() {
			if (mPeers != null) {
				return mPeers.size();
			}
			else {
				return 0;
			}
		}

		@Override
		public Object getItem(int position) {
			Log.w(TAG, "getItem:" + mPeers.get(position));
			return mPeers.get(position);
		}

		@Override
		public long getItemId(int position) {
			return position;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent) {
			ViewHolder viewHolder;
			if (convertView == null) {
				convertView = mInflater.inflate(R.layout.sink_pic, null);
				viewHolder = new ViewHolder();
				viewHolder.mTitle = (TextView)convertView.findViewById(R.id.pic_title);
				viewHolder.mImage = (ImageView)convertView.findViewById(R.id.pic_image);
				convertView.setTag(viewHolder);
			}
			else {
				viewHolder = (ViewHolder)convertView.getTag();
			}
	
			viewHolder.mTitle.setText((String)getItem(position));
			viewHolder.mImage.setImageResource(R.drawable.ic_hdmi);

			LayoutParams para;
			para = viewHolder.mImage.getLayoutParams();
			para.width = mDisplayMetrics.widthPixels/5;
			para.height = mDisplayMetrics.heightPixels/4;
			viewHolder.mImage.setLayoutParams(para);
			return convertView;
		}
	}
}
