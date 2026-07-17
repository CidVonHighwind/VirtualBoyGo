#include "app/OpenXrApp.h"
#include "io/AndroidBridge.h"
#include "input/ButtonMapping.h"

#include <openxr/openxr_platform.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace
{
    constexpr int32_t kMaxMenuRenderScale = 6;
    constexpr float kRadiansToDegrees = 57.2957795f;
    // Recalibrated so the user-facing 1.0x setting has the same physical
    // height as the previous 1.4x setting (1.2m * 1.4) at
    // kReferenceScreenDistanceMeters.
    constexpr float kScreenQuadHeightMeters = 1.68f;
    // The distance kScreenQuadHeightMeters/kMenuDistanceMeters were tuned at
    // - matches AppSettings::screenDistance's own default (see Settings.h).
    // The screen's physical size is scaled by screenDistance/this reference
    // (see RenderScreenLayer's quadHeight) so moving the screen closer or
    // farther away doesn't change its apparent (angular) size - only the
    // Scale setting does that. Since the cylinder's arc width scales exactly
    // the same way its radius (== screenDistance) does, this also keeps a
    // curved screen's angular wrap-around constant regardless of Distance.
    constexpr float kReferenceScreenDistanceMeters = 2.2f;
    // Keep the menu slightly closer than the emulator screen for depth.
    constexpr float kMenuForwardOffsetMeters = 0.05f;
    // The menu's own distance is fixed, deliberately not tied to
    // AppSettings::screenDistance - that setting only moves the emulator
    // screen now (see RenderMenuLayer/UpdateMenuRenderScale). A fresh
    // install looks identical to before this split.
    constexpr float kMenuDistanceMeters = kReferenceScreenDistanceMeters - kMenuForwardOffsetMeters;
    // FollowHeadMode::Smooth's chase rate - ported from FrontendGo's
    // FOLLOW_SPEED (LayerBuilder.h), which drove the same
    // slerp(current, goal, speed*dt)-per-frame smoothing there.
    constexpr float kFollowHeadSmoothSpeed = 1.0f;

    void CheckXr(XrResult result, const char *what)
    {
        if (XR_FAILED(result))
        {
            throw std::runtime_error(std::string("OpenXR call failed: ") + what + " (" + std::to_string(result) + ")");
        }
    }

    // Minimal quaternion math for the screen/menu quad pose - ported from
    // FrontendGo's LayerBuilder::CylinderModelMatrix (a rotation *matrix*
    // composition for a VrApi cylinder layer), adapted to quaternions for an
    // OpenXR XrCompositionLayerQuad's pose.orientation.
    float DegToRad(float deg) { return deg * 3.14159265f / 180.0f; }

    XrQuaternionf QuatFromAxisAngle(float x, float y, float z, float angleRad)
    {
        const float half = angleRad * 0.5f;
        const float s = std::sin(half);
        return {x * s, y * s, z * s, std::cos(half)};
    }

    XrQuaternionf QuatMultiply(const XrQuaternionf &a, const XrQuaternionf &b)
    {
        return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
    }

    XrVector3f QuatRotateVector(const XrQuaternionf &q, const XrVector3f &v)
    {
        const XrVector3f u{q.x, q.y, q.z};
        const float s = q.w;
        const float dotUV = u.x * v.x + u.y * v.y + u.z * v.z;
        const float dotUU = u.x * u.x + u.y * u.y + u.z * u.z;
        const XrVector3f crossUV{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
        return {2.0f * dotUV * u.x + (s * s - dotUU) * v.x + 2.0f * s * crossUV.x,
                2.0f * dotUV * u.y + (s * s - dotUU) * v.y + 2.0f * s * crossUV.y,
                2.0f * dotUV * u.z + (s * s - dotUU) * v.z + 2.0f * s * crossUV.z};
    }

    // Shortest-path spherical interpolation, normalized-lerp near t==0/1 to
    // avoid dividing by a near-zero sin(theta) - used by FollowHeadMode::Smooth
    // to chase the live head orientation gradually instead of snapping to it
    // every frame (see RenderFrame's kFollowHeadSmoothSpeed usage).
    XrQuaternionf QuatSlerp(const XrQuaternionf &a, const XrQuaternionf &b, float t)
    {
        float bx = b.x, by = b.y, bz = b.z, bw = b.w;
        float dot = a.x * bx + a.y * by + a.z * bz + a.w * bw;
        if (dot < 0.0f)
        {
            bx = -bx;
            by = -by;
            bz = -bz;
            bw = -bw;
            dot = -dot;
        }
        dot = std::clamp(dot, -1.0f, 1.0f);
        if (dot > 0.9995f)
        {
            const XrQuaternionf lerp{a.x + (bx - a.x) * t, a.y + (by - a.y) * t, a.z + (bz - a.z) * t,
                                     a.w + (bw - a.w) * t};
            const float len = std::sqrt(lerp.x * lerp.x + lerp.y * lerp.y + lerp.z * lerp.z + lerp.w * lerp.w);
            return {lerp.x / len, lerp.y / len, lerp.z / len, lerp.w / len};
        }
        const float theta0 = std::acos(dot);
        const float theta = theta0 * t;
        const float sinTheta0 = std::sin(theta0);
        const float s0 = std::cos(theta) - dot * std::sin(theta) / sinTheta0;
        const float s1 = std::sin(theta) / sinTheta0;
        return {a.x * s0 + bx * s1, a.y * s0 + by * s1, a.z * s0 + bz * s1, a.w * s0 + bw * s1};
    }

    // yaw (Y axis) * pitch (X axis) * roll (Z axis), optionally premultiplied
    // by the current (or smoothed - see FollowHeadMode) head orientation -
    // same composition FrontendGo's CylinderModelMatrix used, just as a
    // quaternion instead of 3 rotation matrices. When follow head is off,
    // this project relies on m_appSpace's own LOCAL-space recentering to
    // keep the screen world-fixed, rather than FrontendGo's manual
    // forwardYaw-tracking un-rotate step - a deliberate simplification.
    XrQuaternionf ComputeScreenOrientation(const AppSettings &settings, bool followHeadActive,
                                           const XrQuaternionf &headOrientation)
    {
        const XrQuaternionf qYaw = QuatFromAxisAngle(0, 1, 0, DegToRad(settings.screenYaw));
        const XrQuaternionf qPitch = QuatFromAxisAngle(1, 0, 0, DegToRad(settings.screenPitch));
        const XrQuaternionf qRoll = QuatFromAxisAngle(0, 0, 1, DegToRad(settings.screenRoll));
        const XrQuaternionf localRot = QuatMultiply(QuatMultiply(qYaw, qPitch), qRoll);
        return followHeadActive ? QuatMultiply(headOrientation, localRot) : localRot;
    }

} // namespace

namespace
{
    // Fills in vbButtons/menu-button slots that have never been bound yet
    // (IsSet == false) with this platform's default controller layout -
    // mirrors the exact raw-getter-to-VBButtonBit mapping RenderFrame used
    // to hardcode directly, so first-run behavior is unchanged from before
    // this abstraction existed. Never overwrites a user's saved remap - only
    // genuinely-unset slots (a fresh settings.dat, or one saved before a
    // button existed) get a default.
    void ApplyDefaultGameplayBindings(AppSettings &settings)
    {
        using namespace ButtonMapper;
        auto setDefault = [&](uint32_t vbBit, int device, uint32_t emuButton)
        {
            MappedButton &b = settings.vbButtons[vbBit].Buttons[0];
            if (!b.IsSet)
            {
                b.IsSet = true;
                b.InputDevice = device;
                b.ButtonIndex = static_cast<int>(emuButton);
            }
        };
        setDefault(VBButtonBit::LeftUp, DeviceLeftTouch, EmuButton_LeftStickUp);
        setDefault(VBButtonBit::LeftDown, DeviceLeftTouch, EmuButton_LeftStickDown);
        setDefault(VBButtonBit::LeftLeft, DeviceLeftTouch, EmuButton_LeftStickLeft);
        setDefault(VBButtonBit::LeftRight, DeviceLeftTouch, EmuButton_LeftStickRight);
        setDefault(VBButtonBit::RightUp, DeviceRightTouch, EmuButton_RightStickUp);
        setDefault(VBButtonBit::RightDown, DeviceRightTouch, EmuButton_RightStickDown);
        setDefault(VBButtonBit::RightLeft, DeviceRightTouch, EmuButton_RightStickLeft);
        setDefault(VBButtonBit::RightRight, DeviceRightTouch, EmuButton_RightStickRight);
        setDefault(VBButtonBit::A, DeviceRightTouch, EmuButton_A);
        setDefault(VBButtonBit::B, DeviceRightTouch, EmuButton_B);
        setDefault(VBButtonBit::Select, DeviceLeftTouch, EmuButton_X);
        setDefault(VBButtonBit::Start, DeviceLeftTouch, EmuButton_Y);
        setDefault(VBButtonBit::L, DeviceLeftTouch, EmuButton_Trigger);
        setDefault(VBButtonBit::R, DeviceRightTouch, EmuButton_Trigger);
    }
} // namespace

void OpenXrApp::Initialize(const InitInfo &info)
{
    CreateInstance(info);
    InitializeSystem();
    m_renderer.CreateDevice(m_instance, m_systemId);

    // UiRenderer/Emulator only need the Vulkan device (not the XR session),
    // and CreateSwapchains needs the emulator's screen size to size the
    // quad swapchain, so both must run before InitializeSession/
    // CreateSwapchains.
    m_uiRenderer.Initialize(m_renderer.GetDevice(), m_renderer.GetPhysicalDevice(), m_renderer.GetQueue(),
                            m_renderer.GetQueueFamilyIndex(), m_renderer.GetCommandPool(), m_renderer.GetCommandBuffer());
    m_emulator.Initialize(m_uiRenderer);

    InitializeSession();
    CreateSwapchains();

    m_settings.Load(); // no-op (defaults stand) on first run/missing file
    ApplyDefaultGameplayBindings(m_settings);
    m_appMenu.Initialize(m_uiRenderer, static_cast<VkFormat>(m_colorFormat), m_emulator, m_settings);

    m_input.Initialize(m_instance, m_session);
}

void OpenXrApp::CreateInstance(const InitInfo &info)
{
    std::vector<const char *> extensions = {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
#if defined(XR_USE_PLATFORM_ANDROID)
    if (info.isAndroid)
    {
        extensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
    }
#endif

    // Optional extensions, enabled only if the runtime actually offers them:
    // XR_FB_display_refresh_rate (Quest; see RequestMaxDisplayRefreshRate)
    // and XR_KHR_composition_layer_cylinder (curved screen; see
    // RenderScreenLayer). SteamVR doesn't offer either as of writing.
    uint32_t availableCount = 0;
    CheckXr(xrEnumerateInstanceExtensionProperties(nullptr, 0, &availableCount, nullptr),
            "xrEnumerateInstanceExtensionProperties (count)");
    std::vector<XrExtensionProperties> available(availableCount, {XR_TYPE_EXTENSION_PROPERTIES});
    CheckXr(xrEnumerateInstanceExtensionProperties(nullptr, availableCount, &availableCount, available.data()),
            "xrEnumerateInstanceExtensionProperties");
    for (const XrExtensionProperties &ext : available)
    {
        if (std::strcmp(ext.extensionName, XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME) == 0)
        {
            extensions.push_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
            m_refreshRateExtAvailable = true;
        }
        else if (std::strcmp(ext.extensionName, XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME) == 0)
        {
            extensions.push_back(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME);
            m_cylinderExtAvailable = true;
        }
    }

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = info.instanceCreateNext;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.data();
    std::strncpy(createInfo.applicationInfo.applicationName, "VirtualBoyGo", XR_MAX_APPLICATION_NAME_SIZE - 1);
    createInfo.applicationInfo.applicationVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

    CheckXr(xrCreateInstance(&createInfo, &m_instance), "xrCreateInstance");

    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    CheckXr(xrGetInstanceProperties(m_instance, &instanceProperties), "xrGetInstanceProperties");
    m_runtimeName = instanceProperties.runtimeName;
    std::fprintf(stderr, "[OpenXR] Runtime: %s\n", m_runtimeName.c_str());
}

void OpenXrApp::InitializeSystem()
{
    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    CheckXr(xrGetSystem(m_instance, &systemInfo, &m_systemId), "xrGetSystem");

    uint32_t viewConfigCount = 0;
    CheckXr(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_viewConfigType, 0, &viewConfigCount, nullptr),
            "xrEnumerateViewConfigurationViews (count)");
    m_configViews.resize(viewConfigCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    CheckXr(xrEnumerateViewConfigurationViews(m_instance, m_systemId, m_viewConfigType, viewConfigCount, &viewConfigCount,
                                              m_configViews.data()),
            "xrEnumerateViewConfigurationViews");
    m_views.resize(viewConfigCount, {XR_TYPE_VIEW});
}

void OpenXrApp::InitializeSession()
{
    XrGraphicsBindingVulkan2KHR graphicsBinding = m_renderer.GetGraphicsBinding();

    XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
    createInfo.next = &graphicsBinding;
    createInfo.systemId = m_systemId;
    CheckXr(xrCreateSession(m_instance, &createInfo, &m_session), "xrCreateSession");

    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    CheckXr(xrCreateReferenceSpace(m_session, &spaceInfo, &m_appSpace), "xrCreateReferenceSpace");
}

void OpenXrApp::CreateSwapchains()
{
    uint32_t formatCount = 0;
    CheckXr(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr), "xrEnumerateSwapchainFormats (count)");
    std::vector<int64_t> formats(formatCount);
    CheckXr(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()),
            "xrEnumerateSwapchainFormats");
    m_colorFormat = m_renderer.SelectSwapchainFormat(formats);

    m_swapchains.resize(m_configViews.size());
    for (size_t i = 0; i < m_configViews.size(); ++i)
    {
        const XrViewConfigurationView &vp = m_configViews[i];
        Swapchain &sc = m_swapchains[i];
        sc.width = vp.recommendedImageRectWidth;
        sc.height = vp.recommendedImageRectHeight;

        XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainInfo.arraySize = 1;
        swapchainInfo.format = m_colorFormat;
        swapchainInfo.width = sc.width;
        swapchainInfo.height = sc.height;
        swapchainInfo.mipCount = 1;
        swapchainInfo.faceCount = 1;
        swapchainInfo.sampleCount = 1;
        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        CheckXr(xrCreateSwapchain(m_session, &swapchainInfo, &sc.handle), "xrCreateSwapchain (projection)");

        uint32_t imageCount = 0;
        CheckXr(xrEnumerateSwapchainImages(sc.handle, 0, &imageCount, nullptr),
                "xrEnumerateSwapchainImages (projection count)");
        sc.images.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        CheckXr(xrEnumerateSwapchainImages(sc.handle, imageCount, &imageCount,
                                           reinterpret_cast<XrSwapchainImageBaseHeader *>(sc.images.data())),
                "xrEnumerateSwapchainImages (projection)");
    }

    // Dedicated per-eye swapchains for the emulator screen's quad
    // composition layers, each sized to *half* the emulator's (side-by-side,
    // both eyes) screen width so each renders 1:1 instead of being scaled -
    // see OpenXrApp.h's member comment for why two independent swapchains
    // instead of one shared/cropped one. Falls back to the menu's own size
    // if no screen is loaded.
    const int32_t screenWidth = m_emulator.HasScreen() ? static_cast<int32_t>(m_emulator.GetScreenWidth() * Emulator::kScale)
                                                       : static_cast<int32_t>(kMenuWidth * kMenuScale);
    const int32_t screenHeight = m_emulator.HasScreen() ? static_cast<int32_t>(m_emulator.GetScreenHeight() * Emulator::kScale)
                                                        : static_cast<int32_t>(kMenuHeight * kMenuScale);
    const int32_t eyeWidth = m_emulator.HasScreen() ? screenWidth / 2 : screenWidth;
    for (Swapchain *sc : {&m_screenSwapchainLeft, &m_screenSwapchainRight})
    {
        sc->width = eyeWidth;
        sc->height = screenHeight;
        sc->mipLevels = 1;

        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        info.arraySize = 1;
        info.format = m_colorFormat;
        info.width = sc->width;
        info.height = sc->height;
        info.mipCount = 1;
        info.faceCount = 1;
        info.sampleCount = 1;
        info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        CheckXr(xrCreateSwapchain(m_session, &info, &sc->handle), "xrCreateSwapchain (screen eye)");

        uint32_t imageCount = 0;
        CheckXr(xrEnumerateSwapchainImages(sc->handle, 0, &imageCount, nullptr),
                "xrEnumerateSwapchainImages (screen eye count)");
        sc->images.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        CheckXr(xrEnumerateSwapchainImages(sc->handle, imageCount, &imageCount,
                                           reinterpret_cast<XrSwapchainImageBaseHeader *>(sc->images.data())),
                "xrEnumerateSwapchainImages (screen eye)");
    }

    EnsureMenuSwapchain();
}

