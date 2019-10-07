/*
 * Copyright (C) 2014-2016 Freescale Semiconductor, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


package com.freescale.cactusplayer;

import java.sql.Date;
import java.text.SimpleDateFormat;
import java.util.Locale;

import android.app.ActionBar;
import android.app.Activity;
import android.app.ProgressDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.media.MediaPlayer;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.GridView;
import android.widget.ImageButton;
import android.widget.PopupWindow;
import android.widget.SeekBar;
import android.widget.TextView;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.View.OnClickListener;
import android.view.Window;
import android.content.pm.PackageManager;
import android.Manifest;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;

public class VideoMenu extends Activity {
    private static final String TAG   = "CactusPlayer";
    private static final String CLASS = "VideoMenu: ";
    // cache 10000 items each page
    // TODO: add a button to navigate to next 10000 items
    private static final int MAX_ITEMS = 10000;

    private static final int REGISTER_DEVICE      = Menu.FIRST+1;
    private static final int DEREGISTER_DEVICE    = Menu.FIRST+2;

    private ImageTextAdapter mAdapter;
    private GridView mGridView;

    private TaskToScanVideoClips mVideoScanTask = null;

    private HdmiApplication.DemoPresentation mPresentation;
    private HdmiApplication mHdmiApp;
    private static final int Remote_control = Menu.FIRST+1;
    private static final int mPresentation_disabled = 0;
    private static final int mPresentation_playing = 1;
    private static final int mPresentation_pausing = 2;
	
	//Permission related member
	private int mNumPermissionsToRequest = 0;
	private boolean mShouldRequestStoragePermission = false;
	private boolean mFlagHasStoragePermission = true;
	private int mIndexPermissionRequestStorage = 0;
	private static final int PERMISSION_REQUEST_CODE = 0;
/*
    // Try to be smarter
    // Return EXTERNAL_STORAGE_DIRECTORY_SD if SD card is ready
    // Return EXTERNAL_STORAGE_DIRECTORY_UDISK if SD card is absent but udisk is ready
    // Return EXTERNAL_STORAGE_DIRECTORY_SD if both sd card and udisk are not ready
    String propSD = SystemProperties.get("EXTERNAL_STORAGE_STATE_SD", MEDIA_REMOVED);
    String propUDISK = SystemProperties.get("EXTERNAL_STORAGE_STATE_UDISK", MEDIA_REMOVED);
    String propEXTSD = SystemProperties.get("EXTERNAL_STORAGE_STATE_EXTSD", MEDIA_REMOVED);
    if (propSD.equals(MEDIA_MOUNTED) || propSD.equals(MEDIA_MOUNTED_READ_ONLY)) {
    return EXTERNAL_STORAGE_DIRECTORY_SD;
    } else if (propUDISK.equals(MEDIA_MOUNTED) || propUDISK.equals(MEDIA_MOUNTED_READ_ONLY)) {
    return EXTERNAL_STORAGE_DIRECTORY_UDISK;
    } else if (propEXTSD.equals(MEDIA_MOUNTED) || propEXTSD.equals(MEDIA_MOUNTED_READ_ONLY)) {
    return EXTERNAL_STORAGE_DIRECTORY_EXTSD;
    } else {
    return EXTERNAL_STORAGE_DIRECTORY_SD;
    }
    }

    Environment.getExternalStorageDirectory().getAbsolutePath()
*/

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, CLASS + "onCreate");
        super.onCreate(savedInstanceState);
		boolean isRestored = (savedInstanceState != null);
        //HDPlusPlayer.loadNativeLibs();
        //requestWindowFeature(Window.FEATURE_NO_TITLE);//hide label bar
        setContentView(R.layout.videomenu);
		checkPermission();
        ActionBar mActionBar = getActionBar();
        mActionBar.setHomeButtonEnabled(true);
        mActionBar.setDisplayHomeAsUpEnabled(true);
    }
	
	private void checkPermission(){

		if(checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
				!= PackageManager.PERMISSION_GRANTED){
			mNumPermissionsToRequest++;
			mShouldRequestStoragePermission  = true;
		}else{
			mFlagHasStoragePermission  = true;
		}

		String[] permissionToRequest = new String[mNumPermissionsToRequest];
		int permissionRequestIndex = 0;

		if(mShouldRequestStoragePermission){
			permissionToRequest[permissionRequestIndex] = Manifest.permission.WRITE_EXTERNAL_STORAGE;
			mIndexPermissionRequestStorage= permissionRequestIndex;
			permissionRequestIndex++;
		}

		if(permissionToRequest.length > 0){
			requestPermissions(permissionToRequest, PERMISSION_REQUEST_CODE);
		}else{
			initView();
		}
	}	

	@Override
	public void onRequestPermissionsResult(int requestCode,
			String permissions[], int[] grantResults) {
		switch (requestCode) {
		case PERMISSION_REQUEST_CODE:
			if (grantResults.length > 0
					&& grantResults[0] == PackageManager.PERMISSION_GRANTED) {
				Log.v(TAG, "Grant permission successfully");
				initView();
			} else {
				popupWarningDialog();
			}
			break;
		default:
			break;
		}
	}
	
	private void popupWarningDialog(){

		DialogInterface.OnClickListener dialogOnclicListener=new DialogInterface.OnClickListener(){

			@Override
			public void onClick(DialogInterface dialog, int which) {
				switch(which){
				case Dialog.BUTTON_POSITIVE:
					initView();
					break;
				case Dialog.BUTTON_NEGATIVE:
					VideoMenu.this.finish();
					break;
				default:
					break;
				}
			}
		};

		AlertDialog.Builder builder=new AlertDialog.Builder(this);
		builder.setTitle(R.string.Warning);
		builder.setMessage(R.string.PermissionNotGrant);
		builder.setPositiveButton(R.string.OK,dialogOnclicListener);
		builder.setNegativeButton(R.string.Cancel, dialogOnclicListener);
		builder.create().show();
	}	
	
	private void initView(){
        mAdapter = new ImageTextAdapter(this);
        mGridView = (GridView) findViewById(R.id.contentview);
        if(mGridView == null)
            return;
        mGridView.setAdapter(mAdapter);
        mGridView.setOnItemClickListener(new OnItemClickListener() {
            public void onItemClick(AdapterView<?> parent, View v, int position, long id) {
                ImageTextView itv = (ImageTextView)v;
                mAdapter.clickItem(itv);
                //if the presentation is running, restart a new one
                if((itv!=null)&&(!itv.isDir())){
                    Uri a = itv.get_myUri();
                    mHdmiApp = (HdmiApplication)getApplication();
                    mPresentation = mHdmiApp.getPresentation();
                    if(mPresentation != null){
                        if(mPresentation.getPresentationState() == mPresentation_playing ||
                            mPresentation.getPresentationState() == mPresentation_pausing){
                                mPresentation.setPresentationState(mPresentation_playing);
                                mPresentation.show();
                                mPresentation.startVideo(a);
                            }
                     }
                 }
             }
        });

        mVideoScanTask = new TaskToScanVideoClips();
        mVideoScanTask.Attach(this);
        mVideoScanTask.SetTask(0);
        try {
            mVideoScanTask.execute("");
        }
        catch(IllegalStateException e) {
            Log.d(TAG, CLASS + "Exception caught during executing scanning task");
        }
	}
	
    //--------------------------------remote control -------------------------------

    @Override
    public boolean onPrepareOptionsMenu (Menu menu) {
        MenuItem item = null;
        super.onPrepareOptionsMenu(menu);
        Log.d(TAG,"onPrepareOptionsMenu ");
        mHdmiApp = (HdmiApplication)getApplication();
        mPresentation = mHdmiApp.getPresentation();
        item = menu.findItem(Remote_control);
        if(mPresentation != null){
            int myPresentationState = mPresentation.getPresentationState();
            if(item != null && (myPresentationState == mPresentation_playing
                ||myPresentationState == mPresentation_pausing))
                    item.setEnabled(true);
            else
                item.setEnabled(false);
        }
        else
            item.setEnabled(false);
                return true;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        MenuItem item = menu.add(0, Remote_control, 0, getString(R.string.Remote_control));
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        boolean ret = super.onOptionsItemSelected(item);
        int itemId = item.getItemId();
        if(itemId == Remote_control){
            mHdmiApp = (HdmiApplication)getApplication();
            mPresentation = mHdmiApp.getPresentation();
            Uri uri1 = mPresentation.getmyUri();
            Intent intent;
            intent = new Intent(VideoMenu.this, VideoPlayer.class);
            intent.setData(uri1);
            startActivity(intent);  //send file name
        }
        else if(itemId == android.R.id.home){
            onBackPressed();
            return true;
        }
        return ret;
    }

    @Override
    protected void onRestart() {
        // TODO Auto-generated method stub
        super.onRestart();
    }

    @Override
    protected void onDestroy() {
        // TODO Auto-generated method stub
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        if(mAdapter.onBackPressed())
            return;
        super.onBackPressed(); // allows standard use of backbutton for page 1
    }

    public void refreshDialog(View parent) {
        LayoutInflater inflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        final View dialogView = inflater.inflate(R.layout.popupwindow, null ,false);
        dialogView.setBackgroundResource(R.drawable.rounded_corners_view);
        final PopupWindow pw = new PopupWindow(dialogView, 480, LayoutParams.WRAP_CONTENT, true);
        pw.setBackgroundDrawable(getResources().getDrawable(R.drawable.rounded_corners_pop));

        Button btnOK     = (Button)   dialogView.findViewById(R.id.BtnOK);
        btnOK.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                pw.dismiss();
            }
        });
        pw.showAtLocation(parent, Gravity.CENTER, 0, 0);
    }

    // start a task to list all avi/mkv video clips
    private class TaskToScanVideoClips extends AsyncTask<String, Integer, Integer> {
        private ProgressDialog mDialog = null;
        private Activity mContext      = null;
        private int mOffset            = 0;
        private ItemData[] mData       = new ItemData[VideoMenu.MAX_ITEMS];
        private int mCount             = 0;

        private class CancelListener implements DialogInterface.OnClickListener, DialogInterface.OnCancelListener {
            public void onClick(DialogInterface dialog, int which) {
            }

            public void onCancel(DialogInterface dialog) {
                cancel(false);
            }
        }

        private CancelListener mCancelListener = new CancelListener();


        //-----------------------------------------------------------------------------------------
        // Attach/detach to/from activity
        //-----------------------------------------------------------------------------------------
        public void Attach(Activity act) {
            mContext   = act;
            if(mDialog != null && mContext != null){
                mDialog.setOwnerActivity(mContext);
                mDialog.show();
            }
        }

        public void Detach() {
            if(mDialog != null){
                mDialog.dismiss();
            }
            mContext   = null;
        }

        //-----------------------------------------------------------------------------------------
        // parameters for the task
        //-----------------------------------------------------------------------------------------
        public void SetTask(int offset) {
            if(offset < 0)
                mOffset = 0;
            else
                mOffset = offset;
        }

        //-----------------------------------------------------------------------------------------
        // overrides
        //-----------------------------------------------------------------------------------------
        @Override
        protected void onPreExecute() {
            createDialog();
        }

        /** The system calls this to perform work in a worker thread and
        * delivers it the parameters given to AsyncTask.execute() */
        protected Integer doInBackground(String... sUrl) {
        int count = 0; // counter for mkv/avi clip

        if(OpenAudioDB() == -1)
            return -1;

        if(OpenVideoDB() == -1){
            CloseAudioDB();
            return -1;
        }

        while(true) {
            String info[] = new String[11];
            int ret = ReadAudioRecord(info);
            if(ret == -1){
                ret = ReadVideoRecord(info);
                if(ret == -1)
                    break;
            }

            // filter avi/mkv
            String mime = info[3];
            Log.d(TAG,"title is " + info[0] + " mime is " + mime);
            if(mime != null && (
                    mime.equals("video/mpeg4") ||
                    mime.equals("video/mp4") ||
                    mime.equals("video/3gp") ||
                    mime.equals("video/3gpp") ||
                    mime.equals("video/3gpp2") ||
                    mime.equals("video/webm") ||
                    mime.equals("video/avi") ||
                    mime.equals("video/x-flv") ||
                    mime.equals("video/x-msvideo") ||
                    mime.equals("video/x-ms-asf") ||
                    mime.equals("video/x-ms-wmv") ||
                    mime.equals("video/x-mpeg") ||
                    mime.equals("video_unsupport/avi") ||
                    mime.equals("video/matroska") ||
                    mime.equals("video/x-matroska") ||
                    mime.equals("video_unsupport/matroska") ||
                    mime.equals("video/mp2p") ||
                    mime.equals("video/mp2ts") ||
                    mime.equals("video/rmff") ||
                    mime.equals("video/flv") ||
                    mime.startsWith("audio/") )) {
                // skip to offset
                if(count >= mOffset) {
                    mData[mCount] = new ItemData();
                    mData[mCount].mTitle    = info[0];
                    mData[mCount].mName     = info[1];
                    mData[mCount].mPath     = info[2];
                    mData[mCount].mMime     = info[3];
                    mData[mCount].mDuration = Long.parseLong(info[4]);
                    if(mime.startsWith("video/"))
                        mData[mCount].mArt      = info[9];
                    mData[mCount].mBucket   = info[10];
                    // bucket name should not be null
                    if(mData[mCount].mBucket == null)
                        mData[mCount].mBucket = "General";

                    mCount++;

                    if(mCount == VideoMenu.MAX_ITEMS)
                        break;
                 }
                 count++;
            }
        }

        CloseAudioDB();
        CloseVideoDB();

        return 0;
    }

    @Override
    protected void onCancelled() {
        destroyDialog();
    }

    @Override
    protected void onPostExecute(Integer result) {
        destroyDialog();
        if(mContext != null) {
            ((VideoMenu)mContext).mAdapter.setData(mData, mCount);
            if(mCount == 0)
                refreshDialog(findViewById(R.id.parentview));
        }
    }

    //-----------------------------------------------------------------------------------------
    // private functions
    //-----------------------------------------------------------------------------------------
    private void createDialog() {
        mDialog = null;

        // dialog needs activity
        if(mContext != null){
            mDialog = new ProgressDialog(mContext);
            mDialog.setTitle(mContext.getString(R.string.Scan));
            mDialog.setMessage("Scanning clips");
            mDialog.setCancelable(true);
            mDialog.setOnCancelListener(mCancelListener);
            mDialog.show();
        }
    }

    private void destroyDialog() {
        if(mDialog != null){
            mDialog.dismiss();
            mDialog = null;
        }
    }

    private Cursor mVideoCursor;
    private Cursor mAudioCursor;
    private int	  mFirstVideoRecord = 1;
    private int	  mFirstAudioRecord = 1;
    private ContentResolver contentResolver;

    public int OpenAudioDB() {
        if(mAudioCursor != null || mContext == null)
            return -1; // previous not closed

        contentResolver = mContext.getContentResolver();

        // _data:         [MediaStore.MediaColumns.DATA, DATA STREAM] path name
        // _display_name: [MediaStore.MediaColumns.DISPLAY_NAME, TEXT] file name
        // _size:         [MediaStore.MediaColumns.SIZE, long] file size
        // mine_type:     [MediaStore.MediaColumns.MIME_TYPE, TEXT] e.g. audio/mepg
        // title:         [MediaStore.MediaColumns.TITLE, TEXT]
        // duration:      [MediaStore.Video.VideoColumns.DURATION, long] in ms
        // artist:        [MediaStore.Video.VideoColumns.ARTIST, TEXT]
        // album:         [MediaStore.Video.VideoColumns.ALBUM, TEXT]
        // datetaken:     [MediaStore.Video.VideoColumns.DATE_TAKEN, long] The date & time that the image was taken in units of milliseconds since jan 1, 1970.
        // resolution:    [MediaStore.Video.VideoColumns.RESOLUTION, TEXT] The resolution of the video file, formatted as "XxY".
        // _id:           [MediaStore.Video.Media._ID, long]
        String [] columns = {
            MediaStore.MediaColumns.DATA,// 0
            MediaStore.MediaColumns.DISPLAY_NAME,// 1
            MediaStore.MediaColumns.SIZE,// 2
            MediaStore.MediaColumns.MIME_TYPE,// 3
            MediaStore.MediaColumns.TITLE,// 4
            MediaStore.Audio.AudioColumns.DURATION, // 5
            MediaStore.Audio.AudioColumns.ARTIST,// 6
            MediaStore.Audio.AudioColumns.ALBUM,// 7
                /*
            MediaStore.Video.VideoColumns.DATE_TAKEN,// 8
            MediaStore.Video.VideoColumns.RESOLUTION,// 9
            MediaStore.Video.VideoColumns._ID,// 10
            MediaStore.Video.VideoColumns.BUCKET_DISPLAY_NAME// 11
  */      };

        // TODO: use where clause
        mAudioCursor = contentResolver.query(// also can use managedQuery
            MediaStore.Audio.Media.EXTERNAL_CONTENT_URI,
            columns,
            null, // where
            null, // args for where
            MediaStore.MediaColumns.DISPLAY_NAME + " ASC"); // order

        if(mAudioCursor != null) {
            mFirstAudioRecord = 1;
        }

        return 0;
    }

    public int OpenVideoDB() {
        if(mVideoCursor != null || mContext == null)
            return -1; // previous not closed

        contentResolver = mContext.getContentResolver();

        // _data:         [MediaStore.MediaColumns.DATA, DATA STREAM] path name
        // _display_name: [MediaStore.MediaColumns.DISPLAY_NAME, TEXT] file name
        // _size:         [MediaStore.MediaColumns.SIZE, long] file size
        // mine_type:     [MediaStore.MediaColumns.MIME_TYPE, TEXT] e.g. audio/mepg
        // title:         [MediaStore.MediaColumns.TITLE, TEXT]
        // duration:      [MediaStore.Video.VideoColumns.DURATION, long] in ms
        // artist:        [MediaStore.Video.VideoColumns.ARTIST, TEXT]
        // album:         [MediaStore.Video.VideoColumns.ALBUM, TEXT]
        // datetaken:     [MediaStore.Video.VideoColumns.DATE_TAKEN, long] The date & time that the image was taken in units of milliseconds since jan 1, 1970.
        // resolution:    [MediaStore.Video.VideoColumns.RESOLUTION, TEXT] The resolution of the video file, formatted as "XxY".
        // _id:           [MediaStore.Video.Media._ID, long]
        String [] columns = {
            MediaStore.MediaColumns.DATA,// 0
            MediaStore.MediaColumns.DISPLAY_NAME,// 1
            MediaStore.MediaColumns.SIZE,// 2
            MediaStore.MediaColumns.MIME_TYPE,// 3
            MediaStore.MediaColumns.TITLE,// 4
            MediaStore.Video.VideoColumns.DURATION, // 5
            MediaStore.Video.VideoColumns.ARTIST,// 6
            MediaStore.Video.VideoColumns.ALBUM,// 7
            MediaStore.Video.VideoColumns.DATE_TAKEN,// 8
            MediaStore.Video.VideoColumns.RESOLUTION,// 9
            MediaStore.Video.VideoColumns._ID,// 10
            MediaStore.Video.VideoColumns.BUCKET_DISPLAY_NAME// 11
        };

        // TODO: use where clause
        mVideoCursor = contentResolver.query(// also can use managedQuery
            MediaStore.Video.Media.EXTERNAL_CONTENT_URI,
            columns,
            null, // where
            null, // args for where
            MediaStore.MediaColumns.DISPLAY_NAME + " ASC"); // order

        if(mVideoCursor != null) {
            mFirstVideoRecord = 1;
        }

        return 0;
    }

    public void CloseVideoDB() {
        if(mVideoCursor != null) {
            mVideoCursor.close();
            mVideoCursor = null;
        }
    }

    public void CloseAudioDB() {
        if(mAudioCursor != null) {
            mAudioCursor.close();
            mAudioCursor = null;
        }
    }

    public int ReadAudioRecord(String [] info) {
        if(mAudioCursor == null) {
            return -1;
        }

        boolean toNext;
        if(mFirstAudioRecord == 1) {
            toNext = mAudioCursor.moveToFirst();
            mFirstAudioRecord = 0;
        }
        else {
            toNext = mAudioCursor.moveToNext();
        }

        if(toNext == false) {
            return -1;
        }
/*
        long vidoId = mAudioCursor.getLong(10);
        String art = GetVideoArt(vidoId);

        long millis = mAudioCursor.getLong(8);
        Date date = new Date(millis);
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
*/
        info[0] = mAudioCursor.getString(4);  // title
        info[1] = mAudioCursor.getString(1);  // display name
        info[2] = mAudioCursor.getString(0);  // path
        info[3] = mAudioCursor.getString(3);  // mime
        info[4] = Long.toString(mAudioCursor.getLong(5)); // duration
        info[5] = mAudioCursor.getString(6);  // artist
        info[6] = mAudioCursor.getString(7);  // album
  //      info[7] = mAudioCursor.getString(9);  // resolution
  //      info[8] = sdf.format(date); // date
  //      info[9] = art; // art
        info[10] = mAudioCursor.getString(3);  // default : use mine as bucket
        int p1 = info[2].lastIndexOf('/');
        if(p1 > 0){
            int p2 = info[2].lastIndexOf('/', p1 - 1);
            if(p2 != -1)
                info[10] = info[2].substring(p2 + 1, p1);
        }
            

        //Log.d(VideoMenu.TAG, VideoMenu.CLASS + info[0]+"," + info[1]+"," + info[2]+"," + info[3]+"," + info[4]+"," + info[5]+"," + info[6]);

        return 0;
    }

    // info size: at least 11
    public int ReadVideoRecord(String [] info) {
        if(mVideoCursor == null) {
            return -1;
        }

        boolean toNext;
        if(mFirstVideoRecord == 1) {
            toNext = mVideoCursor.moveToFirst();
            mFirstVideoRecord = 0;
        }
        else {
            toNext = mVideoCursor.moveToNext();
        }

        if(toNext == false) {
            return -1;
        }

        long vidoId = mVideoCursor.getLong(10);
        String art = GetVideoArt(vidoId);

        long millis = mVideoCursor.getLong(8);
        Date date = new Date(millis);
        SimpleDateFormat sdf = new SimpleDateFormat("yyyy-MM-dd", Locale.US);

        info[0] = mVideoCursor.getString(4);  // title
        info[1] = mVideoCursor.getString(1);  // display name
        info[2] = mVideoCursor.getString(0);  // path
        info[3] = mVideoCursor.getString(3);  // mime
        info[4] = Long.toString(mVideoCursor.getLong(5)); // duration
        info[5] = mVideoCursor.getString(6);  // artist
        info[6] = mVideoCursor.getString(7);  // album
        info[7] = mVideoCursor.getString(9);  // resolution
        info[8] = sdf.format(date); // date
        info[9] = art; // art
        info[10] = mVideoCursor.getString(11);  // bucket

        //Log.d(VideoMenu.TAG, VideoMenu.CLASS + info[0] + info[1] + info[2] + info[3] + info[4] + info[5] + info[6] + info[7] + info[8] + info[9]);

        return 0;
    }

    private String GetVideoArt(long videoId) {
        String [] columns = { MediaStore.Video.Thumbnails.DATA };
        String art = null;

        Cursor cursor = contentResolver.query(
            MediaStore.Video.Thumbnails.EXTERNAL_CONTENT_URI,
            columns,
            MediaStore.Video.Thumbnails.VIDEO_ID + " = '" + videoId + "'",
            null,
            null);

        if(cursor != null) {
            if(cursor.moveToFirst()) {
                do {
                    art = cursor.getString(0); // first column
                    if(art != null) {
                        break;
                    }
                } while(cursor.moveToNext());
            }

            cursor.close();
        }

        return art;
    }

 }
}
