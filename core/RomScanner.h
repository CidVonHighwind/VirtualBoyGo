#pragma once

#include <string>
#include <vector>

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
//  - Android: /sdcard/VB - a plain, easy-to-find path a user can drop files
//    into with any file manager or `adb push`, rather than the app's
//    sandboxed external-files folder. Requires the MANAGE_EXTERNAL_STORAGE
//    permission (see AndroidManifest.xml) since it's outside the app's own
//    scoped storage - the user has to grant "All files access" once via
//    Settings (or `adb shell appops set --uid <pkg> MANAGE_EXTERNAL_STORAGE
//    allow` for testing).
//  - Windows debug builds: a fixed path into this repo's checked-in sample
//    ROMs (sd/VB), for zero-setup local testing.
//  - Windows release builds: a "VB" folder next to the executable, same
//    working-directory convention LoadAssetBytes already uses for assets.
std::vector<RomEntry> ScanRoms();
