#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct RomEntry
{
    std::string name;     // file name without extension, for display (e.g. "Golf (U) [!]")
    std::string fullPath; // for loading the ROM later - a filesystem path, or a content:// URI on Android
};

// Everything core/ needs from the OS/platform it's running on. One concrete
// implementation per platform (DesktopPlatform, AndroidPlatform), owned by
// that platform's entry point and threaded down through Initialize() calls -
// core/ itself never branches on a platform macro.
class Platform
{
public:
    virtual ~Platform() = default;

    // True once a still-accessible ROMs folder is picked.
    virtual bool HasRomsFolder() const = 0;
    // Whether this platform has a "Change ROMs Folder..." concept at all
    // (Android's SAF picker; desktop ROMs are just a fixed relative folder).
    virtual bool SupportsChangeRomsFolder() const = 0;
    // No-op if !SupportsChangeRomsFolder().
    virtual void RequestChangeRomsFolder() = 0;

    // Scans for .vb ROMs, sorted case-insensitively. Empty if there's
    // nowhere to look or no ROMs found.
    virtual std::vector<RomEntry> ScanRoms() = 0;
    // Reads a ROM's raw bytes given its RomEntry::fullPath - empty on error.
    virtual std::vector<uint8_t> ReadRomFile(const std::string &path) = 0;

    // Writable data (settings.dat, save states, .srm) - fileName resolved
    // relative to the ROMs folder. inStatesDir selects the folder root vs
    // the "States" subfolder (save states/previews).
    virtual bool WriteRomsFile(const std::string &fileName, bool inStatesDir, const void *data, size_t size) = 0;
    // Read counterpart to WriteRomsFile - empty if not found or unreadable.
    virtual std::vector<uint8_t> ReadRomsFile(const std::string &fileName, bool inStatesDir) = 0;
    virtual bool RomsFileExists(const std::string &fileName, bool inStatesDir) const = 0;

    // 0-100 device battery level, -1 if unavailable.
    virtual int GetBatteryPercent() const = 0;

    // Loads a bundled app asset (font/icon/image) - APK assets on Android,
    // a file next to the running executable on desktop. Empty if not found.
    virtual std::vector<uint8_t> LoadAssetBytes(const std::string &name) = 0;
};
