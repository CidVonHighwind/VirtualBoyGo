#include "RomScanner.h"

#if defined(__ANDROID__)
#include "AndroidRomAccess.h"
#else
#include <filesystem>
#if defined(_DEBUG)
#include "DebugPaths.h"
#endif
#endif

#include <algorithm>
#include <cctype>

namespace
{
    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return s;
    }

#if !defined(__ANDROID__)
    std::string RomDirectory()
    {
#if defined(_DEBUG)
        // This repo's checked-in sample ROMs, for zero-setup local testing -
        // derived from the source tree's own location (see DebugPaths.h), so it
        // follows the checkout. Release builds use the relative path below.
        return DebugSdVbDir();
#else
        // Relative to the working directory, same convention LoadAssetBytes
        // uses for assets (see AssetLoader.h) - the exe's own folder for how
        // this app is packaged/run.
        return "VB";
#endif
    }
#endif
} // namespace

std::vector<RomEntry> ScanRoms()
{
    std::vector<RomEntry> roms;

#if defined(__ANDROID__)
    for (AndroidRomAccess::RomFile &file : AndroidRomAccess::ListRomFiles())
        roms.push_back({std::move(file.name), std::move(file.uri)});
#else
    const std::string dir = RomDirectory();
    if (dir.empty())
        return roms;

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec)
        return roms;

    for (const auto &entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec)
            break;
        if (!entry.is_regular_file())
            continue;

        const std::filesystem::path &path = entry.path();
        if (ToLower(path.extension().string()) != ".vb")
            continue;

        roms.push_back({path.stem().string(), path.string()});
    }
#endif

    std::sort(roms.begin(), roms.end(),
              [](const RomEntry &a, const RomEntry &b)
              { return ToLower(a.name) < ToLower(b.name); });

    return roms;
}
