package com.aai.tools;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.io.ByteArrayOutputStream;

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Build;
import android.util.Log;
import android.view.View;
import android.os.RecoverySystem;
import android.app.AlertDialog;
import android.app.ListActivity;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;
import java.util.List;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.content.*;

public class UpdatesystemActivity extends ListActivity {
    
    private static final String LOG_TAG = "UpdateSystem";
    private static final boolean DEBUG = true;
	
	private static String DEVICPROJECTNAME;
	private static float DEVICEVERSION;
	
	private static final int SUCCESS = 1;
	private static final int FAILURE = 0;
	
	private ProgressDialog diamsg;
	
	private List<String> items=null;
	private List<String> paths=null;
	private String rootPath="/storage";
	private TextView mPath;
	
	WakeLock mWakelock;
	Context mContext;
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);      
        
        if (DEBUG) Log.d(LOG_TAG, "onCreate");  
        mContext = this;
        
		getSystemVersion();
		
        init();
        
		setContentView(R.layout.dirlist);
        
        mPath=(TextView)findViewById(R.id.mPath);
    
		getFileDir(rootPath);
		
		PowerManager pm = (PowerManager)mContext.getSystemService(mContext.POWER_SERVICE);
		mWakelock = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, "OTA Wakelock");
		
		
    }
    
    private void init(){
    	diamsg = new ProgressDialog(this);
    	diamsg.setTitle("Please wait");
    	diamsg.setMessage("Check Update file...");
    	diamsg.setCancelable(true);
    	diamsg.setIndeterminate(true);
    	
    	DEVICPROJECTNAME = "";
    	DEVICEVERSION = -1;
    }
    
	private void getFileDir(String filePath)
	{
		mPath.setText(filePath);
		  
		items=new ArrayList<String>();
		paths=new ArrayList<String>();  
		File f=new File(filePath);  
		File[] files = null ;
		if (f.canRead())
		{
			files=f.listFiles();
		}

		if(!filePath.equals(rootPath))
		{
		  items.add("Back to "+rootPath);
		  paths.add(rootPath);
		  items.add("Back to ../");
		  paths.add(f.getParent());
		}
		if(files != null)
		{
			for(int i=0;i<files.length;i++)
			{
			  File file=files[i];
			  if (file.isDirectory() || file.getName().endsWith(".zip") || file.getName().endsWith(".rtx"))
			  {
				if (DEBUG) Log.d(LOG_TAG, "file.getName()=" + file.getName());  
				items.add(file.getName());
				paths.add(file.getPath());
			  }
			}
		}

		ArrayAdapter<String> fileList = new ArrayAdapter<String>(this,R.layout.file_row, items);
		setListAdapter(fileList);
	}
  
    @Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
    	final int filenum = position;
		File file = new File(paths.get(position));
		if (file.isDirectory())
		{
		  getFileDir(paths.get(position));
		}
		else
		{ 
			new AlertDialog.Builder(this)
				.setTitle(R.string.system_update)
				.setMessage(R.string.check_update)
				.setPositiveButton("Yes",
						new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface arg0, int arg1) {
								installThread install = new installThread();
								install.setNum(filenum);
								install.start();
								diamsg.show();
							}
						})
				.setNegativeButton("No",
						new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface arg0, int arg1) {
								
							}
						})
				.show();
		}
	}

    class installThread extends Thread{
    	private int filenum = -1;
    	
    	public void setNum(int num){
    		filenum = num;
    	}
    	
    	public void run(){
    		Looper.prepare();

			try{
				if(writeRecovery(paths.get(filenum))){
					Message message = new Message();  
					message.what = SUCCESS;
					handler.sendMessage(message);
				}
				else{
					Message message = new Message();  
					message.what = FAILURE;
					handler.sendMessage(message);
				}
			} catch (IOException e) {
					Message message = new Message();  
					message.what = FAILURE;
					handler.sendMessage(message);
			}
    	}
    }
    
    Handler handler =  new Handler(){
		@Override
		public void handleMessage(Message msg) {
			diamsg.dismiss();		
			switch (msg.what){
				case SUCCESS:
					//ShowMessage("Installed successful");
					break;
				case FAILURE:
					ShowMessage("File Format or Verseion fail!");
					break;
			}
		}
    };
    
    private void ShowMessage(String str){
    	Toast.makeText(UpdatesystemActivity.this, str, Toast.LENGTH_SHORT).show();
    }
    
    private boolean writeRecovery(String sfile) throws IOException {
		Log.d(LOG_TAG, "sfile=" + sfile);
		File file = new File(sfile); 
		
		String path_abs = "/mnt/udisk/";
		String file_name = file.getName();
		File recoveryFile = new File(path_abs+file_name);
		
		if(file.getName().endsWith(".rtx"))
		{
			if(compareSystemVersion(sfile))
			{
				RecoverySystem.installPackage(this, recoveryFile);
				return true;
			}
		}
		else if(file.getName().endsWith(".zip"))
		{
			if(upatefileverify(sfile))
			{
				RecoverySystem.installPackage(this, recoveryFile);
				return true;
			}
		}
		
	    return false;
	}
   
    private boolean compareSystemVersion(String path) {
		File systemzip = new File(path);
		FileInputStream _zipinput;
		try {
			_zipinput = new FileInputStream(systemzip);
			byte[] head = new byte[16];
			_zipinput.read(head);
			_zipinput.close();		
			
			/////////////////////////check force update/////////////////////////// 
			String filehead = new String(head);
			Log.d(LOG_TAG, "filehead=" + filehead);  
     
			String forceupdate = filehead.substring(0,3);
			String fileProjectname = filehead.substring(8,12);
			String fileVersion = filehead.substring(12,16);

	       
	       if (DEBUG) Log.d(LOG_TAG, "fileProjectname=" + fileProjectname);        
	       if (DEBUG) Log.d(LOG_TAG, "fileVersion=" + fileVersion);        
	       if(forceupdate.equals("RTX")){
	       	//if( fileVersion > DEVICEVERSION){
	       		return true;
	       	//}
	       }else{
	       	if(fileProjectname.equals(DEVICPROJECTNAME)){
	       		if( fileVersion.equals(DEVICEVERSION))
						return true;
	       	}		
	       }
		} catch (FileNotFoundException e) {
			e.printStackTrace();
		} catch (IOException e) {
			e.printStackTrace();
		} catch (NumberFormatException e) {
			e.printStackTrace();
		}
		return false;
	}
	
	RecoverySystem.ProgressListener recoveryVerifyListener = new RecoverySystem.ProgressListener() {
		public void onProgress(int progress) {
			Log.d(LOG_TAG, "verify progress" + progress);
		}
	};
	
	private boolean upatefileverify(String path)
	{
		boolean bret = false;
		
		File recoveryFile = new File(path);
		  try {
        	 //mWakelock.acquire();
        	 RecoverySystem.verifyPackage(recoveryFile, recoveryVerifyListener, null);
        	 bret = true;
         } catch (Exception e1) {
        	 e1.printStackTrace();
        	 return bret;
         } finally {
        	 //mWakelock.release();
         }
         
         return bret;
		
	}
    
    public void getSystemVersion(){
    	String systemversion = Build.ID;
 
		if(systemversion.length() > 7)
		{
			DEVICPROJECTNAME = systemversion.substring(0, 4);
			String sVer = systemversion.substring(4, 8) ;
			boolean bVer = isNumeric(sVer);
			if(bVer)
			{
				try{
					DEVICEVERSION = new Float(sVer);
				} catch (Exception e) { 
					e.printStackTrace(); 
				}
			}
		}
		if (DEBUG) Log.d(LOG_TAG, "getSystemVersion=" + DEVICEVERSION);    
    }
    
    private static boolean isNumeric(String str){
		if(str == null)
		{
			return false;
		}
		
	   for(int i=str.length();--i>=0;){
	      int chr=str.charAt(i);
	      if(chr<46 || chr>57 || chr==47)
	         return false;
	   }
	   
	   return true;
    }

}
