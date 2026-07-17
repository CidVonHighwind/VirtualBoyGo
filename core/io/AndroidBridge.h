#pragma once

// JNI bridge to MainActivity.java's SAF ROMs folder (see RomScanner.h) plus
// misc device queries (battery). Header-only so it needs no per-platform
// .cpp wiring; no-op off Android.
#if defined(__ANDROID__)

#include <jni.h>

#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

namespace AndroidBridge
{
    struct RomFile
    {
        std::string name; // display name, without the .vb extension
        std::string uri;  // content:// document URI - pass back to ReadFile
    };

    inline JavaVM *g_vm = nullptr;
    inline jobject g_activity = nullptr; // global ref, set once by Init

    // Called once at startup, before anything else here.
    inline void Init(JavaVM *vm, jobject activityClazz)
    {
        g_vm = vm;
        JNIEnv *env = nullptr;
        vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        g_activity = env->NewGlobalRef(activityClazz);
    }

    // True once a still-accessible ROMs folder is picked (checks the OS's
    // persisted-permission list, so external revocation shows up).
    inline bool HasRomsFolder()
    {
        if (!g_vm || !g_activity)
            return false;
        JNIEnv *env = nullptr;
        g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID getUri = env->GetMethodID(activityClass, "getRomsTreeUriString", "()Ljava/lang/String;");
        auto uri = static_cast<jstring>(env->CallObjectMethod(g_activity, getUri));
        const bool has = uri != nullptr;
        env->DeleteLocalRef(activityClass);
        if (uri)
            env->DeleteLocalRef(uri);
        return has;
    }

    // Clears the ROMs folder; picker re-fires on next relaunch since
    // HasRomsFolder() is now false. Doesn't restart the app itself.
    inline void RequestChangeRomsFolder()
    {
        if (!g_vm || !g_activity)
            return;
        JNIEnv *env = nullptr;
        g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID request = env->GetMethodID(activityClass, "requestChangeRomsFolder", "()V");
        env->CallVoidMethod(g_activity, request);
        env->DeleteLocalRef(activityClass);
    }

    // Empty if no folder is picked yet, or it contains no .vb files.
    inline std::vector<RomFile> ListRomFiles()
    {
        std::vector<RomFile> result;
        if (!g_vm || !g_activity)
            return result;
        JNIEnv *env = nullptr;
        g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID listMethod = env->GetMethodID(activityClass, "listRomFiles", "()[Ljava/lang/String;");
        auto array = static_cast<jobjectArray>(env->CallObjectMethod(g_activity, listMethod));
        env->DeleteLocalRef(activityClass);
        if (!array)
            return result;

        const jsize count = env->GetArrayLength(array);
        for (jsize i = 0; i + 1 < count; i += 2)
        {
            auto nameStr = static_cast<jstring>(env->GetObjectArrayElement(array, i));
            auto uriStr = static_cast<jstring>(env->GetObjectArrayElement(array, i + 1));

            const char *nameChars = env->GetStringUTFChars(nameStr, nullptr);
            const char *uriChars = env->GetStringUTFChars(uriStr, nullptr);
            result.push_back({nameChars, uriChars});
            env->ReleaseStringUTFChars(nameStr, nameChars);
            env->ReleaseStringUTFChars(uriStr, uriChars);

            env->DeleteLocalRef(nameStr);
            env->DeleteLocalRef(uriStr);
        }
        env->DeleteLocalRef(array);
        return result;
    }

