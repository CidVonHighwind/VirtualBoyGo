#pragma once

#include "io/Platform.h"

#include <jni.h>

struct AAssetManager;

// Platform implementation for Android - JNI bridge to MainActivity.java's
// Storage Access Framework ROMs folder (SAF instead of a plain /sdcard path:
// at target SDK 34 that needs MANAGE_EXTERNAL_STORAGE, which kills and
// restarts the process the moment it's granted) plus misc device queries
// (battery).
class AndroidPlatform : public Platform
{
public:
    // vm/activityClazz/assetManager all come from android_app::activity -
    // called once at startup, before anything else here.
    AndroidPlatform(JavaVM *vm, jobject activityClazz, AAssetManager *assetManager);

    bool HasRomsFolder() const override;
    bool SupportsChangeRomsFolder() const override { return true; }
    void RequestChangeRomsFolder() override;

    std::vector<RomEntry> ScanRoms() override;
    std::vector<uint8_t> ReadRomFile(const std::string &path) override;

    bool WriteRomsFile(const std::string &fileName, bool inStatesDir, const void *data, size_t size) override;
    std::vector<uint8_t> ReadRomsFile(const std::string &fileName, bool inStatesDir) override;
    bool RomsFileExists(const std::string &fileName, bool inStatesDir) const override;

    int GetBatteryPercent() const override;

    std::vector<uint8_t> LoadAssetBytes(const std::string &name) override;

private:
    // Shared by ReadRomFile/ReadRomsFile - fd < 0 returns empty.
    static std::vector<uint8_t> ReadAllFromFd(int fd);

    JavaVM *m_vm = nullptr;
    jobject m_activity = nullptr; // global ref, set once by the constructor
    AAssetManager *m_assetManager = nullptr;
};
