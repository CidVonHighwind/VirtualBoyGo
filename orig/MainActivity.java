package com.nintendont.virtualboygo;

import android.os.Bundle;
import android.util.Log;
import android.content.Intent;

import com.oculus.vrappframework.VrActivity;

import android.Manifest;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.BatteryManager;
import android.os.Environment;
import android.support.v4.content.ContextCompat;

import java.io.File;

public class MainActivity extends VrActivity {
    public static final String TAG = "VirtualBoyGo";

    /** Load jni .so on initialization */
    static {
        Log.d(TAG, "LoadLibrary");
        System.loadLibrary("ovrapp");
    }

    public static native long nativeSetAppInterface(VrActivity act, String fromPackageNameString,
                                                    String commandString, String uriString);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.d("MainActivity", "ask for permission");
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_CALENDAR)
                != PackageManager.PERMISSION_GRANTED) {
            // Permission is not granted
            Log.d("MainActivity", "permission granted");
        } else {
            Log.d("MainActivity", "permission not granted");
        }

        CreateFolder();

        Intent intent = getIntent();
        String commandString = VrActivity.getCommandStringFromIntent(intent);
        String fromPackageNameString = VrActivity.getPackageStringFromIntent(intent);
        String uriString = VrActivity.getUriStringFromIntent(intent);

        setAppPtr(nativeSetAppInterface(this, fromPackageNameString, commandString, uriString));
    }

    public void CreateFolder() {
        Log.d("MainActivity", "create folder");
        // states folder is needed to create state files
        String storageDir = Environment.getExternalStorageDirectory().toString() + "/Roms/VB/States/";
        File folder = new File(storageDir);
        if (!folder.exists()) {
            folder.mkdirs();
            Log.d("MainActivity", "created emulator directory");
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
