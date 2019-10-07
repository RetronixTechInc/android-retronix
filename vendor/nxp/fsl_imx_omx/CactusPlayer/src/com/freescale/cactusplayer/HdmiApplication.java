/*
 * Copyright 2012 The Android Open Source Project
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
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

package com.freescale.cactusplayer;

import android.widget.VideoView;

import android.util.Log;
import android.media.MediaPlayer;
import android.media.MediaPlayer.OnCompletionListener;
import android.media.MediaPlayer.OnErrorListener;
import android.media.MediaPlayer.OnPreparedListener;
import android.media.MediaRouter.RouteInfo;
import android.media.MediaRouter;
import android.app.Application;
import android.app.Presentation;
import java.io.File;
import java.io.FilenameFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.UserHandle;
import java.util.List;
import java.util.ArrayList;
import android.net.Uri;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ArrayAdapter;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.Spinner;
import android.widget.TextView;
import android.view.Display;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.AdapterView;
import android.media.ThumbnailUtils;
import android.provider.MediaStore;
import android.content.Context;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.view.WindowManager;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.provider.Settings;
import android.hardware.display.DisplayManager;
import android.net.wifi.WifiManager;
public class HdmiApplication extends Application{
    private static final String TAG = "HdmiApplication";
    private MediaRouter mMediaRouter = null;
    private DemoPresentation mPresentation = null;
    private Uri mVideoUri;
    public boolean mVideoRunning = false;
    private int mVideoIndex = -1;
    private boolean mPresentation_loopstatus = false;
    private boolean mPresentationEnded = false;

    public final static String EXTRA_HDMI_PLUGGED_STATE = "state";
    public final static String ACTION_HDMI_PLUGGED = "android.intent.action.HDMI_PLUGGED";
    public final static String PLATFORM_CHECK = "/sys/class/mxc_ipu/mxc_ipu/dev";

    // It was defined in DisplayManager.java, later removed from Android version O. We have to define it here.
    public static final String ACTION_WIFI_DISPLAY_DISCONNECTION =
                 "android.hardware.display.action.WIFI_DISPLAY_DISCONNECTION";

    public interface Callback {
        public void onPresentationEnded(boolean end);
        public void onPresentationChanged(boolean plugin);
    }

    private Callback mListener = null;
    private Callback mPresentationChangedListener = null;

    public void addListener(Callback listener) {
        mListener = listener;
        mPresentationChangedListener = listener;
    }

    public DemoPresentation getPresentation() {
        return mPresentation;
    }

    public boolean getVideoRunning() {
        return mVideoRunning;
    }

    public int getVideoIndex() {
        return mVideoIndex;
    }

    public boolean getmPresentationEnded(){
        return mPresentationEnded;
    }

    public void setmPresentationEnded(boolean ended){
        mPresentationEnded = ended;
    }

    public void onCreate() {
        super.onCreate();
        mMediaRouter = (MediaRouter)getSystemService(Context.MEDIA_ROUTER_SERVICE);
        mMediaRouter.addCallback(MediaRouter.ROUTE_TYPE_LIVE_VIDEO, mMediaRouterCallback);
        // updatePresentation();
        // Get the current route and its presentation display.
        MediaRouter.RouteInfo route = mMediaRouter.getSelectedRoute(
                MediaRouter.ROUTE_TYPE_LIVE_VIDEO);
        Display presentationDisplay = route != null ? route.getPresentationDisplay() : null;
        if (mPresentation == null && presentationDisplay != null) {
            Log.w(TAG, "Showing presentation on display: " + presentationDisplay);
            mPresentation = new DemoPresentation(this, presentationDisplay);
            try {
                WindowManager.LayoutParams l = mPresentation.getWindow().getAttributes();
                //l.type = WindowManager.LayoutParams.FIRST_SYSTEM_WINDOW + 100;
                l.type = WindowManager.LayoutParams.TYPE_SYSTEM_ALERT;
             } catch (WindowManager.InvalidDisplayException ex) {
                Log.w(TAG, "Couldn't show presentation!  Display was removed in "
                        + "the meantime.", ex);
                mPresentation = null;
             }
         }

    }


    private final MediaRouter.SimpleCallback mMediaRouterCallback =
        new MediaRouter.SimpleCallback() {
            @Override
            public void onRouteSelected(MediaRouter router, int type, RouteInfo info) {
                Log.w(TAG, "onRouteSelected: type=" + type + ", info=" + info);
                updatePresentation();
            }

            @Override
            public void onRouteUnselected(MediaRouter router, int type, RouteInfo info) {
                Log.w(TAG, "onRouteUnselected: type=" + type + ", info=" + info);
                updatePresentation();
            }

            @Override
            public void onRoutePresentationDisplayChanged(MediaRouter router, RouteInfo info) {
                Log.w(TAG, "onRoutePresentationDisplayChanged: info=" + info);
                updatePresentation();
            }
        };

        private void updatePresentation() {
            // Get the current route and its presentation display.
            MediaRouter.RouteInfo route = mMediaRouter.getSelectedRoute(
                MediaRouter.ROUTE_TYPE_LIVE_VIDEO);
                Display presentationDisplay = route != null ? route.getPresentationDisplay() : null;
                // presentationDisplay.getRefreshRate();
                // Dismiss the current presentation if the display has changed.
                if (mPresentation != null && mPresentation.getDisplay() != presentationDisplay) {
                     Log.w(TAG, "Dismissing presentation because the current route no longer "
                     + "has a presentation display.");
                     //mPresentation disconnect
                     if (mPresentationChangedListener != null) {
                         mPresentationChangedListener.onPresentationChanged(false);
                     }

                     mPresentation.dismiss();
                     mPresentation = null;
                }

                // Show a new presentation if needed.
                if (mPresentation == null && presentationDisplay != null) {
                    Log.w(TAG, "Showing presentation on display: " + presentationDisplay);
                    mPresentation = new DemoPresentation(this, presentationDisplay);
                    try {
                         WindowManager.LayoutParams l = mPresentation.getWindow().getAttributes();
                         //l.type = WindowManager.LayoutParams.FIRST_SYSTEM_WINDOW + 100;
                         l.type = WindowManager.LayoutParams.TYPE_SYSTEM_ALERT;
                         //mPresentation connect again
                         if (mPresentationChangedListener != null) {
                             mPresentationChangedListener.onPresentationChanged(true);
                          }
                    } catch (WindowManager.InvalidDisplayException ex) {
                         Log.w(TAG, "Couldn't show presentation!  Display was removed in "
                         + "the meantime.", ex);
                         mPresentation = null;
                    }
                 }
         }

        public class DemoPresentation extends Presentation {
            protected static final String TAG2 = "DemoPresentation ";
            private VideoView mvideoview;
            private SeekBar mseekbar;
            private TextView mcurrentpos,mduration;
            private static final int    SHOW_PROGRESSBAR  = 0;
            private static final int    SHOW_PROGRESSTIME = 1;
            private int myPresentationState;
            private ProgressHandler mHandler;

            private class ProgressHandler extends Handler {
                @Override
                public void handleMessage(Message msg) {
                    switch (msg.what) {
                        case SHOW_PROGRESSBAR:
                             mseekbar.setMax(mvideoview.getDuration());
                             mseekbar.setProgress(mvideoview.getCurrentPosition());
                             mHandler.sendEmptyMessageDelayed(SHOW_PROGRESSBAR, 500);
                             break;

                        case SHOW_PROGRESSTIME:
                             show_ProgressTime();
                             mHandler.sendEmptyMessageDelayed(SHOW_PROGRESSTIME, 500);
                             break;

                        default:
                             break;
                     }
                 }
            };

            public void show_ProgressTime(){
                //mPresentation duration
                int a=mvideoview.getDuration();
                long seconds = a / 1000;
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
                mduration.setText(s);
                seconds = mvideoview.getCurrentPosition() / 1000;
                minutes = seconds / 60;
                seconds = seconds - minutes * 60;
                hours = minutes / 60;
                minutes = minutes - hours * 60;
                s = "";
                if(hours < 10)
                    s += "0";
                s += hours + ":" ;
                if(minutes < 10)
                    s += "0";
                s += minutes + ":";
                if(seconds < 10)
                    s += "0";
                s += seconds;
                mcurrentpos.setText(s);
           }

           public DemoPresentation(Context context, Display display) {
               super(context, display);
           }

           @Override
           protected void onCreate(Bundle savedInstanceState) {
               // Be sure to call the super class.
               super.onCreate(savedInstanceState);
               // Get the resources for the context of the presentation.
               // Notice that we are getting the resources from the context of the presentation.
               Resources r = getContext().getResources();

               // Inflate the layout.
               setContentView(R.layout.presentation_with_media_router_content);

               // Set up the surface view for visual interest.
               mvideoview = (VideoView)findViewById(R.id.videoview1);
               mvideoview.setOnCompletionListener(new MediaPlayer.OnCompletionListener(){
                  @Override
                  public void onCompletion(MediaPlayer mp)  {
                      if(mPresentation_loopstatus == true){
                          mvideoview.start();
                          mListener.onPresentationEnded(false);
                      }
                      else{
                          mPresentationEnded = true;
                          if(mListener != null){
                              mListener.onPresentationEnded(true);
                              mVideoRunning = false;
                          }
                       }
                  }
               });

               mvideoview.setOnErrorListener(new MediaPlayer.OnErrorListener(){
                  @Override
                  public boolean onError(MediaPlayer mp, int what, int extra){
                      mvideoview.setVideoURI(mVideoUri);
                      mvideoview.start();
                      return true;
                  }
               });
               mseekbar = (SeekBar)findViewById(R.id.seekbar);
               mcurrentpos = (TextView)findViewById(R.id.currentpos);
               mduration = (TextView)findViewById(R.id.duration);
               mHandler = new ProgressHandler();
         }

        private final ContentObserver mSettingsObserver = new ContentObserver(mHandler) {
            @Override
            public void onChange(boolean selfChange, Uri uri) {
                boolean connected = Settings.Global.getInt(getContentResolver(),
                Settings.Global.WIFI_ON, 0) != 0;
                if (!connected) {
                    pauseVideo();
                    Log.d(TAG,"ContentObserver wifi_off pauseVideo");
                }
            }
        };

        private final BroadcastReceiver mReceiver = new BroadcastReceiver(){
            @Override
            public void onReceive(Context context, Intent intent){
                String action = intent.getAction();
                if(action.equals(ACTION_WIFI_DISPLAY_DISCONNECTION)){
                    pauseVideo();
                }
                Log.d(TAG,"BroadcastReceiver WIFI_DISPLAY_DISCONNECTION");
             }
         };

         protected void onStart() {
             super.onStart();
             getContentResolver().registerContentObserver(Settings.Global.getUriFor(
                 Settings.Global.WIFI_ON), false, mSettingsObserver, UserHandle.USER_ALL);
             mHandler.sendEmptyMessageDelayed(SHOW_PROGRESSBAR, 500);
             mHandler.sendEmptyMessageDelayed(SHOW_PROGRESSTIME, 500);
             IntentFilter filter = new IntentFilter();
             filter.addAction(ACTION_WIFI_DISPLAY_DISCONNECTION);
             registerReceiver(mReceiver, filter);
         }

         protected void onStop() {
             super.onStop();
             unregisterReceiver(mReceiver);
             mHandler.removeMessages(SHOW_PROGRESSBAR);
             mHandler.removeMessages(SHOW_PROGRESSTIME);
         }

         public void setPresentation_LoopStatus(boolean loopstatus){
             mPresentation_loopstatus = loopstatus;
         }

         public boolean getPresentation_LoopStatus(){
             return mPresentation_loopstatus;
         }

         public void startVideo(Uri uri) {
             mVideoRunning = true;
             mVideoUri = uri;
             mvideoview.setVideoURI(uri);
             mvideoview.requestFocus();
             mvideoview.start();
         }

         public Uri getmyUri(){
             return mVideoUri;
         }

         public void mseekTo(int msec){
             mVideoRunning = true;
             int ms = msec;
             mvideoview.seekTo(ms);
         }

         public int getcurrentPos(){
             int mcurPos = mvideoview.getCurrentPosition();
             return mcurPos;
         }

         public int getDuration(){
             return mvideoview.getDuration();
         }

         //   0 means no presentation
         //   1 means presentation is playing
         //   2 means presentation is pausing
         public void setPresentationState(int State){
             myPresentationState = State;
         }

         public int getPresentationState(){
             return myPresentationState;
         }

         public void pauseVideo() {
             mVideoRunning = false;
             mvideoview.pause();
         }

         public void resumeVideo() {
             mVideoRunning = true;
             mvideoview.start();
         }

         public void stopVideo() {
             mVideoRunning = false;
             mvideoview.stopPlayback();
         }
    }
}

