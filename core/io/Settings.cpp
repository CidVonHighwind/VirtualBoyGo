#include "io/Settings.h"
#include "io/Platform.h"

#include <cstring>
#include <string>
#include <vector>

namespace
{
    constexpr const char *kSettingsFileName = "settings.dat";

    // Shared by both platform Load paths.
    bool ApplyLoadedSettings(AppSettings &self, int version, const AppSettings &loaded)
    {
        if (version != AppSettings::kVersion)
            return false; // stale layout - leave self untouched, defaults stand

        self = loaded;
        return true;
    }
} // namespace

void AppSettings::Save(Platform &platform) const
{
    std::vector<uint8_t> bytes(sizeof(int) + sizeof(AppSettings));
    const int version = kVersion;
    std::memcpy(bytes.data(), &version, sizeof(version));
    std::memcpy(bytes.data() + sizeof(version), this, sizeof(AppSettings));
    platform.WriteRomsFile(kSettingsFileName, true, bytes.data(), bytes.size());
}

bool AppSettings::Load(Platform &platform)
{
    const std::vector<uint8_t> bytes = platform.ReadRomsFile(kSettingsFileName, true);
    if (bytes.size() < sizeof(int) + sizeof(AppSettings))
        return false;

    int version = 0;
    std::memcpy(&version, bytes.data(), sizeof(version));
    AppSettings loaded;
    std::memcpy(&loaded, bytes.data() + sizeof(version), sizeof(AppSettings));
    return ApplyLoadedSettings(*this, version, loaded);
}
