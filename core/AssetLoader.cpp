#include "AssetLoader.h"

#if defined(__ANDROID__)

#include <android/asset_manager.h>

namespace
{
    AAssetManager *g_assetManager = nullptr;
}

void SetAndroidAssetManager(AAssetManager *assetManager) { g_assetManager = assetManager; }

std::vector<uint8_t> LoadAssetBytes(const char *name)
{
    if (!g_assetManager)
        return {};
    AAsset *asset = AAssetManager_open(g_assetManager, name, AASSET_MODE_BUFFER);
    if (!asset)
        return {};
    const off_t length = AAsset_getLength(asset);
    std::vector<uint8_t> data(static_cast<size_t>(length));
    AAsset_read(asset, data.data(), data.size());
    AAsset_close(asset);
    return data;
}

#else

#include <fstream>
#include <iterator>

std::vector<uint8_t> LoadAssetBytes(const char *name)
{
    std::ifstream in(name, std::ios::binary);
    if (!in)
        return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

#endif
