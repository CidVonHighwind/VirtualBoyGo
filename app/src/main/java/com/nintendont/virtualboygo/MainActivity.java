// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.nintendont.virtualboygo;

import android.Manifest;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;

import java.io.File;

import static android.content.ContentValues.TAG;

/**
 * When using NativeActivity, we currently need to handle loading of dependent shared libraries
 * manually before a shared library that depends on them is loaded, since there is not currently a
 * way to specify a shared library dependency for NativeActivity via the manifest meta-data.
 *
 * <p>The simplest method for doing so is to subclass NativeActivity with an empty activity that
 * calls System.loadLibrary on the dependent libraries, which is unfortunate when the goal is to
 * write a pure native C/C++ only Android activity.
 *
 * <p>A native-code only solution is to load the dependent libraries dynamically using dlopen().
 * However, there are a few considerations, see:
 * https://groups.google.com/forum/#!msg/android-ndk/l2E2qh17Q6I/wj6s_6HSjaYJ
 *
 * <p>1. Only call dlopen() if you're sure it will succeed as the bionic dynamic linker will
 * remember if dlopen failed and will not re-try a dlopen on the same lib a second time.
 *
 * <p>2. Must remember what libraries have already been loaded to avoid infinitely looping when
 * libraries have circular dependencies.
 */
public class MainActivity extends android.app.NativeActivity
        implements ActivityCompat.OnRequestPermissionsResultCallback {
    static {
        System.loadLibrary("vrapi");
        System.loadLibrary("virtualboygo");
    }

    public String[] NeededPermissions = {Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE};
    public static final int PERMISSIONS_REQUEST_READ_WRITE_STORAGE = 789439745;

    public static native void nativeReloadRoms(long appPtr);

    public static native long nativeSetAppInterface(android.app.NativeActivity act);

    Long appPtr = 0L;

    boolean externalStoragePermissionGranted = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreateMainActivity");

        super.onCreate(null);

        appPtr = nativeSetAppInterface(this);

        // See if we can read a movie from sdcard ...
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED ||
                ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "requestPermission");
            ActivityCompat.requestPermissions(this, NeededPermissions, PERMISSIONS_REQUEST_READ_WRITE_STORAGE);

        } else {
            Log.d(TAG, "hasPermission");
            CreateFolder();
            externalStoragePermissionGranted = true;
        }
    }

    // ActivityCompat.OnRequestPermissionsResultCallback
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == PERMISSIONS_REQUEST_READ_WRITE_STORAGE && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "onRequestPermissionsResult permission GRANTED");
            CreateFolder();
            externalStoragePermissionGranted = true;
            runOnUiThread(
                    new Thread() {
                        @Override
                        public void run() {
                            nativeReloadRoms(appPtr);
                        }
                    });
        } else {
            Log.d(TAG, "onRequestPermissionsResult permission DENIED");
            externalStoragePermissionGranted = false;
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

    public String getExternalFilesDir() {
        return this.getExternalFilesDir(null).toString();
    }

    public String getInternalStorageDir() {
        return Environment.getExternalStorageDirectory().toString();
    }

    public int GetBatteryLevel() {
        Intent batteryIntent = registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));

        int level = batteryIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, 0);
        int scale = batteryIntent.getIntExtra(BatteryManager.EXTRA_SCALE, 100);

        return (int) ((level / (float) scale) * 100);
    }
}
