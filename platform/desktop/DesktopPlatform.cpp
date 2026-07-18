#include "desktop/DesktopPlatform.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return s;
    }

#if defined(_DEBUG)
    // This repo's checked-in sample-ROM / writable-data folder (<repo>/sd/VB),
    // for zero-setup local testing. Derived from this file's own path (three
    // parents up from <repo>/platform/desktop/DesktopPlatform.cpp) so it
    // follows the checkout instead of a hardcoded absolute path.
    std::string DebugSdVbDir()
    {
        const std::filesystem::path repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
        return (repoRoot / "sd" / "VB").string();
    }
#endif

    std::string RomDirectory()
    {
#if defined(_DEBUG)
        return DebugSdVbDir(); // repo's checked-in sample ROMs
#else
        return "VB"; // relative to the exe's folder, like LoadAssetBytes
#endif
    }

    std::string RomsPath(const std::string &fileName, bool inStatesDir)
    {
        return RomDirectory() + (inStatesDir ? "/States/" : "/") + fileName;
    }
} // namespace

std::vector<RomEntry> DesktopPlatform::ScanRoms()
{
    std::vector<RomEntry> roms;

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

    std::sort(roms.begin(), roms.end(),
              [](const RomEntry &a, const RomEntry &b)
              { return ToLower(a.name) < ToLower(b.name); });

    return roms;
}

std::vector<uint8_t> DesktopPlatform::ReadRomFile(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool DesktopPlatform::WriteRomsFile(const std::string &fileName, bool inStatesDir, const void *data, size_t size)
{
    const std::string path = RomsPath(fileName, inStatesDir);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    return static_cast<bool>(out);
}

std::vector<uint8_t> DesktopPlatform::ReadRomsFile(const std::string &fileName, bool inStatesDir)
{
    std::ifstream in(RomsPath(fileName, inStatesDir), std::ios::binary);
    if (!in)
        return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool DesktopPlatform::RomsFileExists(const std::string &fileName, bool inStatesDir) const
{
    std::error_code ec;
    return std::filesystem::exists(RomsPath(fileName, inStatesDir), ec) && !ec;
}

std::vector<uint8_t> DesktopPlatform::LoadAssetBytes(const std::string &name)
{
    // Relative to the working directory - the exe's own folder for how this
    // app is packaged/run.
    std::ifstream in(name, std::ios::binary);
    if (!in)
        return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
