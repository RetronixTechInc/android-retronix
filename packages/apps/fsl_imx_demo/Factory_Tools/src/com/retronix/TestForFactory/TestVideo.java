package com.retronix.TestForFactory;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.InvalidParameterException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.io.FileInputStream;
import java.io.FileOutputStream;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.MediaPlayer;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.CountDownTimer;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.Display;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;
import android.view.WindowManager;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.VideoView;
import android.provider.Settings;
import android.net.SntpClient;
import android.os.Message;
import android.os.Handler;
import android.os.SystemClock;

public class TestVideo extends Activity{
	private String TAG = "TestVideo";
    private static final boolean DEBUG = false;

	private LinearLayout mLinearLayout,mExtsdLayout,mUdiskLayout,mOtgLayout,mContent;
	private PopupWindow menu;
	private int screenWidth,screenHeigh;
	private View view;	
	private ArrayList<String> videolist,usb_photolist,otg_photolist;
	private int mVideoPlayCount,mUsbPhotoPlayCount,mOtgPhotoPlayCount,mVideoFileSize,mUsbPhotoFileSize,mOtgPhotoFileSize;
	private VideoView mVideoview;
	private ImageView mUSBImageView,mOTGImageView;
	private boolean mIsVideoPlay,mIsUsbPhotoPlay,mIsOtgPhotoPlay,mIsSDCheck,mIsUSBCheck,mIsOTGCheck;
	private String mVideoFileStr, mUsbPhotoFileStr, mOtgPhotoFileStr;
	private TextView mExtsdstr,mExtsdlist,mExtsdplay,mUdiskstr,mUdisklist,mOtgstr,mOtglist,mRunTime,mFTPdownload,mComportCorrect,mComportError;
	private final String PhotoUSBPath = "/mnt/udisk"+"/RTXPHOTO/";
	private final String PhotoOTGPath = "/mnt/sdcard"+"/RTXPHOTO/";
	private final String VideoSDPath = "/mnt/extsd/RTXVIDEO/";
	private final String FTPfilePath = "/mnt/extsd/FTP.txt";
	private final int StoragecheckTime = 1000;
	private final int PhotoAnimatiobTime = 2000;
	private long mStartTime;
	private static String USERNAME,PASSWORD,HOST,REMOTE_VIDEO1,FILESIZE1,REMOTE_VIDEO2,FILESIZE2;
	private final String LOCAL_VIDEO1 = "ftpVideo1.mp4";
	private final String LOCAL_VIDEO2 = "ftpVideo2.mp4";
	private static boolean mIsLocal1, mIsLocal2, mIsDownload;
	private Process mFtp;
	private GetFTPfile mFTPthread;
	private BroadcastReceiver mFTPReceiver;
	private String mNetinfo = "" ,mNetTimeStr = "";
	
	private Button mComportButton;
	private	 static boolean isEnableComportTest = false;
	
	private SerialPort mSerialPort = null;
	protected OutputStream mOutputStream;
	private InputStream mInputStream;
	private int comport_count, comport_rev_count;
	private ReadThread mReadThread;
		
	private boolean mCheckNetTime;
	private static String ErrorLogPath = "/mnt/sdcard/MPTestLog.txt";
	
	String Comport_KEYWORD = "Comport Send Count :";
	private String comport_msg;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
		
		setContentView(R.layout.videolayout);
		
		mStartTime = System.currentTimeMillis();
		WriteErrorLog("-------------------------START TestVideo ACTIVITY-------------------------");

		findViews();
		
		initVariable();	
		
		checkStorage.post(r);
        
		registerbrocastrec();
		
	    setListeners(); 
                
