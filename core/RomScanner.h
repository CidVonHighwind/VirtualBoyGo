#pragma once

#include <string>
#include <vector>

#if defined(__ANDROID__)
// Must be called once (from android_main) before any ScanRoms call - pass
// ANativeActivity::externalDataPath, the app's own external storage folder
// (e.g. /sdcard/Android/data/<package>/files). Unlike arbitrary SD-card
// paths, this directory needs no runtime permissions under scoped storage,
// on any Android version - the same reason most Quest homebrew apps use it
// for user-dropped content.
void SetAndroidRomDir(const char* path);
#endif

struct RomEntry
{
    std::string name;      // file name without extension, for display (e.g. "Golf (U) [!]")
    std::string fullPath;  // full path, for loading the ROM's bytes later
};

// Scans the platform's ROM directory for .vb files, sorted alphabetically
// (case-insensitive). Returns an empty vector if the directory doesn't
// exist or has no ROMs - callers should treat that the same as "no ROMs
// found", not an error.
//
// Directory resolved per-platform:
//  - Android: <externalDataPath>/VB (see SetAndroidRomDir).
//  - Windows debug builds: a fixed path into this repo's checked-in sample
//    ROMs (sd/VB), for zero-setup local testing.
//  - Windows release builds: a "VB" folder next to the executable, same
//    working-directory convention LoadAssetBytes already uses for assets.
std::vector<RomEntry> ScanRoms();
