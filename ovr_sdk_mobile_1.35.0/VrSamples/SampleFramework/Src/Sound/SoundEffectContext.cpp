/************************************************************************************

Filename    :   SoundEffectContext.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "SoundEffectContext.h"

#include "JniUtils.h"

#include "OVR_FileSys.h"
#include "OVR_Std.h"

// Returns true if head equals check plus zero or more characters.
inline bool MatchesHead(const char* head, const char* check) {
    const int l = static_cast<int>(OVR::OVR_strlen(head));
    return 0 == OVR::OVR_strncmp(head, check, l);
}

namespace OVRFW {

const char* CMD_LOAD_SOUND_ASSET = "loadSoundAsset";

ovrSoundEffectContext::ovrSoundEffectContext(JNIEnv& jni_, jobject activity_)
    : SoundPool(jni_, activity_) {
    JavaVM* vm = NULL;
    jni_.GetJavaVM(&vm);
    Ttj.Init(*vm, *this);
}

ovrSoundEffectContext::~ovrSoundEffectContext() {}

void ovrSoundEffectContext::Initialize(ovrFileSys* fileSys) {
    SoundAssetMapping.LoadSoundAssets(fileSys);
}

void ovrSoundEffectContext::Play(const char* name) {
    Ttj.GetMessageQueue().PostPrintf("sound %s", name);
}

void ovrSoundEffectContext::Stop(const char* name) {
    Ttj.GetMessageQueue().PostPrintf("stopSound %s", name);
}

void ovrSoundEffectContext::LoadSoundAsset(const char* name) {
    Ttj.GetMessageQueue().PostPrintf("%s %s", CMD_LOAD_SOUND_ASSET, name);
}

void ovrSoundEffectContext::PlayInternal(JNIEnv& env, const char* name) {
    // Get sound from the asset mapping
    std::string soundFile;
    if (SoundAssetMapping.GetSound(name, soundFile)) {
        // OVR_LOG( "ovrSoundEffectContext::PlayInternal(%s) : %s", name, soundFile.c_str() );
        SoundPool.Play(env, soundFile.c_str());
    } else {
        OVR_WARN(
            "ovrSoundEffectContext::Play called with non-asset-mapping-defined sound: %s", name);
        SoundPool.Play(env, name);
    }
}

void ovrSoundEffectContext::StopInternal(JNIEnv& env, const char* name) {
    // Get sound from the asset mapping
    std::string soundFile;
    if (SoundAssetMapping.GetSound(name, soundFile)) {
        // OVR_LOG( "ovrSoundEffectContext::PlayInternal(%s) : %s", name, soundFile.c_str() );
        SoundPool.Stop(env, soundFile.c_str());
    } else {
        OVR_WARN(
            "ovrSoundEffectContext::Play called with non-asset-mapping-defined sound: %s", name);
        SoundPool.Stop(env, name);
    }
}

//==============================
// Allows for pre-loading of the sound asset file into memory on Android.
//==============================
void ovrSoundEffectContext::LoadSoundAssetInternal(JNIEnv& env, const char* name) {
    // Get sound from the asset mapping
    std::string soundFile;
    if (SoundAssetMapping.GetSound(name, soundFile)) {
        SoundPool.LoadSoundAsset(env, soundFile.c_str());
    } else {
        OVR_WARN(
            "ovrSoundEffectContext::LoadSoundAssetInternal called with non-asset-mapping-defined "
            "sound: %s",
            name);
        SoundPool.LoadSoundAsset(env, name);
    }
}

void ovrSoundEffectContext::TtjCommand(JNIEnv& jni, const char* commandString) {
    if (MatchesHead("sound ", commandString)) {
        PlayInternal(jni, commandString + 6);
        return;
    }
    if (MatchesHead("stopSound ", commandString)) {
        StopInternal(jni, commandString + 10);
        return;
    }
    if (MatchesHead("loadSoundAsset ", commandString)) {
        LoadSoundAssetInternal(jni, commandString + strlen(CMD_LOAD_SOUND_ASSET) + 1);
        return;
    }
}

} // namespace OVRFW
