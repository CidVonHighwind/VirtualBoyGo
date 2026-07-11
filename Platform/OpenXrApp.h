#pragma once

#include "VulkanRenderer.h"

#include <openxr/openxr.h>

#include <string>
#include <vector>

// Owns the OpenXR instance/system/session/swapchain lifecycle and drives a
// VulkanRenderer to draw into each eye's swapchain image. Deliberately
// self-contained (no multi-backend abstraction like hello_xr's
// IGraphicsPlugin/IPlatformPlugin) since this project only ever targets
// Vulkan, on Android (Quest/Frame/Android VR) and PC.
class OpenXrApp {
   public:
    struct InitInfo {
        // Android only: pointer to an XrInstanceCreateInfoAndroidKHR to chain
        // onto XrInstanceCreateInfo::next. Left null on PC.
        const void* instanceCreateNext = nullptr;
        // Android only: XR_KHR_android_create_instance must be enabled.
        bool isAndroid = false;
    };

    void Initialize(const InitInfo& info);
    void Shutdown();

    // Pumps the XR event queue and updates internal session-state tracking.
    void PollEvents(bool& exitRenderLoop, bool& requestRestart);

    bool IsSessionRunning() const { return m_sessionRunning; }

    void RenderFrame();

   private:
    void CreateInstance(const InitInfo& info);
    void InitializeSystem();
    void InitializeSession();
    void CreateSwapchains();
    void HandleSessionStateChanged(const XrEventDataSessionStateChanged& event, bool& exitRenderLoop, bool& requestRestart);
    // Renders into the dedicated quad swapchain and fills out the
    // XrCompositionLayerQuad for it. Returns false if there's nothing to
    // submit yet (e.g. swapchain not created).
    bool RenderQuadLayer(XrCompositionLayerQuad& quadLayer);

    struct Swapchain {
        XrSwapchain handle{XR_NULL_HANDLE};
        int32_t width{0};
        int32_t height{0};
        std::vector<XrSwapchainImageVulkan2KHR> images;
    };

    XrInstance m_instance{XR_NULL_HANDLE};
    XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
    XrSession m_session{XR_NULL_HANDLE};
    XrSpace m_appSpace{XR_NULL_HANDLE};
    XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
    bool m_sessionRunning{false};

    XrViewConfigurationType m_viewConfigType{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
    std::vector<XrViewConfigurationView> m_configViews;
    std::vector<XrView> m_views;
    std::vector<Swapchain> m_swapchains;
    int64_t m_colorFormat{0};

    // Dedicated swapchain for a quad composition layer - the OpenXR
    // equivalent of the old VrApi ovrLayerCylinder2 used to show the
    // emulator screen/menu outside the main eye-buffer projection layer.
    // This is the piece the eventual menu/emulator-screen rendering will
    // build on. Sized to match the loaded test image so it renders at 1:1
    // pixel resolution instead of being up/downscaled.
    Swapchain m_quadSwapchain;
    uint32_t m_testImageWidth{0};
    uint32_t m_testImageHeight{0};
    bool m_testImageLoaded{false};

    VulkanRenderer m_renderer;
};
