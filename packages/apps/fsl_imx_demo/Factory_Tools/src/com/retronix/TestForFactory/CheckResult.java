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
import android.text.TextWatcher;
import android.text.Editable;

public class CheckResult extends Activity {
    private static final String LOG_TAG = "CheckResult";
    private static final boolean DEBUG = false;

	private LinearLayout mLinearLayout;
	
    private TextView mSystemclock, mclockcheck, midcheck, mmaccheck, midresult, mmacresult;
    private EditText mProductid, mEthmac;
    private Button mResulttest;
    private ImageView mresultimageview;
    private boolean bclocktime = false;
    
    private boolean idwrite = false;
    private boolean idresult = false;
    private boolean macwrite = false;
    private boolean macresutl = false;
    private String[] args = { "rtx_uboot", "--read", "all"};

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
				
        if (DEBUG) Log.d(LOG_TAG, "onCreate");        
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
		
		setContentView(R.layout.checkresult);
		
		findViews();
		setListeners();
		init();
	}
	
    private void findViews()
    {
        if (DEBUG) Log.d(LOG_TAG, "findViews");        
        mSystemclock = (TextView) findViewById(R.id.Systemclock1);
        mclockcheck = (TextView) findViewById(R.id.clockcheck1);
        midcheck = (TextView) findViewById(R.id.idcheck1);
        mmaccheck = (TextView) findViewById(R.id.maccheck1);
        midresult = (TextView) findViewById(R.id.idresult1);
        mmacresult = (TextView) findViewById(R.id.macresult1);
        mProductid = (EditText) findViewById(R.id.Productid1);
        mEthmac = (EditText) findViewById(R.id.Ethmac1);
        mResulttest = (Button) findViewById(R.id.Resulttest1);
        mresultimageview = (ImageView)findViewById(R.id.resultimageview);
        
    }

    private void setListeners() {
        if (DEBUG) Log.d(LOG_TAG, "setListeners"); 
        //Add button click listen.
        mResulttest.setOnClickListener(resultcheck);
        //Add Edittext change listen.
    	mProductid.addTextChangedListener(mProductidTextWatcher);

    }

    private Button.OnClickListener resultcheck = new Button.OnClickListener()
    {
		@Override
		public void onClick(View v) {
			// TODO Auto-generated method stub
	        if (DEBUG) Log.d(LOG_TAG, "resultcheck");	 
	        
	        String id1 = ubootfunction("read", "ubProductSerialNO");
	        String id2 = mProductid.getText().toString();
	        TestVideo.WriteErrorLog("MMC ubProductSerialNO=" + id1);
	        TestVideo.WriteErrorLog("Check ubProductSerialNO=" + id2);
	        if(id1.equals(id2)){
	        	midresult.setText("PASS");
	        	TestVideo.WriteErrorLog("result ubProductSerialNO=Pass");
	        	idresult = true;
	        }else{
	        	midresult.setText("NG");
	        	idresult = false;
	        }
	        
	        String mac1 = ubootfunction("read", "ubMAC01");
	        String mac2 = "ng";
        	String Macscan = (mEthmac.getText().toString()).trim();
        	if (DEBUG) Log.d(LOG_TAG, "Tom-----Macscan: " + Macscan);      
        	if(Writetommc.isHexadecimal(Macscan) && Macscan.length() == 12){
        		StringBuffer str = new StringBuffer(Macscan);
        		String strInsert = ":";
        		str.insert(10,strInsert);
        		str.insert(8,strInsert);
        		str.insert(6,strInsert);
        		str.insert(4,strInsert);
        		str.insert(2,strInsert);
        		mac2 = str.toString();
        	}else{
        		dialog_msg("MAC formate error!!!");
        	}
        	TestVideo.WriteErrorLog("MMC ubMAC01=" + mac1);
	        TestVideo.WriteErrorLog("Check ubMAC01=" + mac2);
	        if(mac1.equals(Macscan)){
	        	mmacresult.setText("PASS");
	        	TestVideo.WriteErrorLog("result ubMAC01=Pass");
	        	macresutl = true;
	        }else{
	        	mmacresult.setText("NG");
	        	macresutl = false;
	        }
	        
			SimpleDateFormat format = new SimpleDateFormat("yyyy-MM-dd-HH-mm-ss");
	    	String mLogTime = format.format(System.currentTimeMillis());
	       
	    	if(!idwrite && idresult && !macwrite && macresutl && bclocktime){
	        	dialog_msg_pass();
	        	TestVideo.saveWriteErrorLog1("/mnt/extsd/" + id1 + "-" + mLogTime + "-pass.log" );
	    	}else{
	        	dialog_msg_ng();	        	
		        TestVideo.saveWriteErrorLog1("/mnt/extsd/" + id1 + "-" + mLogTime + "-ng.log" );
	        }
	        
		}
	};
	
	private TextWatcher mProductidTextWatcher = new TextWatcher() {
	    @Override
	    public void afterTextChanged(Editable s) {
	        // TODO Auto-generated method stub
	    }

	    @Override
	    public void beforeTextChanged(CharSequence s, int start, int count, int after) {
	        // TODO Auto-generated method stub
	    }

	    @Override
	    public void onTextChanged(CharSequence s, int start, int before, int count) {
	        // TODO Auto-generated method stub
	    	Log.d(LOG_TAG, "Tom-----mProductid:" + mProductid.getText().toString());
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
        
        //Select All if focus it.
        mProductid.setSelectAllOnFocus(true);
        mEthmac.setSelectAllOnFocus(true);

	}
	
	private void dialog_msg_pass(){
		int id = getResources().getIdentifier("com.retronix.TestForFactory:drawable/" + "testok", null, null);
		mresultimageview.setImageResource(id);
	}
		
	private void dialog_msg_ng(){
		int id = getResources().getIdentifier("com.retronix.TestForFactory:drawable/" + "ng", null, null);
		mresultimageview.setImageResource(id);
	}

	private void dialog_msg(String msg){
		new AlertDialog.Builder(CheckResult.this)
		.setTitle("Check information Fail")
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
			TestVideo.WriteErrorLog("result clockcheck=Pass");
			bclocktime = true;       	
		}else{
			TestVideo.WriteErrorLog("result clockcheck=NG");
		}

		return dts;
	}
	
}
