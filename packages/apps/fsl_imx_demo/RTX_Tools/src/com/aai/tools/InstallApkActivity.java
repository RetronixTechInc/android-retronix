package com.aai.tools;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;

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

import android.util.Log;

public class InstallApkActivity extends ListActivity {
    
    private static final String LOG_TAG = "InstallApk";
    private static final boolean DEBUG = false;

	private static final int SUCCESS = 1;
	private static final int FAILURE = 0;
	private ProgressDialog diamsg;
	
	private List<String> items=null;
	private List<String> paths=null;
	private String rootPath="/mnt";
	private TextView mPath;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        if (DEBUG) Log.d(LOG_TAG, "onCreate");  
        init();
		setContentView(R.layout.dirlist);
        
        mPath=(TextView)findViewById(R.id.mPath);
    
		getFileDir(rootPath);

    }
    
    private void init(){
    	diamsg = new ProgressDialog(this);
    	diamsg.setTitle("Please wait");
    	diamsg.setMessage("Installing...");
    	diamsg.setCancelable(true);
    	diamsg.setIndeterminate(true);
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
			  if (file.isDirectory() || file.getName().endsWith(".apk"))
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
			/*new AlertDialog.Builder(this).setIcon(R.drawable.icon)
						   .setTitle("["+paths.get(position)+"] is File!")
						   .setPositiveButton("OK",
						   new DialogInterface.OnClickListener()
						   {
							 public void onClick(DialogInterface dialog,int whichButton)
							 {
							 }
						   }).show(); */        
			new AlertDialog.Builder(this)
				.setTitle(R.string.Installapk)
				.setMessage("Are you sure install "+file.getName()+"?")
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
    		String[] args = { "pm", "install", "-r", paths.get(filenum)}; 
        	String result = ""; 
        	ProcessBuilder processBuilder = new ProcessBuilder(args); 
        	Process process = null; 
        	InputStream inIs = null; 
        	try {
    	    	ByteArrayOutputStream baos = new ByteArrayOutputStream(); 
    	    	int read = -1; 
    	    	process = processBuilder.start(); 
    	    	inIs = process.getInputStream(); 
    	    	while ((read = inIs.read()) != -1) { 
    	    		baos.write(read); 
    	    	}
    	    	byte[] data = baos.toByteArray(); 
    	    	result = new String(data); 
        	} catch (IOException e) { 
        	e.printStackTrace(); 
        	} catch (Exception e) { 
        	e.printStackTrace(); 
        	} finally {
    	    	try { 
    		    	if (inIs != null) { 
    		    		inIs.close(); 
    		    	} 
    		    } catch (IOException e) { 
    		    	e.printStackTrace(); 
    		    } 
    		    if (process != null) { 
    		    	process.destroy(); 
    	    	} 
        	} 

        	System.out.println("Install result--->"+result);

        	if(result.trim().equalsIgnoreCase("Success")){
	        	Message message = new Message();  
	        	message.what = SUCCESS;
	        	handler.sendMessage(message);
        	}
        	else{
        		Message message = new Message();  
	        	message.what = FAILURE;
	        	handler.sendMessage(message);
        	}
    	}
    }
    
    Handler handler =  new Handler(){
		@Override
		public void handleMessage(Message msg) {
			switch (msg.what){
				case SUCCESS:
					diamsg.dismiss();
					ShowMessage("Installed successful");
					break;
				case FAILURE:
					diamsg.dismiss();
					ShowMessage("Installed unsuccessful");
					break;
			}
		}
    };
    
    private void ShowMessage(String str){
    	Toast.makeText(InstallApkActivity.this, str, Toast.LENGTH_SHORT).show();
    }
    
}
