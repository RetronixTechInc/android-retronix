package com.example.receive_tools;

import android.app.Activity;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import android.util.Log;
import android.os.Bundle;

public class KillprocessActivity extends Activity {

    private static final String LOG_TAG = "KillprocessActivity";
    private static final boolean DEBUG = false;

    private static String[] args = { "pidalive", "pidalive", "/system/bin/mediaserver", "200000", "200000"};

    @Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);     
		if (DEBUG) Log.d(this.toString(), "onCreate");        
	}

    @Override
    protected void onPause(){
            super.onPause();
    }
 
	public static String killfunction(String name, String size, String freesize) {
		// TODO Auto-generated method stub
		int index_start ;
		 
    	args[0] = "pidalive";
    	args[1] = "pidsize";
    	args[2] = name;
    	args[3] = size;
    	args[4] = freesize;
    	
 		String from_mcu = binarycmd(args);
 		
        if("0983681195".equals(from_mcu)){
        	return "ng";
        }else{
			String START_KEYWORD = "NG";
			index_start = from_mcu.indexOf(START_KEYWORD);
			if( index_start < 0)return "pass";
		}
  				
		return "ng";
	}

	public static String alivefunction(String name) {
		// TODO Auto-generated method stub
    	args[0] = "pidalive";
    	args[1] = "pidalive";
    	args[2] = name;
    	args[3] = "";
    	args[4] = "";
    	
 		String from_mcu = binarycmd(args);

        Log.d(LOG_TAG, "alivefunction : " + from_mcu);        
 		
 		int index_start = from_mcu.indexOf("alive");
		if( index_start < 0) return "died" ;
		
		return "alive" ;
	}

    private static String binarycmd(String[] item){
        if (DEBUG) Log.d(LOG_TAG, "binarycmd : " + item);        
    	String result = ""; 
    	ProcessBuilder processBuilder = new ProcessBuilder(item); 
    	Process process = null; 
    	InputStream errIs = null; 
    	InputStream inIs = null; 
    	try { 
	    	ByteArrayOutputStream baos = new ByteArrayOutputStream(); 
	    	int read = -1; 
	    	process = processBuilder.start(); 
	    	errIs = process.getErrorStream(); 
	    	while ((read = errIs.read()) != -1) { 
	    		baos.write(read); 
	    	} 
	    	baos.write('\n'); 
	    	inIs = process.getInputStream(); 
	    	while ((read = inIs.read()) != -1) { 
	    		baos.write(read); 
	    	} 
	    	byte[] data = baos.toByteArray(); 
	    	result = new String(data); 
	        if (DEBUG) Log.d(LOG_TAG, "binarycmd : " + result);
	        return result;
    	} 	
    	catch (IOException e) {
            if (DEBUG) Log.d(LOG_TAG, "binarycmd : IOException = " + e);
    	} 
    	catch (Exception e) {
            if (DEBUG) Log.d(LOG_TAG, "binarycmd : Exception = " + e);
    	} 
    	finally {
	    	try { 
		    	if (errIs != null) { errIs.close(); } 
		    	if (inIs != null) { inIs.close(); } 
		    } 
	    	catch (IOException e) { } 
		    if (process != null) { process.destroy(); } 
    	}
    	return "0983681195";
    }

}
