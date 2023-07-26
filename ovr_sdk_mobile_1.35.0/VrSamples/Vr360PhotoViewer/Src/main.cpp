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
#include "Vr360PhotoViewer.h"

extern "C" {

Vr360PhotoViewer* appPtr = nullptr;

long Java_com_oculus_sdk_vr360photoviewer_MainActivity_nativeSetAppInterface(
    JNIEnv* jni,
    jclass clazz,
    jobject activity) {
    ALOG("nativeSetAppInterface %p", appPtr);
    return reinterpret_cast<jlong>(appPtr);
}

void Java_com_oculus_sdk_vr360photoviewer_MainActivity_nativeReloadPhoto(
    JNIEnv* jni,
    jclass clazz,
    jlong interfacePtr) {
    ALOG("nativeReloadPhoto interfacePtr=%p appPtr=%p", interfacePtr, appPtr);
    if (appPtr && interfacePtr) {
        appPtr->ReloadPhoto();
    } else {
        ALOG("nativeReloadPhoto %p NULL ptr", appPtr);
    }
}

} // extern "C"

//==============================================================
// android_main
//==============================================================
void android_main(struct android_app* app) {
    appPtr = nullptr;
    std::unique_ptr<Vr360PhotoViewer> appl =
        std::unique_ptr<Vr360PhotoViewer>(new Vr360PhotoViewer(0, 0, 0, 0));
    appPtr = appl.get();
    appl->Run(app);
    appPtr = nullptr;
}
