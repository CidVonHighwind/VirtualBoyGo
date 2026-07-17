#include "io/Settings.h"

#if defined(__ANDROID__)
#include "io/AndroidRomAccess.h"
#include <cstring>
#include <vector>
#else
#if defined(_DEBUG)
#include "io/DebugPaths.h"
#endif
#include <filesystem>
#include <fstream>
#endif

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

const XrColor4f kScreenPatterns[6][5] = {
    // Jade - near black -> deep green -> teal -> yellow-green -> cream white
    {{0.02f, 0.03f, 0.02f, 1.0f}, {0.05f, 0.20f, 0.10f, 1.0f}, {0.10f, 0.45f, 0.40f, 1.0f}, {0.55f, 0.75f, 0.35f, 1.0f}, {0.97f, 0.96f, 0.85f, 1.0f}},
    // Ocean - near black -> navy -> azure -> sky blue -> ice white
    {{0.01f, 0.02f, 0.04f, 1.0f}, {0.05f, 0.15f, 0.35f, 1.0f}, {0.10f, 0.35f, 0.65f, 1.0f}, {0.55f, 0.80f, 0.90f, 1.0f}, {0.95f, 0.98f, 1.00f, 1.0f}},
    // Sunset - near black -> deep purple -> magenta -> orange -> pale gold
    {{0.04f, 0.01f, 0.05f, 1.0f}, {0.30f, 0.05f, 0.35f, 1.0f}, {0.75f, 0.20f, 0.25f, 1.0f}, {0.95f, 0.55f, 0.20f, 1.0f}, {1.00f, 0.92f, 0.75f, 1.0f}},
    // Ember - near black -> maroon -> deep red -> amber -> pale yellow
    {{0.03f, 0.01f, 0.00f, 1.0f}, {0.35f, 0.04f, 0.02f, 1.0f}, {0.65f, 0.15f, 0.02f, 1.0f}, {0.95f, 0.55f, 0.10f, 1.0f}, {1.00f, 0.95f, 0.75f, 1.0f}},
    // Frost - near black -> indigo -> violet -> lavender -> ice white
    {{0.02f, 0.02f, 0.05f, 1.0f}, {0.15f, 0.15f, 0.40f, 1.0f}, {0.40f, 0.35f, 0.70f, 1.0f}, {0.75f, 0.75f, 0.95f, 1.0f}, {0.98f, 0.98f, 1.00f, 1.0f}},
    // Toxic - near black -> dark olive -> teal-green -> chartreuse -> pale lime
    {{0.02f, 0.03f, 0.00f, 1.0f}, {0.15f, 0.30f, 0.02f, 1.0f}, {0.35f, 0.55f, 0.05f, 1.0f}, {0.65f, 0.85f, 0.15f, 1.0f}, {0.95f, 1.00f, 0.70f, 1.0f}},
};

namespace
{
    constexpr const char *kSettingsFileName = "settings.dat";

#if !defined(__ANDROID__)
    // Settings live next to the ROMs (the only writable-location precedent in
    // this project), so this mirrors RomScanner.cpp's RomDirectory() per-platform
    // layout - the shared _DEBUG folder comes from DebugPaths.h so it isn't
    // duplicated as a literal path. On Android they instead go through the SAF
    // ROMs-folder grant (AndroidRomAccess), same as .srm saves - a raw /sdcard
    // path is neither readable nor writable at target SDK 34 (see RomScanner.h).
    std::string SettingsFilePath()
    {
#if defined(_DEBUG)
        return DebugSdVbDir() + "/" + kSettingsFileName;
#else
        return std::string("VB/") + kSettingsFileName;
#endif
    }
#endif

    // Version check + v7->v8 screenScale baseline conversion, shared by both
    // platform Load paths. Version 8 defines 1.0x as the old 1.4x physical
    // size; converting keeps existing users' apparent screen size identical.
    bool ApplyLoadedSettings(AppSettings &self, int version, AppSettings &loaded)
    {
        constexpr int kPreviousScaleBaselineVersion = 7;
        if (version != AppSettings::kVersion && version != kPreviousScaleBaselineVersion)
            return false; // stale layout - leave self untouched, defaults stand

        if (version == kPreviousScaleBaselineVersion)
            loaded.screenScale /= 1.4f;

        self = loaded;
        return true;
    }
} // namespace

void AppSettings::Save() const
{
#if defined(__ANDROID__)
    std::vector<uint8_t> bytes(sizeof(int) + sizeof(AppSettings));
    const int version = kVersion;
    std::memcpy(bytes.data(), &version, sizeof(version));
    std::memcpy(bytes.data() + sizeof(version), this, sizeof(AppSettings));
    AndroidRomAccess::WriteRomsFile(kSettingsFileName, false, bytes.data(), bytes.size());
#else
    const std::string path = SettingsFilePath();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return;

    const int version = kVersion;
    out.write(reinterpret_cast<const char *>(&version), sizeof(version));
    out.write(reinterpret_cast<const char *>(this), sizeof(AppSettings));
#endif
}

bool AppSettings::Load()
{
#if defined(__ANDROID__)
    const std::vector<uint8_t> bytes = AndroidRomAccess::ReadRomsFile(kSettingsFileName, false);
    if (bytes.size() < sizeof(int) + sizeof(AppSettings))
        return false;

    int version = 0;
    std::memcpy(&version, bytes.data(), sizeof(version));
    AppSettings loaded;
    std::memcpy(&loaded, bytes.data() + sizeof(version), sizeof(AppSettings));
    return ApplyLoadedSettings(*this, version, loaded);
#else
    std::ifstream in(SettingsFilePath(), std::ios::binary);
    if (!in)
        return false;

    int version = 0;
    in.read(reinterpret_cast<char *>(&version), sizeof(version));
    if (!in)
        return false;

    AppSettings loaded;
    in.read(reinterpret_cast<char *>(&loaded), sizeof(AppSettings));
    if (!in)
        return false;

    return ApplyLoadedSettings(*this, version, loaded);
#endif
}
