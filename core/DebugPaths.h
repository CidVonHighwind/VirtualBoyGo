#pragma once

#include <filesystem>
#include <string>

// _DEBUG-only: the repo's checked-in sample-ROM / writable-data folder
// (<repo>/sd/VB), used for zero-setup local testing (see RomScanner.cpp /
// Settings.cpp). Derived from this header's own build-time source path
// (__FILE__ is <repo>/core/DebugPaths.h, so two parents up is <repo>) so it
// follows the checkout wherever it lives instead of a hardcoded absolute
// path. Only meaningful for a debug build run on the machine it was built on;
// release builds use a plain relative path instead (see the callers).
inline std::string DebugSdVbDir()
{
    const std::filesystem::path repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path();
    return (repoRoot / "sd" / "VB").string();
}
