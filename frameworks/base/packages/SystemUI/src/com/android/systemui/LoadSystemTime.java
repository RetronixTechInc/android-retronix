/*
 * Copyright (C) 2007 The Android Open Source Project
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

package com.android.systemui;

import com.android.internal.app.ProcessStats;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;

import android.graphics.PixelFormat;
import android.graphics.Color;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuff;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.util.Log;
import java.util.List;
import java.util.ArrayList;
import java.util.ListIterator;
import java.util.Collections;
import java.util.Date;
import java.text.SimpleDateFormat;

public class LoadSystemTime extends Service {
    private SurfaceView mView;
    private List<String> mTimeList;
    private String mPattern;
    private SimpleDateFormat mSdf;
    private String mTimeString;
    private Paint mAddedPaint;
    private String mTime;
    private WindowManager wm;
    private WindowManager.LayoutParams params;
    private int mIndex;
    private final int mLineNumber = 12;
    private final int mTimeForDelay = 10;
    private int mCanvasWidth;
    private final int mTextSize = 30;
    private class LoadView extends SurfaceView implements SurfaceHolder.Callback {
        private Handler mHandler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                if (msg.what == 1) {
                    updateDisplay();
                    Message m = obtainMessage(1);
                    sendMessageDelayed(m, mTimeForDelay);
                }
            }
        };

        private SurfaceHolder surfaceHolder;
        LoadView(Context c) {
            super(c);

            setPadding(4, 4, 4, 4);
            int textSize = mTextSize;

            mAddedPaint = new Paint();
            mAddedPaint.setAntiAlias(false);
            mAddedPaint.setTextSize(textSize);
            mAddedPaint.setARGB(255, 128, 255, 128);

            surfaceHolder = this.getHolder();
            getHolder().setFormat(PixelFormat.TRANSLUCENT);
            surfaceHolder.addCallback(this);
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            mHandler.sendEmptyMessage(1);
        }

        @Override
        protected void onDetachedFromWindow() {
            super.onDetachedFromWindow();
            mHandler.removeMessages(1);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
             setMeasuredDimension(mCanvasWidth,mTextSize*mLineNumber);
        }

        @Override
        public void onDraw(Canvas canvas) {
            final int RIGHT = getWidth()-1;

            for(int j=0; j < mLineNumber; j++){
                canvas.drawText(mTimeList.get(j),RIGHT-mAddedPaint.measureText(mTimeList.get(j))-2,mTextSize+mTextSize*j,mAddedPaint);
            }
        }

        public void repaint(){
           Canvas c = null;
           try{
             c = surfaceHolder.lockCanvas();
             c.drawColor(Color.TRANSPARENT,PorterDuff.Mode.CLEAR);
             onDraw(c);
           } finally {
              if(c != null){
                surfaceHolder.unlockCanvasAndPost(c);
              }
           }
        }
        void updateDisplay() {
             mTimeString = mSdf.format(new Date());
             mTimeList.remove(mIndex);
             mTimeList.add(mIndex,mTimeString);
             mIndex++;
             if(mIndex == mLineNumber){
                mIndex = 0;
             }
             repaint();
        }
       public void surfaceDestroyed (SurfaceHolder holder){}
       public void surfaceCreated (SurfaceHolder holder){}
       public void surfaceChanged (SurfaceHolder holder, int format, int width, int height){}

    }

    @Override
    public void onCreate() {
        super.onCreate();
        mIndex = 0;
        mPattern = "mm:ss:SSS";
        mSdf = new SimpleDateFormat(mPattern);
        mTimeList = new ArrayList<String>();
        for(int i = 0; i < mLineNumber; i++ ){
           mTimeString = mSdf.format(new Date());
           mTimeList.add(mTimeString);
        }
        params = new WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_SECURE_SYSTEM_OVERLAY,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|
            WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
            PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.RIGHT | Gravity.TOP;
        params.setTitle("Load System Time");
        mView = new LoadView(this);
        mCanvasWidth = (int)mAddedPaint.measureText(mTimeList.get(0))+4;
        wm = (WindowManager)getSystemService(WINDOW_SERVICE);
        wm.addView(mView, params);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        ((WindowManager)getSystemService(WINDOW_SERVICE)).removeView(mView);
        mView = null;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

}
