// Android (Quest/Frame/Android VR) entry point via android_native_app_glue's
// NativeActivity. Mirrors Platform/PC/Main.cpp's loop, adapted for Android's
// event pump and OpenXR's Android-specific loader/instance init.
#include "OpenXrApp.h"
#include "AssetLoader.h"
#include "RomScanner.h"

#include <openxr/openxr_platform.h>

#include <android/log.h>
#include <android_native_app_glue.h>

#define LOG_TAG "VirtualBoyGo"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

struct AppState {
    bool resumed = false;
};

void HandleAppCmd(struct android_app* app, int32_t cmd) {
    auto* state = reinterpret_cast<AppState*>(app->userData);
    switch (cmd) {
        case APP_CMD_RESUME:
            state->resumed = true;
            break;
        case APP_CMD_PAUSE:
            state->resumed = false;
            break;
        default:
            break;
    }
}

}  // namespace

void android_main(struct android_app* app) {
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    AppState state;
    app->userData = &state;
    app->onAppCmd = HandleAppCmd;

    SetAndroidAssetManager(app->activity->assetManager);
    // App-specific external storage (e.g. /sdcard/Android/data/<package>/
    // files) - no runtime permission needed under scoped storage, unlike an
    // arbitrary SD-card path. Users drop ROMs into its "VB" subfolder.
    SetAndroidRomDir(app->activity->externalDataPath);

    // OpenXR's Android loader needs explicit init before xrCreateInstance.
    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
    if (XR_SUCCEEDED(
            xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader)))) {
        XrLoaderInitInfoAndroidKHR loaderInitInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitInfo.applicationVM = app->activity->vm;
        loaderInitInfo.applicationContext = app->activity->clazz;
        initializeLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInitInfo));
    }

    XrInstanceCreateInfoAndroidKHR androidCreateInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidCreateInfo.applicationVM = app->activity->vm;
    androidCreateInfo.applicationActivity = app->activity->clazz;

    OpenXrApp xrApp;
    bool initialized = false;

    bool requestRestart = false;
    bool destroyRequested = false;

    while (!destroyRequested) {
        for (;;) {
            int events;
            struct android_poll_source* source;
            const int timeoutMs = (!state.resumed && (!initialized || !xrApp.IsSessionRunning())) ? -1 : 0;
            if (ALooper_pollAll(timeoutMs, nullptr, &events, reinterpret_cast<void**>(&source)) < 0) break;
            if (source) source->process(app, source);
            if (app->destroyRequested) {
                destroyRequested = true;
                break;
            }
        }
        if (destroyRequested) break;

        if (!initialized && state.resumed) {
            try {
                OpenXrApp::InitInfo info;
                info.instanceCreateNext = &androidCreateInfo;
                info.isAndroid = true;
                xrApp.Initialize(info);
                initialized = true;
                LOGI("OpenXrApp initialized");
            } catch (const std::exception& ex) {
                LOGE("OpenXrApp init failed: %s", ex.what());
                break;
            }
        }

        if (!initialized) continue;

        bool exitRenderLoop = false;
        xrApp.PollEvents(exitRenderLoop, requestRestart);
        if (exitRenderLoop) break;

        if (xrApp.IsSessionRunning()) {
            xrApp.RenderFrame();
        }
    }

    if (initialized) xrApp.Shutdown();
    app->activity->vm->DetachCurrentThread();
}
