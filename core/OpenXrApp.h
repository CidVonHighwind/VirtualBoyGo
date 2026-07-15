#pragma once

#include "Emulator.h"
#include "Settings.h"
#include "VulkanRenderer.h"
#include "XrInput.h"
#include "ui/AppMenu.h"
#include "ui/UiRenderer.h"

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
    // (Re)creates m_menuSwapchain at the active m_menuRenderScale tier -
    // see the definition's doc comment for why it must be sized exactly
    // (full-rect submission) instead of allocated once at the max tier.
    void EnsureMenuSwapchain();
    void UpdateMenuRenderScale();
    void HandleSessionStateChanged(const XrEventDataSessionStateChanged& event, bool& exitRenderLoop, bool& requestRestart);
    // Renders the emulator screen into its two dedicated per-eye quad
    // swapchains and fills out an XrCompositionLayerQuad for each - the
    // emulator always renders both VB eyes packed side-by-side into a single
    // combined frame (see Emulator's class comment), so DrawScreen crops the
    // matching half (Emulator::Eye::Left/Right) into each swapchain
    // separately. Two independent swapchains rather than one shared/cropped
    // one - some runtimes (confirmed: SteamVR) don't handle one swapchain
    // image being referenced by two simultaneous composition layer
    // submissions well (manifested as VK_ERROR_DEVICE_LOST at session sync -
    // likely their Vulkan interop's shared-texture/fence bookkeeping
    // expecting a 1:1 acquire/layer relationship). Returns false (leaving
    // both untouched) if there's nothing to submit yet (e.g. swapchains not
    // created).
    bool RenderScreenLayer(XrCompositionLayerQuad& leftQuadLayer, XrCompositionLayerQuad& rightQuadLayer);
    // Renders the menu into its own dedicated quad swapchain, positioned a
    // little closer to the viewer than the screen layer so it visibly
    // floats in front of it instead of sitting flush on the same plane.
    bool RenderMenuLayer(XrCompositionLayerQuad& quadLayer);

    struct Swapchain {
        XrSwapchain handle{XR_NULL_HANDLE};
        int32_t width{0};
        int32_t height{0};
        uint32_t mipLevels{1};
        std::vector<XrSwapchainImageVulkan2KHR> images;
    };

    XrInstance m_instance{XR_NULL_HANDLE};
    XrSystemId m_systemId{XR_NULL_SYSTEM_ID};
    XrSession m_session{XR_NULL_HANDLE};
    XrSpace m_appSpace{XR_NULL_HANDLE};
    XrSessionState m_sessionState{XR_SESSION_STATE_UNKNOWN};
    bool m_sessionRunning{false};
    std::string m_runtimeName;

    XrViewConfigurationType m_viewConfigType{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
    std::vector<XrViewConfigurationView> m_configViews;
    std::vector<XrView> m_views;
    std::vector<Swapchain> m_swapchains;
    int64_t m_colorFormat{0};

    // Dedicated swapchains for the emulator screen's quad composition layer,
    // one per eye (see RenderScreenLayer's doc comment for why not one
    // shared/cropped swapchain) - the OpenXR equivalent of the old VrApi
    // ovrLayerCylinder2 used to show the emulator screen outside the main
    // eye-buffer projection layer. Each sized to half the emulator's
    // (side-by-side, both eyes) screen width at Emulator::kScale (falling
    // back to AppMenu::kMenuWidth/kMenuHeight if no screen is loaded) so
    // they render at 1:1 pixel resolution instead of being up/downscaled.
    Swapchain m_screenSwapchainLeft;
    Swapchain m_screenSwapchainRight;
    // Dedicated swapchain for the menu's own quad composition layer -
    // separate from the screen so the menu can be positioned at its own
    // depth (see RenderMenuLayer) instead of being baked into the same
    // texture/plane as the screen. Sized exactly to the active PPD-derived
    // tier and recreated when the tier changes (see EnsureMenuSwapchain). It
    // is cleared to transparent so the compositor blends through to the
    // screen.
    Swapchain m_menuSwapchain;
    // Integer supersampling tier selected from the headset's recommended
    // pixels-per-degree and the menu quad's current angular size.
    int32_t m_menuRenderScale{2};

    VulkanRenderer m_renderer;
    UiRenderer m_uiRenderer;
    XrInput m_input;
    Emulator m_emulator;
    AppMenu m_appMenu;
    AppSettings m_settings;
    // Set by RenderFrame right before RenderScreenLayer/RenderMenuLayer, from
    // xrLocateViews' output - only valid (m_headPoseValid true) on frames
    // where the runtime actually located fresh view poses (see RenderFrame's
    // posesValid check). Used for the Follow Head setting's head-locked
    // orientation - see ComputeScreenOrientation in OpenXrApp.cpp.
    XrQuaternionf m_headOrientation{0.0f, 0.0f, 0.0f, 1.0f};
    bool m_headPoseValid{false};
    uint32_t m_buttonStates[3]{};
    uint32_t m_lastButtonStates[3]{};
    // Edge detection for the left controller's menu button toggling
    // m_appMenu open/closed (see XrInput::IsMenuButtonPressed) - a held
    // button shouldn't toggle every frame.
    bool m_lastMenuButtonPressed{false};
};
