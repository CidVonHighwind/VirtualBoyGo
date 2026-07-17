#pragma once

#include <string>
#include <vector>

struct RomEntry
{
    std::string name;     // file name without extension, for display
    std::string fullPath; // for loading the ROM later - a filesystem path, or a content:// URI on Android (see below)
};

// Scans for .vb ROMs, sorted case-insensitively. Empty vector if there's
// nowhere to look or no ROMs found.
//
// Android: fullPath is a content:// URI from the user's SAF-granted folder.
// Windows: sd/VB in debug, "VB" next to the executable in release.
std::vector<RomEntry> ScanRoms();

#if !defined(__ANDROID__) && defined(_DEBUG)
// The repo's checked-in sample-ROM / writable-data folder (<repo>/sd/VB),
// also used by Settings.cpp for the debug settings path.
std::string DebugSdVbDir();
#endif
