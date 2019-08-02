package com.example.receive_tools;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.util.Log;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.preference.CheckBoxPreference;
import java.util.StringTokenizer;

public class EthernetActivity {
    private final String LOG_TAG = "EthernetActivity";
    private static final boolean DEBUG = false;

    public EthernetActivity() {
      
    }
    
    private boolean isNumericAddress(String ipaddr) {
    	  
        //  Check if the string is valid
        
        if ( ipaddr == null || ipaddr.length() < 7 || ipaddr.length() > 15)
          return false;
          
        //  Check the address string, should be n.n.n.n format
        
        StringTokenizer token = new StringTokenizer(ipaddr,".");
        if ( token.countTokens() != 4)
          return false;

        while ( token.hasMoreTokens()) {
          
          //  Get the current token and convert to an integer value
          
          String ipNum = token.nextToken();
          
          try {
            int ipVal = Integer.valueOf(ipNum).intValue();
            if ( ipVal < 0 || ipVal > 255)
              return false;
          }
          catch (NumberFormatException ex) {
            return false;
          }
        }
        
        //  Looks like a valid IP address
        
        return true;
      }

}
