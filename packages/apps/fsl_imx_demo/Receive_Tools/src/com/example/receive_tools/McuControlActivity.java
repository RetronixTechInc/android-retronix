package com.example.receive_tools;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.text.format.Time;
import android.util.Log;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.text.DecimalFormat;
import java.text.SimpleDateFormat;
import java.util.TimeZone;

import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

public class McuControlActivity extends Activity {

    private static final String LOG_TAG = "McuControlActivity";
    private static final boolean DEBUG = false;

    //for New MCU FW
    private static String[] args = { "efm32fn", "2", "12", "-v", "30"};
    //for Old MCU FW
//    private static String mcufw = "old";
    private static String mcufw = "new";

    @Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);     
		if (DEBUG) Log.d(this.toString(), "onCreate");        
	}

    @Override
    protected void onPause(){
            super.onPause();
    }
 
	public static String mcufunction(String item, String arg4) {
		/*	cmd <i2c number> <address> [cmd:-v,-r,-c,-a,-sc,-sa,-sp,-p,-u] [data]
    	-v  : Get version	//1
    	-r  : Reset MCU		//2
    	-c  : Get clock		//3
    	-a  : Get alarm		//4
    	-sc : Set clock		//5
    	-sa : Set alarm		//6
    	-sp : Set power status	//7
    	-p  : Get power status	//8
    	-w  : Get Who Run		//9
    	-u  : Update MCU		//10
    	-sg : Set wdog time		//11
    	-sr : Set wdog times	//12
    	-u  : Get wdog status	//13
		 */
		// TODO Auto-generated method stub
		int item_value = 1;
		int index_start ;
		String get_string = new String() ;
		int index_end ;
		String get_mcu = new String();
		String START_KEYWORD = "Version:";
		String END_KEYWORD = "Version:";
		 
    	args[3] = "-" + item;
    	args[4] = arg4;
    	if ("v".equals(item)) {
    		item_value = 1;
    	} else if("r".equals(item)){
       		item_value = 2;
    	} else if("c".equals(item)){
       		item_value = 3;
    	} else if("a".equals(item)){
       		item_value = 4;
    	} else if("sc".equals(item)){
       		item_value = 5;
    	} else if("sa".equals(item)){
       		item_value = 6;
       		if("old".equals(mcufw)){
       			args[1] = "--setalarm";
       			args[2] = "/dev/rtc0";
       			args[3] = timezone_oldswitch(arg4);
           		if("Disable".equals(args[3])){
           			return "Fail";
           		}
       		}else{
       			args[4] = timezone_switch(arg4);
           		if("Disable".equals(args[4])){
           			return "Fail";
           		}
       		}
    	} else if("sp".equals(item)){
       		item_value = 7;
    	} else if("p".equals(item)){
       		item_value = 8;
    	} else if("w".equals(item)){
       		item_value = 9;
    	} else if("u".equals(item)){
       		item_value = 10;
    	} else if("sg".equals(item)){
       		item_value = 11;
    	} else if("sr".equals(item)){
       		item_value = 12;
    	} else if("g".equals(item)){
       		item_value = 13;
    	} else{
       		item_value = 1;
    	}
		String from_mcu = binarycmd(args);
  		if("old".equals(mcufw)){
  			return "pass";
  		}
        if("0983681195".equals(from_mcu)){
        	return "ng";
        }
  		
		switch(item_value){
		case 1:
			START_KEYWORD = "Version:";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			if( index_start < 0)return "ng";
			get_mcu = from_mcu.substring(index_start);
			break;
		case 2:
			return from_mcu;
		case 3:
		case 4:
			START_KEYWORD = "Time: ";
			END_KEYWORD = " -";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			if( index_start < 0)return "ng";
			get_string = from_mcu.substring(index_start);
			index_end = get_string.indexOf(END_KEYWORD);
			get_mcu = timezone_check(from_mcu.substring(index_start, index_start + index_end));
			break;
		case 5:
			return from_mcu;
		case 6:
			START_KEYWORD = "success";
			index_start = from_mcu.indexOf(START_KEYWORD);
			if( index_start < 0)return "ng";
			get_mcu = from_mcu.substring(index_start, index_start + 7);
			break;
		case 7:
			return from_mcu;
		case 8:
			START_KEYWORD = "Power Status:";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			if( index_start < 0)return "ng";
			get_mcu = from_mcu.substring(index_start);
			break;
		case 9:
			START_KEYWORD = "Who :";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			if( index_start < 0)return "ng";
			get_mcu = from_mcu.substring(index_start);
		break;
		case 10:
			return from_mcu;
		case 11:
			START_KEYWORD = "success";
			index_start = from_mcu.indexOf(START_KEYWORD);
			if( index_start < 0)return "ng";
			get_mcu = from_mcu.substring(index_start, index_start + 7);
			break;
		case 12:
			return from_mcu;
		case 13:
			START_KEYWORD = "GetWdog_Status:";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			if( index_start < 0)return "ng";
			get_mcu = from_mcu.substring(index_start);
		break;
        default:
			START_KEYWORD = "Version:";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			if( index_start < 0)return "ng";
			get_mcu = from_mcu.substring(index_start);
			break;
		}		
		
		 return get_mcu;
	}

	private static String timezone_check(String s) {     
		if (DEBUG) Log.d(LOG_TAG, "s : " + s); 
			try {   
				SimpleDateFormat sdf = new SimpleDateFormat("yy / MM / dd : HH : mm : ss");
				SimpleDateFormat ref = new SimpleDateFormat("yyyy/MM/dd:HH:mm:ss");
		        Calendar cd = Calendar.getInstance();
		        cd.setTime(sdf.parse(s));
				if(cd.get(Calendar.YEAR) <= 2){
					return "Disable";
				}
				cd.add(Calendar.YEAR, 70);
				cd.add(Calendar.SECOND, TimeZone.getDefault().getRawOffset()/1000);
				return ref.format(cd.getTime());   
		    } catch (Exception e) {   
		            return null;   
		    }
	}
	
	private static String timezone_switch(String s) {     
		if (DEBUG) Log.d(LOG_TAG, "s : " + s); 
			try {   
				SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd:HH:mm:ss");
				SimpleDateFormat ref = new SimpleDateFormat("yy/MM/dd:HH:mm:ss");
		        Calendar cd = Calendar.getInstance();
		        cd.setTime(sdf.parse(s));
				if(cd.get(Calendar.YEAR) < 1970){
					return "Disable";
				}
				cd.add(Calendar.YEAR, - 1970);
				cd.add(Calendar.SECOND, -(TimeZone.getDefault().getRawOffset()/1000));
				return ref.format(cd.getTime());   
		    } catch (Exception e) {   
		            return "Disable";   
		    }
	}
	
	private static String timezone_oldswitch(String s) {     
		if (DEBUG) Log.d(LOG_TAG, "s : " + s); 
        long _now, _set, _diff = 0;
        Time _TimeYMD1 = new Time();
        _TimeYMD1.setToNow();
        _now = (_TimeYMD1.toMillis(false))/1000;
        
		try {   
			SimpleDateFormat sdf = new SimpleDateFormat("yyyy/MM/dd:HH:mm:ss");
			Date d1 = sdf.parse(s);
			_set = d1.getTime()/1000;	//getTime is GMT+0 time. 
			if(_set > _now){
				_diff = _set - _now;
			}else{
				return "Disable";				
			}
			if (DEBUG) Log.d(LOG_TAG, "Tom################_set : " + _set + "; _now:" + _now + "; _diff:" + _diff);
			return String.valueOf(_diff);   
	    } catch (Exception e) {   
	        return "Disable";   
	    }
        
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
