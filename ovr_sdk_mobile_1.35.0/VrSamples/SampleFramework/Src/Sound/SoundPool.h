/************************************************************************************

Filename    :   SoundPool.h
Content     :
Created     :
Authors     :

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "OVR_Types.h"

namespace OVRFW {

// Pooled sound player for playing sounds from the APK. Must be
// created/destroyed from the same thread.
class ovrSoundPool {
   public:
    ovrSoundPool(JNIEnv& jni_, jobject activity_);
    ~ovrSoundPool();

    // Not thread safe on Android.
    void Play(JNIEnv& env, const char* soundName);
    void Stop(JNIEnv& env, const char* soundName);
    void LoadSoundAsset(JNIEnv& env, const char* soundName);

   private:
    // private assignment operator to prevent copying
    ovrSoundPool& operator=(ovrSoundPool&);

   private:
    JNIEnv& jni;
    jobject pooler;
    jmethodID playMethod;
    jmethodID stopMethod;
    jmethodID preloadMethod;
    jmethodID releaseMethod;
};

} // namespace OVRFW
