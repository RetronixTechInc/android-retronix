package com.aai.tools;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.text.format.Time;
import android.widget.TextView;
import android.util.Log;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.text.DecimalFormat;
import java.text.SimpleDateFormat;
import java.util.TimeZone;
import android.content.SharedPreferences;
import android.widget.RadioButton;

public class McuwakeupActivity extends Activity {

    private static final String LOG_TAG = "McuwakeupActivity";
    private static final boolean DEBUG = false;

    private Button button_save;
    private Button button_getalarm;
    private Button button_disable;
    private Button button_cancel;
    private EditText set_wakeup_time;
    private TextView show_wakeup_time;
    private EditText edittext_mcu1;
    private EditText edittext_mcu2;
    private RadioButton radio_keeppower_on;
    private RadioButton radio_keeppower_off;

    public static SharedPreferences settings;
    public static final String PREF = "REBOOT_PREF";
    private static final String PREF_MCU_BUS = "MCU_BUS";
    private static final String PREF_MCU_ADDR = "MCU_ADDR";

    private static String pref_mcu_bus ;
    private static String pref_mcu_addr ;

    private String[] args = { "efm32fn", "2", "12", "-v", "30"};

	private String wakeup_time ;
    private long mSeconds;

    @Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
      
		if (DEBUG) Log.d(this.toString(), "onCreate");        

        setContentView(R.layout.mcuwakeup);
		
