// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.oculus.sdk.vrcinema;

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
import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.SurfaceTexture;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.Surface;
import java.io.File;
import java.io.IOException;

public class MainActivity extends android.app.NativeActivity
    implements ActivityCompat.OnRequestPermissionsResultCallback,
        MediaPlayer.OnVideoSizeChangedListener,
        MediaPlayer.OnCompletionListener,
        MediaPlayer.OnErrorListener,
        AudioManager.OnAudioFocusChangeListener {
  static {
    System.loadLibrary("vrapi");
    System.loadLibrary("vrcinema");
  }

  public static final String TAG = "VrCinema";

  public static native void nativeSetVideoSize(long appPtr, int width, int height);

  public static native SurfaceTexture nativePrepareNewVideo(long appPtr);

  public static native void nativeVideoCompletion(long appPtr);

  public static native long nativeSetAppInterface(android.app.NativeActivity act);

  SurfaceTexture movieTexture = null;
  Surface movieSurface = null;
  MediaPlayer mediaPlayer = null;
  AudioManager audioManager = null;
  Long appPtr = 0L;
  String movieFilePath = "";
  boolean externalStoragePermissionGranted = false;

  // Converts some thing like "/sdcard" to "/sdcard/", always ends with "/" to indicate folder path
  public static String toFolderPathFormat(String inStr) {
    if (inStr == null || inStr.length() == 0) {
      return "/";
    }

    if (inStr.charAt(inStr.length() - 1) != '/') {
      return inStr + "/";
    }

    return inStr;
  }

  public static String toAbsolutePath(String relativePath) {
    String[] rootPaths =
        new String[] {
          "",
          "/",
          "/storage/extSdCard/",
          toFolderPathFormat(Environment.getExternalStorageDirectory().getPath())
        };

    for (int i = 0; i < rootPaths.length; i++) {
      String path = rootPaths[i] + relativePath;
      File file = new File(path);
      if (file.exists()) {
        return path;
      }
    }
    return relativePath;
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    Log.d(TAG, "onCreate");

    super.onCreate(savedInstanceState);

    Intent intent = getIntent();
    String commandString = ""; // / TODO
    String fromPackageNameString = ""; // / TODO
    String uriString = ""; // / TODO

    appPtr = nativeSetAppInterface(this);

    // See if we can read a movie from sdcard ...
    if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
        != PackageManager.PERMISSION_GRANTED) {
      ActivityCompat.requestPermissions(
          this, new String[] {Manifest.permission.WRITE_EXTERNAL_STORAGE}, 0);
    } else {
      externalStoragePermissionGranted = true;
    }

    audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
  }

  @Override
  protected void onDestroy() {
    Log.d(TAG, "onDestroy");

    if (mediaPlayer != null) {
      mediaPlayer.release();
      mediaPlayer = null;
    }

    super.onDestroy();
  }

  @Override
  protected void onPause() {
    Log.d(TAG, "onPause()");
    if (mediaPlayer != null) {
      mediaPlayer.pause();
    }
    super.onPause();
  }

  @Override
  protected void onResume() {
    Log.d(TAG, "onResume()");
    if (mediaPlayer != null) {
      mediaPlayer.start();
    }
    super.onResume();
  }

  // called from native code for starting movie
  public void startMovieFromNative(final String pathName) {
    Log.d(TAG, "startMovieFromNative");

    // Save movie file
    movieFilePath = toAbsolutePath(pathName);
    startMovie();
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
              movieFilePath = toAbsolutePath(movieFilePath);
              startMovie();
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

  public void startMovie() {
    Log.v(TAG, "startMovie " + movieFilePath);

    if (externalStoragePermissionGranted) {
      runOnUiThread(
          new Thread() {
            @Override
            public void run() {
              Log.d(TAG, "startMovieFromNative");
              startMovieAfterPermissionGranted();
            }
          });
    } else {
      Log.v(TAG, "startMovie doesn't have permissions yet returning");
    }

    Log.v(TAG, "returning");
  }

  public void startMovieAfterPermissionGranted() {
    Log.v(TAG, "startMovieAfterPermissionGranted movieFilePath=" + movieFilePath);

    synchronized (this) {
      // Have native code pause any playing movie, allocate a new external texture, and create a
      // surfaceTexture with it.
      movieTexture = nativePrepareNewVideo(appPtr);
      if (movieTexture == null) {
        Log.w(TAG, "startMovieAfterPermissionGranted - could not create movieTexture ");
        return;
      }
      movieSurface = new Surface(movieTexture);

      if (mediaPlayer != null) {
        mediaPlayer.release();
      }

      Log.v(TAG, "MediaPlayer.create");

      synchronized (this) {
        mediaPlayer = new MediaPlayer();
      }
      mediaPlayer.setOnVideoSizeChangedListener(this);
      mediaPlayer.setOnCompletionListener(this);
      mediaPlayer.setSurface(movieSurface);

      try {
        Log.v(TAG, "mediaPlayer.setDataSource()");
        mediaPlayer.setDataSource(movieFilePath);
        try {
          Log.v(TAG, "mediaPlayer.prepare");
          mediaPlayer.prepare();
        } catch (IOException t) {
          Log.e(TAG, "mediaPlayer.prepare failed:" + t.getMessage());
        }
      } catch (IOException t) {
        Log.e(TAG, "mediaPlayer.setDataSource failed:" + t.getMessage());
      }
      Log.v(TAG, "mediaPlayer.start");

      /// Always restart movie from the beginning
      mediaPlayer.seekTo(0);
      mediaPlayer.setLooping(false);

      try {
        mediaPlayer.start();
      } catch (IllegalStateException ise) {
        Log.d(TAG, "mediaPlayer.start(): Caught illegalStateException: " + ise.toString());
      }

      mediaPlayer.setVolume(1.0f, 1.0f);
    }
  }

  /// MediaPlayer.OnErrorListener
  public boolean onError(MediaPlayer mp, int what, int extra) {
    Log.e(TAG, "MediaPlayer.OnErrorListener - what : " + what + ", extra : " + extra);
    return false;
  }

  /// MediaPlayer.OnVideoSizeChangedListener
  public void onVideoSizeChanged(MediaPlayer mp, int width, int height) {
    Log.v(TAG, String.format("onVideoSizeChanged: %dx%d", width, height));
    if (width == 0 || height == 0) {
      Log.e(
          TAG,
          "The video size is 0. Could be because there was no video, no display surface was set, or the value was not determined yet.");
    } else {
      nativeSetVideoSize(appPtr, width, height);
    }
  }

  /// MediaPlayer.OnCompletionListener
  public void onCompletion(MediaPlayer mp) {
    Log.v(TAG, String.format("onCompletion"));
    nativeVideoCompletion(appPtr);
  }

  /// AudioManager.OnAudioFocusChangeListener
  public void onAudioFocusChange(int focusChange) {
    switch (focusChange) {
      case AudioManager.AUDIOFOCUS_GAIN:
        // resume() if coming back from transient loss, raise stream volume if duck applied
        Log.d(TAG, "onAudioFocusChangedListener: AUDIOFOCUS_GAIN");
        break;
      case AudioManager.AUDIOFOCUS_LOSS: // focus lost permanently
        // stop() if isPlaying
        Log.d(TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS");
        break;
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT: // focus lost temporarily
        // pause() if isPlaying
        Log.d(TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS_TRANSIENT");
        break;
      case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK: // focus lost temporarily
        // lower stream volume
        Log.d(TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK");
        break;
      default:
        break;
    }
  }

  public void pauseMovie() {
    Log.d(TAG, "pauseMovie()");
    if (mediaPlayer != null) {
      try {
        mediaPlayer.pause();
      } catch (IllegalStateException t) {
        Log.e(TAG, "pauseMovie() caught illegalStateException");
      }
    }
  }

  public void resumeMovie() {
    Log.d(TAG, "resumeMovie()");
    if (mediaPlayer != null) {
      try {
        mediaPlayer.start();
      } catch (IllegalStateException t) {
        Log.e(TAG, "resumeMovie() caught illegalStateException");
      }
    }
  }
}
