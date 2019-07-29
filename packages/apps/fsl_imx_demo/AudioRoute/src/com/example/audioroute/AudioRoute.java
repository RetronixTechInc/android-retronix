/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (C) 2012-2015 Freescale Semiconductor, Inc.
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

package com.example.audioroute;

import java.io.File;
import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.CheckBox;
import android.widget.Button;
import android.widget.RadioButton;
import android.widget.TextView;
import android.media.AudioManager;
import android.media.AudioSystem;
import android.media.AudioDeviceInfo;
import android.os.SystemProperties;

public class AudioRoute extends Activity {

    private final static String TAG = "AudioRoute";

    private TextView mCommand;
    private CheckBox mSpdifIn;
    private TextView mTextViewOutputChoose;
    private CheckBox mCheckBoxPassthrough;
    private TextView mTextViewHDMIAudio;
    private CheckBox mCheckBoxHDMIAudio;

    private AudioManager mAudioManager;
	private AudioDeviceInfo[] ADI;
	/** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Create and attach the view that is responsible for painting.

        mAudioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
		ADI = mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);//artietest
		for(int i=0 ; i<ADI.length ; i++)
			Log.d(TAG, "------deviceid=" +ADI[i].getId()+", type="+ADI[i].getType()+", devicename="+ADI[i].getProductName());

        File fileHdmi=new File("proc/asound/imxhdmisoc");
        File fileSpdif=new File("proc/asound/imxspdif");
        if(fileHdmi.exists()&&!fileSpdif.exists())
        {
         setContentView(R.layout.passthrough);
         mCheckBoxPassthrough = (CheckBox)findViewById(R.id.CheckPassthrough);
         mTextViewOutputChoose = (TextView) findViewById(R.id.AudioOutputText);
         mCheckBoxHDMIAudio = (CheckBox)findViewById(R.id.CheckHDMIAudio);
         mTextViewHDMIAudio = (TextView) findViewById(R.id.TextHDMIAudio);
        }
        else if(!fileHdmi.exists()&&fileSpdif.exists())
        {
         setContentView(R.layout.spdifin);
         mSpdifIn = (CheckBox)findViewById(R.id.spdif_in);
         mCommand = (TextView) findViewById(R.id.commandText);
        }
        else if(fileHdmi.exists()&&fileSpdif.exists())
        {
         setContentView(R.layout.main);
         mCheckBoxPassthrough = (CheckBox)findViewById(R.id.CheckPassthrough);
         mSpdifIn = (CheckBox)findViewById(R.id.spdif_in);
         mCommand = (TextView) findViewById(R.id.commandText);
         mTextViewOutputChoose = (TextView) findViewById(R.id.AudioOutputText);
         mCheckBoxHDMIAudio = (CheckBox)findViewById(R.id.CheckHDMIAudio);
         mTextViewHDMIAudio = (TextView) findViewById(R.id.TextHDMIAudio);
        }
        else 
        {
         setContentView(R.layout.nodevice);
         Log.d(TAG,"no device connected");
        }

       if(fileHdmi.exists()&&!fileSpdif.exists())
        {
         mTextViewOutputChoose.setText(getResources().getString(R.string.output_choose));
         mCheckBoxPassthrough.setText(getResources().getString(R.string.Enable_Passthrough));
        }
      else if(!fileHdmi.exists()&&fileSpdif.exists())
        {
         mCommand.setText(getResources().getString(R.string.device_choose));
         mSpdifIn.setText(getResources().getString(R.string.Enable_SPDIF_In));
        }
      else if(fileHdmi.exists()&&fileSpdif.exists())
        {
         mTextViewOutputChoose.setText(getResources().getString(R.string.output_choose));
         mCheckBoxPassthrough.setText(getResources().getString(R.string.Enable_Passthrough));
         mCommand.setText(getResources().getString(R.string.device_choose));
         mSpdifIn.setText(getResources().getString(R.string.Enable_SPDIF_In));
        }
      else
          Log.d(TAG,"no device connected");

      if(fileHdmi.exists()&&!fileSpdif.exists())
        {
        String value = SystemProperties.get("persist.audio.pass.through");
        Log.d(TAG,"property persist.audio.pass.through is " + value);
        if(value.equals("2000"))
            mCheckBoxPassthrough.setChecked(true);
        else
            mCheckBoxPassthrough.setChecked(false);

        mCheckBoxPassthrough.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.d(TAG, "CheckBox Passthrough is clicked");
                SystemProperties.set("persist.audio.pass.through",mCheckBoxPassthrough.isChecked() ? "2000" : "0" );
            }
        });
        }
      else if (!fileHdmi.exists()&&fileSpdif.exists())
         {
         if (AudioSystem.getDeviceConnectionState(AudioSystem.DEVICE_IN_AUX_DIGITAL, "") == AudioSystem.DEVICE_STATE_AVAILABLE)
            mSpdifIn.setChecked(true);
          else
            mSpdifIn.setChecked(false);

        mSpdifIn.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.d(TAG, "SPDIF IN is clicked");

                setupAudioRoute();
            }
        });
         }
      else if(fileHdmi.exists()&&fileSpdif.exists())
         {
         if (AudioSystem.getDeviceConnectionState(AudioSystem.DEVICE_IN_AUX_DIGITAL, "") == AudioSystem.DEVICE_STATE_AVAILABLE)
            mSpdifIn.setChecked(true);
          else
            mSpdifIn.setChecked(false);

        mSpdifIn.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.d(TAG, "SPDIF IN is clicked");

                setupAudioRoute();
            }
        });

        String value = SystemProperties.get("persist.audio.pass.through");
        Log.d(TAG,"property persist.audio.pass.through is " + value);
        if(value.equals("2000"))
            mCheckBoxPassthrough.setChecked(true);
        else
            mCheckBoxPassthrough.setChecked(false);

        mCheckBoxPassthrough.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.d(TAG, "CheckBox Passthrough is clicked");
                SystemProperties.set("persist.audio.pass.through",mCheckBoxPassthrough.isChecked() ? "2000" : "0" );
            }
        });
       }
      else
      Log.d(TAG, "no device connected and can't do some operation");
      
	if(fileHdmi.exists())
	{
     	mTextViewHDMIAudio.setText(getResources().getString(R.string.hdmi_audio));
    	mCheckBoxHDMIAudio.setText(getResources().getString(R.string.hdmi_audio_en));

        String value = SystemProperties.get("persist.hdmi.audio.enable");
        Log.d(TAG,"persist.hdmi.audio.enable is " + value);
        if(value.equals("2000"))
            mCheckBoxHDMIAudio.setChecked(true);
        else
            mCheckBoxHDMIAudio.setChecked(false);
    	
    	mCheckBoxHDMIAudio.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                Log.d(TAG, "CheckBox HDMI Audio is clicked, value="+mCheckBoxHDMIAudio.isChecked());
                SystemProperties.set("persist.hdmi.audio.enable",mCheckBoxHDMIAudio.isChecked() ? "2000" : "0" );
                if( mCheckBoxHDMIAudio.isChecked() )
                {
                	Log.d(TAG, "------turn on HDMI------");
                	mAudioManager.setWiredDeviceConnectionState(AudioSystem.DEVICE_OUT_HDMI, 1, "", "hdmi");
                   	//mAudioManager.setWiredDeviceConnectionState(AudioSystem.DEVICE_OUT_WIRED_HEADPHONE, 0, "", "headphone");
                   	Log.d(TAG, "Speaker is: "+mAudioManager.isSpeakerphoneOn()+" , Headset is: "+mAudioManager.isWiredHeadsetOn());
                }
                else
                {
               		Log.d(TAG, "------turn off HDMI------");
                   	mAudioManager.setWiredDeviceConnectionState(AudioSystem.DEVICE_OUT_HDMI, 0, "", "hdmi");
                   	//mAudioManager.setWiredDeviceConnectionState(AudioSystem.DEVICE_OUT_WIRED_HEADPHONE, 1, "", "headphone");
                   	Log.d(TAG, "Speaker is: "+mAudioManager.isSpeakerphoneOn()+" , Headset is: "+mAudioManager.isWiredHeadsetOn());
                }
            }
        });
	}


    }

    private void setupAudioRoute() {
        Log.d(TAG, "SPDIF IN "+ mSpdifIn.isChecked());
        mAudioManager.setWiredDeviceConnectionState(AudioSystem.DEVICE_IN_AUX_DIGITAL, mSpdifIn.isChecked() ? 1 : 0, "", "");
    }

}
