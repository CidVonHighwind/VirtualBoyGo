// Android (Quest/Frame/Android VR) entry point via android_native_app_glue's
// NativeActivity. Mirrors Platform/PC/Main.cpp's loop, adapted for Android's
// event pump and OpenXR's Android-specific loader/instance init.
#include "app/OpenXrApp.h"
#include "io/AssetLoader.h"
#include "io/AndroidRomAccess.h"
#include "input/ButtonMapping.h"

#include <openxr/openxr_platform.h>

#include <android/input.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#define LOG_TAG "VirtualBoyGo"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace
{

    struct AppState
    {
        bool resumed = false;
        // Physical gamepad (e.g. a Bluetooth Xbox controller) state, built up
        // from raw AInputEvents in HandleInputEvent - NativeActivity has no
        // polling API for this (unlike desktop's glfwGetGamepadState, see
        // pc2d's Main.cpp). keyBits latches on AKEYCODE_* DOWN/UP edges;
        // axisBits is fully recomputed on every motion event (sticks + D-pad
        // hat), since motion events carry absolute axis values, not deltas.
        uint32_t gamepadKeyBits = 0;
        uint32_t gamepadAxisBits = 0;
        // Left stick click (L3) and the Xbox/Guide button (AKEYCODE_BUTTON_MODE,
        // if it reaches the app at all - many systems, quite possibly Horizon OS
        // too, reserve it for their own system menu and never deliver it here;
        // L3 is the reliable fallback) - see
        // OpenXrApp::SetGamepadMenuButtonPressed's doc comment for why this
        // exists at all: a gamepad has no dedicated menu button like the Touch
        // controllers do, so these stand in for it.
        bool gamepadMenuButtonHeld = false;
        bool gamepadGuideButtonHeld = false;
    };

    void HandleAppCmd(struct android_app *app, int32_t cmd)
    {
        auto *state = reinterpret_cast<AppState *>(app->userData);
        switch (cmd)
        {
        case APP_CMD_RESUME:
            state->resumed = true;
            break;
        case APP_CMD_PAUSE:
            state->resumed = false;
            // Defensive: if a controller disconnects (or the OS just stops
            // delivering its events) while backgrounded, there may be no UP
            // event to clear a held button - don't let it get stuck on.
            state->gamepadKeyBits = 0;
            state->gamepadAxisBits = 0;
            state->gamepadMenuButtonHeld = false;
            state->gamepadGuideButtonHeld = false;
            break;
        default:
            break;
        }
    }

    // AKEYCODE_* -> ButtonMapper::EmuButton_* bit, matching pc2d's
    // PollDesktopButtonState GLFW_GAMEPAD_BUTTON_* mapping. 0 for keycodes this
    // app doesn't bind (caller lets those fall through to default handling).
    uint32_t GamepadKeyBit(int32_t keyCode)
    {
        using namespace ButtonMapper;
        switch (keyCode)
        {
        case AKEYCODE_BUTTON_A:
            return ButtonMapping[EmuButton_A];
        case AKEYCODE_BUTTON_B:
            return ButtonMapping[EmuButton_B];
        case AKEYCODE_BUTTON_X:
            return ButtonMapping[EmuButton_X];
        case AKEYCODE_BUTTON_Y:
            return ButtonMapping[EmuButton_Y];
        case AKEYCODE_BUTTON_L1:
            return ButtonMapping[EmuButton_LShoulder];
        case AKEYCODE_BUTTON_R1:
            return ButtonMapping[EmuButton_RShoulder];
        case AKEYCODE_BUTTON_START:
            return ButtonMapping[EmuButton_Enter];
        case AKEYCODE_BUTTON_SELECT:
            return ButtonMapping[EmuButton_Back];
        case AKEYCODE_DPAD_UP:
            return ButtonMapping[EmuButton_Up];
        case AKEYCODE_DPAD_DOWN:
            return ButtonMapping[EmuButton_Down];
        case AKEYCODE_DPAD_LEFT:
            return ButtonMapping[EmuButton_Left];
        case AKEYCODE_DPAD_RIGHT:
            return ButtonMapping[EmuButton_Right];
        default:
            return 0;
        }
    }

    int32_t HandleInputEvent(struct android_app *app, AInputEvent *event)
    {
        const int32_t source = AInputEvent_getSource(event);
        if (!(source & AINPUT_SOURCE_GAMEPAD) && !(source & AINPUT_SOURCE_JOYSTICK))
            return 0; // not a gamepad - let the OS handle it as usual

        auto *state = reinterpret_cast<AppState *>(app->userData);

        if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY)
        {
            const int32_t keyCode = AKeyEvent_getKeyCode(event);
            const int32_t action = AKeyEvent_getAction(event);
            if (keyCode == AKEYCODE_BUTTON_THUMBL)
            {
                state->gamepadMenuButtonHeld = (action == AKEY_EVENT_ACTION_DOWN);
                return 1;
            }
            if (keyCode == AKEYCODE_BUTTON_MODE)
            {
                state->gamepadGuideButtonHeld = (action == AKEY_EVENT_ACTION_DOWN);
                return 1;
            }
            const uint32_t bit = GamepadKeyBit(keyCode);
            if (bit == 0)
                return 0;
            if (action == AKEY_EVENT_ACTION_DOWN)
                state->gamepadKeyBits |= bit;
            else if (action == AKEY_EVENT_ACTION_UP)
                state->gamepadKeyBits &= ~bit;
            return 1;
        }

        if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
        {
            using namespace ButtonMapper;
            constexpr float kDeadzone = 0.5f;
            auto axis = [event](int32_t axisId)
            { return AMotionEvent_getAxisValue(event, axisId, 0); };
            // Standard Android generic-gamepad axis layout: X/Y = left stick,
            // Z/RZ = right stick (same convention pc2d's GLFW axes follow).
            const float leftX = axis(AMOTION_EVENT_AXIS_X);
            const float leftY = axis(AMOTION_EVENT_AXIS_Y);
            const float rightX = axis(AMOTION_EVENT_AXIS_Z);
            const float rightY = axis(AMOTION_EVENT_AXIS_RZ);
            // Some controllers report the D-pad as a hat switch instead of (or
            // alongside) AKEYCODE_DPAD_* key events.
            const float hatX = axis(AMOTION_EVENT_AXIS_HAT_X);
            const float hatY = axis(AMOTION_EVENT_AXIS_HAT_Y);

            uint32_t bits = 0;
            if (leftX < -kDeadzone)
                bits |= ButtonMapping[EmuButton_LeftStickLeft];
            if (leftX > kDeadzone)
                bits |= ButtonMapping[EmuButton_LeftStickRight];
            if (leftY < -kDeadzone)
                bits |= ButtonMapping[EmuButton_LeftStickUp];
            if (leftY > kDeadzone)
                bits |= ButtonMapping[EmuButton_LeftStickDown];
            if (rightX < -kDeadzone)
                bits |= ButtonMapping[EmuButton_RightStickLeft];
            if (rightX > kDeadzone)
                bits |= ButtonMapping[EmuButton_RightStickRight];
            if (rightY < -kDeadzone)
                bits |= ButtonMapping[EmuButton_RightStickUp];
            if (rightY > kDeadzone)
                bits |= ButtonMapping[EmuButton_RightStickDown];
            if (hatX < -kDeadzone)
                bits |= ButtonMapping[EmuButton_Left];
            if (hatX > kDeadzone)
                bits |= ButtonMapping[EmuButton_Right];
            if (hatY < -kDeadzone)
                bits |= ButtonMapping[EmuButton_Up];
            if (hatY > kDeadzone)
                bits |= ButtonMapping[EmuButton_Down];
            state->gamepadAxisBits = bits;
            return 1;
        }

        return 0;
    }

} // namespace

