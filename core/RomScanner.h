#pragma once

#include <string>
#include <vector>

struct RomEntry
{
    std::string name;      // file name without extension, for display (e.g. "Golf (U) [!]")
    std::string fullPath;  // for loading the ROM later - a filesystem path, or a content:// URI on Android (see below)
};

// Scans for .vb ROMs, sorted case-insensitively. Empty vector if there's
// nowhere to look or no ROMs - callers treat both as "no ROMs found".
//
// Per-platform:
//  - Android: fullPath is a content:// document URI (see AndroidRomAccess.h).
//    The user picks a folder once via the Storage Access Framework; the grant
//    persists and needs no runtime permission. SAF is used instead of a plain
//    /sdcard path because at target SDK 34 that needs MANAGE_EXTERNAL_STORAGE,
//    which kills and restarts the process the moment it's granted.
//  - Windows debug: fixed path into this repo's sample ROMs (sd/VB).
//  - Windows release: a "VB" folder next to the executable.
std::vector<RomEntry> ScanRoms();
