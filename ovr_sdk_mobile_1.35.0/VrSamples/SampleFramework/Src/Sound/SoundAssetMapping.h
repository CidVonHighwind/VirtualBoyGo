/************************************************************************************

Filename    :   SoundAssetMapping.h
Content     :   Sound asset manager via json definitions
Created     :   October 22, 2013
Authors     :   Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

Description :

SoundAssetMapping is a simple sound asset management class which allows sound assets to be
easily replaced without recompilation.

SoundAssetMapping is driven by a JSON file in which sounds are mapped as key-value pairs,
where a value is the actual path to the wav file. For example:

    "sv_touch_active" : "sv_touch_active.wav"

These key-value pairs are defined in a json file: sound_assets.json.

Typically, we first load "res/raw/sound_assets.json" which references the wav files within
the VrAppFramework resource folder. In addition, we also load an app specific definition file
in the app: "assets/sound_assets.json". The app file may define app specific sounds placed next
to it in the assets folder. Additionally, it may also be used to override the VrAppFramework
sounds by either redefining the sound as an empty string to remove the sound or define it to
point at a new sound file - without the need to modify the code that actually plays the sound.

For development, SoundAssetMapping checks for a sound_assets.json within the Oculus folder
either on the internal or external sdcard. If this file is found, it is solely used to load
sounds.

*************************************************************************************/

#pragma once

#include <string>
#include <unordered_map>
#include "OVR_JSON.h"

namespace OVRFW {

class ovrSoundAssetMapping {
   public:
    ovrSoundAssetMapping() {}

    void LoadSoundAssets(class ovrFileSys* fileSy);
    bool HasSound(const char* soundName) const;
    bool GetSound(const char* soundName, std::string& outSound) const;

   private:
    void LoadSoundAssetsFromJsonObject(const std::string& url, std::shared_ptr<OVR::JSON> dataFile);
    void LoadSoundAssetsFromPackage(const std::string& url, const char* jsonFile);

    std::unordered_map<std::string, std::string>
        SoundMap; // Maps hashed sound name to sound asset URL
};

} // namespace OVRFW
