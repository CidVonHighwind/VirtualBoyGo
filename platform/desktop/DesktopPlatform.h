#pragma once

#include "io/Platform.h"

// Platform implementation for Windows/desktop (both platform/pc/'s OpenXR
// build and platform/pc2d's flat debug window) - a fixed relative ROMs
// folder on disk instead of Android's SAF grant, so no per-instance JNI
// state to hold; stateless/copyable.
class DesktopPlatform : public Platform
{
public:
    bool HasRomsFolder() const override { return true; } // no picker - always "has" the fixed folder
    bool SupportsChangeRomsFolder() const override { return false; }
    void RequestChangeRomsFolder() override {}

    std::vector<RomEntry> ScanRoms() override;
    std::vector<uint8_t> ReadRomFile(const std::string &path) override;

    bool WriteRomsFile(const std::string &fileName, bool inStatesDir, const void *data, size_t size) override;
    std::vector<uint8_t> ReadRomsFile(const std::string &fileName, bool inStatesDir) override;
    bool RomsFileExists(const std::string &fileName, bool inStatesDir) const override;

    int GetBatteryPercent() const override { return -1; } // no real battery to read off Android

    std::vector<uint8_t> LoadAssetBytes(const std::string &name) override;
};
