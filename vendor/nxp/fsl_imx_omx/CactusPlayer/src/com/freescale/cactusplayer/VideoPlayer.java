 /*
 * Copyright (C) 2013-2016 Freescale Semiconductor, Inc.
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
 /* Copyright 2018 NXP */


package com.freescale.cactusplayer;

import java.io.File;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;
import java.io.FilenameFilter;

import android.app.Activity;
import android.app.Dialog;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.UserHandle;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
//import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;
import android.widget.SeekBar;
import android.widget.TextView;
//import android.widget.Toast;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.Context;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.view.View.OnClickListener;

import android.media.MediaPlayer;
import android.media.ThumbnailUtils;
import android.media.MediaPlayer.OnCompletionListener;
import android.media.MediaPlayer.OnErrorListener;
import android.media.AudioManager;
import java.io.IOException;

import com.freescale.cactusplayer.HdmiApplication;
import com.freescale.cactusplayer.R;

import android.os.Parcel;
import android.provider.MediaStore;
import android.app.ActionBar;
//import android.view.MenuInflater;
import android.media.TimedText;
import android.database.ContentObserver;
import android.provider.Settings;
import android.hardware.display.DisplayManager;
import android.net.wifi.WifiManager;
import android.media.PlaybackParams;
import com.freescale.cactusplayer.utils.PathUtils;
import android.content.pm.PackageManager;
import android.Manifest;
public class VideoPlayer extends Activity implements HdmiApplication.Callback,
    SeekBar.OnSeekBarChangeListener {
        private static final String TAG   = "Cactus";
        private static final String CLASS = "VideoPlayer: ";

        private static final int STATE_INVALID = -1;
        private static final int PLAYER_STOPPED = 0;
        private static final int PLAYER_PREPARED = 1;
        private static final int PLAYER_PLAYING = 2;
        private static final int PLAYER_PAUSED  = 3;
        private static final int PLAYER_EXITED  = 4;


    //------------------------------------------------------------------------------------
    // widgets
    //-----------------------------------------------------------------------------------
        private MediaPlayer mMediaPlayer = null;
    //private   SurfaceHolder mPlayerSurfaceHolder = null;
        private int         mVideoWidth;
        private int         mVideoHeight;
        private int         mSurfaceWidth;
        private int         mSurfaceHeight;
        private int         mSpeed = 1;
        private boolean     mDragging;
        private int         mAudioTrackCount = 0;
        private int         mSubtitleTrackCount = 0;
        private int         mCurSubtitleTrack = -1;
        private int         mCurSubtitleIndex = -1;
        private int         mCurAudioTrack = -1;
        private int         mCurAudioIndex = -1;
        private boolean     mLoopFile = false;
        private boolean     mCastScreen = false;

     // video display
        private   VideoView    mVideoView;
        private   SurfaceHolder mSurfaceHolder = null;

    // subtitle
        private   AutoHideTextView    mSubtitleTextView;
        private   AutoHideTextView    mSubtitleInfoView;

    // track, ff/rw information
        private   AutoHideTextView    mInfoView;

        private   AutoHideTextView    mTrackInfoView;

    // error sign
        private   AutoHideTextView    mErrSign;

    // control bar and seek bar
        private   ImageButton mBtnFastBack;
        private   ImageButton mBtnPlayPause;
        private   ImageButton mBtnFastForward;
        private   TextView    mCurPosView;
        private   TextView    mEndPosView;
        private   SeekBar     mProgressBar;

    // waiting dialog
        private Dialog mWaitingDialog = null;

        private PowerManager.WakeLock wl = null;

    //------------------------------------------------------------------------------------
    // properties (url, speed, state, etc.)
    //------------------------------------------------------------------------------------
        private   String      mUrl       = null;
        private   String      mMime      = null;
        private   Uri         uri1       = null;
        private   int         mPlayState = PLAYER_STOPPED;
        private   int         mTargetState = STATE_INVALID;
        private   float       mPlaySpeed = 1;
        private   long        mDuration  = 0; // cache of file duration and current playback position
        private   long        mCurPos    = 0;
        private   boolean     mSubtitleEnabled = true;
        private   long        mTimeOffset = 0;

    //------------------------------------------------------------------------------------
    // constants
    //------------------------------------------------------------------------------------
        private static final int UPDATE_PLAYBACK_STATE  = 1;
        private static final int UPDATE_PLAYBACK_SPEED  = 2;
        private static final int UPDATE_PLAYBACK_PROGRESS = 4;
        private static final int UPDATE_PLAYBACK_ERROR  = 8;

        private static final int LOOP_FILE  = Menu.FIRST+1;
        private static final int SELECT_AUDIO    = Menu.FIRST+2;
        private static final int SELECT_SUBTITLE  = Menu.FIRST+3;
        private static final int CLOSE_SUBTITLE  = Menu.FIRST+4;
        private static final int Cast_Screen = Menu.FIRST+5;
        private static final int Quit = Menu.FIRST+6;

        private static final int mPresentation_disabled = 0;
        private static final int mPresentation_playing = 1;
        private static final int mPresentation_pausing = 2;
        private long progressBarProhibitTime = -1;

        private String locale = "eng";

        private static final int    SHOW_PROGRESS = 1;
        private static final int RESTART_PAUSE = 0;

        private final static String IMEDIA_PLAYER = "android.media.IMediaPlayer";
        private static final int INVOKE_ID_SELECT_TRACK = 4;

        private static final float forward_speed[] = {0.5f, 1.5f, 2, 4};
        private static final float backward_speed[] = {-2, -4, -8, -16};
        private int forward_speed_index = -1;
        private int backward_speed_index = -1;

        private HdmiApplication.DemoPresentation mPresentation;
        private HdmiApplication mHdmiApp;
        private int mVideoIndex = -1;
        private boolean mVideoRunning = false;
        private Bitmap mVideoBitmap;
        private ImageView mVideoImage;
        private ImageButton mImageButton;
        private List<String> mVideoFileList = null;
        private boolean CastScreen_flag = false;
        private int mcurrpos = 0;
        private int mCurrentProgress = 0;
        private long mLastDuration = 0;
        private int mLastPlayState = -1;
        private boolean restart = false;
        private boolean bGetTrackInfo = false;
	//Permission related member
	private int mNumPermissionsToRequest = 0;
	private boolean mShouldRequestStoragePermission = false;
	private boolean mFlagHasStoragePermission = true;
	private int mIndexPermissionRequestStorage = 0;
	private static final int PERMISSION_REQUEST_CODE = 0;

// onPresentationEnded
    public void onPresentationEnded(boolean end) {
        mPresentation = mHdmiApp.getPresentation();
        if (end) {
            mImageButton.setImageDrawable(getResources().getDrawable(R.drawable.ic_media_play));
            mPresentation.setPresentationState(mPresentation_pausing);
            mPresentation.pauseVideo();
            mImageButton.postInvalidate();
            mImageButton.setVisibility(View.VISIBLE);
        }
    }


// onPresentationChanged
    public void onPresentationChanged(boolean plugin) {
        mPresentation = mHdmiApp.getPresentation();
        //dismiss
        if (!plugin) {
            Log.d(TAG, "PresentationChanged");
            if(CastScreen_flag == true) {
                CastScreen_flag = false;

                if (!mHdmiApp.getmPresentationEnded()) {
                    mcurrpos = mPresentation.getcurrentPos();
                }
                else {
                    mCurrentProgress = 0;
                }
                Local_onCreate();
                mHdmiApp.setmPresentationEnded(false);
            }
            else {
                pause();
                mLastPlayState = -1;
            }
        }
        mLastPlayState = PLAYER_PAUSED;
    }



	private void parseIntent(Intent intent) {
		Log.d(TAG, CLASS + "parseIntent");
		if(intent == null) {
			return;
		}

		uri1 = intent.getData();
        mMime = intent.getType();
		if(uri1 != null) {
			// set url to load
			mUrl = PathUtils.getPath(this, uri1);
			mCurrentProgress = 0;
			// display waiting dialog if possible
			/*
			if(mWaitingDialog == null)
			{
				mWaitingDialog = new Dialog(this, android.R.style.Theme_Translucent);
				mWaitingDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
				mWaitingDialog.setContentView(R.layout.myprogressdlg);
				mWaitingDialog.setCancelable(true); // just a indication; no need to block user operations
				mWaitingDialog.setCanceledOnTouchOutside(false);
				mWaitingDialog.show();
			}
			*/
		}
	}



    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, CLASS + "onCreate");
        super.onCreate(savedInstanceState);
        boolean isRestored = (savedInstanceState != null);
        checkPermission();
        ActionBar mActionBar = getActionBar();
        mActionBar.setHomeButtonEnabled(true);
        mActionBar.setDisplayHomeAsUpEnabled(true);
    }

    public void initView(){
        //loadNativeLibs();
        initLocaleTable();
        Intent intent = getIntent();
        parseIntent(intent);
        mHdmiApp = (HdmiApplication)getApplication();
        mPresentation = mHdmiApp.getPresentation();
        mHdmiApp.addListener(this);
        if(mPresentation != null){
            if(mPresentation.getPresentationState() == mPresentation_playing ||
                mPresentation.getPresentationState() == mPresentation_pausing)
                    Remote_onCreate();
            else
                Local_onCreate();
        }
        else
            Local_onCreate();

    }

	private void checkPermission(){

		if(checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
				!= PackageManager.PERMISSION_GRANTED){
			mNumPermissionsToRequest++;
			mShouldRequestStoragePermission  = true;
		}else{
			mFlagHasStoragePermission  = true;
		}

		String[] permissionToRequest = new String[mNumPermissionsToRequest];
		int permissionRequestIndex = 0;

		if(mShouldRequestStoragePermission){
			permissionToRequest[permissionRequestIndex] = Manifest.permission.WRITE_EXTERNAL_STORAGE;
			mIndexPermissionRequestStorage= permissionRequestIndex;
			permissionRequestIndex++;
		}

		if(permissionToRequest.length > 0){
			requestPermissions(permissionToRequest, PERMISSION_REQUEST_CODE);
		}else{
			Log.i(TAG,"no need to request permission.WRITE_EXTERNAL_STORAGE");
		}
	}

    public void Local_onCreate(){
        CastScreen_flag = false ;
        setContentView(R.layout.scaleplayer);
        mVideoView = (VideoView)findViewById(R.id.SurfaceView);
        mVideoView.getHolder().addCallback(mSHCallback);
        mBtnFastBack     = (ImageButton) findViewById(R.id.fastback);
        mBtnPlayPause    = (ImageButton) findViewById(R.id.playpause);
        mBtnFastForward  = (ImageButton) findViewById(R.id.fastforward);
        mBtnFastBack  .setOnClickListener(mOnBtnClicked);
        mBtnPlayPause  .setOnClickListener(mOnBtnClicked);
        mBtnFastForward  .setOnClickListener(mOnBtnClicked);
        mCurPosView      = (TextView)    findViewById(R.id.currentpos);
        mEndPosView      = (TextView)    findViewById(R.id.duration);
        mBtnPlayPause    .setBackgroundResource(R.drawable.play);
        mInfoView        = (AutoHideTextView)    findViewById(R.id.info);
        mInfoView        .setVisibility(View.INVISIBLE);
        mSubtitleTextView  = (AutoHideTextView)    findViewById(R.id.subtitletext);
        mSubtitleTextView  .setVisibility(View.INVISIBLE);
        mSubtitleInfoView  = (AutoHideTextView)    findViewById(R.id.subtitleinfo);
        mSubtitleInfoView  .setVisibility(View.INVISIBLE);
        mProgressBar     = (SeekBar)      findViewById(R.id.seek);
        mProgressBar     .setOnSeekBarChangeListener(this);
        mLastPlayState = -1;
    }

    public void Remote_onCreate(){
        mHdmiApp = (HdmiApplication)getApplication();
        mHdmiApp.addListener(this);
        CastScreen_flag = true;
        setContentView(R.layout.secondvideo);
        mImageButton = (ImageButton)findViewById(R.id.hdmi_button);
        if(mPresentation.getPresentationState() == mPresentation_playing ){
            mImageButton.setImageDrawable(getResources().getDrawable(R.drawable.ic_media_pause));
        }
        else if (mPresentation.getPresentationState() == mPresentation_pausing){
            mImageButton.setImageDrawable(getResources().getDrawable(R.drawable.ic_media_play));
        }

        mImageButton.setVisibility(View.VISIBLE);
        mImageButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View arg0) {
                switch(arg0.getId()) {
                    case R.id.hdmi_button:
                         mHdmiApp = (HdmiApplication)getApplication();
                         mPresentation = mHdmiApp.getPresentation();
                         if(mPresentation != null){
                             mVideoRunning = mHdmiApp.getVideoRunning();
                             if(mVideoRunning == true) {
                                 if (mPresentation != null) {
                                     mPresentation.pauseVideo();
                                     mPresentation.setPresentationState(mPresentation_pausing);
                                     mImageButton.setImageDrawable(getResources().getDrawable(R.drawable.ic_media_play));
                                     mImageButton.setVisibility(View.VISIBLE);
                                 }
                              }
                              else {
                                  if (mPresentation != null) {
                                      mPresentation.resumeVideo();
                                      mPresentation.setPresentationState(mPresentation_playing);
                                      mImageButton.setImageDrawable(getResources().getDrawable(R.drawable.ic_media_pause));
                                      mImageButton.setVisibility(View.VISIBLE);
                                      mHdmiApp.setmPresentationEnded(false);
                                 }
                             }
                         }
                         else
                             break;

                         break;

                     default:
                         break;
                }
            }
         });
      }


   @Override
   protected void onNewIntent (Intent intent) {
        Log.d(TAG, CLASS + "onNewIntent");
        // if video view not ready, just record the smil file
        if(mVideoView.getSurface() == null) {
          parseIntent(intent);
          return;
        }
        // stop current player
        stop();
        mUrl = null;
        // get new file path
        parseIntent(intent);
   }

   private final ContentObserver mSettingsObserver = new ContentObserver(null) {
        @Override
        public void onChange(boolean selfChange, Uri uri) {
             boolean connected = Settings.Global.getInt(getContentResolver(),
             Settings.Global.WIFI_ON, 0) != 0;
             if (!connected) {
                  mHdmiApp = (HdmiApplication)getApplication();
                  mPresentation = mHdmiApp.getPresentation();
                  if(mPresentation != null){
                      if(mPresentation.getPresentationState() == mPresentation_playing ||
                          mPresentation.getPresentationState() == mPresentation_pausing){
                              mPresentation.pauseVideo();
                              mPresentation.setPresentationState(mPresentation_pausing);
                      }
                      else {
                          pause();
                          mLastPlayState = -1;
                      }
                  }
                  Log.d(TAG,"ContentObserver wifi_off pausePresentation");
              }
          }
    };

    private final BroadcastReceiver mReceiver = new BroadcastReceiver(){
        @Override
        public void onReceive(Context context, Intent intent){
            String action = intent.getAction();
            if(action.equals(HdmiApplication.ACTION_WIFI_DISPLAY_DISCONNECTION)){
                mHdmiApp = (HdmiApplication)getApplication();
                mPresentation = mHdmiApp.getPresentation();
                if(mPresentation != null){
                    if(mPresentation.getPresentationState() == mPresentation_playing ||
                        mPresentation.getPresentationState() == mPresentation_pausing){
                            mPresentation.pauseVideo();
                            mPresentation.setPresentationState(mPresentation_pausing);
                    }
                    else {
                        pause();
                        mLastPlayState = -1;
                    }
                }
                Log.d(TAG,"BroadcastReceiver WIFI_DISPLAY_DISCONNECTION");
            }
        }
   };

   @Override
   protected void onStart() {
        Log.d(TAG, CLASS + "onStart");
        super.onStart();
       // keep screen bright
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        wl = pm.newWakeLock(PowerManager.FULL_WAKE_LOCK, "Cactus Player");
        wl.acquire();
        getContentResolver().registerContentObserver(Settings.Global.getUriFor(
            Settings.Global.WIFI_ON), false, mSettingsObserver);
        IntentFilter filter = new IntentFilter();
        filter.addAction(HdmiApplication.ACTION_WIFI_DISPLAY_DISCONNECTION);
        registerReceiver(mReceiver, filter);
   }


   @Override
   protected void onResume() {
       Log.d(TAG, CLASS + "onResume mLastPlayState:" + mLastPlayState);
       super.onResume();

       if(mLastPlayState != PLAYER_PAUSED && mLastPlayState != PLAYER_PLAYING) {
           initView();
       } else if (false == CastScreen_flag) {
           setContentView(R.layout.scaleplayer);
           mVideoView = (VideoView)findViewById(R.id.SurfaceView);
           mVideoView.getHolder().addCallback(mSHCallback);
           mBtnFastBack     = (ImageButton) findViewById(R.id.fastback);
           mBtnPlayPause    = (ImageButton) findViewById(R.id.playpause);
           mBtnFastForward  = (ImageButton) findViewById(R.id.fastforward);
           mBtnFastBack  .setOnClickListener(mOnBtnClicked);
           mBtnPlayPause  .setOnClickListener(mOnBtnClicked);
           mBtnFastForward  .setOnClickListener(mOnBtnClicked);
           mCurPosView      = (TextView)    findViewById(R.id.currentpos);
           mEndPosView      = (TextView)    findViewById(R.id.duration);
           mBtnPlayPause    .setBackgroundResource(R.drawable.play);
           mInfoView        = (AutoHideTextView)    findViewById(R.id.info);
           mInfoView        .setVisibility(View.INVISIBLE);
           mSubtitleTextView  = (AutoHideTextView)    findViewById(R.id.subtitletext);
           mSubtitleTextView  .setVisibility(View.INVISIBLE);
           mSubtitleInfoView  = (AutoHideTextView)    findViewById(R.id.subtitleinfo);
           mSubtitleInfoView  .setVisibility(View.INVISIBLE);
           mProgressBar     = (SeekBar)      findViewById(R.id.seek);
           mProgressBar     .setOnSeekBarChangeListener(this);
       }

       if(mVideoView != null)
           mVideoView.setVisibility(View.VISIBLE);

       if (mMediaPlayer != null && mLastPlayState != PLAYER_PAUSED) {
           Log.d(TAG, CLASS + "onResume call mMediaPlayer.start()");
           mMediaPlayer.start();
           mPlayState = PLAYER_PLAYING;
           Log.d(TAG,"onResume: player is started and state is playing");
           if(mDuration < 0)
               mDuration = mMediaPlayer.getDuration();
            updateButtons(UPDATE_PLAYBACK_STATE, mPlayState, 0, 0);
            mHandler.sendEmptyMessage(SHOW_PROGRESS);
       }

       if (mMediaPlayer != null) {
           mLastPlayState = -1;
       }
    }

    @Override
    protected void onRestart() {
        // TODO Auto-generated method stub
        super.onRestart();
    }


    @Override
    protected void onPause() {
        Log.d(TAG, CLASS + "onPause play state:" + mPlayState);
        super.onPause();
        mLastPlayState = mPlayState;
        if(mPlayState == PLAYER_PLAYING){
            pause();
            Log.d(TAG,"speed is "  + mPlaySpeed);
            if(mPlaySpeed != 1)
                setNormalSpeed();
        }
        else{
            // If current state is stopped or prepared, player may switch to playing state in surfaceChanged or onPrepared,
            // so onPause fails to make player really paused. This shall be avoided. save a flag here, check it in
            // surfaceChanged and onPrepared and then pause player.
            mTargetState = PLAYER_PAUSED;
        }

    }

    @Override
    protected void onStop() {
        Log.d(TAG, CLASS + "onStop");
        super.onStop();
        unregisterReceiver(mReceiver);
        //stop();
        //mUrl = null;
        // stop screen bright
        wl.release();
        // clean gui to not disturb onStart() next time
        //initPlay();
        //mcurrpos=mMediaPlayer.getCurrentPosition();
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG, CLASS + "onDestroy");
        super.onDestroy();
    }


    //------------------------------------------------------------------------------------
    // Update GUI
    //------------------------------------------------------------------------------------

    private void updateButtons(int reason, long val1, long val2, float val3) {
        if(reason == UPDATE_PLAYBACK_SPEED)
        {
		    float newSpeed = val3;
			if(newSpeed == 1)
			{
    			mInfoView.setText(null, 0);
			}
			else
			{
				// trick play
				String text;
				if(newSpeed > 0f)
					text = "FF X" + newSpeed;
				else
					text = "RW X" + (-newSpeed);

                if(val1 == 0){
                    mInfoView.setText(text, -1);
                    mSubtitleTextView.setText(null,0);
                }
                else if(val1 == -1){
                    text = text.concat(" Fail");
                    mInfoView.setText(text, -1);
                }

			}
        }
        else if(reason == UPDATE_PLAYBACK_STATE){
            mPlayState = (int) val1;
            //Log.d(TAG,"play state changed to " + mPlayState);
        }
        else if(reason == UPDATE_PLAYBACK_PROGRESS){
            /**
            if(val3 == 0){ // no video frame updated, maybe this event can be discarded
                if(progressBarProhibitTime > 0) {
                    long curTime = System.currentTimeMillis();
                    if(curTime - progressBarProhibitTime < 2000)
                        return;

                    progressBarProhibitTime = -1;
                 }
             }
            **/

            if(val1 > 0)
                mDuration = val1;
            else if(mDuration == 0 && mMediaPlayer != null && mPlayState != PLAYER_STOPPED)
                mDuration = mMediaPlayer.getDuration();
            val2 -= mTimeOffset; // offset by mTimeOffset
            if(mDuration != 0 && val2 > mDuration)
                val2 = mDuration;
            else if(val2 < 0)
                val2 = 0;
            mCurPos   = val2;
            long seconds = mDuration / 1000;
            long minutes = seconds / 60;
            seconds = seconds - minutes * 60;
            long hours = minutes / 60;
            minutes = minutes - hours * 60;
            String s = "";
            if(hours < 10)
                s += "0";
            s += hours + ":";
            if(minutes < 10)
                s += "0";
            s += minutes + ":";
            if(seconds < 10)
                s += "0";
            s += seconds;
            mEndPosView.setText(s);
            seconds = mCurPos / 1000;
            minutes = seconds / 60;
            seconds = seconds - minutes * 60;
            hours = minutes / 60;
            minutes = minutes - hours * 60;
            s = "";
            if(hours < 10)
                s += "0";
            s += hours + ":";
            if(minutes < 10)
                s += "0";
            s += minutes + ":";
            if(seconds < 10)
                s += "0";
            s += seconds;
            mCurPosView.setText(s);
            if(mDuration > 0) {
                int pos = (int) (mCurPos/1000*1000 * 100 / mDuration + 1);
                if(pos < 0 || mCurPos == 0)
                    pos = 0;
                else if(pos > 100)
                    pos = 100;
                mProgressBar.setProgress(pos);
            }
            else {
                mProgressBar.setProgress(0);
            }
        }
        else if(reason == UPDATE_PLAYBACK_ERROR){
            // show error sign
            //mErrSign.setVisibility(View.VISIBLE);
            // dismiss waiting dialog
            if(mWaitingDialog != null){
                mWaitingDialog.dismiss();
                mWaitingDialog = null;
            }
        }
        if(mPlayState == PLAYER_EXITED) { // exit
            finish();
        }
        // now update buttons/progressbar
        boolean enablefb = true;
        boolean enableplaypause = true;
        boolean enableff = true;
        boolean enablestop = true;
        boolean enablesub = true;
        boolean enableseek = true;
        boolean enableplay = true;

        if(mUrl == null) {
            enablefb = false;
            enableplaypause = false;
            enableff = false;
            enablestop = false;
            enablesub = false;
            enableseek = false;
            enableplay = false;
        }

        if(mPlayState == PLAYER_PLAYING)
            mBtnPlayPause.setBackgroundResource(R.drawable.pause);
        else // paused or stopped
            mBtnPlayPause.setBackgroundResource(R.drawable.play);

        if(mPlayState == PLAYER_STOPPED) {
            // stopped: BOF or EOF or user stopped
            // finish(); // back to storefront, according to DivX requirement
            // need to distinguish the reason
            enablefb = false;
            enableplaypause = true;
            enableff = false;
            enablestop = true;
            enablesub = false;
            enableseek = false;
            enableplay = true;
        }
        else { // 0 - playing; 1 - paused
            if(mPlaySpeed != 1)
                enableseek = false;
        }

        if(mPlayState == PLAYER_PAUSED || mPlayState == PLAYER_STOPPED)
            pauseMetadataShow();
        else
            resumeMetadataShow();

        mBtnFastBack   .setEnabled(enablefb);
        mBtnPlayPause  .setEnabled(enableplaypause);
        mBtnFastForward.setEnabled(enableff);
        mProgressBar   .setEnabled(enableseek);
    }

    //------------------------------------------------------------------------------------
    // Playback control
    //------------------------------------------------------------------------------------

    private void initPlay() {
        mPlayState   = PLAYER_STOPPED;
        mPlaySpeed   = 0;
        mLastDuration = mDuration;
        mDuration    = 0;
        mCurPos      = 0;
        mTimeOffset  = 0;

        resumeMetadataShow();
        if(mWaitingDialog != null){
            mWaitingDialog.dismiss();
            mWaitingDialog = null;
        }
        mInfoView.setText(null, 0);
        mSubtitleTextView.setText(null,0);
        mSubtitleTextView.setText(null,0);
        mEndPosView.setText(null);
        mCurPosView.setText(null);
        mProgressBar.setProgress(0);
        mCurSubtitleIndex = -1;
        mCurSubtitleTrack = -1;
        updateButtons(-1, 0, 0, 0);
    }

    SurfaceHolder.Callback mSHCallback = new SurfaceHolder.Callback(){
        public void surfaceChanged(SurfaceHolder holder, int format,
            int w, int h){
            Log.d(TAG,"surfaceChanged: parameter w/h " + w + "/" + h + " mSurfaceW/H " + mSurfaceWidth + "/" + mSurfaceHeight + ",mPlayState: " + mPlayState + " mTargetState: " + mTargetState);
            mSurfaceWidth = w;
            mSurfaceHeight = h;
            boolean hasValidSize = (mVideoWidth == w && mVideoHeight == h);
            if(mLastPlayState == -1 && mPlayState == PLAYER_PAUSED){
                mLastPlayState = mPlayState;
            }
            if(mMediaPlayer != null && hasValidSize && mPlayState == PLAYER_PREPARED) {
                start();
                if(mTargetState == PLAYER_PAUSED && mPlayState == PLAYER_PLAYING){
                    pause();
                    mTargetState = STATE_INVALID;
                }
            }
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            // TODO Auto-generated method stub
            Log.d(TAG,"surfaceCreated");
            mSurfaceHolder = holder;
            initPlay();
            play();
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            // TODO Auto-generated method stub
            mSurfaceHolder = null;
            stop();
        }
    };

    MediaPlayer.OnPreparedListener mPreparedListener = new MediaPlayer.OnPreparedListener() {
        public void onPrepared(MediaPlayer mp) {
            mPlaySpeed = 1;
            Log.d(TAG,"onPrepared: play state " + mPlayState + " video:" + mVideoWidth + "," + mVideoHeight + " mSurface:" + mSurfaceWidth + ","
                + mSurfaceHeight + " mTargetState:" + mTargetState);
            mPlayState = PLAYER_PREPARED; // from now on, we can call start(), getDuration()
            if (mMime != null && mMime.startsWith("video") && mVideoWidth != 0 && mVideoHeight != 0){
                mVideoView.getHolder().setFixedSize(mVideoWidth, mVideoHeight);
                if (mSurfaceWidth != mVideoWidth || mSurfaceHeight != mVideoHeight) {
                    return;
                }
            }

            if(mCurrentProgress <= 0 && mcurrpos <= 0){
                mDuration = -1;
            }
            if(mPlayState == PLAYER_PAUSED){
                mLastPlayState = PLAYER_PAUSED;
            }
            start();
            if(mTargetState == PLAYER_PAUSED && mPlayState == PLAYER_PLAYING){
                pause();
                mTargetState = STATE_INVALID;
            }

        }
    };

    MediaPlayer.OnVideoSizeChangedListener mSizeChangedListener =
        new MediaPlayer.OnVideoSizeChangedListener() {
            public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
                Log.d(TAG,"onVideoSizeChanged");
                mVideoWidth = mp.getVideoWidth();
                mVideoHeight = mp.getVideoHeight();
                mVideoView.onVideoSizeChanged(mVideoWidth, mVideoHeight, 1, 1);
            }
    };

    private MediaPlayer.OnCompletionListener mCompletionListener =
        new MediaPlayer.OnCompletionListener() {
            public void onCompletion(MediaPlayer mp) {
                Log.d(TAG,"onCompletion");
            /*
            if(mPlaySpeed < 0){
                mPlaySpeed = 1;
                updateButtons(UPDATE_PLAYBACK_SPEED, 0, 0, mPlaySpeed);
                return;
            }
            */

            /*
            if(mPlaySpeed >= 2){
                mPlaySpeed = 1;
                updateButtons(UPDATE_PLAYBACK_SPEED, 0, 0, mPlaySpeed);
                return;
            }
            */
                stop();
                updateButtons(UPDATE_PLAYBACK_PROGRESS, 0, 0, 0);
                if(mLoopFile){
                    //play();
                    mVideoView.setVisibility(View.VISIBLE);  // call Play() in surfaceCreated()
                    if(mCurSubtitleTrack >= 0){
                        Log.d(TAG,"onCompletion: select subtitle " + mCurSubtitleTrack);
                        mMediaPlayer.selectTrack(mCurSubtitleTrack);
                    }
                    if(mCurAudioTrack >= 0){
                        mMediaPlayer.selectTrack(mCurAudioTrack);
                        Log.d(TAG,"onCompletion: select audio " + mCurAudioTrack);
                    }
                }
            /*
            if(mLoopFile){
                seekTo(0);
                start();
            }
            else
                stop();
            */
            }
        };

    private MediaPlayer.OnErrorListener mErrorListener =
        new MediaPlayer.OnErrorListener() {
            public boolean onError(MediaPlayer mp, int framework_err, int impl_err) {
                Log.d(TAG, "Error: " + framework_err + "," + impl_err);
                return true;
            }
        };

    private MediaPlayer.OnTimedTextListener mTimedTextListener =
        new MediaPlayer.OnTimedTextListener() {
            public void onTimedText(MediaPlayer mp, TimedText text){
                Log.d(TAG,"onTimeText"+ mPlaySpeed);
                if(mPlaySpeed == 1){
                    if(text != null && null != text.getText()){
                        int start_pos = 0;
                        if(text.getText().startsWith("{\\pos")){
                            start_pos = text.getText().indexOf('}') + 1;
                        }
                        mSubtitleTextView.setText(text.getText().substring(start_pos), 0);
                    }
                    else
                        mSubtitleTextView.setText(null, 0);
                }
            }

        };

    class FileAccept implements FilenameFilter
    {
        String str = null;
        FileAccept(String s){
            str = s;
        }
        public boolean accept(File dir,String name){
            if(name.endsWith(".srt") && name.startsWith(str))
                return true;
            else
                return false;
        }
    }
    private void play() {
        if(mUrl != null && mSurfaceHolder != null) {
            try {
                mMediaPlayer = new MediaPlayer();
                mMediaPlayer.setOnPreparedListener(mPreparedListener);
                mMediaPlayer.setOnVideoSizeChangedListener(mSizeChangedListener);
                mDuration = -1;
                mMediaPlayer.setOnCompletionListener(mCompletionListener);
                mMediaPlayer.setOnErrorListener(mErrorListener);
                mMediaPlayer.setOnTimedTextListener(mTimedTextListener);
                mMediaPlayer.setDataSource(getBaseContext(), uri1);
                if(mVideoView == null)
                    Log.d(TAG, "mVideoView is null");
                if(mSurfaceHolder.getSurface() == null)
                    Log.d(TAG, "surface is null");
                mMediaPlayer.setDisplay(mSurfaceHolder);
                mMediaPlayer.setAudioStreamType(AudioManager.STREAM_MUSIC);
                mMediaPlayer.setScreenOnWhilePlaying(true);
                Log.w(TAG,"Url is " + mUrl);

                do{
                    if(true == mUrl.startsWith("udp"))
                        break;
                    if(mUrl.lastIndexOf('.') <= 0 || mUrl.lastIndexOf('.') >= mUrl.length()-1)
                        break;
                    if(mUrl.lastIndexOf('/') < 0 || mUrl.lastIndexOf('/') >= mUrl.length()-1)
                        break;

                    File f_url = new File(mUrl);
                    if (!f_url.exists())
                        break;
                    //Log.w(TAG,"abs path:" + f_url.getAbsolutePath() + " path:" + f_url.getPath() + " name:" + f_url.getName());
                    //Log.w(TAG,"parent:" + f_url.getParent());
                    File f_path = f_url.getParentFile();
                    String name = f_url.getName();
                    FileAccept filenameFilter = new FileAccept(name.substring(0, name.lastIndexOf('.')));
                    File list[] = f_path.listFiles(filenameFilter);
                    for(int i = 0; i < list.length; i++){
                        String srt_file = list[i].getAbsolutePath();
                        Log.w(TAG, "srt:" + srt_file);
                        mMediaPlayer.addTimedTextSource(srt_file, MediaPlayer.MEDIA_MIMETYPE_TEXT_SUBRIP);
                    }
                }while(false);
                mMediaPlayer.prepareAsync();
            } catch (IOException ex) {
                Log.w(TAG, "Unable to open content: " + mUrl, ex);
                return;
            } catch (IllegalArgumentException ex) {
                Log.w(TAG, "Unable to open content: " + mUrl, ex);
                return;
            }
        }
    }

    public void mediaStart() {
        if(mMediaPlayer != null) {
            if(mcurrpos != 0){
                mDuration = mLastDuration;
                if(mDuration <= 0 && mPlayState != PLAYER_STOPPED){
                    mDuration = mMediaPlayer.getDuration();
                }
                seekTo(mcurrpos);
                updateButtons(UPDATE_PLAYBACK_PROGRESS, 0, mcurrpos, 0);
                mcurrpos = 0;
            }
            else if (mCurrentProgress > 0) {
                mDuration = mLastDuration;
                if(mDuration <= 0 && mPlayState != PLAYER_STOPPED) {
                    mDuration = mMediaPlayer.getDuration();
                }
                long newposition = (mDuration * mCurrentProgress) / 100L;
                seekTo( (int) newposition);
                updateButtons(UPDATE_PLAYBACK_PROGRESS, 0, mCurrentProgress * mDuration / 100, 0);
            }
            else if(mDuration < 0 && mPlayState != PLAYER_STOPPED) {
                mDuration = mMediaPlayer.getDuration();
                updateButtons(UPDATE_PLAYBACK_STATE, mPlayState, 0, 0);
            }

            mHandler.sendEmptyMessage(SHOW_PROGRESS);
        }
    }

    public void start() {
        if(mMediaPlayer == null) {
            return;
        }
        mediaStart();
        Log.d(TAG,"start(), last state " + mLastPlayState);
        if(mLastPlayState != PLAYER_PAUSED) {
            mMediaPlayer.start();
            Log.d(TAG,"start(), change state to playing");
            mPlayState = PLAYER_PLAYING;
        }
        else {
            Log.d(TAG,"start(), change state to " + mLastPlayState);
            mPlayState = mLastPlayState;
        }
        mLastPlayState = -1;
    }

    private void pause() {
        if(mMediaPlayer != null) {
            mMediaPlayer.pause();
            mPlayState = PLAYER_PAUSED;
            updateButtons(UPDATE_PLAYBACK_STATE, mPlayState, 0, 0);
        }
    }

    private void stop() {
        if(mMediaPlayer != null) {
            mMediaPlayer.stop();
            mMediaPlayer.release();
            mMediaPlayer = null;
            mPlayState = PLAYER_STOPPED;
            Log.d(TAG,"stop(), state is stopped");
            mPlaySpeed = 1;
            updateButtons(UPDATE_PLAYBACK_STATE, mPlayState, 0, 0);
            mSubtitleTextView.setText(null,0);
            mInfoView.setText(null,0);
            mVideoView.setVisibility(View.INVISIBLE);
        }
    }

    private void fastForward() {
        if(mMediaPlayer != null) {
            int savedState = -1;
            float newSpeed;
            long val1 = 0;

            int newIndex = forward_speed_index;

            if(++newIndex >= forward_speed.length)
                newIndex = 0;

            newSpeed = forward_speed[newIndex];

            if(mPlayState == PLAYER_PAUSED || mPlayState == PLAYER_PLAYING){
                Log.d(TAG,"set speed to " + newSpeed);
                try{
                    mMediaPlayer.setPlaybackParams(new PlaybackParams().setSpeed(newSpeed));
                }
                catch (Exception e){
                    Log.d(TAG, "Failed to setPlaybackkParams " + e);
                    val1 = -1 ;
                }

                mPlaySpeed = newSpeed;
                forward_speed_index = newIndex;
                updateButtons(UPDATE_PLAYBACK_SPEED, val1, 0, mPlaySpeed);
                backward_speed_index = -1;
                if(mPlayState == PLAYER_PAUSED)
                    start();
            }
            else{
                Log.d(TAG,"wrong state " + mPlayState + " to ff");
            }

        }
    }

    private void fastBackward() {
        Log.d(TAG,"fastBackward");

        if(false){//mMediaPlayer != null) {
            int savedState = -1;
            float newSpeed;

            if(++backward_speed_index >= backward_speed.length)
                backward_speed_index = 0;

            newSpeed = backward_speed[backward_speed_index];

            if(mPlayState == PLAYER_PLAYING || mPlayState == PLAYER_PAUSED){
                Log.d(TAG,"set speed to " + newSpeed);
                mMediaPlayer.setPlaybackParams(new PlaybackParams().setSpeed(newSpeed));
                mPlaySpeed = newSpeed;
                updateButtons(UPDATE_PLAYBACK_SPEED, 0, 0, mPlaySpeed);
                forward_speed_index = -1;
                if(mPlayState == PLAYER_PAUSED)
                    start();
            }
            else{
                Log.d(TAG,"wrong state to fw");
            }
        }

    }

    private void setNormalSpeed() {
        if(mMediaPlayer != null) {
            mPlaySpeed = 1;
            Log.d(TAG,"set speed to " + mPlaySpeed);
            mMediaPlayer.setPlaybackParams(new PlaybackParams().setSpeed(mPlaySpeed));

            updateButtons(UPDATE_PLAYBACK_SPEED, 0, 0, mPlaySpeed);
            forward_speed_index = backward_speed_index = -1;
        }
    }

    private void seekTo(long ms) {
        if(mMediaPlayer != null) {
            mMediaPlayer.seekTo((int)ms);
        }
    }

    private void createAudioAndSubtitleMenu(Menu menu)
    {
        Log.d(TAG,"createAudioAndSubtitleMenu");
        //add select audio icon
        if(mMediaPlayer != null){
            MediaPlayer.TrackInfo[] ti ;
            try{
                ti = mMediaPlayer.getTrackInfo();

            }
            catch (Exception e){
                Log.d(TAG, "Failed to get track info when creating menu");
                return ;
            }
            int totalCount = ti.length;
            Log.v(TAG,"total track count " + totalCount);
            for(int i = 0; i< totalCount ; i++){
                if(ti[i].getTrackType() == MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_AUDIO)
                    mAudioTrackCount++;
                else if(ti[i].getTrackType() == MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_TIMEDTEXT)
                    mSubtitleTrackCount++;
            }
        }

        if(mAudioTrackCount >= 2 && null == menu.findItem(SELECT_AUDIO)){
            menu.add(0, SELECT_AUDIO, 0, getString(R.string.SelectAudio));
        }
        
        if(mSubtitleTrackCount >= 1){
            menu.add(0, SELECT_SUBTITLE, 0, getString(R.string.SelectSubtitle));
            menu.add(0, CLOSE_SUBTITLE,	0, getString(R.string.CloseSubtitle));
        }

    }
    //------------------------------------------------------------------------------------
    // GUI messages
    //------------------------------------------------------------------------------------

    @Override
    public boolean onPrepareOptionsMenu (Menu menu) {
        MenuItem item;
        super.onPrepareOptionsMenu(menu);
        Log.d(TAG,"==========onPrepareOptionsMenu ");
        boolean enabled = false;
        mHdmiApp = (HdmiApplication)getApplication();
        mPresentation = mHdmiApp.getPresentation();
        if(mPlaySpeed == 1 && (mPlayState == PLAYER_PLAYING || mPlayState == PLAYER_PAUSED))
            enabled = true;

        if((mPlayState == PLAYER_PLAYING || mPlayState == PLAYER_PAUSED) && false == bGetTrackInfo){
            createAudioAndSubtitleMenu( menu);
            bGetTrackInfo = true;
        }

        item = menu.findItem(SELECT_AUDIO);
        if(item != null)
            item.setEnabled(enabled);
        item = menu.findItem(SELECT_SUBTITLE);
        if(item != null)
            item.setEnabled(enabled);
        enabled = enabled && mCurSubtitleTrack >= 0;
        item = menu.findItem(CLOSE_SUBTITLE);
        if(item != null)
            item.setEnabled(enabled);

        item = menu.findItem(Cast_Screen);
        if(item != null){
            if(mPresentation != null){
                if( mPresentation.getPresentationState() == mPresentation_playing ||
                    mPresentation.getPresentationState() == mPresentation_pausing){
                        item.setEnabled(true);
                }
                else{
                    item.setEnabled(true);
                    item.setCheckable(true);
                    item.setChecked(false);
                }
            }
            else if(mPresentation == null && CastScreen_flag == true){
                item.setCheckable(true);
                item.setChecked(false);
                item.setEnabled(false);
                CastScreen_flag = false;
            }
            else if(mPresentation == null && CastScreen_flag == false){
                item.setEnabled(false);
                item.setChecked(false);
            }
        }
        if(mPresentation != null){
            if( mPresentation.getPresentationState() == mPresentation_playing ||
                mPresentation.getPresentationState() == mPresentation_pausing){
                    item = menu.findItem(Quit);
                    item.setEnabled(true);
                    item.setCheckable(true);
            }
            else {
                item = menu.findItem(Quit);
                item.setEnabled(false);
            }
        }
        else {
            item = menu.findItem(Quit);
            item.setEnabled(false);
        }
        return true;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        Log.d(TAG,"==========onCreateOptionsMenu ");
        super.onCreateOptionsMenu(menu);
        /*
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.operation, menu);
        */
        mHdmiApp = (HdmiApplication)getApplication();
        mPresentation = mHdmiApp.getPresentation();
        MenuItem item = menu.add(0, LOOP_FILE,	0, getString(R.string.LoopFile));
        MenuItem item2 = menu.add(1,Cast_Screen,1,getString(R.string.CastScreen));
        MenuItem item3 = menu.add(2,Quit,2, getString(R.string.quit));
        item.setCheckable(true);
        if(mPresentation != null){
            if (mPresentation.getPresentation_LoopStatus() == true){
                item.setChecked(true);
                mLoopFile = true;
            }
            else
                item.setChecked(false);

            item2.setCheckable(true);
            item3.setCheckable(true);
            if( mPresentation.getPresentationState() == mPresentation_playing ||
                mPresentation.getPresentationState() == mPresentation_pausing){
                    item2.setChecked(true);
                    CastScreen_flag = true;
                    item3.setEnabled(true);
            }
            else {
                item2.setChecked(false);
                item3.setEnabled(false);
                CastScreen_flag = false;
            }
        }
        else {
            item2.setEnabled(false);
            item3.setEnabled(false);
            CastScreen_flag = false;
        }

        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        boolean ret = super.onOptionsItemSelected(item);
        int itemId = item.getItemId();
        if(itemId == LOOP_FILE){
            if (item.isChecked()) {
                item.setChecked(false);
                if(mPresentation != null)
                    mPresentation.setPresentation_LoopStatus(false);
            }
            else {
                item.setChecked(true);
                if(mPresentation != null)
                    mPresentation.setPresentation_LoopStatus(true);
            }
            mLoopFile = item.isChecked();
            //if(mMediaPlayer != null)
            //mMediaPlayer.setLooping(mLoopFile);
        }
     //******************cast screen ***********************
        else if(itemId == Cast_Screen){
            if (item.isChecked()) {
                if(mPresentation != null){
                    item.setChecked(false);
                    CastScreen_flag = false;
                    if(!mHdmiApp.getmPresentationEnded())
                        mcurrpos = mPresentation.getcurrentPos();
                    mPresentation.cancel();
                    mPresentation.setPresentationState(mPresentation_disabled);
                    mPlayState = PLAYER_PLAYING;
                    Local_onCreate();
                    mHdmiApp.setmPresentationEnded(false);
                }
                else{
                    item.setChecked(false);
                    CastScreen_flag = false;
                    mPlayState = PLAYER_PLAYING;
                    Local_onCreate();
                }
            }
            else {
                item.setChecked(true);
                if(mPlayState == PLAYER_PLAYING){
                    mMediaPlayer.pause();
                    mPlayState = PLAYER_PAUSED;
                }
                mPresentation.setPresentationState(mPresentation_playing);
                mPresentation.show();
                mPresentation.startVideo(uri1);
                if(mPlayState != PLAYER_STOPPED){
                    int curr = mMediaPlayer.getCurrentPosition();
                        mPresentation.mseekTo(curr);
                }
                Remote_onCreate();
                CastScreen_flag = true;
            }
        }

        //************************totally quit
        if(itemId == Quit){
            if (item.isChecked()) {
                item.setChecked(false);
            }
            else {
                item.setChecked(true);
                mPresentation.cancel();
                System.exit(0);
                CastScreen_flag = false;
            }
            //if(mMediaPlayer != null)
            //    mMediaPlayer.setLooping(mLoopFile);
        }

        else if(itemId == SELECT_AUDIO){
            selectAudioDialog();
        }

        else if(itemId == SELECT_SUBTITLE){
            selectSubtitleDialog();
        }

        else if(itemId == CLOSE_SUBTITLE){
            if(mCurSubtitleTrack >= 0 && mMediaPlayer != null){
                Log.v(TAG,"to deselected track  " + mCurSubtitleTrack);
                try{
                    mMediaPlayer.deselectTrack(mCurSubtitleTrack);
                }
                catch(Exception e){
                    Log.d(TAG, "Failed to deselect track !!!");
                }
                mCurSubtitleTrack = -1;
                mCurSubtitleIndex = -1;
                mSubtitleTextView.setText(null,0);
            }
        }
        else if(itemId == android.R.id.home){
            this.finish();
            return true;
        }
        return ret;
    }



    /** Button messages
    */
    private OnClickListener mOnBtnClicked = new OnClickListener() {
        public void onClick(View view) {
            if(view == mBtnFastBack){
                fastBackward();
            }
            else if(view == mBtnPlayPause){
                 forward_speed_index = backward_speed_index = -1;

                if(mPlayState == PLAYER_PLAYING){
                    if(mPlaySpeed != 1)
                        setNormalSpeed();
                    pause();
                }
                else if(mPlayState == PLAYER_PAUSED){
                    mLastPlayState = -1;
                    mMediaPlayer.start();
                    mPlayState = PLAYER_PLAYING;
                    updateButtons(UPDATE_PLAYBACK_STATE, mPlayState, 0, 0);
                    mHandler.sendEmptyMessage(SHOW_PROGRESS);
                }
                else if(mPlayState == PLAYER_STOPPED){
                    //play();
                    mLastPlayState = -1;
                    mVideoView.setVisibility(View.VISIBLE);
                    // call play() in surfaceCreated()
                }
            }
            else if(view == mBtnFastForward){
                if(mPlayState != PLAYER_PLAYING && mPlayState != PLAYER_PAUSED)
                    return;
                fastForward();
            }
        }
    };

    /** Seekbar messages
     */
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        if (mDuration != 0) {
            mCurrentProgress = progress;
        }
        if (!fromUser) {
            // We're not interested in programmatically generated changes to
            // the progress bar's position.
            return;
        }
        if(mDuration == 0)
            return;

        long newposition = (mDuration * progress) / 100L;
        seekTo( (int) newposition);
        updateButtons(UPDATE_PLAYBACK_PROGRESS, 0, progress * mDuration / 100, 0);
    }


    public void onStartTrackingTouch(SeekBar seekBar) {
        Log.d(TAG,"onStartTrackingTouch");
        mDragging = true;
        mHandler.removeMessages(SHOW_PROGRESS);
    }

    public void onStopTrackingTouch(SeekBar seekBar) {
        mDragging = false;
        mHandler.sendEmptyMessage(SHOW_PROGRESS);
    }

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            int pos;
            switch (msg.what) {
                case SHOW_PROGRESS:
                    if(mMediaPlayer == null)
                        break;

                    if (!mDragging && mMediaPlayer.isPlaying()) {
                        int curr = mMediaPlayer.getCurrentPosition();
                        updateButtons(UPDATE_PLAYBACK_PROGRESS, 0, curr, 0);
                        msg = obtainMessage(SHOW_PROGRESS);
                        sendMessageDelayed(msg, 500);//1000 - (pos % 1000));
                     }
                        break;

            }
        }
    };

    private void selectAudioDialog() {

        String[] audioList;
        MediaPlayer.TrackInfo[] ti;
        int totalCount = 0;
        int audioCount = 0;
        if(mMediaPlayer == null)
            return;
        ti = mMediaPlayer.getTrackInfo();
        totalCount = ti.length;
        Log.v(TAG,"total track count " + totalCount);
        for(int i = 0; i< totalCount ; i++){
            if(ti[i].getTrackType() == MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_AUDIO)
                audioCount++;
        }
        audioList = new String[audioCount];
        audioCount = 0;
        for(int i = 0; i< totalCount ; i++){
            Log.v(TAG,"track " + i + " type is " + ti[i].getTrackType()+ " lan " + ti[i].getLanguage());
            if(ti[i].getTrackType() == MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_AUDIO){
                audioList[audioCount] = ti[i].getLanguage();
                audioCount++;
            }
        }

        LayoutInflater inflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        final View dialogView = inflater.inflate(R.layout.selectaudio, null ,false);
        dialogView.setBackgroundResource(R.drawable.rounded_corners_view);

        final PopupWindow pw = new PopupWindow(dialogView, 480, LayoutParams.WRAP_CONTENT, true);
        pw.setBackgroundDrawable(getResources().getDrawable(R.drawable.rounded_corners_pop));

        ListView list = (ListView) dialogView.findViewById(R.id.list);

        // add item index to chapter name
        String items[] = new String[audioList.length];
        int i;
        for(i=0; i<audioList.length; i++) {
            items[i] = Integer.toString(i+1) + ": "; // add index to title name
            if(audioList[i] != null){
                String fullName = map.get(audioList[i]);
                if(fullName != null)
                    items[i] += fullName;
                else
                    items[i] += "Audio track " + Integer.toString(i+1);
            }
            else
                items[i] += "Audio track " + Integer.toString(i+1);
        }

        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this,
        android.R.layout.simple_list_item_1,
        items);
        list.setAdapter(adapter);
        list.setOnItemClickListener(new OnItemClickListener(){
        @Override
            public void onItemClick(AdapterView<?> av, View v, int i, long l) {
                if(mMediaPlayer != null) {
                    //String itemText = ((TextView)v).getText().toString();
                    //String sub[] = itemText.split(":");
                    //Log.d(TAG,"====clicked item is " + itemText + " index " + i);
                    //nativeExecuteCommand("at " + sub[0]);
                    int index = -1;
                    MediaPlayer.TrackInfo[] ti = mMediaPlayer.getTrackInfo();
                    int totalCount = ti.length;
                    for(int j = 0; j< totalCount ; j++){
                        if(ti[j].getTrackType() == MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_AUDIO)
                            index++;
                        if(index == i){
                            //mMediaPlayer.selectTrack(j);
                            Parcel request = Parcel.obtain();
                            Parcel reply = Parcel.obtain();
                            try {
                                request.writeInterfaceToken(IMEDIA_PLAYER);
                                request.writeInt(INVOKE_ID_SELECT_TRACK);
                                request.writeInt(j);
                                mMediaPlayer.invoke(request, reply);
                            } finally {
                                request.recycle();
                                reply.recycle();
                            }
                            //mInfoView.setText();
                            mCurAudioTrack = j;
                            mCurAudioIndex = i;
                            Log.d(TAG,"select audio track " + mCurAudioTrack);
                            break;
                        }
                    }
                }
                pw.dismiss();
            }
        });


        Button btnCancel = (Button)   dialogView.findViewById(R.id.BtnCancel);
        btnCancel.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                pw.dismiss();
            }
        });
        pw.showAtLocation((View)findViewById(R.id.ActiveWindow), Gravity.CENTER, 0, 0);
    }

    private void selectSubtitleDialog() {
        String[] subtitleList;
        MediaPlayer.TrackInfo[] ti;
        int totalCount = 0;
        int subtitleCount = 0;
        if(mMediaPlayer == null)
            return;
        ti = mMediaPlayer.getTrackInfo();
        totalCount = ti.length;
        Log.v(TAG,"total track count " + totalCount);
        for(int i = 0; i< totalCount ; i++){
            if(ti[i].getTrackType() == MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_TIMEDTEXT)
                subtitleCount++;
        }

        subtitleList = new String[subtitleCount];
        subtitleCount = 0;
        for(int i = 0; i < totalCount ; i++){
            Log.v(TAG,"track " + i + " type is " + ti[i].getTrackType()+ " lan " + ti[i].getLanguage());
            if(ti[i].getTrackType() == MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_TIMEDTEXT){
                subtitleList[subtitleCount] = ti[i].getLanguage();
                subtitleCount++;
            }
        }

        LayoutInflater inflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        final View dialogView = inflater.inflate(R.layout.selectsubtitle, null ,false);
        dialogView.setBackgroundResource(R.drawable.rounded_corners_view);

        final PopupWindow pw = new PopupWindow(dialogView, 480, LayoutParams.WRAP_CONTENT, true);
        pw.setBackgroundDrawable(getResources().getDrawable(R.drawable.rounded_corners_pop));

        ListView list = (ListView) dialogView.findViewById(R.id.list);

        // add item index to chapter name
        String items[] = new String[subtitleList.length];
        int i;
        for(i = 0; i < subtitleList.length; i++) {
            items[i] = Integer.toString(i+1) + ": "; // add index to title name
            if(subtitleList[i] != null){
                String fullName = map.get(subtitleList[i]);
                if(fullName != null)
                    items[i] += fullName;
                else
                    items[i] += "Subtitle track " + Integer.toString(i+1);
            }
            else
                items[i] += "Subtitle track " + Integer.toString(i+1);
        }

        ArrayAdapter<String> adapter = new ArrayAdapter<String>(this,
        android.R.layout.simple_list_item_1,
        items);
        list.setAdapter(adapter);
        list.setOnItemClickListener(new OnItemClickListener(){
            @Override
            public void onItemClick(AdapterView<?> av, View v, int i, long l) {
                if(mMediaPlayer != null) {
                    int index = -1;
                    MediaPlayer.TrackInfo[] ti = mMediaPlayer.getTrackInfo();
                    int totalCount = ti.length;
                    for(int j = 0; j < totalCount ; j++){
                        Log.v(TAG,"track " + j + " type is " + ti[j].getTrackType());
                        if(ti[j].getTrackType() == MediaPlayer.TrackInfo.MEDIA_TRACK_TYPE_TIMEDTEXT)
                            index++;
                        if(index == i){
                            Log.v(TAG,"selected track " + j);
                            try{
                                mMediaPlayer.selectTrack(j);
                            }
                            catch(Exception e){
                                Log.d(TAG, "Failed to select subtitle track!!!");
                                break;
                            }
                            mCurSubtitleTrack = j;
                            mCurSubtitleIndex = i;
                            break;
                        }
                    }
                }
                pw.dismiss();
            }
        });


        Button btnCancel = (Button)   dialogView.findViewById(R.id.BtnCancel);
        btnCancel.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                pw.dismiss();
            }
        });
        pw.showAtLocation((View)findViewById(R.id.ActiveWindow), Gravity.CENTER, 0, 0);
    }


    private void pauseMetadataShow() {
        //mErrSign.pause();
        mInfoView.pause();
        mSubtitleTextView.pause();
    }

    private void resumeMetadataShow() {
        //mErrSign.resume();
        mInfoView.resume();
        mSubtitleTextView.resume();
    }

    private Map<String, String> map = new HashMap<String, String>();

    private void initLocaleTable() {
        map.put("alb", "Albanian");
        map.put("sqi", "Albanian");
        map.put("sq" , "Albanian");
        map.put("ara", "Arabic");
        map.put("ar" , "Arabic");
        map.put("arm", "Armenian");
        map.put("hye", "Armenian");
        map.put("hy" , "Armenian");
        map.put("art", "Artificial languages");
        map.put("ast", "Asturian");
        map.put("aus", "Australian languages");
        map.put("aze", "Azerbaijani");
        map.put("az" , "Azerbaijani");
        map.put("ast", "Bable");
        map.put("bat", "Baltic languages");
        map.put("bam", "Bambara");
        map.put("bm" , "Bambara");
        map.put("chi", "Chinese");
        map.put("zho", "Chinese");
        map.put("zh" , "Chinese");
        map.put("zha", "Chuang");
        map.put("za" , "Chuang");
        map.put("mus", "Creek");
        map.put("cze", "Czech");
        map.put("ces", "Czech");
        map.put("cs" , "Czech");
        map.put("dan", "Danish");
        map.put("da" , "Danish");
        map.put("dut", "Dutch");
        map.put("nld", "Dutch");
        map.put("nl" , "Dutch");
        map.put("eng", "English");
        map.put("en" , "English");
        map.put("est", "Estonian");
        map.put("et" , "Estonian");
        map.put("fan", "Fang");
        map.put("fat", "Fanti");
        map.put("fin", "Finnish");
        map.put("fi" , "Finnish");
        map.put("fre", "French");
        map.put("fra", "French");
        map.put("fr" , "French");
        map.put("geo", "Georgian");
        map.put("kat", "Georgian");
        map.put("ka" , "Georgian");
        map.put("ger", "German");
        map.put("deu", "German");
        map.put("de" , "German");
        map.put("hun", "Hungarian");
        map.put("hu" , "Hungarian");
        map.put("ice", "Icelandic");
        map.put("isl", "Icelandic");
        map.put("is" , "Icelandic");
        map.put("ind", "Indonesian");
        map.put("in" , "Indonesian");
        map.put("ira", "Iranian languages");
        map.put("gle", "Irish");
        map.put("ga" , "Irish");
        map.put("ita", "Italian");
        map.put("it" , "Italian");
        map.put("jpn", "Japanese");
        map.put("ja" , "Japanese");
        map.put("kon", "Kongo");
        map.put("kg" , "Kongo");
        map.put("kor", "Korean");
        map.put("ko" , "Korean");
        map.put("kur", "Kurdish");
        map.put("ku" , "Kurdish");
        map.put("lao", "Lao");
        map.put("lo" , "Lao");
        map.put("lat", "Latin");
        map.put("la" , "Latin");
        map.put("per", "Persian");
        map.put("fas", "Persian");
        map.put("fa" , "Persian");
        map.put("phi", "Philippine languages");
        map.put("pol", "Polish");
        map.put("pl" , "Polish");
        map.put("por", "Portuguese");
        map.put("pt" , "Portugueses");
        map.put("rum", "Romanian");
        map.put("ron", "Romanian");
        map.put("ro" , "Romanian");
        map.put("rus", "Russian");
        map.put("ru" , "Russian");
        map.put("sco", "sco");
        map.put("srp", "Serbian");
        map.put("sr" , "Serbian");
        map.put("spa", "Spanish");
        map.put("es" , "Spanish");
        map.put("swe", "Swedish");
        map.put("sv" , "Swedish");
        map.put("tha", "Thai");
        map.put("th" , "Thai");
        map.put("tur", "Turkish");
        map.put("tr" , "Turkish");
        map.put("ukr", "Ukrainian");
        map.put("uk" , "Ukrainian");
        map.put("vie", "Vietnamese");
        map.put("vi" , "Vietnamese");
        map.put("nor", "Norwegian");
        map.put("no" , "Norwegian");
        map.put("grc", "Greek");
        map.put("gre", "Greek");
        map.put("ell", "Greek");
        map.put("el" , "Greek");
    }
    private String localeToLangName(String locale) {
        String name = null;
        if(locale != null)
            name = map.get(locale);
        if(name == null)
            name = "Unspecified language";
        return name;
    }
}

