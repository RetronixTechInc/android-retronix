/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc.
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

import com.fsl.wfd.WfdSink;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import java.util.List;
import java.util.ArrayList;
import android.widget.VideoView;
import android.provider.Settings;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.GridView;
import android.widget.TextView;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.view.View;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View.OnTouchListener;
import android.view.MotionEvent;
import android.view.InputDevice;
import android.view.Display;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.Window;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.ViewGroup.LayoutParams;
import android.content.Context;
import android.content.DialogInterface;
import android.content.IntentFilter;
import android.content.Intent;
import android.util.Log;
import android.util.DisplayMetrics;
import android.os.Handler;
import android.os.Bundle;
import android.os.Message;
import android.os.Parcelable;
import android.content.BroadcastReceiver;
import android.net.wifi.WifiManager;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pManager;
import android.view.View.OnSystemUiVisibilityChangeListener;
import android.os.UserHandle;
import android.os.UserManager;
import android.media.AudioManager;

public class WfdSinkActivity extends Activity implements SurfaceHolder.Callback
{
    private static final String TAG = "WfdSinkActivity";
    private static final int UPDATE_GRID_VIEW = 0x10;
    private static final int UPDATE_BUTTON_SHOW = 0x11;
    private static final int UPDATE_SURFACE = 0x12;
    private static final int START_PLAY = 0x13;
    private static final int STOP_PLAY = 0x14;
    private static final int DO_CONNECTED = 0x15;
    private static final int START_SEARCH = 0x16;
    private static final int DELAY = 6000;
    private WfdSink mWfdSink;
    private SurfaceHolder mSurfaceHolder = null;
    private boolean mWaitingForSurface = false;
    private boolean mStarted = false;
    private boolean mConnected = false;