// (Re)creates the menu swapchain sized exactly to the active PPD-derived
// render tier, so the submitted quad always references the full image.
// SteamVR device-loses when a composition layer's imageRect covers only a
// sub-rectangle of a larger swapchain image, so an allocate-max-tier-once,
// submit-active-subrect scheme is not an option. Tier changes (distance/
// screen-scale settings) are rare, so the recreation cost doesn't matter.
void OpenXrApp::EnsureMenuSwapchain()
{
    const int32_t width = kMenuWidth * m_menuRenderScale;
    const int32_t height = kMenuHeight * m_menuRenderScale;
    if (m_menuSwapchain.handle != XR_NULL_HANDLE && m_menuSwapchain.width == width && m_menuSwapchain.height == height)
        return;

    if (m_menuSwapchain.handle != XR_NULL_HANDLE)
    {
        // Every submit in this codebase queue-wait-idles, so nothing of ours
        // is in flight - but UiRenderer still caches framebuffers keyed by
        // the old swapchain's soon-to-be-destroyed VkImages.
        m_uiRenderer.InvalidateRenderTargets();
        xrDestroySwapchain(m_menuSwapchain.handle);
        m_menuSwapchain = {};
    }

    m_menuSwapchain.width = width;
    m_menuSwapchain.height = height;
    XrSwapchainCreateInfo menuInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    menuInfo.arraySize = 1;
    menuInfo.format = m_colorFormat;
    menuInfo.width = m_menuSwapchain.width;
    menuInfo.height = m_menuSwapchain.height;
    menuInfo.mipCount = 1;
    menuInfo.faceCount = 1;
    menuInfo.sampleCount = 1;
    menuInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    CheckXr(xrCreateSwapchain(m_session, &menuInfo, &m_menuSwapchain.handle), "xrCreateSwapchain (menu)");
    uint32_t menuImageCount = 0;
    CheckXr(xrEnumerateSwapchainImages(m_menuSwapchain.handle, 0, &menuImageCount, nullptr),
            "xrEnumerateSwapchainImages (menu count)");
    m_menuSwapchain.images.resize(menuImageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    CheckXr(xrEnumerateSwapchainImages(m_menuSwapchain.handle, menuImageCount, &menuImageCount,
                                       reinterpret_cast<XrSwapchainImageBaseHeader *>(m_menuSwapchain.images.data())),
            "xrEnumerateSwapchainImages (menu)");
}

void OpenXrApp::UpdateMenuRenderScale()
{
    if (m_views.empty() || m_configViews.empty())
        return;

    // OpenXR does not expose a single display-PPD property. The best runtime
    // supplied estimate is its recommended eye-buffer resolution divided by
    // the located view FOV. Use the densest axis/eye so neither direction is
    // undersampled.
    float headsetPpd = 0.0f;
    const size_t viewCount = std::min(m_views.size(), m_configViews.size());
    for (size_t i = 0; i < viewCount; ++i)
    {
        const float horizontalDegrees = (m_views[i].fov.angleRight - m_views[i].fov.angleLeft) * kRadiansToDegrees;
        const float verticalDegrees = (m_views[i].fov.angleUp - m_views[i].fov.angleDown) * kRadiansToDegrees;
        if (horizontalDegrees > 0.0f)
            headsetPpd = std::max(headsetPpd, m_configViews[i].recommendedImageRectWidth / horizontalDegrees);
        if (verticalDegrees > 0.0f)
            headsetPpd = std::max(headsetPpd, m_configViews[i].recommendedImageRectHeight / verticalDegrees);
    }
    if (headsetPpd <= 0.0f)
        return;

    // Preserve the panel's existing real-world size (the legacy scale-2
    // pixel dimensions times the screen layer's meters-per-pixel at the
    // default 1.0x screen scale - the user's screen-scale setting must not
    // resize the menu), then find how many pixels that angular area warrants
    // at the headset's PPD.
    const float metersPerPixel = m_screenSwapchainLeft.height != 0
                                     ? kScreenQuadHeightMeters / static_cast<float>(m_screenSwapchainLeft.height)
                                     : 1.0f;
    const float panelWidthMeters = kMenuWidth * kMenuScale * metersPerPixel;
    const float panelHeightMeters = kMenuHeight * kMenuScale * metersPerPixel;
    const float angularWidthDegrees = 2.0f * std::atan(panelWidthMeters / (2.0f * kMenuDistanceMeters)) * kRadiansToDegrees;
    const float angularHeightDegrees = 2.0f * std::atan(panelHeightMeters / (2.0f * kMenuDistanceMeters)) * kRadiansToDegrees;
    const float requiredWidth = angularWidthDegrees * headsetPpd;
    const float requiredHeight = angularHeightDegrees * headsetPpd;
    const int32_t scale = std::clamp(static_cast<int32_t>(std::ceil(std::max(
                                         requiredWidth / static_cast<float>(kMenuWidth),
                                         requiredHeight / static_cast<float>(kMenuHeight)))),
                                     1, kMaxMenuRenderScale);
    if (scale != m_menuRenderScale)
    {
        m_menuRenderScale = scale;
        m_appMenu.SetMenuScale(m_uiRenderer, static_cast<float>(scale));
    }
}

// Preferred: the highest rate that's a near-integer multiple of the VB's
// 50.27Hz (within 0.5%) - VB frames then hold a constant number of vsyncs,
// i.e. perfectly even pacing. No current Quest rate (72/80/90/120) comes
// close, so in practice the fallback applies: the highest offered rate,
// where the unavoidable mixed-vsync holds are shortest (72Hz alternates
// 1/2-vsync holds, visible judder; 120Hz alternates 2/3 at 8.3ms each) and
// tracking latency is lowest. Best-effort: no-op where the extension is
// missing (SteamVR) or the request fails.
void OpenXrApp::RequestMaxDisplayRefreshRate()
{
    if (!m_refreshRateExtAvailable)
        return;

    PFN_xrEnumerateDisplayRefreshRatesFB pfnEnumerateRates = nullptr;
    PFN_xrRequestDisplayRefreshRateFB pfnRequestRate = nullptr;
    xrGetInstanceProcAddr(m_instance, "xrEnumerateDisplayRefreshRatesFB",
                          reinterpret_cast<PFN_xrVoidFunction *>(&pfnEnumerateRates));
    xrGetInstanceProcAddr(m_instance, "xrRequestDisplayRefreshRateFB",
                          reinterpret_cast<PFN_xrVoidFunction *>(&pfnRequestRate));
    if (!pfnEnumerateRates || !pfnRequestRate)
        return;

    uint32_t rateCount = 0;
    if (XR_FAILED(pfnEnumerateRates(m_session, 0, &rateCount, nullptr)) || rateCount == 0)
        return;
    std::vector<float> rates(rateCount);
    if (XR_FAILED(pfnEnumerateRates(m_session, rateCount, &rateCount, rates.data())))
        return;

    float bestAny = 0.0f;
    float bestMultiple = 0.0f;
    for (const float rate : rates)
    {
        bestAny = std::max(bestAny, rate);
        const float ratio = rate / Emulator::kCoreFps;
        if (ratio >= 0.995f && std::abs(ratio - std::round(ratio)) <= 0.005f * ratio)
            bestMultiple = std::max(bestMultiple, rate);
    }
    const float best = bestMultiple > 0.0f ? bestMultiple : bestAny;

    if (best > 0.0f && XR_SUCCEEDED(pfnRequestRate(m_session, best)))
        std::fprintf(stderr, "[OpenXR] Requested display refresh rate: %.0f Hz%s\n", best,
                     bestMultiple > 0.0f ? " (VB frame-locked)" : "");
}

void OpenXrApp::HandleSessionStateChanged(const XrEventDataSessionStateChanged &event, bool &exitRenderLoop,
                                          bool &requestRestart)
{
    m_sessionState = event.state;

    switch (m_sessionState)
    {
    case XR_SESSION_STATE_READY:
    {
        XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
        beginInfo.primaryViewConfigurationType = m_viewConfigType;
        CheckXr(xrBeginSession(m_session, &beginInfo), "xrBeginSession");
        m_sessionRunning = true;
        RequestMaxDisplayRefreshRate();
        break;
    }
    case XR_SESSION_STATE_STOPPING:
        m_sessionRunning = false;
        xrEndSession(m_session);
        break;
    case XR_SESSION_STATE_EXITING:
        exitRenderLoop = true;
        requestRestart = false;
        break;
    case XR_SESSION_STATE_LOSS_PENDING:
        exitRenderLoop = true;
        requestRestart = true;
        break;
    default:
        break;
    }
}

void OpenXrApp::PollEvents(bool &exitRenderLoop, bool &requestRestart)
{
    exitRenderLoop = false;
    requestRestart = false;

    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &event) == XR_SUCCESS)
    {
        switch (event.type)
        {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            HandleSessionStateChanged(*reinterpret_cast<const XrEventDataSessionStateChanged *>(&event), exitRenderLoop,
                                      requestRestart);
            break;
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            exitRenderLoop = true;
            requestRestart = false;
            break;
        default:
            break;
        }
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

bool OpenXrApp::RenderScreenLayer(XrCompositionLayerQuad &leftQuadLayer, XrCompositionLayerQuad &rightQuadLayer,
                                  XrCompositionLayerCylinderKHR &leftCylinderLayer,
                                  XrCompositionLayerCylinderKHR &rightCylinderLayer, bool &outUsedCylinder)
{
    if (m_screenSwapchainLeft.handle == XR_NULL_HANDLE || m_screenSwapchainRight.handle == XR_NULL_HANDLE)
    {
        return false;
    }

    const bool useCylinder = m_settings.curvedScreen && m_cylinderExtAvailable;
    outUsedCylinder = useCylinder;

    const float aspect = m_screenSwapchainLeft.height != 0
                             ? static_cast<float>(m_screenSwapchainLeft.width) / m_screenSwapchainLeft.height
                             : 1.0f;
    // Scaled by Distance/kReferenceScreenDistanceMeters so moving the screen
    // closer/farther away doesn't change its apparent size - see that
    // constant's doc comment.
    const float quadHeight = kScreenQuadHeightMeters * m_settings.screenScale *
                             (m_settings.screenDistance / kReferenceScreenDistanceMeters);
    const bool followHeadActive = m_settings.followHeadMode != FollowHeadMode::Off && m_headPoseValid;
    const XrQuaternionf &followOrientation =
        m_settings.followHeadMode == FollowHeadMode::Smooth ? m_smoothedHeadOrientation : m_headOrientation;
    const XrQuaternionf orientation = ComputeScreenOrientation(m_settings, followHeadActive, followOrientation);
    const XrVector3f forward = QuatRotateVector(orientation, XrVector3f{0.0f, 0.0f, -1.0f});
    const XrVector3f right = QuatRotateVector(orientation, XrVector3f{1.0f, 0.0f, 0.0f});
    const XrVector3f basePosition{forward.x * m_settings.screenDistance, forward.y * m_settings.screenDistance,
                                  forward.z * m_settings.screenDistance};
    // Curvature radius equals Distance itself, so the viewer always sits
    // exactly on the cylinder's axis - the screen surface is then
    // equidistant in every direction within the visible arc, i.e. Distance
    // becomes "how big a cylinder surrounds me" rather than a fixed curve
    // bolted onto a flat placement. A cylinder layer's pose is its axis, not
    // its near surface (unlike a quad's pose, which sits right on the
    // surface) - pushing the axis back by exactly `radius` here means it
    // lands at basePosition - forward*distance, i.e. the space origin.
    const float cylinderRadius = m_settings.screenDistance;
    const XrVector3f cylinderAxisPosition{basePosition.x - forward.x * cylinderRadius,
                                          basePosition.y - forward.y * cylinderRadius,
                                          basePosition.z - forward.z * cylinderRadius};
    // Central angle chosen so the visible arc's width (radius * centralAngle)
    // matches the flat quad's width at the same settings, so toggling curved
    // on/off doesn't change the screen's apparent horizontal size.
    const float centralAngle = (quadHeight * aspect) / cylinderRadius;
    const XrColor4f tint{m_settings.colorR, m_settings.colorG, m_settings.colorB, 1.0f};
    const Emulator::Eye rightEyeCrop = m_settings.useThreeDeeMode ? Emulator::Eye::Right : Emulator::Eye::Left;

    struct LayerInfo
    {
        Swapchain *swapchain;
        XrCompositionLayerQuad *quad;
        XrCompositionLayerCylinderKHR *cylinder;
        XrEyeVisibility visibility;
        Emulator::Eye eye;
        float ipdSign;
    };
    const LayerInfo infos[] = {{&m_screenSwapchainLeft, &leftQuadLayer, &leftCylinderLayer, XR_EYE_VISIBILITY_LEFT,
                                Emulator::Eye::Left, -1.0f},
                               {&m_screenSwapchainRight, &rightQuadLayer, &rightCylinderLayer, XR_EYE_VISIBILITY_RIGHT,
                                rightEyeCrop, 1.0f}};
    for (const LayerInfo &info : infos)
    {
        Swapchain &sc = *info.swapchain;
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t imageIndex = 0;
        CheckXr(xrAcquireSwapchainImage(sc.handle, &acquireInfo, &imageIndex), "xrAcquireSwapchainImage (screen eye)");
        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        CheckXr(xrWaitSwapchainImage(sc.handle, &waitInfo), "xrWaitSwapchainImage (screen eye)");
        m_uiRenderer.BeginFrame(sc.images[imageIndex].image, static_cast<VkFormat>(m_colorFormat),
                                static_cast<uint32_t>(sc.width), static_cast<uint32_t>(sc.height),
                                m_appMenu.GetBackgroundColor());
        if (m_emulator.HasScreen())
            m_emulator.DrawScreen(m_uiRenderer, 0.0f, 0.0f, static_cast<float>(sc.width),
                                  static_cast<float>(sc.height), info.eye, tint, m_settings.selectedPattern);
        m_uiRenderer.EndFrame();
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        CheckXr(xrReleaseSwapchainImage(sc.handle, &releaseInfo), "xrReleaseSwapchainImage (screen eye)");

        const float ipdHalf = m_settings.useThreeDeeMode ? (m_settings.ipdOffset * 0.5f * info.ipdSign) : 0.0f;

        XrSwapchainSubImage subImage{};
        subImage.swapchain = sc.handle;
        subImage.imageRect.offset = {0, 0};
        subImage.imageRect.extent = {sc.width, sc.height};
        subImage.imageArrayIndex = 0;

        if (useCylinder)
        {
            XrCompositionLayerCylinderKHR &cylinderLayer = *info.cylinder;
            cylinderLayer.layerFlags = 0;
            cylinderLayer.space = m_appSpace;
            cylinderLayer.eyeVisibility = info.visibility;
            cylinderLayer.subImage = subImage;
            cylinderLayer.pose.orientation = orientation;
            cylinderLayer.pose.position = {cylinderAxisPosition.x + right.x * ipdHalf,
                                           cylinderAxisPosition.y + right.y * ipdHalf,
                                           cylinderAxisPosition.z + right.z * ipdHalf};
            cylinderLayer.radius = cylinderRadius;
            cylinderLayer.centralAngle = centralAngle;
            cylinderLayer.aspectRatio = aspect;
        }
        else
        {
            XrCompositionLayerQuad &quadLayer = *info.quad;
            quadLayer.layerFlags = 0;
            quadLayer.space = m_appSpace;
            quadLayer.eyeVisibility = info.visibility;
            quadLayer.subImage = subImage;
            quadLayer.pose.orientation = orientation;
            quadLayer.pose.position = {basePosition.x + right.x * ipdHalf, basePosition.y + right.y * ipdHalf,
                                       basePosition.z + right.z * ipdHalf};
            quadLayer.size = {quadHeight * aspect, quadHeight};
        }
    }
    return true;
}

bool OpenXrApp::RenderMenuLayer(XrCompositionLayerQuad &quadLayer)
{
    if (!m_appMenu.IsVisible())
    {
        return false;
    }
    EnsureMenuSwapchain();

    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t imageIndex = 0;
    CheckXr(xrAcquireSwapchainImage(m_menuSwapchain.handle, &acquireInfo, &imageIndex), "xrAcquireSwapchainImage (menu)");

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    CheckXr(xrWaitSwapchainImage(m_menuSwapchain.handle, &waitInfo), "xrWaitSwapchainImage (menu)");

    m_appMenu.RenderToBuffer(m_uiRenderer);

    // Fully transparent clear - only the rounded menu panel itself should
    // be opaque, so the compositor blends the rest of this layer through to
    // the screen layer behind it (see this layer's UNPREMULTIPLIED_ALPHA/
    // BLEND_TEXTURE_SOURCE_ALPHA flags below).
    m_uiRenderer.BeginFrame(m_menuSwapchain.images[imageIndex].image, static_cast<VkFormat>(m_colorFormat),
                            static_cast<uint32_t>(m_menuSwapchain.width), static_cast<uint32_t>(m_menuSwapchain.height),
                            XrColor4f{0.0f, 0.0f, 0.0f, 0.0f});
    m_appMenu.Draw(m_uiRenderer, 0.0f, 0.0f);
    m_uiRenderer.EndFrame();

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    CheckXr(xrReleaseSwapchainImage(m_menuSwapchain.handle, &releaseInfo), "xrReleaseSwapchainImage (menu)");

    // Same meters-per-pixel scale as the screen layer at its default 1.0x
    // size, so the menu doesn't appear to change size just for being on its
    // own swapchain now - deliberately NOT multiplied by the screen-scale
    // setting, so scaling the screen never resizes the menu. Both per-eye
    // screen swapchains share the same height, so either works here.
    const float metersPerPixel =
        m_screenSwapchainLeft.height != 0 ? kScreenQuadHeightMeters / static_cast<float>(m_screenSwapchainLeft.height) : 1.0f;

    const bool followHeadActive = m_settings.followHeadMode != FollowHeadMode::Off && m_headPoseValid;
    const XrQuaternionf &followOrientation =
        m_settings.followHeadMode == FollowHeadMode::Smooth ? m_smoothedHeadOrientation : m_headOrientation;
    const XrQuaternionf orientation = ComputeScreenOrientation(m_settings, followHeadActive, followOrientation);
    const XrVector3f forward = QuatRotateVector(orientation, XrVector3f{0.0f, 0.0f, -1.0f});

    quadLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    quadLayer.space = m_appSpace;
    quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    quadLayer.subImage.swapchain = m_menuSwapchain.handle;
    quadLayer.subImage.imageRect.offset = {0, 0};
    quadLayer.subImage.imageRect.extent = {m_menuSwapchain.width, m_menuSwapchain.height};
    quadLayer.subImage.imageArrayIndex = 0;
    quadLayer.pose.orientation = orientation;
    // Same direction as the screen layer (both centered on the view axis)
    // but at its own fixed distance - see kMenuDistanceMeters.
    quadLayer.pose.position = {forward.x * kMenuDistanceMeters, forward.y * kMenuDistanceMeters,
                               forward.z * kMenuDistanceMeters};
    // Resolution is PPD-driven, but physical size deliberately remains the
    // original scale-2 size. Otherwise selecting a sharper tier would also
    // make the panel larger in the headset.
    quadLayer.size = {kMenuWidth * kMenuScale * metersPerPixel, kMenuHeight * kMenuScale * metersPerPixel};
    return true;
}

void OpenXrApp::UpdateBatteryPercent(float deltaSeconds)
{
#if defined(__ANDROID__)
    m_batteryPollSeconds += deltaSeconds;
    if (m_batteryPollSeconds < 1.0f)
        return;
    m_batteryPollSeconds = 0.0f;
    m_appMenu.SetBatteryPercent(AndroidBridge::GetBatteryPercent());
#else
    (void)deltaSeconds; // no real battery to read (e.g. SteamVR on PC) - indicator stays hidden
#endif
}

void OpenXrApp::RenderFrame()
{
    XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    CheckXr(xrWaitFrame(m_session, &frameWaitInfo, &frameState), "xrWaitFrame");

    XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    CheckXr(xrBeginFrame(m_session, &frameBeginInfo), "xrBeginFrame");

    static auto lastFrameTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const float deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
    lastFrameTime = now;

    std::memcpy(m_lastButtonStates, m_buttonStates, sizeof(m_buttonStates));
    m_input.Sync(m_session);
    m_input.GetButtonStates(m_buttonStates);
    // XrInput always zeroes this slot (see its class comment) - a physical
    // gamepad isn't an OpenXR device, so it's fed in separately.
    m_buttonStates[ButtonMapper::DeviceGamepad] = m_gamepadButtonState;

    const bool menuButtonPressed = m_input.IsMenuButtonPressed() || m_gamepadMenuButtonPressed;
    if (menuButtonPressed && !m_lastMenuButtonPressed)
        m_appMenu.ToggleOpen();
    m_lastMenuButtonPressed = menuButtonPressed;

    m_appMenu.Update(m_buttonStates, m_lastButtonStates, deltaSeconds);
    UpdateBatteryPercent(deltaSeconds);

    // Mapping capture receives only real, hand-specific controller inputs.
    // Generic direction bits exist solely for menu navigation and are
    // deliberately excluded so they can never be saved as a gameplay bind.
    for (int device : {ButtonMapper::DeviceLeftTouch, ButtonMapper::DeviceRightTouch})
    {
        for (int bit = 0; bit < ButtonMapper::EmuButtonCount; ++bit)
        {
            if (bit >= static_cast<int>(ButtonMapper::EmuButton_Up) &&
                bit <= static_cast<int>(ButtonMapper::EmuButton_Right))
                continue;
            const uint32_t mask = ButtonMapper::ButtonMapping[bit];
            if ((m_buttonStates[device] & mask) && !(m_lastButtonStates[device] & mask))
                m_appMenu.SubmitRawMappingInput({true, device, bit});
        }
    }
    // A physical gamepad has no dedicated menu-nav hardware to disambiguate
    // from - unlike Touch, its D-pad/stick bits ARE the real buttons, so
    // they're eligible for capture too (matches pc2d's PollGameplayInput).
    for (int bit = 0; bit < ButtonMapper::EmuButtonCount; ++bit)
    {
        const uint32_t mask = ButtonMapper::ButtonMapping[bit];
        if ((m_buttonStates[ButtonMapper::DeviceGamepad] & mask) && !(m_lastButtonStates[ButtonMapper::DeviceGamepad] & mask))
            m_appMenu.SubmitRawMappingInput({true, ButtonMapper::DeviceGamepad, bit});
    }

    // Translated from m_buttonStates (already populated above by
    // m_input.GetButtonStates, extended in XrInput::Sync to also carry
    // per-hand stick/trigger/X/Y bits) via AppSettings::vbButtons - see
    // ButtonMapper::TranslateToVBBitmask's doc comment. Default bindings
    // (applied in Initialize when a slot has never been bound) mirror this
    // function's previous hardcoded mapping exactly: left thumbstick -> Left
    // D-Pad, right thumbstick -> Right D-Pad, A/B -> VB A/B, X/Y ->
    // Select/Start, left/right index triggers -> L/R. Only fed to the core
    // while the menu is closed, same reasoning as pc2d's Main.cpp.
    uint32_t gameplayButtonStates[3] = {m_buttonStates[0], m_buttonStates[1], m_buttonStates[2]};
    m_appMenu.ApplyGameplayInputSuppression(gameplayButtonStates);
    const uint32_t joypadBits = m_appMenu.IsOpen()
                                    ? 0
                                    : ButtonMapper::TranslateToVBBitmask(gameplayButtonStates, m_settings.vbButtons);
    m_emulator.SetGameplayInput(joypadBits);
    // Pause emulation while the menu is open so gameplay doesn't run away
    // unseen behind it - same as pc2d's Main.cpp. The screen layer keeps
    // redrawing the last streamed frame.
    if (!m_appMenu.IsOpen())
    {
        m_emulator.RunFrame(deltaSeconds);
    }

    std::vector<XrCompositionLayerBaseHeader *> layers;
    XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::vector<XrCompositionLayerProjectionView> projectionViews;
    if (frameState.shouldRender)
    {
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCountOutput = 0;
        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = m_viewConfigType;
        locateInfo.displayTime = frameState.predictedDisplayTime;
        locateInfo.space = m_appSpace;
        CheckXr(xrLocateViews(m_session, &locateInfo, &viewState, static_cast<uint32_t>(m_views.size()), &viewCountOutput,
                              m_views.data()),
                "xrLocateViews");

        const bool posesValid = (viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) &&
                                (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT);

        // Approximate "head orientation" as view 0's (left eye's) - only
        // used for the Follow Head setting's targets, so the small
        // difference from a true head-center pose doesn't matter. Left
        // false/identity on frames without a fresh valid pose (m_headOrientation
        // then just keeps whatever it was last set to) rather than snapping
        // the screen every time tracking briefly drops.
        m_headPoseValid = posesValid && viewCountOutput > 0;
        if (m_headPoseValid)
        {
            m_headOrientation = m_views[0].pose.orientation;
            // Kept chasing the live head orientation every frame regardless
            // of the active mode, so switching into FollowHeadMode::Smooth
            // never starts with a jarring snap from a stale smoothed value.
            const float t = std::clamp(kFollowHeadSmoothSpeed * deltaSeconds, 0.0f, 1.0f);
            m_smoothedHeadOrientation = QuatSlerp(m_smoothedHeadOrientation, m_headOrientation, t);
            UpdateMenuRenderScale();
        }

        if (posesValid)
        {
            projectionViews.resize(viewCountOutput);

            // Main eye buffers stay plain black - all real content lives in
            // the quad layers below (menu/emulator screen).
            for (uint32_t i = 0; i < viewCountOutput; ++i)
            {
                Swapchain &sc = m_swapchains[i];

                XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                uint32_t imageIndex = 0;
                CheckXr(xrAcquireSwapchainImage(sc.handle, &acquireInfo, &imageIndex),
                        "xrAcquireSwapchainImage (projection)");

                XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitInfo.timeout = XR_INFINITE_DURATION;
                CheckXr(xrWaitSwapchainImage(sc.handle, &waitInfo), "xrWaitSwapchainImage (projection)");

                projectionViews[i] = XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                projectionViews[i].pose = m_views[i].pose;
                projectionViews[i].fov = m_views[i].fov;
                projectionViews[i].subImage.swapchain = sc.handle;
                projectionViews[i].subImage.imageRect.offset = {0, 0};
                projectionViews[i].subImage.imageRect.extent = {sc.width, sc.height};

                m_renderer.RenderEye(sc.images[imageIndex].image, m_colorFormat, static_cast<uint32_t>(sc.width),
                                     static_cast<uint32_t>(sc.height));

                XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CheckXr(xrReleaseSwapchainImage(sc.handle, &releaseInfo), "xrReleaseSwapchainImage (projection)");
            }

            projectionLayer.space = m_appSpace;
            projectionLayer.viewCount = static_cast<uint32_t>(projectionViews.size());
            projectionLayer.views = projectionViews.data();
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&projectionLayer));
        }
    }

    // Screen first, menu second - the compositor blends layers back-to-front
    // in submission order, and the menu should end up in front. Two screen
    // layers (left/right eye), quad or cylinder depending on the curved-
    // screen setting - see RenderScreenLayer.
    XrCompositionLayerQuad screenQuadLeft{XR_TYPE_COMPOSITION_LAYER_QUAD};
    XrCompositionLayerQuad screenQuadRight{XR_TYPE_COMPOSITION_LAYER_QUAD};
    XrCompositionLayerCylinderKHR screenCylinderLeft{XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
    XrCompositionLayerCylinderKHR screenCylinderRight{XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
    bool screenIsCylinder = false;
    if (frameState.shouldRender && RenderScreenLayer(screenQuadLeft, screenQuadRight, screenCylinderLeft,
                                                     screenCylinderRight, screenIsCylinder))
    {
        if (screenIsCylinder)
        {
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&screenCylinderLeft));
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&screenCylinderRight));
        }
        else
        {
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&screenQuadLeft));
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&screenQuadRight));
        }
    }
    XrCompositionLayerQuad menuLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if (frameState.shouldRender && RenderMenuLayer(menuLayer))
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&menuLayer));

    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
    frameEndInfo.displayTime = frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
    frameEndInfo.layers = layers.data();
    CheckXr(xrEndFrame(m_session, &frameEndInfo), "xrEndFrame");
}

