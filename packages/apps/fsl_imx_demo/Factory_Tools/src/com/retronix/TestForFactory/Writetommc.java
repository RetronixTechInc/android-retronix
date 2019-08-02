package com.retronix.TestForFactory;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.EditText;
import android.widget.Button;
import android.util.Log;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;

import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.widget.ImageView;

public class Writetommc extends Activity {
    private static final String LOG_TAG = "Writetommc";
    private static final boolean DEBUG = false;

	private LinearLayout mLinearLayout;
	
    private TextView mSystemclock, mclockcheck, midcheck, mmaccheck;
    private EditText mProductid, mEthmac;
    private Button mFinishtest;
    private ImageView mresultimageview;
    private boolean bclocktime = false;
    
    private boolean idwrite = false;
    private boolean macwrite = false;
    private String[] args = { "rtx_uboot", "--read", "all"};

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
				
        if (DEBUG) Log.d(LOG_TAG, "onCreate");        
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
		
		setContentView(R.layout.writemmc);
		
		findViews();
		setListeners();
		init();
	}
	
    private void findViews()
    {
        if (DEBUG) Log.d(LOG_TAG, "findViews");        
        mSystemclock = (TextView) findViewById(R.id.Systemclock);
        mclockcheck = (TextView) findViewById(R.id.clockcheck);
        midcheck = (TextView) findViewById(R.id.idcheck);
        mmaccheck = (TextView) findViewById(R.id.maccheck);
        mProductid = (EditText) findViewById(R.id.Productid);
        mEthmac = (EditText) findViewById(R.id.Ethmac);
        mFinishtest = (Button) findViewById(R.id.Finishtest);
        
        mProductid.setSelectAllOnFocus(true);
        mEthmac.setSelectAllOnFocus(true);
        mresultimageview = (ImageView)findViewById(R.id.writeemmcimageview);
    }

    private void setListeners() {
        if (DEBUG) Log.d(LOG_TAG, "setListeners");        
        mFinishtest.setOnClickListener(writemmc);
    }

    private Button.OnClickListener writemmc = new Button.OnClickListener()
    {
		@Override
		public void onClick(View v) {
			// TODO Auto-generated method stub
	        if (DEBUG) Log.d(LOG_TAG, "writemmc");
	        
	        String test = new String();
	        boolean writeidresult = false;
	        boolean writemacresult = false;
	        
	        //if(idwrite){
	        	String IDscan = (mProductid.getText().toString()).trim();
	          	//if(isHexadecimal(IDscan) && IDscan.length() >= 30 && IDscan.length() <= 40){
	          	if(IDscan.length() >= 1 && IDscan.length() <= 30){
	          		test = ubootfunction("write", "ubProductSerialNO=" + IDscan);
	          		if("Pass".equals(test)){
	          			writeidresult = true;
	          		}
	          		TestVideo.WriteErrorLog("write ubProductSerialNO=" + mProductid.getText().toString());
	          	}else{
	          		dialog_msg("The ubProductSerialNO123 formate is error!");
	          	}
	        //}else{
	        //	dialog_msg("The ubProductSerialNO is not null");
	        //}
	        
	        //if(macwrite){
	        	String Macscan = (mEthmac.getText().toString()).trim();
	        	if (DEBUG) Log.d(LOG_TAG, "Tom-----Macscan: " + Macscan);      
	        	if(isHexadecimal(Macscan) && Macscan.length() == 12){
	        		/*StringBuffer str = new StringBuffer(Macscan);
	        		String strInsert = ":";
	        		str.insert(10,strInsert);
	        		str.insert(8,strInsert);
	        		str.insert(6,strInsert);
	        		str.insert(4,strInsert);
	        		str.insert(2,strInsert);
	        		if (DEBUG) Log.d(LOG_TAG, "Tom-----MacWrite: " + str.toString());
	        		test = ubootfunction("write", "ubMAC01=" + str.toString());
	        		*/
	        		test = ubootfunction("write", "ubMAC01=" + Macscan);
	          		if("Pass".equals(test)){
	          			writemacresult = true;
	          		}
	        		TestVideo.WriteErrorLog("write ubMAC01=" + mEthmac.getText().toString());
	          	}else{
	          		dialog_msg("The ubMAC01 formate is error!");
	          	}
	        //}else{
	        //	dialog_msg("The ubMAC01 is not null");
	        //}
	        
	        if(writeidresult && writemacresult){
	        	dialog_msg_pass();
	        }else{
	        	dialog_msg_ng();
	        }
		}
		

	};
	
	private void init(){
		if(DEBUG) Log.d(LOG_TAG, "init.");
		mSystemclock.setText(getsystemclock());
		if(bclocktime)mclockcheck.setText("PASS");
		else mclockcheck.setText("NG");
		
        String test = new String();
        
        test = ubootfunction("read", "ubProductSerialNO");
        midcheck.setText(test);
        if( "null".equals(test) ) idwrite = true;
        
        test = ubootfunction("read", "ubMAC01");
        mmaccheck.setText(test);
        if( "null".equals(test) ) macwrite = true;
	}
	
    @Override
    public void onStop()
    {
		if(DEBUG) Log.d(LOG_TAG, "onStop.");
        super.onStop();
        Writetommc.this.finish();
    }
	
	@Override
	protected void onDestroy() {			
		if(DEBUG) Log.d(LOG_TAG, "onDestroy.");
		super.onDestroy();
	}

	private void dialog_msg_pass(){
		int id = getResources().getIdentifier("com.retronix.TestForFactory:drawable/" + "pass", null, null);
		mresultimageview.setImageResource(id);
	}
		
	private void dialog_msg_ng(){
		int id = getResources().getIdentifier("com.retronix.TestForFactory:drawable/" + "ng", null, null);
		mresultimageview.setImageResource(id);
	}

	private void dialog_msg(String msg){
		new AlertDialog.Builder(Writetommc.this)
		.setTitle("Write information to eMMC")
		.setMessage(msg)
		.setPositiveButton(R.string.dialog_sure,
			new DialogInterface.OnClickListener() {
				@Override
				public void onClick(DialogInterface arg0, int arg1) {
				}
			})
		.show();
	}
		
	private String ubootfunction(String cmd, String variable) {
		/*	cmd [cmd:--clean,--read,--write,--delete] [[variable][=data]]
    	--clean  : Clean all variable	//1
    	--read   : read variable		//2
    	--write  : write variable		//3
    	--delete : delete varialbe		//4
    	variable:
    	ulCheckCode:0xabcdefcc
        ubMagicCode: 0xab 0xcd 0xef 0x12 0x34 0x56 0x78 0x90 0xab 0xab 0xab 0xab 0xab 0xab 0xab 0xab
        ubMAC01:ab:ec:12:34:56:ff
        ubMAC02:null
        ubMAC03:null
        ubProductName:null
        ubProductSerialNO:null
        ubBSPVersion:null
        ulPasswordLen:0
        ubPassword:null
        ulFunction:0
        ulCmd:0
        ulStatus:0
		 */
		// TODO Auto-generated method stub
		int item_value = 1;
		String from_mmc = new String();
		
		int index_start ;
		String get_mmc = new String() ;
		int index_end ;
		
		String START_KEYWORD = "Pass";
		String END_KEYWORD = "\0";
    	
    	if ("read".equals(cmd)) {
    		item_value = 1;
    		args[1] = "--read";
        	args[2] = variable;    		
    	} else if("write".equals(cmd)){
       		item_value = 2;
    		args[1] = "--write";
        	args[2] = variable;    		
    	} else if("delete".equals(cmd)){
       		item_value = 3;
    		args[1] = "--delete";
        	args[2] = variable;    		
    	} else if("clean".equals(cmd)){
       		item_value = 4;
    		args[1] = "--clean";
        	args[2] = "";    		
    	} else{
       		item_value = 1;
    		args[1] = "--read";
        	args[2] = "all";    		
    	}
    	
		from_mmc = binarycmd(args);

		switch(item_value){
		case 1:
			if("all".equals(variable)){
				get_mmc = from_mmc;
				break;
			}else{
				START_KEYWORD = variable + ":";
				index_start = from_mmc.indexOf(START_KEYWORD) + START_KEYWORD.length();
				if( index_start < 0)return "ng";
				get_mmc = from_mmc.substring(index_start);
				break;
			}
		case 2:
		case 3:
		case 4:
			START_KEYWORD = "Pass";
			index_start = from_mmc.indexOf(START_KEYWORD) + START_KEYWORD.length();
			if( index_start != 0)get_mmc = "Pass";
			else get_mmc = "ng";
        default:
			break;
		}		
		
		 return get_mmc.trim();
	}

    public static String binarycmd(String[] item){
        if (DEBUG) Log.d(LOG_TAG, "binarycmd : " + item[0] + " " + item[1] + " " + item[2]);        
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
    	
    	return "1738-model";
    }


	//Get system time and check year is over 2013.
	private boolean clockcheck(){
		Calendar mCalendar = new GregorianCalendar();
		int _year = mCalendar.get(Calendar.YEAR) ;
        if (DEBUG) Log.d(LOG_TAG, "_year : " + _year);	
		if(_year > 2013){	//get clock is correct from MCU at start up.
			return true;
		}
		return false;    
	}
	
	//Get system time and check year is over 113 + 1900.
	private String getsystemclock(){
        if (DEBUG) Log.d(LOG_TAG, "getsystemclock");        
		//先行定義時間格式
		SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");
		//取得現在時間
		Date dt = new Date();
		//透過SimpleDateFormat的format方法將Date轉為字串
		String dts=sdf.format(dt);
		
		int _year = dt.getYear() ;
        if (DEBUG) Log.d(LOG_TAG, "_year : " + _year);	
		if(_year > 113){	//get clock is correct from MCU at start up.
			bclocktime = true;       	
		}

		return dts;
	}
	
	public static boolean isHexadecimal(String text) {
	    text = text.trim();

	    char[] hexDigits = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	            'a', 'b', 'c', 'd', 'e', 'f', 'A', 'B', 'C', 'D', 'E', 'F' };

	    int hexDigitsCount = 0;

	    for (char symbol : text.toCharArray()) {
	        for (char hexDigit : hexDigits) {
	            if (symbol == hexDigit) {
	                hexDigitsCount++;
	                break;
	            }
	        }
	    }

	    return true ? hexDigitsCount == text.length() : false;
	}
	
}