    private ImageButton mImageButton;
    private ArrayList<String> mSourcePeers = new ArrayList<String>();
    private GridView mGridView;
    private SinkView mSurfaceView;
    private PictureAdapter mPictureAdapter;
    DisplayMetrics mDisplayMetrics;
    private boolean mButtonShow = false;
    private String mThisName;
    private TextView currentdevice;
    private TextView status;
    private ProgressBar progressBar;
    private LinearLayout layout_devicelist;
    private LinearLayout layout;
    private RelativeLayout relativelayout;
    private int navigation;
    public ProgressBar getProgressBar() {
            return progressBar;
    }
    public void setProgressBar(ProgressBar progressBar) {
            this.progressBar = progressBar;
    }
    private final BroadcastReceiver mWifiP2pReceiver = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (WfdSink.WFD_DEVICE_LIST_CHANGED_ACTION.equals(action)) {
                WifiP2pDevice[] devices;
                Parcelable[] devs;

                devs = intent.getParcelableArrayExtra(WfdSink.EXTRA_DEVICE_LIST);

                devices = new WifiP2pDevice[devs.length];
                for (int i=0; i<devs.length; i++) {
                    devices[i] = (WifiP2pDevice)devs[i];
                }

                mSourcePeers.clear();
                for (WifiP2pDevice device : devices) {
                    mSourcePeers.add(device.deviceName);
                }

                if (devs.length > 0) {
                    mWfdSink.setMiracastMode(WifiP2pManager.MIRACAST_SINK);
                }

                mPictureAdapter.setSourcePeers(mSourcePeers);
                mHandler.sendEmptyMessage(UPDATE_GRID_VIEW);
                mGridView.postInvalidate();
            }
            if (WfdSink.WFD_DEVICE_CONNECTED_ACTION.equals(action)) {
                Message msg = mHandler.obtainMessage(DO_CONNECTED);
                boolean connected = intent.getBooleanExtra(WfdSink.EXTRA_CONNECTION_CHANGED, false);
                msg.arg1 = (connected == true) ? 1 : 0;
                mHandler.sendMessage(msg);
            }
            if (WfdSink.SINK_VIDEO_SIZE_CHANGED_ACTION.equals(action)) {
                int width = intent.getIntExtra(WfdSink.SINK_VIDEO_WIDTH, 0);
                int height = intent.getIntExtra(WfdSink.SINK_VIDEO_HEIGHT, 0);
                if (width != 0 && height != 0) {
                    Log.d(TAG, "video size changed " + width + " x " + height);
                    mSurfaceView.getHolder().setFixedSize(width, height);
                    mSurfaceView.onVideoSizeChanged(width, height, 1, 1);
                }
            }
            if (WfdSink.WFD_THIS_DEVICE_UPDATE_ACTION.equals(action)) {
                currentdevice = (TextView)findViewById(R.id.currentdevice);
                if (mThisName != mWfdSink.getDeviceName())
                    mThisName = mWfdSink.getDeviceName();
                String ssid = getString(R.string.wifi_ssid) + ":";
                currentdevice.setText(ssid + mWfdSink.getDeviceName());
            }

        }
    };


    private static View sink_main;
    private View divider_view;
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().getDecorView()
                    .setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                           | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WfdSink.WFD_DEVICE_LIST_CHANGED_ACTION);
        intentFilter.addAction(WfdSink.WFD_DEVICE_CONNECTED_ACTION);
        intentFilter.addAction(WfdSink.WFD_THIS_DEVICE_UPDATE_ACTION);
        intentFilter.addAction(WfdSink.SINK_VIDEO_SIZE_CHANGED_ACTION);
        registerReceiver(mWifiP2pReceiver, intentFilter);
        sink_main = getLayoutInflater().from(this).inflate(R.layout.sink_main, null);

        mWfdSink = new WfdSink(this);
        setContentView(sink_main);

        mSurfaceView = (SinkView)findViewById(R.id.sink_preview);
        SurfaceHolder holder = mSurfaceView.getHolder();
        holder.addCallback(this);
        holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
        mSurfaceView.setOnTouchListener(new OnTouchListener() {
                @Override
                public boolean onTouch(View v, MotionEvent event) {
                    Log.d(TAG, "sendrolon sink view on touch event x=" + event.getRawX() + "y=" + event.getRawY());
                    /*
                    if(event.isFromSource(InputDevice.SOURCE_MOUSE)) {
                        Log.d(TAG,"sink view on mouse event but we do NOT support mouse now.");
                        return true;
                    }
                    */
                    mWfdSink.sendTouchEvent(event);
                    return true;
                }
        });

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
        mGridView.setVisibility(View.VISIBLE);
        mGridView.setAdapter(mPictureAdapter);

        startSearch();
    }

    @Override
    protected void onRestart() {
        super.onRestart();
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(WfdSink.WFD_DEVICE_LIST_CHANGED_ACTION);
        intentFilter.addAction(WfdSink.WFD_DEVICE_CONNECTED_ACTION);
        intentFilter.addAction(WfdSink.WFD_THIS_DEVICE_UPDATE_ACTION);
        intentFilter.addAction(WfdSink.SINK_VIDEO_SIZE_CHANGED_ACTION);
        registerReceiver(mWifiP2pReceiver, intentFilter);
        WifiManager manager = (WifiManager) getSystemService(WIFI_SERVICE);
        mWfdSink = new WfdSink(this);
        if(manager.isWifiEnabled()){

        }else{
            wifiDialog();
        }
        startSearch();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if(keyCode == KeyEvent.KEYCODE_BACK) {
            this.exitDialog();
        } else if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
                || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            mWfdSink.sendKeyEvent(keyCode, event);
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }
  
    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        
        if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
                || keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
            mWfdSink.sendKeyEvent(keyCode, event);
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (event.isFromSource(InputDevice.SOURCE_MOUSE)) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_SCROLL:
                    Log.i(TAG,"sendrolon GenericMotionEvent detail is: " + event.toString());
                   // mWfdSink.sendTouchEvent(event);
                    mWfdSink.sendScrollEvent(event);
                break;
                case MotionEvent.ACTION_HOVER_MOVE:
                    mWfdSink.sendTouchEvent(event);
                    Log.i(TAG,"sendrolon GenerMotionEvent HoverMoved." + event.toString());
                break;
                default:
                    Log.i(TAG,"GenericMotionEvent: other mouse event " 
                              + event.getAction() + "x=" + event.getX() 
                              + "y=" + event.getY());
            }
        }

        return super.onGenericMotionEvent(event);
        

    }

    private void wifiDialog() {
        Dialog dialog = new AlertDialog.Builder(WfdSinkActivity.this)
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
        Dialog dialog = new AlertDialog.Builder(WfdSinkActivity.this)
            .setTitle(R.string.program_exit)
            .setMessage(R.string.program_exit_sure)
            .setIcon(R.drawable.ic_dialog)
            .setPositiveButton(R.string.wifi_confirm, new DialogInterface.OnClickListener(){

                    @Override
                    public void onClick(DialogInterface arg0, int arg1) {
                        WfdSinkActivity.this.finish();
                    }

                    }).setNegativeButton(R.string.wifi_cancel, new DialogInterface.OnClickListener() {

                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            sink_main.setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
                        }
                        }).create();
        dialog.show();

    }

    public Handler mHandler = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case UPDATE_GRID_VIEW:
                    mGridView.setAdapter(mPictureAdapter);
                    break;

                case UPDATE_SURFACE:
                    SurfaceHolder holder = (SurfaceHolder)msg.obj;
                    handleUpdateSurface(holder);
                    break;

                case START_PLAY:
                    handleStartPlay();
                    break;

                case STOP_PLAY:
                    handleStopPlay();
                    break;

                case DO_CONNECTED:
                    status.setText(R.string.wifi_status_connected);
                    mHandler.removeMessages(START_SEARCH);
                    boolean connected = (msg.arg1 == 1);
                    handleConnected(connected);
                    sink_main.setSystemUiVisibility(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
                    break;
                case START_SEARCH:
                    mWfdSink.startSearch();
                    mHandler.sendEmptyMessageDelayed(START_SEARCH,DELAY);
                    mWfdSink.setMiracastMode(WifiP2pManager.MIRACAST_SINK);
                    break;
            }
        }
    };

    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        // Make sure we have a surface in the holder before proceeding.
        if (holder.getSurface() == null) {
            Log.d(TAG, "holder.getSurface() == null");
            return;
        }

        Log.i(TAG, "surfaceChanged. w=" + w + ". h=" + h);
        Message msg = mHandler.obtainMessage(UPDATE_SURFACE, holder);
        mHandler.sendMessage(msg);
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "surfaceCreated.");
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "surfaceDestroyed.");
        mSurfaceHolder = null;
    }

    private void handleUpdateSurface(SurfaceHolder holder) {
        mSurfaceHolder = holder;
        if (mWaitingForSurface) {
            mWaitingForSurface = false;
            mWfdSink.startRtsp(mSurfaceHolder.getSurface());
            mStarted = true;
        }
    }

    private void handleStartPlay() {
        if (mSurfaceHolder == null) {
            mWaitingForSurface = true;
            return;
        }
        mWfdSink.startRtsp(mSurfaceHolder.getSurface());
        mStarted = true;
    }

    private void handleStopPlay() {
        if (!mStarted) {
            return;
        }
        mWfdSink.stopRtsp();
        mWfdSink.stopRtp();
        mSourcePeers.clear();
        mPictureAdapter.setSourcePeers(mSourcePeers);
        mGridView.setAdapter(mPictureAdapter);
        mGridView.postInvalidate();
        layout.setVisibility(View.VISIBLE);
        layout_devicelist.setVisibility(View.VISIBLE);
        mSurfaceView.setVisibility(View.INVISIBLE);
        mGridView.setVisibility(View.VISIBLE);
        mStarted = false;
        disconnectPeer();
        mConnected = false;
        startSearch();
        //Technically use onRestart on app layer will help to
        //improve the connection success rate due it will
        //release the Framework p2p resource totally.
        //onRestart();
    }

    private void requestAudio() {
        AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        int result = am.requestAudioFocus(null,
                                          AudioManager.STREAM_MUSIC,
                                          AudioManager.AUDIOFOCUS_GAIN);
        if (result != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            Log.w(TAG, "requestAudioFocus failed result is (" + result + ")");
        } else {
            Log.i(TAG, "requestAudioFocus Successfully!");
        }

    }

    private void handleConnected(boolean connected) {
        mConnected = connected;
        if (mConnected) {
            stopSearch();
            layout.setVisibility(View.GONE);
            layout_devicelist.setVisibility(View.GONE);
            mSurfaceView.setVisibility(View.VISIBLE);
            mGridView.setVisibility(View.GONE);
            startPlayer();
            mWfdSink.setMiracastMode(WifiP2pManager.MIRACAST_SINK);
            requestAudio();
        }
        else {
            stopPlayer();
            startSearch();
            mWfdSink.setMiracastMode(WifiP2pManager.MIRACAST_DISABLED);
        }
    }

    private void startSearch() {
        status.setText(R.string.wifi_status_searching);
        mHandler.sendEmptyMessageDelayed(START_SEARCH,DELAY);
    }

    private void stopSearch() {
        mWfdSink.stopSearch();
    }

    private void disconnectPeer() {
        mWfdSink.disconnect();
    }

    private void startPlayer() {
        Message msg = mHandler.obtainMessage(START_PLAY);
        mHandler.sendMessage(msg);
    }

    private void stopPlayer() {
        status.setText(R.string.wifi_status_disconnected);
        Message msg = mHandler.obtainMessage(STOP_PLAY);
        mHandler.sendMessage(msg);
    }


    public void onStart() {
        super.onStart();
        mGridView.setAdapter(mPictureAdapter);
        WifiManager manager = (WifiManager) getSystemService(WIFI_SERVICE);
        if(!manager.isWifiEnabled()){
            wifiDialog();
        }
    }

    public  void onResume() {
        super.onResume();
        mHandler.sendEmptyMessageDelayed(START_SEARCH,DELAY);
    }

    public void onStop() {
        super.onStop();
        unregisterReceiver(mWifiP2pReceiver);
        if (mStarted) {
            handleStopPlay();
        }

        if (mConnected) {
            disconnectPeer();
            mConnected = false;
        }
        stopSearch();
        mWfdSink.setMiracastMode(WifiP2pManager.MIRACAST_DISABLED);
        mWfdSink.dispose();
        mWfdSink = null;
        mHandler.removeMessages(START_SEARCH);

    }

    public void onDestroy() {
        super.onDestroy();
    }

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

