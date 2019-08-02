package com.retronix.TestForFactory;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.RunningServiceInfo;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.MediaPlayer;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.CountDownTimer;
import android.os.Handler;
import android.view.Display;
import android.view.Gravity;
import android.view.View;
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

public class TestUsbSd extends Activity{
	
	private LinearLayout mLinearLayout,mExtsdLayout,mUdiskLayout,mContent;
	private PopupWindow menu;
	private int screenWidth,screenHeigh;
	private View view;	
	private ArrayList<String> videolist,usb_photolist;
	private int mVideoPlayCount,mUsbPhotoPlayCount,mVideoFileSize,mUsbPhotoFileSize;
	private VideoView mVideoview;
	private ImageView mUSBImageView;
	private boolean mIsVideoPlay,mIsUsbPhotoPlay,mIsSDCheck,mIsUSBCheck;
	private String mVideoFileStr, mUsbPhotoFileStr;
	private TextView mExtsdstr,mExtsdlist,mExtsdplay,mUdiskstr,mUdisklist,mRunTime,mFTPdownload;
	private final String PhotoUSBPath = System.getenv("EXTERNAL_STORAGE_UDISK")+"/RTXPHOTO/";
	private final String VideoSDPath = System.getenv("EXTERNAL_STORAGE_EXTSD")+"/RTXVIDEO/";
	private final String FTPfilePath = System.getenv("EXTERNAL_STORAGE_EXTSD")+"/FTP.txt";
	private final int StoragecheckTime = 1000;
	private final int PhotoAnimatiobTime = 2000;
	private boolean mSendMessage,mGetEXTSDstorage,mGetUDISKstorage;
	private static final char TRUE = (char)0x11;
	private static final char FALSE = (char)0x10;
	private long mStartTime;
	private static String USERNAME,PASSWORD,HOST,REMOTE_VIDEO1,REMOTE_VIDEO2,FILESIZE1,FILESIZE2;
	private final String LOCAL_VIDEO1 = "ftpVideo1.mp4";
	private final String LOCAL_VIDEO2 = "ftpVideo2.mp4";
	private static boolean mIsLocal1, mIsLocal2, mIsDownload;
	private Process mFtp;
	private GetFTPfile mFTPthread;
	private BroadcastReceiver mFTPReceiver;
	private String mNetinfo = "";
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
		
		setContentView(R.layout.usbsd);

		ReadFTPfile();
		initLayout();
		initVariable();	
		mStartTime = System.currentTimeMillis();
		checkStorage.post(r);
        
		mFTPReceiver = new BroadcastReceiver() 
	    {
			@Override
			public void onReceive(Context context, Intent intent) 
			{				 			 
				String FTPCommand = intent.getAction();

				if(FTPCommand.equals("ftp_download_success"))
				{
					String localVideo = intent.getStringExtra("localVideo");
					if(mVideoview.isPlaying()){
						mVideoview.pause();
						mVideoview.stopPlayback();
					}
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
						System.out.println("handleMessage->download->LOCAL_VIDEO1");
					}
					else if(!mIsLocal2){
						mFTPthread.setDownloadinfo( VideoSDPath+LOCAL_VIDEO2, REMOTE_VIDEO2,FILESIZE2);
						mFTPthread.start();
						mIsLocal1 = false;
						mIsLocal2 = true;
						System.out.println("handleMessage->download->LOCAL_VIDEO2");
					}
				}
				else if(FTPCommand.equals("ftp_download_error")){
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
        TestUsbSd.this.registerReceiver(mFTPReceiver, filter); 
		
		mLinearLayout.setOnClickListener(new RelativeLayout.OnClickListener(){
			@Override
			public void onClick(View arg0) {
				menu.setAnimationStyle(R.style.pop_bootpm_anim_style);
				menu.setBackgroundDrawable(TestUsbSd.this.getResources().getDrawable(R.drawable.videoback));
				menu.showAtLocation(mLinearLayout, Gravity.CENTER,screenWidth,screenHeigh);
				
				Button videofinish = (Button)view.findViewById(R.id.videofinish);
				videofinish.setOnClickListener(new Button.OnClickListener() {
					@Override
					public void onClick(View v) {
						stopVideo();
						menu.dismiss();
						checkStorage.removeCallbacks(r);
						TestUsbSd.this.finish();
					}
				});
			}
		});
		
		new CountDownTimer(60000,1000){
            @Override
            public void onFinish() {
            	if(!mSendMessage){
            		Intent WaringIntent = new Intent("send_storage_message");
            		char usb,sd,otg;
            		if(mGetUDISKstorage) usb = TRUE;
            		else usb = FALSE;
            		if(mGetEXTSDstorage) sd = TRUE;
            		else sd = FALSE;
            		otg = FALSE;
            		WaringIntent.putExtra("usb", usb);
            		WaringIntent.putExtra("sd", sd);
            		WaringIntent.putExtra("otg", otg);
            		TestUsbSd.this.sendBroadcast(WaringIntent);
            	}
            }
            @Override
            public void onTick(long millisUntilFinished) {
            	if(!mSendMessage){
	            	if(mGetEXTSDstorage && mGetUDISKstorage){
	            		Intent WaringIntent = new Intent("send_storage_message");
	            		WaringIntent.putExtra("usb", TRUE);
	            		WaringIntent.putExtra("sd", TRUE);
	            		WaringIntent.putExtra("otg", FALSE);
	            		TestUsbSd.this.sendBroadcast(WaringIntent);
	            		mSendMessage = true;
	            	}
            	}
            }
        }.start();
	}
	
	private void initLayout(){
		Display display = this.getWindowManager().getDefaultDisplay();
		screenWidth = display.getWidth();
		screenHeigh = display.getHeight();	
		mLinearLayout = (LinearLayout)findViewById(R.id.layout);
		mExtsdLayout = (LinearLayout)findViewById(R.id.extsdlayout);
		mExtsdLayout.setLayoutParams(new LinearLayout.LayoutParams(screenWidth/2, LinearLayout.LayoutParams.FILL_PARENT));
		mUdiskLayout = (LinearLayout)findViewById(R.id.udisklayout);
		mUdiskLayout.setLayoutParams(new LinearLayout.LayoutParams(screenWidth/2, LinearLayout.LayoutParams.FILL_PARENT));
		mContent = (LinearLayout)findViewById(R.id.content);
		mContent.setLayoutParams(new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, screenHeigh-70));
		view = this.getLayoutInflater().inflate(R.layout.video, null);
		menu = new PopupWindow(view);
		menu.setWidth(LayoutParams.WRAP_CONTENT);
		menu.setHeight(LayoutParams.WRAP_CONTENT);
		menu.setFocusable(true);
		mVideoview = (VideoView)findViewById(R.id.videoview);
		mUSBImageView = (ImageView)findViewById(R.id.udiskimageview);
		mExtsdstr = (TextView)findViewById(R.id.extsdstr);
		mExtsdlist = (TextView)findViewById(R.id.extsdlist);
		mExtsdplay = (TextView)findViewById(R.id.playing_video);
		mUdiskstr = (TextView)findViewById(R.id.udiskstr);
		mUdisklist = (TextView)findViewById(R.id.udisklist);
		mRunTime = (TextView)findViewById(R.id.time);
		mFTPdownload = (TextView)findViewById(R.id.ftpdownload);
	}
	
