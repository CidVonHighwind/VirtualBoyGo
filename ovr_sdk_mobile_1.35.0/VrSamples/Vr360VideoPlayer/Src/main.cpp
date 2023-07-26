/*******************************************************************************

Filename    :   Main.cpp
Content     :   Base project for mobile VR samples
Created     :   February 21, 2018
Authors     :   John Carmack, J.M.P. van Waveren, Jonathan Wright
Language    :   C++

Copyright:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include "Platform/Android/Android.h"

#include <android/window.h>
#include <android/native_window_jni.h>
#include <android_native_app_glue.h>

#include <memory>

#include "Appl.h"
#include "Vr360VideoPlayer.h"

extern "C" {

Vr360VideoPlayer* appPtr = nullptr;

long Java_com_oculus_sdk_vr360videoplayer_MainActivity_nativeSetAppInterface(
    JNIEnv* jni,
    jclass clazz,
    jobject activity) {
    ALOG("nativeSetAppInterface %p", appPtr);
    return reinterpret_cast<jlong>(appPtr);
}

void Java_com_oculus_sdk_vr360videoplayer_MainActivity_nativeSetVideoSize(
    JNIEnv* jni,
    jclass clazz,
    jlong interfacePtr,
    int width,
    int height) {
    ALOG("nativeSetVideoSize interfacePtr=%p appPtr=%p", interfacePtr, appPtr);
    Vr360VideoPlayer* videoPlayer = appPtr;
    if (videoPlayer && interfacePtr) {
        videoPlayer->SetVideoSize(width, height);
    } else {
        ALOG("nativeSetVideoSize %p NULL ptr", videoPlayer);
    }
}

jobject Java_com_oculus_sdk_vr360videoplayer_MainActivity_nativePrepareNewVideo(
    JNIEnv* jni,
    jclass clazz,
    jlong interfacePtr) {
    ALOG("nativePrepareNewVideo interfacePtr=%p appPtr=%p", interfacePtr, appPtr);
    jobject surfaceTexture = nullptr;
    Vr360VideoPlayer* videoPlayer = appPtr;
    if (videoPlayer && interfacePtr) {
        videoPlayer->GetScreenSurface(surfaceTexture);
    } else {
        ALOG("nativePrepareNewVideo %p NULL ptr", videoPlayer);
    }
    return surfaceTexture;
}

void Java_com_oculus_sdk_vr360videoplayer_MainActivity_nativeVideoCompletion(
    JNIEnv* jni,
    jclass clazz,
    jlong interfacePtr) {
    ALOG("nativeVideoCompletion interfacePtr=%p appPtr=%p", interfacePtr, appPtr);
    Vr360VideoPlayer* videoPlayer = appPtr;
    if (videoPlayer && interfacePtr) {
        videoPlayer->VideoEnded();
    } else {
        ALOG("nativeVideoCompletion %p NULL ptr", videoPlayer);
    }
}

} // extern "C"

//==============================================================
// android_main
//==============================================================
void android_main(struct android_app* app) {
    appPtr = nullptr;
    std::unique_ptr<Vr360VideoPlayer> appl =
        std::unique_ptr<Vr360VideoPlayer>(new Vr360VideoPlayer(0, 0, 0, 0));
    appPtr = appl.get();
    appl->Run(app);
    appPtr = nullptr;
}
