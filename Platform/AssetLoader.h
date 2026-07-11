#pragma once

#include <cstdint>
#include <vector>

#if defined(__ANDROID__)
struct AAssetManager;
// Must be called once (from android_main) before any LoadAssetBytes call.
void SetAndroidAssetManager(AAssetManager* assetManager);
#endif

// Loads a file from Android's APK assets, or (on PC) a file next to the
// running executable. Returns an empty vector if not found.
std::vector<uint8_t> LoadAssetBytes(const char* name);
