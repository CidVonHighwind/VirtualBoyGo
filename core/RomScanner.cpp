#include "RomScanner.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace
{
#if defined(__ANDROID__)
std::string g_androidRomDir;
#endif

std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string RomDirectory()
{
#if defined(__ANDROID__)
    return g_androidRomDir.empty() ? std::string() : g_androidRomDir + "/VB";
#elif defined(_DEBUG)
    // Fixed path to this repo's checked-in sample ROMs - zero-setup local
    // testing, at the cost of only working on this machine/checkout. Release
    // builds use the relative path below instead.
    return "C:/Users/Patrick/Desktop/VirtualBoyGo Rework/VirtualBoyGo/sd/VB";
#else
    // Relative to the working directory, same convention LoadAssetBytes
    // uses for assets (see AssetLoader.h) - the exe's own folder for how
    // this app is packaged/run.
    return "VB";
#endif
}
} // namespace

#if defined(__ANDROID__)
void SetAndroidRomDir(const char* path) { g_androidRomDir = path ? path : ""; }
#endif

std::vector<RomEntry> ScanRoms()
{
    std::vector<RomEntry> roms;

    const std::string dir = RomDirectory();
    if (dir.empty())
        return roms;

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec)
        return roms;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec)
            break;
        if (!entry.is_regular_file())
            continue;

        const std::filesystem::path& path = entry.path();
        if (ToLower(path.extension().string()) != ".vb")
            continue;

        roms.push_back({path.stem().string(), path.string()});
    }

    std::sort(roms.begin(), roms.end(),
              [](const RomEntry& a, const RomEntry& b) { return ToLower(a.name) < ToLower(b.name); });

    return roms;
}
