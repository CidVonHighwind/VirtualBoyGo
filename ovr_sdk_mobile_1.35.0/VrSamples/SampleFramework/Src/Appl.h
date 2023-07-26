/*******************************************************************************

Filename    :   Appl.h
Content     :   VR application base class.
Created     :   March 20, 2018
Authors     :   Jonathan Wright
Language    :   C++

Copyright:  Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include <memory>

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_Input.h"

#include "OVR_Math.h"

#include "System.h"

#include "Platform/Android/Android.h"

#include <android_native_app_glue.h>
#include <android/keycodes.h>

#include "Render/Egl.h"
#include "Render/Framebuffer.h"
#include "Render/SurfaceRender.h"

namespace OVRFW {

class ovrKeyEvent {
   public:
    ovrKeyEvent(const int32_t keyCode, const int32_t action, const double t)
        : KeyCode(keyCode), Action(action), Time(t) {}

    int32_t KeyCode = 0;
    int32_t Action = 0;
    double Time = 0.0;
};

class ovrTouchEvent {
   public:
    ovrTouchEvent(const int32_t action, const int32_t x_, const int32_t y_, const double t)
        : Action(action), x(x_), y(y_), Time(t) {}

    int32_t Action = 0;
    int32_t x = 0;
    int32_t y = 0;
    double Time = 0.0;
};

struct ovrApplFrameIn {
    int64_t FrameIndex = 0;
    double PredictedDisplayTime = 0.0;
    double RealTimeInSeconds = 0.0;
    float DeltaSeconds = 0.0f;
    ovrTracking2 Tracking;
    ovrInputStateTrackedRemote LeftRemote;
    ovrInputStateTrackedRemote RightRemote;
    ovrInputStateTrackedRemote SingleHandRemote;
    uint16_t SingleHandRemoteTrackpadMaxX = 0u;
    uint16_t SingleHandRemoteTrackpadMaxY = 0u;
    uint32_t AllButtons = 0u;
    uint32_t AllTouches = 0u;
    uint32_t LastFrameAllButtons = 0u;
    uint32_t LastFrameAllTouches = 0u;
    float IPD;
    float EyeHeight = 1.6750f;
    int32_t RecenterCount = 0;
    bool LeftRemoteTracked = false;
    bool RightRemoteTracked = false;
    bool SingleHandRemoteTracked = false;
    bool HeadsetIsMounted = true;
    bool LastFrameHeadsetIsMounted = true;
    std::vector<ovrKeyEvent> KeyEvents;
    std::vector<ovrTouchEvent> TouchEvents;

    inline bool Clicked(const ovrButton& b) const {
        const bool isDown = (b & AllButtons) != 0;
        const bool wasDown = (b & LastFrameAllButtons) != 0;
        return (wasDown && !isDown);
    }

    inline bool Touched(const ovrTouch& t) const {
        const bool isDown = (t & AllTouches) != 0;
        const bool wasDown = (t & LastFrameAllTouches) != 0;
        return (wasDown && !isDown);
    }

    inline bool HeadsetMounted() const {
        return (!LastFrameHeadsetIsMounted && HeadsetIsMounted);
    }

    inline bool HeadsetUnMounted() const {
        return (LastFrameHeadsetIsMounted && !HeadsetIsMounted);
    }
};

class ovrApplFrameOut {
   public:
    ovrApplFrameOut() {}

    explicit ovrApplFrameOut(const bool exitApp) : ExitApp(exitApp) {}

    bool ExitApp = false;
};

struct ovrFrameMatrices {
    OVR::Matrix4f CenterView; // the view transform for the point between the eyes
    OVR::Matrix4f EyeView[2]; // the view transforms for each of the eyes
    OVR::Matrix4f EyeProjection[2]; // the projection transforms for each of the eyes
};

struct ovrRendererOutput {
    ovrLayer_Union2 Layers[ovrMaxLayerCount] = {};
    int NumLayers = 0;
    int FrameFlags = 0;

    ovrFrameMatrices FrameMatrices; // view and projection transforms
    std::vector<ovrDrawSurface> Surfaces; // list of surfaces to render
};

class ovrAppl {
   public:
    //============================
    // public interface
    enum ovrLifecycle { LIFECYCLE_UNKNOWN, LIFECYCLE_RESUMED, LIFECYCLE_PAUSED };

    enum ovrRenderState {
        RENDER_STATE_LOADING, // show the loading icon
        RENDER_STATE_RUNNING, // render frames
        RENDER_STATE_ENDING, // show a black frame transition
    };

    ovrAppl(
        const int32_t mainThreadTid,
        const int32_t renderThreadTid,
        const int cpuLevel,
        const int gpuLevel,
        bool useMultiView = true,
        bool overrideBackButtons = false)
        : CpuLevel(cpuLevel),
          GpuLevel(gpuLevel),
          MainThreadTid(mainThreadTid),
          RenderThreadTid(renderThreadTid),
          UseMultiView(useMultiView),
          OverrideBackButtons(overrideBackButtons),
          NumFramebuffers(useMultiView ? 1 : VRAPI_FRAME_LAYER_EYE_MAX) {}

    // Called from the OS callback when the app pauses or resumes
    void SetLifecycle(const ovrLifecycle lc);

    // Called from the OS lifecycle callback when the app's window is destroyed
    void SetWindow(const void* window);

    // Called one time when the application process starts.
    // Returns true if the application initialized successfully.
    bool Init(const ovrAppContext* context, const ovrInitParms* initParms = nullptr);

    // Called one time when the applicatoin process exits
    void Shutdown(const ovrAppContext* context);

    // Called to handle any lifecycle state changes. This will call
    // AppPaused() and AppResumed()
    void HandleLifecycle(const ovrAppContext* context);

    // Returns true if the application has a valid VR session object.
    bool HasActiveVRSession() const;

    // Called once per frame when the VR session is active.
    ovrApplFrameOut Frame(ovrApplFrameIn& in);

    // Called once per frame to allow the application to render eye buffers.
    void RenderFrame(const ovrApplFrameIn& in);

    // Called from the OS callback when a key event occurs.
    void AddKeyEvent(const int32_t keyCode, const int32_t action);

    // Called from the OS callback when a touch event occurs.
    void AddTouchEvent(const int32_t action, const int32_t x, const int32_t y);

    // Handle VrApi system events.
    void HandleVREvents(ovrApplFrameIn& in);

    // Handle VRAPI input updates.
    void HandleVRInputEvents(ovrApplFrameIn& in);

    // App entry point
    void Run(struct android_app* app);

    //============================
    // public context interface

    // Returns the application's context
    const ovrAppContext* GetContext() const {
        return Context;
    }

    // Returns the application's session obj for VrApi calls
    ovrMobile* GetSessionObject() const {
        return SessionObject;
    }

   protected:
    //============================
    // protected interface

    // Returns the application's current lifecycle state
    ovrLifecycle GetLifecycle() const;

    // Returns the application's current window pointer
    const void* GetWindow() const;

    // Set the clock levels immediately, as long as there is a valid VR session.
    void SetClockLevels(const int32_t cpuLevel, const int32_t gpuLevel);

    // Returns the CPU and GPU clock levels last set by SetClockLevels().
    void GetClockLevels(int32_t& cpuLevel, int& gpuLevel) const;

    uint64_t GetFrameIndex() const {
        return FrameIndex;
    }

    double GetDisplayTime() const {
        return DisplayTime;
    }

    int GetNumFramebuffers() const {
        return NumFramebuffers;
    }

    ovrFramebuffer* GetFrameBuffer(int eye) {
        return Framebuffer[NumFramebuffers == 1 ? 0 : eye].get();
    }

    //============================
    // App functions
    // All App* function can be overridden by the derived application class to
    // implement application-specific behaviors

    // Called when the application initializes.
    // Must return true if the application initializes successfully.
    virtual bool AppInit(const ovrAppContext* context);
    // Called when the application shuts down
    virtual void AppShutdown(const ovrAppContext* context);
    // Called when the application is resumed by the system.
    virtual void AppResumed(const ovrAppContext* contet);
    // Called when the application is paused by the system.
    virtual void AppPaused(const ovrAppContext* context);
    // Called once per frame when the VR session is active.
    virtual ovrApplFrameOut AppFrame(const ovrApplFrameIn& in);
    // Called once per frame to allow the application to render eye buffers.
    virtual void AppRenderFrame(const ovrApplFrameIn& in, ovrRendererOutput& out);
    // Called once per eye each frame for default renderer
    virtual void AppRenderEye(const ovrApplFrameIn& in, ovrRendererOutput& out, int eye);
    // Called once per eye each frame for default renderer
    virtual void AppEyeGLStateSetup(const ovrApplFrameIn& in, const ovrFramebuffer* fb, int eye);
    // Called when BACK, etc is pressed to signal app quitting
    virtual void AppHandleInputShutdownRequest(ovrRendererOutput& out);
    // Called when app loses focus
    virtual void AppLostFocus();
    // Called when app re-gains focus
    virtual void AppGainedFocus();

    //============================
    // Default helpers
    void DefaultRenderFrame_Loading(const ovrApplFrameIn& in, ovrRendererOutput& out);
    void DefaultRenderFrame_Running(const ovrApplFrameIn& in, ovrRendererOutput& out);
    void DefaultRenderFrame_Ending(const ovrApplFrameIn& in, ovrRendererOutput& out);

    void ChangeVolume(int volumeDelta);

    void GetIntentStrings(
        std::string& intentFromPackage,
        std::string& intentJSON,
        std::string& intentURI);

   protected:
    float SuggestedEyeFovDegreesX = 90.0f;
    float SuggestedEyeFovDegreesY = 90.0f;
    ovrMobile* SessionObject = nullptr;
    ovrEgl Egl;

   private:
    const ovrAppContext* Context = nullptr;
    ovrLifecycle Lifecycle = LIFECYCLE_UNKNOWN;
    const void* Window = nullptr;
    uint64_t FrameIndex = 0;
    double DisplayTime = 0.0;
    int32_t CpuLevel = 0;
    int32_t GpuLevel = 0;
    int32_t MainThreadTid = 0;
    int32_t RenderThreadTid = 0;
    uint32_t LastFrameAllButtons = 0u;
    uint32_t LastFrameAllTouches = 0u;
    bool LastFrameHeadsetIsMounted = true;

    bool UseMultiView;
    bool OverrideBackButtons;
    std::unique_ptr<ovrFramebuffer> Framebuffer[VRAPI_FRAME_LAYER_EYE_MAX];
    int NumFramebuffers;

    std::mutex KeyEventMutex;
    std::vector<ovrKeyEvent> PendingKeyEvents;
    std::mutex TouchEventMutex;
    std::vector<ovrTouchEvent> PendingTouchEvents;
};

} // namespace OVRFW
