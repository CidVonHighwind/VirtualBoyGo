#include "io/RomScanner.h"

#if defined(__ANDROID__)
#include "io/AndroidBridge.h"
#else
#include <filesystem>
#endif

#include <algorithm>
#include <cctype>

#if !defined(__ANDROID__) && defined(_DEBUG)
std::string DebugSdVbDir()
{
    // Derived from this file's own path (three parents up from
    // <repo>/core/io/RomScanner.cpp) so it follows the checkout instead of
    // a hardcoded absolute path.
    const std::filesystem::path repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    return (repoRoot / "sd" / "VB").string();
}
#endif

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
        return DebugSdVbDir(); // repo's checked-in sample ROMs
#else
        return "VB"; // relative to the exe's folder, like LoadAssetBytes
#endif
    }
#endif
} // namespace

std::vector<RomEntry> ScanRoms()
{
    std::vector<RomEntry> roms;

#if defined(__ANDROID__)
    for (AndroidBridge::RomFile &file : AndroidBridge::ListRomFiles())
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
