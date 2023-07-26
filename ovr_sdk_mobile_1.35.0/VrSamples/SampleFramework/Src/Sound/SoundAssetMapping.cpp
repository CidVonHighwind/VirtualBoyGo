/************************************************************************************

Filename    :   SoundAssetMapping.cpp
Content     :   Sound asset manager via json definitions
Created     :   October 22, 2013
Authors     :   Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#include "SoundAssetMapping.h"

#include "OVR_JSON.h"
#include "OVR_LogUtils.h"
#include "OVR_FileSys.h"

#include <cassert>

#include "PackageFiles.h"

#include "Misc/Log.h"

using OVR::JSON;

namespace OVRFW {

static const char* DEV_SOUNDS_RELATIVE = "Oculus/sound_assets.json";
static const char* VRLIB_SOUNDS = "res/raw/sound_assets.json";
static const char* APP_SOUNDS = "assets/sound_assets.json";

void ovrSoundAssetMapping::LoadSoundAssets(ovrFileSys* fileSys) {
    std::vector<std::string> searchPaths;
    searchPaths.push_back("/storage/extSdCard/"); // FIXME: This does not work for Android-M
    searchPaths.push_back("/sdcard/");

    // First look for sound definition using SearchPaths for dev
    std::string foundPath;
    if (GetFullPath(searchPaths, DEV_SOUNDS_RELATIVE, foundPath)) {
        std::shared_ptr<JSON> dataFile = JSON::Load(foundPath.c_str());
        if (dataFile == NULL) {
            ALOGE_FAIL(
                "ovrSoundAssetMapping::LoadSoundAssets failed to load JSON meta file: %s",
                foundPath.c_str());
        }
        foundPath =
            foundPath.substr(0, foundPath.length() - std::string("sound_assets.json").length());
        LoadSoundAssetsFromJsonObject(foundPath, dataFile);
    } else // if that fails, we are in release - load sounds from vrappframework/res/raw and the
           // assets folder
    {
        if (ovr_PackageFileExists(VRLIB_SOUNDS)) {
            LoadSoundAssetsFromPackage("res/raw/", VRLIB_SOUNDS);
        }
        if (ovr_PackageFileExists(APP_SOUNDS)) {
            LoadSoundAssetsFromPackage("", APP_SOUNDS);
        }
    }

    if (SoundMap.empty()) {
        ALOG("SoundManger - failed to load any sound definition files!");
    }
}

bool ovrSoundAssetMapping::HasSound(const char* soundName) const {
    auto soundMapping = SoundMap.find(soundName);
    return (soundMapping != SoundMap.end());
}

bool ovrSoundAssetMapping::GetSound(const char* soundName, std::string& outSound) const {
    auto soundMapping = SoundMap.find(soundName);
    if (soundMapping != SoundMap.end()) {
        outSound = soundMapping->second;
        return true;
    } else {
        ALOGW("ovrSoundAssetMapping::GetSound failed to find %s", soundName);
    }

    return false;
}

void ovrSoundAssetMapping::LoadSoundAssetsFromPackage(
    const std::string& url,
    const char* jsonFile) {
    int bufferLength = 0;
    void* buffer = NULL;
    ovr_ReadFileFromApplicationPackage(jsonFile, bufferLength, buffer);
    if (!buffer) {
        OVR_FAIL("ovrSoundAssetMapping::LoadSoundAssetsFromPackage failed to read %s", jsonFile);
    }

    auto dataFile = JSON::Parse(reinterpret_cast<char*>(buffer));
    if (!dataFile) {
        OVR_FAIL(
            "ovrSoundAssetMapping::LoadSoundAssetsFromPackage failed json parse on %s", jsonFile);
    }
    free(buffer);

    LoadSoundAssetsFromJsonObject(url, dataFile);
}

void ovrSoundAssetMapping::LoadSoundAssetsFromJsonObject(
    const std::string& url,
    std::shared_ptr<OVR::JSON> dataFile) {
    assert(dataFile);

    // Read in sounds - add to map
    auto sounds = dataFile->GetItemByName("Sounds");
    assert(sounds);

    const unsigned numSounds = sounds->GetItemCount();

    for (unsigned i = 0; i < numSounds; ++i) {
        auto sound = sounds->GetItemByIndex(i);
        assert(sound);

        std::string fullPath(url);
        fullPath += sound->GetStringValue().c_str();

        // Do we already have this sound?
        auto soundMapping = SoundMap.find(sound->Name);
        if (soundMapping != SoundMap.end()) {
            ALOG(
                "SoundManger - adding Duplicate sound %s with asset %s",
                sound->Name.c_str(),
                fullPath.c_str());
        } else // add new sound
        {
            ALOG("SoundManger read in: %s -> %s", sound->Name.c_str(), fullPath.c_str());
        }
        SoundMap[sound->Name] = fullPath;
    }
}

} // namespace OVRFW
