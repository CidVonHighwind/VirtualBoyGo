#include "Settings.h"

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
// Same per-platform layout RomScanner.cpp's RomDirectory() uses (not
// exported from there, so duplicated here) - settings live next to the ROMs,
// the only writable-location precedent in this project.
std::string SettingsFilePath()
{
#if defined(__ANDROID__)
    return "/sdcard/VB/settings.dat";
#elif defined(_DEBUG)
    return "C:/Users/Patrick/Desktop/VirtualBoyGo Rework/VirtualBoyGo/sd/VB/settings.dat";
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
    if (!in || version != kVersion)
        return false; // missing/stale file - leave *this untouched, defaults stand

    AppSettings loaded;
    in.read(reinterpret_cast<char *>(&loaded), sizeof(AppSettings));
    if (!in)
        return false;

    *this = loaded;
    return true;
}