	private void initVariable(){		
		videolist = new ArrayList<String>();
		usb_photolist = new ArrayList<String>();
		mVideoFileSize = 0;
		mUsbPhotoFileSize = 0;
		mVideoPlayCount = 0;
		mUsbPhotoPlayCount = 0;
		mIsVideoPlay = false;
		mIsUsbPhotoPlay = false;
		mIsSDCheck = false;
		mIsUSBCheck = false;
		mVideoFileStr = "SD card video List : \n";
		mUsbPhotoFileStr = "USB photo List : \n";
		mExtsdstr.setText("");
		mExtsdlist.setText(mVideoFileStr);
		mExtsdplay.setText("");
		mUdiskstr.setText("");
		mUdisklist.setText(mUsbPhotoFileStr);
		mSendMessage = false;
		mGetEXTSDstorage = false;
		mGetUDISKstorage = false;
		mIsLocal1 = false;
		mIsLocal2 = false;
		mIsDownload = false;
		mFTPdownload.setText("");
	}
	
	private void ReadFTPfile(){
		File file = new File(FTPfilePath);
		if(!file.exists()){
			ShowMessage("No file! Please put file FTP.txt in SD card.");
			return;
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
        	return;
        }
        USERNAME = ftpdata[0];
        PASSWORD = ftpdata[1];
        HOST = ftpdata[2];
        REMOTE_VIDEO1 = ftpdata[3];      
        FILESIZE1 = ftpdata[4];
        REMOTE_VIDEO2 = ftpdata[5];
        FILESIZE2 = ftpdata[6];
        System.out.println("USERNAME:"+USERNAME);
        System.out.println("PASSWORD:"+PASSWORD);
        System.out.println("HOST:"+HOST);
        System.out.println("REMOTE_VIDEO1:"+REMOTE_VIDEO1);
        System.out.println("REMOTE_VIDEO2:"+REMOTE_VIDEO2);
        System.out.println("FILESIZE1:"+FILESIZE1);
        System.out.println("FILESIZE2:"+FILESIZE2);
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
			try {
				String[] mFTPdata = new String[8];
				mFTPdata[0] = "ftpget";
				mFTPdata[1] = "-u";
				mFTPdata[2] = USERNAME;
				mFTPdata[3] = "-p";
				mFTPdata[4] = PASSWORD;
				mFTPdata[5] = HOST;
				mFTPdata[6] = localVideo;
				mFTPdata[7] = remoteVideo;
				
				ProcessBuilder builder = new ProcessBuilder(mFTPdata);
				mFtp = builder.start();
				InputStream is = mFtp.getInputStream();
				BufferedInputStream in = new BufferedInputStream(is);

				while(in.read() != -1 ){
				}
				mFtp.destroy();
				mFtp = null;} 			
			catch (IOException e) {  
	        	System.out.println("handleMessage->file error");
			}
			catch (NullPointerException e) { 
	        	System.out.println("handleMessage->file error");
			}	

			if(new File(localVideo).length() == mCurrentSize){
				mIsDownload = true;
				Intent ftpInfoS = new Intent("ftp_download_success");
				ftpInfoS.putExtra("localVideo", localVideo);
				TestUsbSd.this.sendBroadcast(ftpInfoS);
		        System.out.println("handleMessage->SUCCESS");
			}else{
				Intent ftpInfoE = new Intent("ftp_download_error");
				TestUsbSd.this.sendBroadcast(ftpInfoE);
	        	System.out.println("handleMessage->FAILURE");
			}
		}
	}
    
    private void ShowMessage(String str){
    	Toast.makeText(TestUsbSd.this, str, Toast.LENGTH_LONG).show();
    }
	
	private void ClearVideo(){
		videolist = new ArrayList<String>();
		mVideoFileSize = 0;
		mVideoPlayCount = 0;
		mIsVideoPlay = false;
		mIsSDCheck = false;
		mVideoFileStr = "SD card video List : \n";
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

	Handler checkStorage = new Handler();
    Runnable r = new Runnable()
    {
    	public void run()
    	{
    		//show run time
    		long interval = (System.currentTimeMillis() - mStartTime)/1000;
    		long hour = interval/3600;
    		long minute = (interval%3600)/60;
    		long second = (interval%3600)%60;
    		mRunTime.setText("Run time : "+hour+" : "+minute+" : "+second);
    		
    		//show ftp file download info
    		if(checkNet()){
	    		if(mIsLocal1 && !mIsLocal2){
	    			mFTPdownload.setText(mNetinfo+" Download "+REMOTE_VIDEO1+" ,file size : "+new File(VideoSDPath+LOCAL_VIDEO1).length()+" bytes");
	    		} else if(mIsLocal2 && !mIsLocal1){
	    			mFTPdownload.setText(mNetinfo+" Download "+REMOTE_VIDEO2+" ,file size : "+new File(VideoSDPath+LOCAL_VIDEO2).length()+" bytes");
	    		} 
    		}else {
    			mNetinfo = "Net error!!";
    			if(mFtp != null) mFtp.destroy();
    			if(mFTPthread != null) mFTPthread.interrupt();
    			mFTPdownload.setText("No network!!!");
    		}
    		
    		if(checkUSBstate()){
    			mGetUDISKstorage = true;
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

    		if(checkSDstate()){
    			mGetEXTSDstorage = true;
    			if(mVideoFileSize > 0) {
        			playVideo();
    			}
        		else{
        			stopVideo();
        			if(mFtp != null) mFtp.destroy();
        			if(mFTPthread != null) mFTPthread.interrupt();
        			mExtsdstr.setText("Please check SD card video file.");
        		}
    		}
    		
    		checkStorage.postDelayed(r, StoragecheckTime);
    	}
    };
	
    private boolean checkNet(){
    	ConnectivityManager conManager = (ConnectivityManager) getSystemService(CONNECTIVITY_SERVICE);
    	NetworkInfo networInfo = conManager.getActiveNetworkInfo();       
    	if (networInfo == null || !networInfo.isAvailable()){
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
    
    private boolean checkSDstate(){
    	if(!new File(VideoSDPath).canWrite()){
    		ClearVideo();
    		mExtsdstr.setText("Please check SD.");
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
		
		if(!mIsVideoPlay){
			mIsVideoPlay = true;
			mVideoview.setVideoPath(videolist.get(mVideoPlayCount));
			mVideoview.requestFocus();
			mVideoview.start();
			mExtsdplay.setText("Playing:"+videolist.get(mVideoPlayCount));
						
			if(checkNet()){
				//開始下載FTP檔案影片1
				mFTPthread = new GetFTPfile();
				if(!mIsLocal1){
					mFTPthread.setDownloadinfo( VideoSDPath+LOCAL_VIDEO1, REMOTE_VIDEO1,FILESIZE1);
					mFTPthread.start();
					mIsLocal1 = true;
					mIsLocal2 = false;
					System.out.println("playVideo->download->LOCAL_VIDEO1");
				}
				else if(!mIsLocal2){
					mFTPthread.setDownloadinfo( VideoSDPath+LOCAL_VIDEO2, REMOTE_VIDEO2,FILESIZE2);
					mFTPthread.start();
					mIsLocal1 = false;
					mIsLocal2 = true;
					System.out.println("playVideo->download->LOCAL_VIDEO2");
				}
			}
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
					stopVideo();
					initVariable();
					mExtsdstr.setText("Video playing error!!!");
					return true;
				}
			});
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
	
	private void stopVideo(){
		if(mIsVideoPlay){
			mIsVideoPlay = false;
			mVideoview.pause();
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
	protected void onDestroy() {
		super.onDestroy();	
		if(mFtp != null) mFtp.destroy();
		unregisterReceiver(mFTPReceiver);
		if(mFTPthread != null) mFTPthread.interrupt();
	}
}
