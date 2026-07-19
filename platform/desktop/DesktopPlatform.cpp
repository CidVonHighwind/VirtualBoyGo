#include "desktop/DesktopPlatform.h"

#if defined(_WIN32)
// Pulls in <windows.h> - without this, its min/max macros mangle every
// std::min/std::max call in this file (see AudioOutput.cpp's identical fix
// for the same issue with miniaudio's WASAPI backend).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

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
    // This repo's checked-in sample-ROM / writable-data folder
    // (<repo>/sd/roms), for zero-setup local testing. Derived from this
    // file's own path (three parents up from
    // <repo>/platform/desktop/DesktopPlatform.cpp) so it follows the
    // checkout instead of a hardcoded absolute path.
    std::string DebugSdRomsDir()
    {
        const std::filesystem::path repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
        return (repoRoot / "sd" / "roms").string();
    }
#endif

    std::string RomDirectory()
    {
#if defined(_DEBUG)
        return DebugSdRomsDir(); // repo's checked-in sample ROMs
#else
        return "roms"; // relative to the exe's folder, like LoadAssetBytes
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

#if defined(_WIN32)
std::vector<uint8_t> DesktopPlatform::LoadAssetBytes(const std::string &name)
{
    // Embedded into the .exe as RCDATA resources (see
    // platform/desktop/DesktopAssets.rc.in / CMakeLists.txt) rather than
    // read from loose files next to it.
    struct Entry
    {
        const char *name;
        LPCWSTR resourceId;
    };
    static const Entry kEmbedded[] = {
        {"fonts/VirtualLogo.ttf", L"FONT_VIRTUALLOGO"},
        {"fonts/Roboto-Regular.ttf", L"FONT_ROBOTO_REGULAR"},
        {"fonts/Roboto-Bold.ttf", L"FONT_ROBOTO_BOLD"},
        {"icons/icons_atlas_10.png", L"ICON_ATLAS_10"},
        {"icons/icons_atlas_20.png", L"ICON_ATLAS_20"},
        {"icons/icons_atlas_30.png", L"ICON_ATLAS_30"},
        {"icons/icons_atlas_40.png", L"ICON_ATLAS_40"},
        {"icons/icons_atlas_50.png", L"ICON_ATLAS_50"},
        {"icons/icons_atlas_60.png", L"ICON_ATLAS_60"},
        {"game_image.png", L"GAME_IMAGE"},
    };

    for (const Entry &entry : kEmbedded)
    {
        if (name != entry.name)
            continue;

        const HMODULE module = GetModuleHandleW(nullptr);
        // MAKEINTRESOURCEW(10), not RT_RCDATA - that macro expands via the
        // ambient UNICODE define, which isn't set for this target, so it
        // resolves to the ANSI (LPSTR) form and won't compile against the
        // wide FindResourceW below.
        const HRSRC resInfo = FindResourceW(module, entry.resourceId, MAKEINTRESOURCEW(10));
        if (!resInfo)
            return {};
        const HGLOBAL resHandle = LoadResource(module, resInfo);
        if (!resHandle)
            return {};
        const auto *data = static_cast<const uint8_t *>(LockResource(resHandle));
        const DWORD size = SizeofResource(module, resInfo);
        if (!data || size == 0)
            return {};
        return std::vector<uint8_t>(data, data + size);
    }
    return {};
}
#else
std::vector<uint8_t> DesktopPlatform::LoadAssetBytes(const std::string &name)
{
    // Non-Windows desktop fallback - relative to the working directory, the
    // exe's own folder for how this app is packaged/run.
    std::ifstream in(name, std::ios::binary);
    if (!in)
        return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
#endif
