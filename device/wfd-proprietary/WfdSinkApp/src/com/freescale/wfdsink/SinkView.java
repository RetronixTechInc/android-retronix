/*
 * Copyright (C) 2013-2015 Freescale Semiconductor, Inc.
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

import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.content.Context;
import android.os.SystemProperties;
import android.util.AttributeSet;
import android.util.Log;

public class SinkView extends SurfaceView {
    private String TAG = "SinkView";

    //define the TS stream resolution.
    private int mVideoWidth;
    private int mVideoHeight;
    private float mOldAspectRatio;

    private void initVideoView() {
        mVideoWidth        = 0;
        mVideoHeight       = 0;
        mOldAspectRatio    = (float) 0.0;
    }

    public SinkView(Context context) {
        super(context);
        initVideoView();
    }

    public SinkView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initVideoView();
    }

    public SinkView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        initVideoView();
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {

        Log.d(TAG, "onMeasure:" + " mVideoW " + mVideoWidth + ", mVideoH " +
                mVideoHeight + ", widthMeasureSpec " + widthSpec +
                ", heightMeasureSpec " + heightSpec);
        if(mVideoWidth == 0 || mVideoHeight == 0) { // no video now
            setMeasuredDimension(1, 1);
            return;
        }

        int previewHeight = MeasureSpec.getSize(heightSpec);
        int previewWidth = MeasureSpec.getSize(widthSpec);
        if (previewWidth > previewHeight * mVideoWidth / mVideoHeight) {
            previewWidth = previewHeight * mVideoWidth / mVideoHeight;
        }
        else {
            previewHeight = previewWidth * mVideoHeight / mVideoWidth;
        }

        super.onMeasure(MeasureSpec.makeMeasureSpec(previewWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(previewHeight, MeasureSpec.EXACTLY));
    }

    public void onVideoSizeChanged(int newWidth, int newHeight, int sarW, int sarH) {
        Log.d(TAG, "onVideoSizeChanged: " + newWidth + " x " + newHeight + ", (" + sarW + "/" + sarH + ")");

        float sar = (float)sarW / sarH;

        if(sar >= 1.0f) {
            mVideoWidth = (int)(sar * newWidth + 0.5);
            mVideoHeight = newHeight;
        }
        else {
            mVideoWidth	 = newWidth;
            mVideoHeight = (int)(newHeight / sar + 0.5);
        }

        // video size changed, but view content size is unchanged if picture aspect ratio is unchanged
        float newAspectRatio = (float)mVideoWidth/mVideoHeight;
        float diff = newAspectRatio - mOldAspectRatio;
        if(diff < 0) diff = -diff;
        float distortion = diff / newAspectRatio;
        if(mOldAspectRatio == 0 || distortion > 0.05) {
            // will call onMeasure, and then surfaceChanged(size is same as setFixedSize)
            getHolder().setFixedSize(mVideoWidth, mVideoHeight);
            mOldAspectRatio = newAspectRatio;
        }
    }
}
