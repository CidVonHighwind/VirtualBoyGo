/************************************************************************************

Filename    :   SoundPool.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "SoundPool.h"
#include "JniUtils.h"

namespace OVRFW {

ovrSoundPool::ovrSoundPool(JNIEnv& jni_, jobject activity_) : jni(jni_) {
    jclass cls = ovr_GetLocalClassReference(&jni_, activity_, "com/oculus/sound/SoundPooler");

    jmethodID ctor = ovr_GetMethodID(&jni_, cls, "<init>", "(Landroid/content/Context;)V");
    jobject localPooler = jni_.NewObject(cls, ctor, activity_);
    pooler = jni_.NewGlobalRef(localPooler);
    jni_.DeleteLocalRef(localPooler);

    playMethod = ovr_GetMethodID(&jni_, cls, "play", "(Ljava/lang/String;)V");
    stopMethod = ovr_GetMethodID(&jni_, cls, "stop", "(Ljava/lang/String;)V");
    preloadMethod = ovr_GetMethodID(&jni_, cls, "loadSoundAsset", "(Ljava/lang/String;)V");
    releaseMethod = ovr_GetMethodID(&jni, cls, "release", "()V");
    jni_.DeleteLocalRef(cls);
}

ovrSoundPool::~ovrSoundPool() {
    jni.CallVoidMethod(pooler, releaseMethod);
    // TODO: check for exceptions?
    jni.DeleteGlobalRef(pooler);
}

void ovrSoundPool::Play(JNIEnv& env, const char* soundName) {
    jstring cmdString = (jstring)ovr_NewStringUTF(&env, soundName);
    env.CallVoidMethod(pooler, playMethod, cmdString);
    env.DeleteLocalRef(cmdString);
}

void ovrSoundPool::Stop(JNIEnv& env, const char* soundName) {
    jstring cmdString = (jstring)ovr_NewStringUTF(&env, soundName);
    env.CallVoidMethod(pooler, stopMethod, cmdString);
    env.DeleteLocalRef(cmdString);
}

void ovrSoundPool::LoadSoundAsset(JNIEnv& env, const char* soundName) {
    jstring cmdString = (jstring)ovr_NewStringUTF(&env, soundName);
    env.CallVoidMethod(pooler, preloadMethod, cmdString);
    env.DeleteLocalRef(cmdString);
}

} // namespace OVRFW
