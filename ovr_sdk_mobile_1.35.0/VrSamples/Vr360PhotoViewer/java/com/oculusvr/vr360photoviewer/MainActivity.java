// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.oculus.sdk.vr360photoviewer;

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
 * <p>2. Must rememeber what libraries have already been loaded to avoid infinitely looping when
 * libraries have circular dependencies.
 */
import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;

public class MainActivity extends android.app.NativeActivity
    implements ActivityCompat.OnRequestPermissionsResultCallback {
  static {
    System.loadLibrary("vrapi");
    System.loadLibrary("vr360photoviewer");
  }

  public static final String TAG = "Vr360PhotoViewer";

  public static native void nativeReloadPhoto(long appPtr);

  public static native long nativeSetAppInterface(android.app.NativeActivity act);

  Long appPtr = 0L;
  boolean externalStoragePermissionGranted = false;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    Log.d(TAG, "onCreate");

    super.onCreate(savedInstanceState);

    appPtr = nativeSetAppInterface(this);

    // See if we can read a movie from sdcard ...
    if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
        != PackageManager.PERMISSION_GRANTED) {
      ActivityCompat.requestPermissions(
          this, new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE}, 0);
    } else {
      externalStoragePermissionGranted = true;
    }
  }

  // ActivityCompat.OnRequestPermissionsResultCallback
  @Override
  public void onRequestPermissionsResult(
      int requestCode, String[] permissions, int[] grantResults) {
    for (int i = 0; i < permissions.length; i++) {
      switch (permissions[i]) {
        case Manifest.permission.WRITE_EXTERNAL_STORAGE:
          {
            if (grantResults[i] == PackageManager.PERMISSION_GRANTED) {
              Log.d(TAG, "onRequestPermissionsResult permission GRANTED");
              externalStoragePermissionGranted = true;
              runOnUiThread(
                  new Thread() {
                    @Override
                    public void run() {
                      nativeReloadPhoto(appPtr);
                    }
                  });
            } else {
              Log.d(TAG, "onRequestPermissionsResult permission DENIED");
              externalStoragePermissionGranted = false;
            }
            return;
          }

        default:
          break;
      }
    }
  }
}
