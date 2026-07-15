#include "Settings.h"

#if defined(_DEBUG) && !defined(__ANDROID__)
#include "DebugPaths.h"
#endif

#include <filesystem>
#include <fstream>
#include <string>

const XrColor4f kPredefColors[11] = {
    {1.0f, 0.0f, 0.0f, 1.0f},
    {0.9f, 0.3f, 0.1f, 1.0f},
    {1.0f, 0.85f, 0.1f, 1.0f},
    {0.25f, 1.0f, 0.1f, 1.0f},
    {0.0f, 1.0f, 0.45f, 1.0f},
    {0.0f, 1.0f, 0.85f, 1.0f},
    {0.0f, 0.85f, 1.0f, 1.0f},
    {0.15f, 1.0f, 1.0f, 1.0f},
    {0.75f, 0.65f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 0.3f, 0.2f, 1.0f},
};

namespace
{
// Settings live next to the ROMs (the only writable-location precedent in
// this project), so this mirrors RomScanner.cpp's RomDirectory() per-platform
// layout - the shared _DEBUG folder comes from DebugPaths.h so it isn't
// duplicated as a literal path.
std::string SettingsFilePath()
{
#if defined(__ANDROID__)
    return "/sdcard/VB/settings.dat";
#elif defined(_DEBUG)
    return DebugSdVbDir() + "/settings.dat";
#else
    return "VB/settings.dat";
#endif
}
} // namespace

void AppSettings::Save() const
{
    const std::string path = SettingsFilePath();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return;

    const int version = kVersion;
    out.write(reinterpret_cast<const char *>(&version), sizeof(version));
    out.write(reinterpret_cast<const char *>(this), sizeof(AppSettings));
}

bool AppSettings::Load()
{
    std::ifstream in(SettingsFilePath(), std::ios::binary);
    if (!in)
        return false;

    int version = 0;
    in.read(reinterpret_cast<char *>(&version), sizeof(version));
    constexpr int kPreviousScaleBaselineVersion = 7;
    if (!in || (version != kVersion && version != kPreviousScaleBaselineVersion))
        return false; // missing/stale file - leave *this untouched, defaults stand

    AppSettings loaded;
    in.read(reinterpret_cast<char *>(&loaded), sizeof(AppSettings));
    if (!in)
        return false;

    // Version 8 defines 1.0x as the old 1.4x physical size. Convert the
    // persisted multiplier so existing users keep exactly the same apparent
    // screen size after the baseline changes.
    if (version == kPreviousScaleBaselineVersion)
        loaded.screenScale /= 1.4f;

    *this = loaded;
    return true;
}
