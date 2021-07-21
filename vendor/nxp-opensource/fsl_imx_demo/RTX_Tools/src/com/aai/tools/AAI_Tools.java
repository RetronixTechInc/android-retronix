package com.aai.tools;

import android.app.ListActivity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.ListView;
import android.widget.SimpleAdapter;

import java.text.Collator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.io.DataOutputStream;
import java.io.DataInputStream;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import android.os.SystemProperties;

public class AAI_Tools extends ListActivity {
    /** Called when the activity is first created. */
    private static final String LOG_TAG = "AAI";
    private static final boolean DEBUG = false;

	public static final String FileSDPath = "/sdcard/";
	public static final String FileEXTSDPath = "/mnt/extsd/";
	public static final String FileUSBPath = "/mnt/udisk/";

	// SU 參數
	private static Process psu = null;
	private static DataOutputStream suoutputStream = null;
	private static DataInputStream suinputStream = null;
	private static String sSuCmd = "ls\n";
	public static String sPackagecmd = null;
	private static final String SU_ROOT_PROP_NAME = "persist.sys.root_access";
	
	@Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (DEBUG) Log.d(LOG_TAG, "onCreate");        
        
        Intent intent = getIntent();
        String path = intent.getStringExtra("com.aai.tools.Path");
        
        if (path == null) {
            path = "";
        }

        setListAdapter(new SimpleAdapter(this, getData(path),
                android.R.layout.simple_list_item_1, new String[] { "title" },
                new int[] { android.R.id.text1 }));
        getListView().setTextFilterEnabled(true);
        
        //SystemProperties.set(SU_ROOT_PROP_NAME, "1");
		/* 得到Root權限 
		  try { 
		  	psu = Runtime.getRuntime().exec("su");
			suoutputStream = new DataOutputStream(psu.getOutputStream());
			suinputStream = new DataInputStream(psu.getInputStream());
		  } catch (IOException e) { 
		  	// TODO Auto-generated catch block e.printStackTrace(); 
		  }
		 */
	}
    
	public static String sSuCommand_test(String[] item) {
		// sSuCmd = "screencap /mnt/secure/1.png\n" ;
		sSuCmd = "";
		for (int i = 0 ; i < item.length ; i ++)
		{
			sSuCmd += item[i] + " ";
		}
		sSuCmd += "\n";

		if (suoutputStream != null) {
			try {
				suoutputStream.writeBytes(sSuCmd);
				suoutputStream.flush();
			} catch (IOException e) {
			}
		} else {
			if (DEBUG)
				Log.d(LOG_TAG, "====sSuCommand====suoutputStream=null");
		}
		
		return "retronix-test-string";
	}
	
    public static String sSuCommand(String[] item){
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
    	
    	return "retronix-test-string";
    }

   protected List<Map<String, Object>> getData(String prefix) {
        List<Map<String, Object>> myData = new ArrayList<Map<String, Object>>();

        Intent mainIntent = new Intent(Intent.ACTION_MAIN, null);
        mainIntent.addCategory(Intent.CATEGORY_SAMPLE_CODE);

        PackageManager pm = getPackageManager();
        List<ResolveInfo> list = pm.queryIntentActivities(mainIntent, 0);

        if (null == list)
            return myData;

        String[] prefixPath;
        String prefixWithSlash = prefix;
        
        if (prefix.equals("")) {
            prefixPath = null;
        } else {
            prefixPath = prefix.split("/");
            prefixWithSlash = prefix + "/";
        }
        
        int len = list.size();
        
        Map<String, Boolean> entries = new HashMap<String, Boolean>();

        for (int i = 0; i < len; i++) {
            ResolveInfo info = list.get(i);
            CharSequence labelSeq = info.loadLabel(pm);
            String label = labelSeq != null
                    ? labelSeq.toString()
                    : info.activityInfo.name;
            
            if (prefixWithSlash.length() == 0 || label.startsWith(prefixWithSlash)) {
                
                String[] labelPath = label.split("/");

                String nextLabel = prefixPath == null ? labelPath[0] : labelPath[prefixPath.length];

                if ((prefixPath != null ? prefixPath.length : 0) == labelPath.length - 1) {
                    addItem(myData, nextLabel, activityIntent(
                            info.activityInfo.applicationInfo.packageName,
                            info.activityInfo.name));
                } else {
                    if (entries.get(nextLabel) == null) {
                        addItem(myData, nextLabel, browseIntent(prefix.equals("") ? nextLabel : prefix + "/" + nextLabel));
                        entries.put(nextLabel, true);
                    }
                }
            }
        }

        Collections.sort(myData, sDisplayNameComparator);
        
        return myData;
    }

    private final static Comparator<Map<String, Object>> sDisplayNameComparator =
        new Comparator<Map<String, Object>>() {
        private final Collator   collator = Collator.getInstance();

        public int compare(Map<String, Object> map1, Map<String, Object> map2) {
            return collator.compare(map1.get("title"), map2.get("title"));
        }
    };

    protected Intent activityIntent(String pkg, String componentName) {
        Intent result = new Intent();
        result.setClassName(pkg, componentName);
        return result;
    }
    
    protected Intent browseIntent(String path) {
        Intent result = new Intent();
        result.setClass(this, AAI_Tools.class);
        result.putExtra("com.aai.tools.Path", path);
        return result;
    }

    protected void addItem(List<Map<String, Object>> data, String name, Intent intent) {
        Map<String, Object> temp = new HashMap<String, Object>();
        temp.put("title", name);
        temp.put("intent", intent);
        data.add(temp);
    }

    @Override
    @SuppressWarnings("unchecked")
    protected void onListItemClick(ListView l, View v, int position, long id) {
        Map<String, Object> map = (Map<String, Object>)l.getItemAtPosition(position);

        Intent intent = (Intent) map.get("intent");
        startActivity(intent);
    }
    
}