    // Shared by ReadFile/ReadRomsFile - fd < 0 returns empty.
    inline std::vector<uint8_t> ReadAllFromFd(int fd)
    {
        std::vector<uint8_t> bytes;
        if (fd < 0)
            return bytes;

        FILE *f = fdopen(fd, "rb");
        if (!f)
        {
            close(fd);
            return bytes;
        }
        fseek(f, 0, SEEK_END);
        const long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size > 0)
        {
            bytes.resize(static_cast<size_t>(size));
            const size_t bytesRead = fread(bytes.data(), 1, bytes.size(), f);
            bytes.resize(bytesRead);
        }
        fclose(f); // also closes the underlying fd
        return bytes;
    }

    // Reads a ROM by its content:// URI (from ListRomFiles) - empty on error.
    inline std::vector<uint8_t> ReadFile(const std::string &documentUri)
    {
        if (!g_vm || !g_activity)
            return {};
        JNIEnv *env = nullptr;
        g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID openMethod = env->GetMethodID(activityClass, "openRomFileDescriptor", "(Ljava/lang/String;)I");
        jstring uriJString = env->NewStringUTF(documentUri.c_str());
        const jint fd = env->CallIntMethod(g_activity, openMethod, uriJString);
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(uriJString);

        return ReadAllFromFd(fd);
    }

    // Writes fileName (truncating), creating it as needed. inStatesDir
    // selects the folder root (.srm) vs "States" subfolder (states/previews).
    inline bool WriteRomsFile(const std::string &fileName, bool inStatesDir, const void *data, size_t size)
    {
        if (!g_vm || !g_activity)
            return false;
        JNIEnv *env = nullptr;
        g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID openMethod = env->GetMethodID(activityClass, "openRomsFileForWrite", "(Ljava/lang/String;Z)I");
        jstring nameJString = env->NewStringUTF(fileName.c_str());
        const jint fd = env->CallIntMethod(g_activity, openMethod, nameJString, static_cast<jboolean>(inStatesDir));
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(nameJString);

        if (fd < 0)
            return false;

        FILE *f = fdopen(fd, "wb");
        if (!f)
        {
            close(fd);
            return false;
        }
        const size_t written = fwrite(data, 1, size, f);
        fclose(f);
        return written == size;
    }

    // Read counterpart to WriteRomsFile - empty if not found or unreadable.
    inline std::vector<uint8_t> ReadRomsFile(const std::string &fileName, bool inStatesDir)
    {
        if (!g_vm || !g_activity)
            return {};
        JNIEnv *env = nullptr;
        g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID openMethod = env->GetMethodID(activityClass, "openRomsFileForRead", "(Ljava/lang/String;Z)I");
        jstring nameJString = env->NewStringUTF(fileName.c_str());
        const jint fd = env->CallIntMethod(g_activity, openMethod, nameJString, static_cast<jboolean>(inStatesDir));
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(nameJString);

        return ReadAllFromFd(fd);
    }

    // 0-100 device battery level, -1 if unavailable (AppMenu::SetBatteryPercent
    // treats -1 as "don't draw").
    inline int GetBatteryPercent()
    {
        if (!g_vm || !g_activity)
            return -1;
        JNIEnv *env = nullptr;
        g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID getBatteryLevel = env->GetMethodID(activityClass, "getBatteryLevel", "()I");
        const int percent = env->CallIntMethod(g_activity, getBatteryLevel);
        env->DeleteLocalRef(activityClass);
        return percent;
    }

    // True if fileName exists in the ROMs folder (root or "States").
    inline bool RomsFileExists(const std::string &fileName, bool inStatesDir)
    {
        if (!g_vm || !g_activity)
            return false;
        JNIEnv *env = nullptr;
        g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

        jclass activityClass = env->GetObjectClass(g_activity);
        jmethodID existsMethod = env->GetMethodID(activityClass, "romsFileExists", "(Ljava/lang/String;Z)Z");
        jstring nameJString = env->NewStringUTF(fileName.c_str());
        const bool exists = env->CallBooleanMethod(g_activity, existsMethod, nameJString, static_cast<jboolean>(inStatesDir));
        env->DeleteLocalRef(activityClass);
        env->DeleteLocalRef(nameJString);
        return exists;
    }
} // namespace AndroidBridge

#else

namespace AndroidBridge
{
    inline bool HasRomsFolder() { return true; } // other platforms don't gate on this
    inline void RequestChangeRomsFolder() {}
    inline int GetBatteryPercent() { return -1; } // no real battery to read off Android
} // namespace AndroidBridge

#endif