void OpenXrApp::Shutdown()
{
    // Swapchains own Vulkan images backed by our VkDevice, so they (and the
    // session) must be torn down before the renderer destroys that device -
    // otherwise xrDestroySwapchain's driver-side vkDestroyImage call
    // segfaults on an already-destroyed device (found via a real crash on
    // Quest when losing focus).
    for (Swapchain &sc : m_swapchains)
    {
        if (sc.handle != XR_NULL_HANDLE)
            xrDestroySwapchain(sc.handle);
    }
    if (m_screenSwapchainLeft.handle != XR_NULL_HANDLE)
        xrDestroySwapchain(m_screenSwapchainLeft.handle);
    if (m_screenSwapchainRight.handle != XR_NULL_HANDLE)
        xrDestroySwapchain(m_screenSwapchainRight.handle);
    if (m_menuSwapchain.handle != XR_NULL_HANDLE)
        xrDestroySwapchain(m_menuSwapchain.handle);
    m_screenSwapchainLeft.handle = XR_NULL_HANDLE;
    m_screenSwapchainRight.handle = XR_NULL_HANDLE;
    m_menuSwapchain.handle = XR_NULL_HANDLE;
    m_swapchains.clear();
    if (m_appSpace != XR_NULL_HANDLE)
        xrDestroySpace(m_appSpace);
    m_input.Shutdown();
    if (m_session != XR_NULL_HANDLE)
        xrDestroySession(m_session);
    m_uiRenderer.Shutdown();
    m_renderer.Shutdown();
    if (m_instance != XR_NULL_HANDLE)
        xrDestroyInstance(m_instance);
    m_appSpace = XR_NULL_HANDLE;
    m_session = XR_NULL_HANDLE;
    m_instance = XR_NULL_HANDLE;
}