		//Show some message
	    ShowMessage("ErrorLogPath : "+ ErrorLogPath);
	}

	private void findViews(){
		Display display = this.getWindowManager().getDefaultDisplay();
		screenWidth = display.getWidth();
		screenHeigh = display.getHeight();	
		mLinearLayout = (LinearLayout)findViewById(R.id.layout);
		mExtsdLayout = (LinearLayout)findViewById(R.id.extsdlayout);
		mExtsdLayout.setLayoutParams(new LinearLayout.LayoutParams(screenWidth/3, LinearLayout.LayoutParams.FILL_PARENT));
		mUdiskLayout = (LinearLayout)findViewById(R.id.udisklayout);
		mUdiskLayout.setLayoutParams(new LinearLayout.LayoutParams(screenWidth/3, LinearLayout.LayoutParams.FILL_PARENT));
		mOtgLayout = (LinearLayout)findViewById(R.id.otglayout);
		mOtgLayout.setLayoutParams(new LinearLayout.LayoutParams(screenWidth/3, LinearLayout.LayoutParams.FILL_PARENT));
		mContent = (LinearLayout)findViewById(R.id.content);
		mContent.setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, screenHeigh*2/3));
		view = this.getLayoutInflater().inflate(R.layout.video, null);
		menu = new PopupWindow(view);
		menu.setWidth(LayoutParams.WRAP_CONTENT);
		menu.setHeight(LayoutParams.WRAP_CONTENT);
		menu.setFocusable(true);

		mVideoview = (VideoView)findViewById(R.id.videoview);
		mVideoview.setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, screenHeigh/3));
		mUSBImageView = (ImageView)findViewById(R.id.udiskimageview);
		mOTGImageView = (ImageView)findViewById(R.id.otgimageview);
		mExtsdstr = (TextView)findViewById(R.id.extsdstr);
		mExtsdlist = (TextView)findViewById(R.id.extsdlist);
		mExtsdplay = (TextView)findViewById(R.id.playing_video);
		mUdiskstr = (TextView)findViewById(R.id.udiskstr);
		mUdisklist = (TextView)findViewById(R.id.udisklist);
		mOtgstr = (TextView)findViewById(R.id.otgstr);
		mOtglist = (TextView)findViewById(R.id.otglist);
		mRunTime = (TextView)findViewById(R.id.time);
		mFTPdownload = (TextView)findViewById(R.id.ftpdownload);
		mComportCorrect = (TextView)findViewById(R.id.ComportCorrect);
		mComportError = (TextView)findViewById(R.id.ComportError);
		mComportButton = (Button)findViewById(R.id.Button_Comport);
		
	}
	
	private void initVariable(){		
		videolist = new ArrayList<String>();
		usb_photolist = new ArrayList<String>();
		otg_photolist = new ArrayList<String>();
		mVideoFileSize = 0;
		mUsbPhotoFileSize = 0;
		mOtgPhotoFileSize = 0;
		mVideoPlayCount = 0;
		mUsbPhotoPlayCount = 0;
		mOtgPhotoPlayCount = 0;
		mIsVideoPlay = false;
		mIsUsbPhotoPlay = false;
		mIsOtgPhotoPlay = false;
		mIsSDCheck = false;
		mIsUSBCheck = false;
		mIsOTGCheck = false;
		mVideoFileStr = "EXTSD card video List : \n";
		mUsbPhotoFileStr = "USB photo List : \n";
		mOtgPhotoFileStr = "SD photo List : \n";
		mExtsdstr.setText("");
		mExtsdlist.setText(mVideoFileStr);
		mExtsdplay.setText("");
		mUdiskstr.setText("");
		mUdisklist.setText(mUsbPhotoFileStr);
		mOtgstr.setText("");
		mOtglist.setText(mOtgPhotoFileStr);
		mIsLocal1 = false;
		mIsLocal2 = false;
		mIsDownload = false;
		mFTPdownload.setText("");
		mCheckNetTime = false;
		mComportCorrect.setVisibility(4);
		mComportError.setVisibility(4);
        mComportButton.setText("Open Comport Test");
	}
	
	private void registerbrocastrec(){
		mFTPReceiver = new BroadcastReceiver() 
	    {
			@Override
			public void onReceive(Context context, Intent intent) 
			{				 			 
				String FTPCommand = intent.getAction();

				if(FTPCommand.equals("ftp_download_success"))
				{
					//WriteErrorLog("BroadcastReceiver->FTP file download success!!");
					String localVideo = intent.getStringExtra("localVideo");
					if(mVideoview.isPlaying()) mVideoview.stopPlayback();
					mVideoview.setVideoPath(localVideo);
					mVideoview.requestFocus();
					mVideoview.start();
					mExtsdplay.setText("Playing:"+localVideo);
					mFTPthread = new GetFTPfile();
					if(!mIsLocal1){
						mFTPthread.setDownloadinfo( VideoSDPath+LOCAL_VIDEO1, REMOTE_VIDEO1,FILESIZE1);
						mFTPthread.start();
						mIsLocal1 = true;
						mIsLocal2 = false;
						if (DEBUG) Log.e(TAG, "BroadcastReceiver->download->LOCAL_VIDEO1");
						WriteErrorLog("BroadcastReceiver->download->LOCAL_VIDEO1");
					}
					else if(!mIsLocal2){
						mFTPthread.setDownloadinfo( VideoSDPath+LOCAL_VIDEO2, REMOTE_VIDEO2,FILESIZE2);
						mFTPthread.start();
						mIsLocal1 = false;
						mIsLocal2 = true;
						if (DEBUG) Log.e(TAG,"BroadcastReceiver->download->LOCAL_VIDEO2");
						WriteErrorLog("BroadcastReceiver->download->LOCAL_VIDEO2");
					}
				}
				else if(FTPCommand.equals("ftp_download_error")){
					WriteErrorLog("BroadcastReceiver->FTP file download error!!");
					if (DEBUG) Log.e(TAG, "FTP download error:Process->mFtp.destory");
					ShowMessage("FTP file download error!!");
					if(mFtp != null) mFtp.destroy();
					if(mFTPthread != null) mFTPthread.interrupt();
				}
			}
	    }; 
	    IntentFilter filter = new IntentFilter();
		filter = new IntentFilter();  
        filter.addAction("ftp_download_success");
        filter.addAction("ftp_download_error");
        TestVideo.this.registerReceiver(mFTPReceiver, filter); 
	}
	
    //Listen for clicks
    private void setListeners() {
        if (DEBUG) Log.d(TAG, "setListeners");        
        mLinearLayout.setOnClickListener(finishclick);
        mComportButton.setOnClickListener(uarttestclick);
    }
    
    //Finish Button setting
    private Button.OnClickListener finishclick = new Button.OnClickListener()
    {
		@Override
		public void onClick(View arg0) {
			menu.setAnimationStyle(R.style.pop_bootpm_anim_style);
			menu.setBackgroundDrawable(TestVideo.this.getResources().getDrawable(R.drawable.videoback));
			menu.showAtLocation(mLinearLayout, Gravity.CENTER,screenWidth,screenHeigh);
			
			Button videofinish = (Button)view.findViewById(R.id.videofinish);
			videofinish.setOnClickListener(new Button.OnClickListener() {
				@Override
				public void onClick(View v) {
					stopVideo();
					menu.dismiss();
					checkStorage.removeCallbacks(r);
					Intent _intent = new Intent(TestVideo.this,Writetommc.class);
			        startActivity(_intent);
					TestVideo.this.finish();
				}
			});
		}
	};


    //uarttest Button setting
    private Button.OnClickListener uarttestclick = new Button.OnClickListener()
    {  
        public void onClick(View v) {
        	
        	if(isEnableComportTest == true)
        	{
        		mComportButton.setText("Open Comport Test");
        		mComportCorrect.setVisibility(0);
        		mComportError.setVisibility(0);
        		StopComportTest();  
        	}
        	else
        	{
        		WriteErrorLog("Start Comport Test");
        		mComportButton.setText("Stop Comport Test");
        		comport_count = 0;
        		comport_rev_count = 0;
        		try {
        			mSerialPort = StartComportTest();
        			mOutputStream = mSerialPort.getOutputStream();
        			mInputStream = mSerialPort.getInputStream();

        			/* Create a receiving thread */
                	_Handler.post(SendThread);
                	
        			mReadThread = new ReadThread();
        			mReadThread.start();

        		} catch (SecurityException e) {
        			mComportError.setText("error_security");
        		} catch (IOException e) {
        			mComportError.setText("error_unknown");
        		} catch (InvalidParameterException e) {
        			mComportError.setText("error_configuration");
        		}
        		mComportCorrect.setVisibility(0);
        		mComportError.setVisibility(0);
        	}           	
        }
      };

	Handler _Handler = new Handler();
    Runnable SendThread = new Runnable() {
		@Override
		public void run() {
			// TODO Auto-generated method stub
			_Handler.postDelayed(SendThread, 1000);
			comport_count++;
			mComportCorrect.setText(Comport_KEYWORD + String.valueOf(comport_count));
			CharSequence t = mComportCorrect.getText();
			char[] text = new char[t.length()];
			for (int i=0; i<t.length(); i++) {
				text[i] = t.charAt(i);
			}
			try {
				mOutputStream.write(new String(text).getBytes());
				mOutputStream.write('\n');
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	};
	
	private void onDataReceived(final byte[] buffer, final int size) {
		runOnUiThread(new Runnable() {
			public void run() {
				comport_msg += new String(buffer, 0, size);
				if(DEBUG) Log.w(TAG, "Tom----onDataReceived size:" + size + ";" + comport_msg);
				int index_start = comport_msg.indexOf(Comport_KEYWORD);
				if( index_start < 0)return;
				comport_msg = "";
				comport_rev_count++;
				mComportError.setText("Comport Receive Count :" + String.valueOf(comport_rev_count));;
			}
		});
	}
	
	private class ReadThread extends Thread {
		@Override
		public void run() {
			super.run();
			while(!isInterrupted()) {
				int size;
				try {
					byte[] buffer = new byte[64];
					if (mInputStream == null) return;
					size = mInputStream.read(buffer);
					if (size > 0) {
						onDataReceived(buffer, size);
					}
				} catch (IOException e) {
					e.printStackTrace();
					return;
				}
			}
		}
	}
	
	private boolean ReadFTPfile(){
		File file = new File(FTPfilePath);
		if(!file.exists()){
			ShowMessage("No file! Please put file FTP.txt in SD card.");
			return false;
		}
        String contents = null;
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new FileReader(file));
            String text = null;
            while ((text = reader.readLine()) != null) {
                contents = text;
            }
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                if (reader != null) {
                    reader.close();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        String[] ftpdata = contents.split(",");
        if(ftpdata.length!=7){
        	ShowMessage("File error! Please check file FTP.txt");
        	return false;
        }
        USERNAME = ftpdata[0];
        PASSWORD = ftpdata[1];
        HOST = ftpdata[2];
        REMOTE_VIDEO1 = ftpdata[3];      
        FILESIZE1 = ftpdata[4];
        REMOTE_VIDEO2 = ftpdata[5];
        FILESIZE2 = ftpdata[6];
        if (DEBUG) Log.e(TAG, "USERNAME:"+USERNAME);
        if (DEBUG) Log.e(TAG, "PASSWORD:"+PASSWORD);
        if (DEBUG) Log.e(TAG, "HOST:"+HOST);
        if (DEBUG) Log.e(TAG, "REMOTE_VIDEO1:"+REMOTE_VIDEO1);
        if (DEBUG) Log.e(TAG, "REMOTE_VIDEO2:"+REMOTE_VIDEO2);
        if (DEBUG) Log.e(TAG, "FILESIZE1:"+FILESIZE1);
        if (DEBUG) Log.e(TAG, "FILESIZE2:"+FILESIZE2);
       
        WriteErrorLog("USERNAME:"+USERNAME);
        WriteErrorLog("PASSWORD:"+PASSWORD);
        WriteErrorLog("HOST:"+HOST);
        WriteErrorLog("REMOTE_VIDEO1:"+REMOTE_VIDEO1);
        WriteErrorLog("REMOTE_VIDEO2:"+REMOTE_VIDEO2);
        WriteErrorLog("FILESIZE1:"+FILESIZE1);
        WriteErrorLog("FILESIZE2:"+FILESIZE2);
        
        return true;
	 }
	
	class GetFTPfile extends Thread{
		private String localVideo = null;
		private String remoteVideo = null;
		private long mCurrentSize = -1;

		public void setDownloadinfo(String local,String remote,String size){
			localVideo = local;
			remoteVideo = remote;
			mCurrentSize = Integer.valueOf(size).longValue();
		}
		
		public void run() {
			if (DEBUG) Log.e(TAG, "Download->start");
			WriteErrorLog("GetFTPfile Thread START---");
			try {
				String[] mFTPdata = new String[9];
				mFTPdata[0] = "busybox";
				mFTPdata[1] = "ftpget";
				mFTPdata[2] = "-u";
				mFTPdata[3] = USERNAME;
				mFTPdata[4] = "-p";
				mFTPdata[5] = PASSWORD;
				mFTPdata[6] = HOST;
				mFTPdata[7] = localVideo;
				mFTPdata[8] = remoteVideo;
				
				ProcessBuilder builder = new ProcessBuilder(mFTPdata);
				mFtp = builder.start();
				InputStream is = mFtp.getInputStream();
				BufferedInputStream in = new BufferedInputStream(is);

				while(in.read() != -1 ){
				}
				mFtp.destroy();
				if (DEBUG) Log.e(TAG, "Thread finish:Process->mFtp.destory");
				mFtp = null;} 			
			catch (IOException e) {  
				if (DEBUG) Log.e(TAG, "IOException->file error");
	        	WriteErrorLog("GetFTPfile Thread ->File IOException error");
			}
			catch (NullPointerException e) { 
				if (DEBUG) Log.e(TAG, "NullPointerException->file error");
	        	WriteErrorLog("GetFTPfile Thread->File NullPointerException error");
			}	

			if(new File(localVideo).length() == mCurrentSize){
				mIsDownload = true;
				Intent ftpInfoS = new Intent("ftp_download_success");
				ftpInfoS.putExtra("localVideo", localVideo);
				TestVideo.this.sendBroadcast(ftpInfoS);
				if (DEBUG) Log.e(TAG, "handleMessage->SUCCESS");
		        WriteErrorLog("GetFTPfile Thread file check->SUCCESS");
			}else{
				Intent ftpInfoE = new Intent("ftp_download_error");
				TestVideo.this.sendBroadcast(ftpInfoE);
				if (DEBUG) Log.e(TAG, "handleMessage->FAILURE");
	        	WriteErrorLog("GetFTPfile Thread file check->FAILURE");
			}
		}
	}
    
    private void ShowMessage(String str){
    	Toast.makeText(TestVideo.this, str, Toast.LENGTH_LONG).show();
    }
	
	private void ClearVideo(){
		videolist = new ArrayList<String>();
		mVideoFileSize = 0;
		mVideoPlayCount = 0;
		mIsVideoPlay = false;
		mIsSDCheck = false;
		mVideoFileStr = "EXTSD card video List : \n";
		mExtsdstr.setText("");
		mExtsdlist.setText(mVideoFileStr);
	}
	
	private void ClearUsbPhoto(){
		usb_photolist = new ArrayList<String>();
		mUsbPhotoFileSize = 0;
		mUsbPhotoPlayCount = 0;
		mIsUsbPhotoPlay = false;
		mIsUSBCheck = false;
		mUsbPhotoFileStr = "USB photo List : \n";
		mUdiskstr.setText("");
		mUdisklist.setText(mUsbPhotoFileStr);
		mUSBImageView.clearFocus();
	}
	
	private void ClearOtgPhoto(){
		otg_photolist = new ArrayList<String>();
		mOtgPhotoFileSize = 0;
		mOtgPhotoPlayCount = 0;
		mIsOtgPhotoPlay = false;
		mIsOTGCheck = false;
		mOtgPhotoFileStr = "SD photo List : \n";
		mOtgstr.setText("");
		mOtglist.setText(mOtgPhotoFileStr);
		mOTGImageView.clearFocus();
	}
	
	Handler checkStorage = new Handler();
    Runnable r = new Runnable()
    {
    	public void run()
    	{
    		//show ftp file download info
    		if(checkNet()){
	    		if(mIsLocal1 && !mIsLocal2){
	    			mFTPdownload.setText(mNetinfo+" Download "+REMOTE_VIDEO1+" ,file size : "+new File(VideoSDPath+LOCAL_VIDEO1).length()+" bytes");
	    		} else if(mIsLocal2 && !mIsLocal1){
	    			mFTPdownload.setText(mNetinfo+" Download "+REMOTE_VIDEO2+" ,file size : "+new File(VideoSDPath+LOCAL_VIDEO2).length()+" bytes");
	    		}
    		}else {
    			mNetinfo = "Net error,System time:"+mNetTimeStr+"!!";
    			if(mFtp != null) {
    				mFtp.destroy();
    				if (DEBUG) Log.e(TAG, "Check net error:Process->mFtp.destory");
    			}
    			if(mFTPthread != null) mFTPthread.interrupt();
    			mFTPdownload.setText("No network!!!"+",System time:"+mNetTimeStr);
    			WriteErrorLog("No network!!!");
    		}
    		
    		if(checkUSBstate()){
    			if(mUsbPhotoFileSize > 0){
    				if(!mIsUsbPhotoPlay){
    					mIsUsbPhotoPlay = true;
    					playUsbPhoto();
    					playUsbphoto.postDelayed(usbphoto, PhotoAnimatiobTime);
    				}
    			}
    			else{
    				playUsbphoto.removeCallbacks(usbphoto);
    			}
    		}
    		else{
				playUsbphoto.removeCallbacks(usbphoto);
			}
    		
    		if(checkOTGstate()){
    			if(mOtgPhotoFileSize > 0){
    				if(!mIsOtgPhotoPlay){
    					mIsOtgPhotoPlay = true;
    					playOtgPhoto();
    					playOtgphoto.postDelayed(otgphoto, PhotoAnimatiobTime);
    				}
    			}
    			else{
    				playOtgphoto.removeCallbacks(otgphoto);
    			}
    		}
    		else{
				playOtgphoto.removeCallbacks(otgphoto);
			}
    		
    		if(checkSDstate()){
    			if(mVideoFileSize > 0) {
    				if(!mIsVideoPlay){
    					mIsVideoPlay = true;
    					playVideo();
    					firstFTPdownload();
    				}    			
    			}
        		else{
        			stopVideo();
        			if(mFtp != null) {
        				mFtp.destroy();
        				if (DEBUG) Log.e(TAG, "Check sd:Process->mFtp.destory");
        			}
        			if(mFTPthread != null) mFTPthread.interrupt();
        			mExtsdstr.setText("Please check SD card video file.");
        		}
    		}
    		
    		checkStorage.postDelayed(r, StoragecheckTime);
    		showruntime();
    	}
    };
	
    private void showruntime(){
		//show run time
		long interval = (System.currentTimeMillis() - mStartTime)/1000;
		long hour = interval/3600;
		long minute = (interval%3600)/60;
		long second = (interval%3600)%60;
		mRunTime.setText("Run time : "+hour+" : "+minute+" : "+second);
    }
    
    private boolean checkNet(){
    	ConnectivityManager conManager = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);
    	NetworkInfo networInfo = conManager.getActiveNetworkInfo();       
    	if (networInfo == null || !networInfo.isAvailable()){
    		if(!mCheckNetTime){
    			SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss");
    	    	mNetTimeStr = format.format(System.currentTimeMillis());
    			mCheckNetTime = true;
    		}
			return false;
    	}else{
    		return true;
    	} 
    }
    
    private boolean checkUSBstate(){
    	if(!new File(PhotoUSBPath).canWrite()){
    		ClearUsbPhoto();
    		mUdiskstr.setText("Please check USB.");
    		return false;
    	}
    	else if(new File(PhotoUSBPath).canWrite()){
    		if(!mIsUSBCheck){
    			mIsUSBCheck = true;
    			getUsbPhotoFilelist(PhotoUSBPath);
	    		mUdisklist.setText(mUsbPhotoFileStr);
	    		mUdiskstr.setText("");
    		}
    		return true;
    	}
		return false;
    }
    
    private boolean checkOTGstate(){
    	if(!new File(PhotoOTGPath).canWrite()){
    		ClearOtgPhoto();
    		mOtgstr.setText("Please check OTG device.");
    		return false;
    	}
    	else if(new File(PhotoOTGPath).canRead()){
    		if(!mIsOTGCheck){
    			mIsOTGCheck = true;
    			getOtgPhotoFilelist(PhotoOTGPath);
	    		mOtglist.setText(mOtgPhotoFileStr);
	    		mOtgstr.setText("");
    		}
    		return true;
    	}
		return false;
    }
    
    private boolean checkSDstate(){
    	if(!new File(VideoSDPath).canWrite()){
    		ClearVideo();
    		mExtsdstr.setText("Please check EXTSD.");
    		return false;
    	}
    	else if(new File(VideoSDPath).canWrite()){
    		if(!mIsSDCheck){
    			mIsSDCheck = true;
    			new File(VideoSDPath+LOCAL_VIDEO1).delete();
				new File(VideoSDPath+LOCAL_VIDEO2).delete();
    			getVideoFilelist(VideoSDPath);
	    		mExtsdlist.setText(mVideoFileStr);
	    		mExtsdstr.setText("");
    		}
    		return true;
    	}
    	return false;
    }
    
	private void playVideo(){
		if(mVideoFileSize <= 0) return;
		if(mVideoPlayCount >= mVideoFileSize) mVideoPlayCount = 0;
		
		mVideoview.setVideoPath(videolist.get(mVideoPlayCount));
		mVideoview.requestFocus();
		mVideoview.start();
		mExtsdplay.setText("Playing:"+videolist.get(mVideoPlayCount));
		
		mVideoview.setOnCompletionListener(new MediaPlayer.OnCompletionListener(){
			public void onCompletion(MediaPlayer arg0)
			{
				if(mIsDownload){
					if(mIsLocal2){
						mVideoview.setVideoPath(VideoSDPath+LOCAL_VIDEO1);
						mVideoview.start();
					}
					else if(mIsLocal1){
						mVideoview.setVideoPath(VideoSDPath+LOCAL_VIDEO2);
						mVideoview.start();
					}
				}
				else {
					mVideoPlayCount++;
					if(mVideoPlayCount >= mVideoFileSize) mVideoPlayCount = 0;
					mVideoview.setVideoPath(videolist.get(mVideoPlayCount));
					mVideoview.start();
					mExtsdplay.setText("Playing:"+videolist.get(mVideoPlayCount));
				}
			}
		});
			
		mVideoview.setOnErrorListener(new MediaPlayer.OnErrorListener() {	
			@Override
			public boolean onError(MediaPlayer arg0, int arg1, int arg2) {
				//stopVideo();
				//initVariable();
				//mExtsdstr.setText("Video playing error!!!");
				if (DEBUG) Log.e(TAG,"Video playing error!!!");
				playVideo();
				//WriteErrorLog("Video playing error!!!");
				return true;
			}
		});
	}
	
	private void firstFTPdownload(){
		if(ReadFTPfile()){
			if(checkNet()){
				//Sync clock from HOST
				if(!"".equals(HOST))
					SyncTime();
				mFTPthread = new GetFTPfile();
				if(!mIsLocal1){
					mFTPthread.setDownloadinfo( VideoSDPath+LOCAL_VIDEO1, REMOTE_VIDEO1,FILESIZE1);
					mFTPthread.start();
					mIsLocal1 = true;
					mIsLocal2 = false;
					if (DEBUG) Log.e(TAG, "playVideo->first download->LOCAL_VIDEO1");
					//WriteErrorLog("playVideo->first download->LOCAL_VIDEO1");
				}
				else if(!mIsLocal2){
					mFTPthread.setDownloadinfo( VideoSDPath+LOCAL_VIDEO2, REMOTE_VIDEO2,FILESIZE2);
					mFTPthread.start();
					mIsLocal1 = false;
					mIsLocal2 = true;
					if (DEBUG) Log.e(TAG, "playVideo->first download->LOCAL_VIDEO2");
					//WriteErrorLog("playVideo->first download->LOCAL_VIDEO2");
				}
			} else{
				if (DEBUG) Log.e(TAG, "playVideo->check no NET!!");
			}
		}
	}
	
	private void playUsbPhoto(){
		if(mUsbPhotoFileSize <= 0) return;
		if(mUsbPhotoPlayCount >= mUsbPhotoFileSize) mUsbPhotoPlayCount = 0;
		mUSBImageView.setImageURI(Uri.parse((usb_photolist.get(mUsbPhotoPlayCount))));
		mUsbPhotoPlayCount++;
	}
	
	Handler playUsbphoto = new Handler();
	Runnable usbphoto = new Runnable(){
		public void run()
		{			
			playUsbphoto.postDelayed(usbphoto, PhotoAnimatiobTime);
			playUsbPhoto();
		}
	};
	
	private void playOtgPhoto(){
		if(mOtgPhotoFileSize <= 0) return;
		if(mOtgPhotoPlayCount >= mOtgPhotoFileSize) mOtgPhotoPlayCount = 0;
		mOTGImageView.setImageURI(Uri.parse((otg_photolist.get(mOtgPhotoPlayCount))));
		mOtgPhotoPlayCount++;
	}
	
	Handler playOtgphoto = new Handler();
	Runnable otgphoto = new Runnable(){
		public void run()
		{			
			playOtgphoto.postDelayed(otgphoto, PhotoAnimatiobTime);
			playOtgPhoto();
		}
	};
	
	private void stopVideo(){
		if(mIsVideoPlay){
			mIsVideoPlay = false;
			mVideoview.stopPlayback();
			mLinearLayout.clearChildFocus(mVideoview);
		}
	}
	
	private void getVideoFilelist(String path){
		if(!new File(path).canWrite()) return;
		File[] sdfiles = new File(path).listFiles(new getVideoFileType());
		for(File sdf : sdfiles){
			videolist.add(sdf.getAbsolutePath());
			mVideoFileStr += sdf.getName()+"\n";
		}
		mVideoFileSize = videolist.size();
	}
	
	private void getUsbPhotoFilelist(String path){
		if(!new File(path).canWrite()) return;
		File[] sdfiles = new File(path).listFiles(new getPhotoFileType());
		for(File sdf : sdfiles){
			usb_photolist.add(sdf.getAbsolutePath());
			mUsbPhotoFileStr += sdf.getName()+"\n";
		}
		mUsbPhotoFileSize = usb_photolist.size();
	}
	
	private void getOtgPhotoFilelist(String path){
		if(!new File(path).canWrite()) return;
		File[] sdfiles = new File(path).listFiles(new getPhotoFileType());
		for(File sdf : sdfiles){
			otg_photolist.add(sdf.getAbsolutePath());
			mOtgPhotoFileStr += sdf.getName()+"\n";
		}
		mOtgPhotoFileSize = otg_photolist.size();
	}

	public class getVideoFileType implements FilenameFilter {
		public boolean accept(File dir, String name)  
	    { 
	        return (name.endsWith(".mpg") || name.endsWith(".mpeg")|| name.endsWith(".3gp") || name.endsWith(".mp4") || name.endsWith(".avi")
	        		|| name.endsWith(".MPG") || name.endsWith(".MPEG")|| name.endsWith(".3GP") || name.endsWith(".MP4") || name.endsWith(".AVI"));
	    }
	}
	
	public class getPhotoFileType implements FilenameFilter {
		public boolean accept(File dir, String name)  
	    { 
	        return (name.endsWith(".png") || name.endsWith(".jpg")|| name.endsWith(".jpeg")
	        		|| name.endsWith(".PNG") || name.endsWith(".JPG")|| name.endsWith(".JPEG"));
	    }
	}

    @Override
    public void onStop()
    {
        super.onStop();
		if(isEnableComportTest)
			StopComportTest();
		checkStorage.removeCallbacks(r);
		stopVideo();
		playUsbphoto.removeCallbacks(usbphoto);
		playOtgphoto.removeCallbacks(otgphoto);
		TestVideo.this.finish();
    }
	
	@Override
	protected void onDestroy() {			
		if(mFtp != null) {
			mFtp.destroy();
			if (DEBUG) Log.e(TAG, "Activity.onDestory:Process->mFtp.destory");
		}
		unregisterReceiver(mFTPReceiver);
		if(mFTPthread != null) mFTPthread.interrupt();
		//WriteErrorLog("onDestroy!!!");
		super.onDestroy();
	}
	    
    private SerialPort StartComportTest() throws SecurityException, IOException
    {
    	SerialPort Port = new SerialPort(new File("/dev/ttymxc0"), 115200);
    	isEnableComportTest = true;
        return Port;
    }
	
    private void StopComportTest()
    {
    	_Handler.removeCallbacks(SendThread);
		if (mReadThread != null)
			mReadThread.interrupt();
		if(mSerialPort != null)
			mSerialPort.close();
		mSerialPort = null;
		isEnableComportTest = false;   	
    }
    
    static void WriteErrorLog(String logStr){
		File mErrorLogfile = new File(ErrorLogPath);
		SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss ");
    	String mLogTime = format.format(System.currentTimeMillis());
    	try {
			FileWriter logfile = new FileWriter(mErrorLogfile,true);
			logfile.write(mLogTime+logStr+"\n");
			logfile.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}
    
    static void saveWriteErrorLog1(String destLocation){
    	boolean saveero;
       	
    	saveero = copyFile(ErrorLogPath, destLocation);
	}

    private static boolean copyFile(String sourceLocation, String destLocation) {
        try {
            File sd = new File("/mnt/extsd");
            if(sd.canWrite()){
                File source=new File(sourceLocation);
                File dest=new File(destLocation);
                if(!dest.exists()){
                    dest.createNewFile();
                }
                if(source.exists()){
                    InputStream  src=new FileInputStream(source);
                    OutputStream dst=new FileOutputStream(dest);
                     // Copy the bits from instream to outstream
                    byte[] buf = new byte[1024];
                    int len;
                    while ((len = src.read(buf)) > 0) {
                        dst.write(buf, 0, len);
                    }
                    src.close();
                    dst.close();
                }
            }
            return true;
        } catch (Exception ex) {
            ex.printStackTrace();
            return false;
        }
    }
    
    private Handler handeler = new Handler();
    public void SyncTime(){
    	Runnable syncTimeRunner = new Runnable() { 
    	@Override 
    	   public void run() { 
    	         SyncTimeLock(); 
    	   } 
    	}; 
    	new Thread(syncTimeRunner).start(); 
    } 

    protected void SyncTimeLock(){
    	SntpClient client = new SntpClient();
    	boolean flag = client.requestTime(HOST, 10000);
    	if (DEBUG) Log.i(TAG, flag + " = client.requestTime(server:" + HOST + ", timeout:"+"10000)");
    	WriteErrorLog("SyncTimeLock flag : " + flag);
    	if(flag){
    		long now = client.getNtpTime() +SystemClock.elapsedRealtime()-client.getNtpTimeReference();
    		SystemClock.setCurrentTimeMillis(now);
    		Message msg = new Message();
    		msg.what = 1;
    		handeler.sendMessage(msg);
    	}else{
   			Message msg = new Message();
   			msg.what = 0;
   			handeler.sendMessage(msg);  
    	}
    }

    
}