	    findViews();
	    restorewakeup();
	    setListeners(); 
	}

    @Override
    protected void onPause(){
            super.onPause();
    }
    
    private void findViews()
    {
        if (DEBUG) Log.d(LOG_TAG, "findViews");        
        button_save = (Button) findViewById(R.id.save);
        button_getalarm = (Button) findViewById(R.id.getalarm);
        button_disable = (Button) findViewById(R.id.disable);
        button_cancel = (Button) findViewById(R.id.cancel);
        set_wakeup_time = (EditText) findViewById(R.id.set_wakeup);
        show_wakeup_time = (TextView) findViewById(R.id.get_wakeup);
        edittext_mcu1 = (EditText) findViewById(R.id.edittext_mcu_1);
        edittext_mcu2 = (EditText) findViewById(R.id.edittext_mcu_2);
        radio_keeppower_on = (RadioButton) findViewById(R.id.power_status_on);
        radio_keeppower_off = (RadioButton) findViewById(R.id.power_status_off);
    }

    // Restore wakeup time
    private void restorewakeup()
    {
		if (DEBUG) Log.d(LOG_TAG, "restorewakeup");        
        getPref(this);
        pref_mcu_bus = settings.getString(PREF_MCU_BUS, "2");
        pref_mcu_addr = settings.getString(PREF_MCU_ADDR, "12");
        if(! "".equals(pref_mcu_bus)) {
        	edittext_mcu1.setText(pref_mcu_bus);
        }
        if(! "".equals(pref_mcu_addr)) {
        	edittext_mcu2.setText(pref_mcu_addr);
        }   
        
        wakeup_time = mcufunction("p");
        Log.d(LOG_TAG, "mcufunction(p); return=" + wakeup_time + "anton");       
        if("00".equals(wakeup_time)) {
        	edittext_mcu1.setText(pref_mcu_bus);
        	radio_keeppower_on.setChecked(false);
        	radio_keeppower_off.setChecked(true);
        }else{
        	radio_keeppower_on.setChecked(true);
        	radio_keeppower_off.setChecked(false);        	
        }
        radio_keeppower_on.setOnClickListener(new RadioButton.OnClickListener() {
            public void onClick(View v) {
            	args[4] = "2";
            	wakeup_time = mcufunction("sp");
            }
        });
        radio_keeppower_off.setOnClickListener(new RadioButton.OnClickListener() {
            public void onClick(View v) {
            	args[4] = "3";
            	wakeup_time = mcufunction("sp");
            }
        });
    }

    // Show wakeup time
    private void showwakeup()
    {
		if (DEBUG) Log.d(LOG_TAG, "showwakeup");
    	//wakeup_time = mcufunction("a");
    	wakeup_time = timezone_check(mcufunction("a"));
        if(! "".equals(wakeup_time)) {
        	show_wakeup_time.setText(String.format("System wake up at(base is 1970 years) : %s" ,wakeup_time)); 
        }else{
        	show_wakeup_time.setText(String.format("System wake up at : Disable"));         	
        }
        
    }

    //Listen for button clicks
    private void setListeners() {
        if (DEBUG) Log.d(LOG_TAG, "setListeners");        
    	button_save.setOnClickListener(saveconfig);
    	button_getalarm.setOnClickListener(getalarm);
    	button_disable.setOnClickListener(disablealarm);
    	button_cancel.setOnClickListener(cancelconfig);
    }

    private Button.OnClickListener saveconfig = new Button.OnClickListener()
    {
		@Override
		public void onClick(View v) {
			// TODO Auto-generated method stub
	        if (DEBUG) Log.d(LOG_TAG, "saveconfig");        

	        long _now = NowTime()/1000;
	        String set_time = new String(set_wakeup_time.getText().toString());
	        int _hour = 0;
	        int _minute = 0;
	        int _sec = 0;
	        long _set;
	        long value;
	        if (DEBUG) Log.d(LOG_TAG, "length() :" + set_time.length());        
 
	      if(set_time.length() == 6){
	        int index_set_time = set_time.indexOf(":");
	        if(index_set_time > 0 ){
		        _hour = Integer.valueOf(set_time.substring(0,2));
		        _minute = Integer.valueOf(set_time.substring(3,5));
		        _sec = Integer.valueOf(set_time.substring(6,8));
	        }else{
		        _hour = Integer.valueOf(set_time.substring(0,2));
		        _minute = Integer.valueOf(set_time.substring(2,4));
		        _sec = Integer.valueOf(set_time.substring(4,6));
	        }
	        _set = (long)(_hour * 3600 + _minute * 60 + _sec);
	        value = _set - _now;
			if(value > 86399){
				value = value-86399;
			}
	      }else{
	    	  value = Long.valueOf(set_time);
	      }
	    	wakeup_time = mcufunction("c");
			args[4] = mcu_addtime(wakeup_time, (int)value);
			wakeup_time = mcufunction("sa");

	    	if (DEBUG) Log.d(LOG_TAG, "wakeup_time :" + wakeup_time);
	    		  
	    	showwakeup();
	    }
	};
	
    private Button.OnClickListener getalarm = new Button.OnClickListener()
    {
		@Override
		public void onClick(View v) {
			// TODO Auto-generated method stub
	        if (DEBUG) Log.d(LOG_TAG, "getalarm");
	    	showwakeup();
		}
	};

    private Button.OnClickListener disablealarm = new Button.OnClickListener()
    {
		@Override
		public void onClick(View v) {
			// TODO Auto-generated method stub
	        if (DEBUG) Log.d(LOG_TAG, "disablealarm");
			args[4] = "00/00/00:00:00:00";
	    	wakeup_time = mcufunction("sa");
	    	if (DEBUG) Log.d(LOG_TAG, "wakeup_time :" + wakeup_time);
	    	showwakeup();
		}
	};

    private Button.OnClickListener cancelconfig = new Button.OnClickListener()
    {
		@Override
		public void onClick(View v) {
			// TODO Auto-generated method stub
	        if (DEBUG) Log.d(LOG_TAG, "cancelconfig");
	        finish();
		}
	};

	//Only caculate the secondary by day.
	private long NowTime() {
		// TODO Auto-generated method stub
        Time _TimeYMD1,_TimeYMD2;
        long _YMD;
        _TimeYMD1 = new Time();
        _TimeYMD2 = new Time();
        _TimeYMD1.setToNow();
        _TimeYMD2.set(_TimeYMD1.monthDay,_TimeYMD1.month,_TimeYMD1.year);
        _YMD = _TimeYMD2.toMillis(false);
        Log.d(LOG_TAG, "_TimeYMD1 : " + _TimeYMD1 + "_TimeYMD2 : " + _TimeYMD2 +"_YMD : " + _YMD);
        mSeconds = _TimeYMD1.toMillis(false) - _YMD;
        return mSeconds;
	}
	    
	private String mcufunction(String item) {
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
		 */
		// TODO Auto-generated method stub
		int item_value = 1;
		int index_start ;
		String get_string = new String() ;
		int index_end ;
		String get_mcu = new String();
		String START_KEYWORD = "Version:";
		String END_KEYWORD = "Version:";
		 
		args[1] = String.valueOf(edittext_mcu1.getText().toString());
    	args[2] = String.valueOf(edittext_mcu2.getText().toString());
    	args[3] = "-" + item;
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
    	} else if("sp".equals(item)){
       		item_value = 7;
    	} else if("p".equals(item)){
       		item_value = 8;
    	} else if("w".equals(item)){
       		item_value = 9;
    	} else if("u".equals(item)){
       		item_value = 10;
    	} else{
       		item_value = 1;
    	}
		String from_mcu = AAI_Tools.sSuCommand(args);
		switch(item_value){
		case 1:
			START_KEYWORD = "Version:";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			get_mcu = from_mcu.substring(index_start);
			break;
		case 2:
			return from_mcu;
		case 3:
		case 4:
			START_KEYWORD = "Time: ";
			END_KEYWORD = " -";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			get_string = from_mcu.substring(index_start);
			index_end = get_string.indexOf(END_KEYWORD);
			get_mcu = from_mcu.substring(index_start, index_start + index_end);
			break;
		case 5:
			return from_mcu;
		case 6:
			START_KEYWORD = "success";
			index_start = from_mcu.indexOf(START_KEYWORD);
			get_mcu = from_mcu.substring(index_start, index_start + 7);
			break;
		case 7:
			return from_mcu;
		case 8:
			START_KEYWORD = "Power Status:";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			get_mcu = from_mcu.substring(index_start, index_start+2);
			break;
		case 9:
			START_KEYWORD = "Who :";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			get_mcu = from_mcu.substring(index_start);
		break;
		case 10:
			return from_mcu;
        default:
			START_KEYWORD = "Version:";
			index_start = from_mcu.indexOf(START_KEYWORD) + START_KEYWORD.length();
			get_mcu = from_mcu.substring(index_start);
			break;
		}		
		
		 return get_mcu;
	}
	
	public static String mcu_addtime(String s, int n) {     
        if (DEBUG) Log.d(LOG_TAG, "mcu_addtime;clock time :" + s + "; add time :" + n); 
        try {   
		SimpleDateFormat sdf = new SimpleDateFormat("yy / MM / dd : HH : mm : ss");
		SimpleDateFormat ref = new SimpleDateFormat("yy/MM/dd:HH:mm:ss");
		sdf.setTimeZone(TimeZone.getTimeZone("GMT"));
		ref.setTimeZone(TimeZone.getTimeZone("GMT"));

        Calendar cd = Calendar.getInstance();
        cd.setTimeZone(TimeZone.getTimeZone("GMT"));
        cd.setTime(sdf.parse(s));

        //cd.add(Calendar.YEAR, n);
        //cd.add(Calendar.MONTH, n);//add/sub n_month months  
        //cd.add(Calendar.DATE, n);//add/sub n_date days   
        //cd.add(Calendar.HOUR, n);//add/sub n_hour hours   
        //cd.add(Calendar.MINUTE, n);//add/sub n_minute minutes   		  
        cd.add(Calendar.SECOND, n);
        return ref.format(cd.getTime());   
        } catch (Exception e) {   
        return "fail";   
        }
    }

	public static String timezone_check(String s) {     
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
	
    // Get preferences Head
    public static SharedPreferences getPref (Context ctxt) {
    	return settings = ctxt.getSharedPreferences(PREF, 0);
    }
}
