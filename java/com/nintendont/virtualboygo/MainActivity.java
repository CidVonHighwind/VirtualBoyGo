/************************************************************************************

 Filename    :   MainActivity.java
 Content     :
 Created     :
 Authors     :

 Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.
 *************************************************************************************/
package com.nintendont.virtualboygo;

import android.Manifest;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.Environment;
import android.provider.ContactsContract;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.content.Intent;

import com.oculus.vrappframework.VrActivity;

import android.support.v4.content.ContextCompat;

import java.io.File;

public class MainActivity extends VrActivity {
    public static final String TAG = "VirtualBoyGo";

    public String[] NeededPermissions = {Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE};
    public static final int MY_PERMISSIONS_REQUEST_READ_CONTACTS = 123987;

    /** Load jni .so on initialization */
    static {
        Log.d(TAG, "LoadLibrary");
        System.loadLibrary("ovrapp");
    }

    public static native long nativeSetAppInterface(VrActivity act, String fromPackageNameString, String commandString, String uriString);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d("MainActivity", "ask for permission");
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED) {
            // Permission is not granted
            Log.d("MainActivity", "permission granted");
            StartApp();
        } else {
            Log.d("MainActivity", "permission not granted");
            ActivityCompat.requestPermissions(this, NeededPermissions, MY_PERMISSIONS_REQUEST_READ_CONTACTS);
        }

        Intent intent = getIntent();
        String fromPackageNameString = VrActivity.getPackageStringFromIntent(intent);
        String commandString = VrActivity.getCommandStringFromIntent(intent);
        String uriString = VrActivity.getUriStringFromIntent(intent);

        setAppPtr(nativeSetAppInterface(this, fromPackageNameString, commandString, uriString));
    }

    public void StartApp() {
        CreateFolder();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        switch (requestCode) {
            case MY_PERMISSIONS_REQUEST_READ_CONTACTS: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    StartApp();
                } else {
                    // permission denied, boo! Disable the
                    // functionality that depends on this permission.
                }
                return;
            }
        }
    }

    public void CreateFolder() {
        Log.d("MainActivity", "create folder");
        // states folder is needed to create state files
        String storageDir = Environment.getExternalStorageDirectory().toString() + "/Roms/VB/States/";
        File folder = new File(storageDir);

        if (!folder.exists()) {
            folder.mkdirs();
            Log.d("MainActivity", "created emulator directory");
        } else {
            Log.d("MainActivity", "folder alread exists: " + storageDir);
        }
    }

    public int GetBatteryLevel() {
        Intent batteryIntent = registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));

        int level = batteryIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, 0);
        int scale = batteryIntent.getIntExtra(BatteryManager.EXTRA_SCALE, 100);

        return (int) ((level / (float) scale) * 100);
    }

    public String getExternalFilesDir() {
        return this.getExternalFilesDir(null).toString();
    }

    public String getInternalStorageDir() {
        return Environment.getExternalStorageDirectory().toString();
    }
}