void android_main(struct android_app *app)
{
    JNIEnv *env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    AppState state;
    app->userData = &state;
    app->onAppCmd = HandleAppCmd;
    app->onInputEvent = HandleInputEvent;

    SetAndroidAssetManager(app->activity->assetManager);
    AndroidRomAccess::Init(app->activity->vm, app->activity->clazz);

    // OpenXR's Android loader needs explicit init before xrCreateInstance.
    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
    if (XR_SUCCEEDED(
            xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", reinterpret_cast<PFN_xrVoidFunction *>(&initializeLoader))))
    {
        XrLoaderInitInfoAndroidKHR loaderInitInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitInfo.applicationVM = app->activity->vm;
        loaderInitInfo.applicationContext = app->activity->clazz;
        initializeLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR *>(&loaderInitInfo));
    }

    XrInstanceCreateInfoAndroidKHR androidCreateInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidCreateInfo.applicationVM = app->activity->vm;
    androidCreateInfo.applicationActivity = app->activity->clazz;

    OpenXrApp xrApp;
    bool initialized = false;

    bool requestRestart = false;
    bool destroyRequested = false;

    while (!destroyRequested)
    {
        for (;;)
        {
            int events;
            struct android_poll_source *source;
            // Gated on HasRomsFolder() too, so the loop stays blocked (no
            // busy-spin) while the ROMs-folder picker is up, like it does for
            // resume.
            const bool readyToInit = state.resumed && AndroidRomAccess::HasRomsFolder();
            const int timeoutMs = (!readyToInit && (!initialized || !xrApp.IsSessionRunning())) ? -1 : 0;
            if (ALooper_pollAll(timeoutMs, nullptr, &events, reinterpret_cast<void **>(&source)) < 0)
                break;
            if (source)
                source->process(app, source);
            if (app->destroyRequested)
            {
                destroyRequested = true;
                break;
            }
        }
        if (destroyRequested)
            break;

        // Wait for a ROMs folder before starting the OpenXR session: the
        // picker (MainActivity.onCreate) runs concurrently with this thread,
        // and launching it once immersed makes Horizon OS drop VR focus for
        // good (see AndroidRomAccess::RequestChangeRomsFolder). Staying non-
        // immersive until it's done keeps focus handoff normal.
        if (!initialized && state.resumed && AndroidRomAccess::HasRomsFolder())
        {
            try
            {
                OpenXrApp::InitInfo info;
                info.instanceCreateNext = &androidCreateInfo;
                info.isAndroid = true;
                xrApp.Initialize(info);
                initialized = true;
                LOGI("OpenXrApp initialized");
            }
            catch (const std::exception &ex)
            {
                LOGE("OpenXrApp init failed: %s", ex.what());
                break;
            }
        }

        if (!initialized)
            continue;

        bool exitRenderLoop = false;
        xrApp.PollEvents(exitRenderLoop, requestRestart);
        if (exitRenderLoop)
            break;

        if (xrApp.IsSessionRunning())
        {
            xrApp.SetGamepadButtonState(state.gamepadKeyBits | state.gamepadAxisBits);
            xrApp.SetGamepadMenuButtonPressed(state.gamepadMenuButtonHeld || state.gamepadGuideButtonHeld);
            xrApp.RenderFrame();
        }
    }

    if (initialized)
        xrApp.Shutdown();
    app->activity->vm->DetachCurrentThread();
}
