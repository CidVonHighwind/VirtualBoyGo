#include "OpenXrApp.h"
#include "ui/ButtonMapping.h"

#include <openxr/openxr_platform.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace
{

    void CheckXr(XrResult result, const char *what)
    {
        if (XR_FAILED(result))
        {
            throw std::runtime_error(std::string("OpenXR call failed: ") + what + " (" + std::to_string(result) + ")");
        }
    }

    // Full mip chain (down to 1x1) - the compositor's own resampling of a quad
    // composition layer onto the eye buffers is a step entirely outside our
    // rendering, and without mips that resampling aliases/shimmers whenever the
    // layer is minified (viewed smaller than native resolution, e.g. far away),
    // no matter how carefully mip 0 itself was drawn.
    uint32_t ComputeMipLevels(uint32_t width, uint32_t height)
    {
        return static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(std::max(width, height))))) + 1;
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

    // yaw (Y axis) * pitch (X axis) * roll (Z axis), optionally premultiplied
    // by the current head orientation when followHead is on - same
    // composition FrontendGo's CylinderModelMatrix used, just as a
    // quaternion instead of 3 rotation matrices. When followHead is off,
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
            MappedButton &b = settings.vbButtons[vbBit];
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

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = info.instanceCreateNext;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.data();
    std::strncpy(createInfo.applicationInfo.applicationName, "VirtualBoyGo", XR_MAX_APPLICATION_NAME_SIZE - 1);
    createInfo.applicationInfo.applicationVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

    CheckXr(xrCreateInstance(&createInfo, &m_instance), "xrCreateInstance");
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
        CheckXr(xrCreateSwapchain(m_session, &swapchainInfo, &sc.handle), "xrCreateSwapchain");

        uint32_t imageCount = 0;
        CheckXr(xrEnumerateSwapchainImages(sc.handle, 0, &imageCount, nullptr), "xrEnumerateSwapchainImages (count)");
        sc.images.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        CheckXr(xrEnumerateSwapchainImages(sc.handle, imageCount, &imageCount,
                                           reinterpret_cast<XrSwapchainImageBaseHeader *>(sc.images.data())),
                "xrEnumerateSwapchainImages");
    }

    // Dedicated per-eye swapchains for the emulator screen's quad
    // composition layers, each sized to *half* the emulator's (side-by-side,
    // both eyes) screen width at a fixed pixel-perfect upscale, so each
    // renders 1:1 instead of being scaled - see OpenXrApp.h's member comment
    // for why two independent swapchains instead of one shared/cropped one.
    // Falls back to the menu's own size if no screen is loaded.
    const int32_t screenWidth = m_emulator.HasScreen() ? static_cast<int32_t>(m_emulator.GetScreenWidth() * Emulator::kScale)
                                                        : static_cast<int32_t>(kMenuWidth * kMenuScale);
    const int32_t screenHeight = m_emulator.HasScreen() ? static_cast<int32_t>(m_emulator.GetScreenHeight() * Emulator::kScale)
                                                         : static_cast<int32_t>(kMenuHeight * kMenuScale);
    const int32_t eyeWidth = m_emulator.HasScreen() ? screenWidth / 2 : screenWidth;

    for (Swapchain *sc : {&m_screenSwapchainLeft, &m_screenSwapchainRight})
    {
        sc->width = eyeWidth;
        sc->height = screenHeight;
        sc->mipLevels = ComputeMipLevels(static_cast<uint32_t>(sc->width), static_cast<uint32_t>(sc->height));

        XrSwapchainCreateInfo screenSwapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        screenSwapchainInfo.arraySize = 1;
        screenSwapchainInfo.format = m_colorFormat;
        screenSwapchainInfo.width = sc->width;
        screenSwapchainInfo.height = sc->height;
        screenSwapchainInfo.mipCount = sc->mipLevels;
        screenSwapchainInfo.faceCount = 1;
        screenSwapchainInfo.sampleCount = 1;
        screenSwapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        CheckXr(xrCreateSwapchain(m_session, &screenSwapchainInfo, &sc->handle), "xrCreateSwapchain (screen)");

        uint32_t screenImageCount = 0;
        CheckXr(xrEnumerateSwapchainImages(sc->handle, 0, &screenImageCount, nullptr),
                "xrEnumerateSwapchainImages (screen count)");
        sc->images.resize(screenImageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        CheckXr(xrEnumerateSwapchainImages(sc->handle, screenImageCount, &screenImageCount,
                                           reinterpret_cast<XrSwapchainImageBaseHeader *>(sc->images.data())),
                "xrEnumerateSwapchainImages (screen)");
    }

    // Dedicated swapchain for the menu's own quad composition layer - see
    // OpenXrApp.h's member comment for why this is separate from the screen.
    // kMenuWidth/kMenuHeight are logical units (see AppMenuLayout.h) -
    // AppMenu::Draw composites its offscreen texture at kMenuWidth*kMenuScale
    // physical pixels, so this swapchain has to match that, not the raw
    // logical size.
    m_menuSwapchain.width = static_cast<int32_t>(kMenuWidth * kMenuScale);
    m_menuSwapchain.height = static_cast<int32_t>(kMenuHeight * kMenuScale);
    m_menuSwapchain.mipLevels =
        ComputeMipLevels(static_cast<uint32_t>(m_menuSwapchain.width), static_cast<uint32_t>(m_menuSwapchain.height));

    XrSwapchainCreateInfo menuSwapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    menuSwapchainInfo.arraySize = 1;
    menuSwapchainInfo.format = m_colorFormat;
    menuSwapchainInfo.width = m_menuSwapchain.width;
    menuSwapchainInfo.height = m_menuSwapchain.height;
    menuSwapchainInfo.mipCount = m_menuSwapchain.mipLevels;
    menuSwapchainInfo.faceCount = 1;
    menuSwapchainInfo.sampleCount = 1;
    menuSwapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    CheckXr(xrCreateSwapchain(m_session, &menuSwapchainInfo, &m_menuSwapchain.handle), "xrCreateSwapchain (menu)");

    uint32_t menuImageCount = 0;
    CheckXr(xrEnumerateSwapchainImages(m_menuSwapchain.handle, 0, &menuImageCount, nullptr),
            "xrEnumerateSwapchainImages (menu count)");
    m_menuSwapchain.images.resize(menuImageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    CheckXr(xrEnumerateSwapchainImages(m_menuSwapchain.handle, menuImageCount, &menuImageCount,
                                       reinterpret_cast<XrSwapchainImageBaseHeader *>(m_menuSwapchain.images.data())),
            "xrEnumerateSwapchainImages (menu)");
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

namespace
{
    // How much closer (in meters) the menu layer floats in front of the
    // screen layer - gives the menu a real depth cue instead of sitting
    // flush on the same plane as the screen. The screen's own distance is
    // now AppSettings::screenDistance (user-adjustable via MoveScreenPage),
    // not a fixed constant.
    constexpr float kMenuForwardOffsetMeters = 0.05f;
} // namespace

bool OpenXrApp::RenderScreenLayer(XrCompositionLayerQuad &leftQuadLayer, XrCompositionLayerQuad &rightQuadLayer)
{
    if (m_screenSwapchainLeft.handle == XR_NULL_HANDLE || m_screenSwapchainRight.handle == XR_NULL_HANDLE)
    {
        return false;
    }

    const float aspect = m_screenSwapchainLeft.height != 0
                             ? static_cast<float>(m_screenSwapchainLeft.width) / static_cast<float>(m_screenSwapchainLeft.height)
                             : 1.0f;
    const float quadHeight = 1.2f * m_settings.screenScale;

    const bool followHeadActive = m_settings.followHead && m_headPoseValid;
    const XrQuaternionf orientation = ComputeScreenOrientation(m_settings, followHeadActive, m_headOrientation);
    const XrVector3f forward = QuatRotateVector(orientation, XrVector3f{0.0f, 0.0f, -1.0f});
    const XrVector3f right = QuatRotateVector(orientation, XrVector3f{1.0f, 0.0f, 0.0f});
    const XrVector3f basePosition{forward.x * m_settings.screenDistance, forward.y * m_settings.screenDistance,
                                  forward.z * m_settings.screenDistance};
    const XrColor4f tint{m_settings.colorR, m_settings.colorG, m_settings.colorB, 1.0f};

    // 2D mode: both eyes draw the same Left crop (mono) instead of splitting
    // Left/Right, and no IPD spatial offset is applied below.
    const Emulator::Eye rightEyeCrop = m_settings.useThreeDeeMode ? Emulator::Eye::Right : Emulator::Eye::Left;

    struct EyeInfo
    {
        Swapchain *sc;
        XrCompositionLayerQuad *quadLayer;
        XrEyeVisibility visibility;
        Emulator::Eye eye;
        float ipdSign; // which side of center this eye offsets toward
    };
    const EyeInfo eyes[] = {
        {&m_screenSwapchainLeft, &leftQuadLayer, XR_EYE_VISIBILITY_LEFT, Emulator::Eye::Left, -1.0f},
        {&m_screenSwapchainRight, &rightQuadLayer, XR_EYE_VISIBILITY_RIGHT, rightEyeCrop, 1.0f},
    };

    for (const EyeInfo &eyeInfo : eyes)
    {
        Swapchain &sc = *eyeInfo.sc;

        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t imageIndex = 0;
        CheckXr(xrAcquireSwapchainImage(sc.handle, &acquireInfo, &imageIndex), "xrAcquireSwapchainImage (screen)");

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        CheckXr(xrWaitSwapchainImage(sc.handle, &waitInfo), "xrWaitSwapchainImage (screen)");

        m_uiRenderer.BeginFrame(sc.images[imageIndex].image, static_cast<VkFormat>(m_colorFormat),
                                static_cast<uint32_t>(sc.width), static_cast<uint32_t>(sc.height),
                                m_appMenu.GetBackgroundColor());
        if (m_emulator.HasScreen())
        {
            m_emulator.DrawScreen(m_uiRenderer, 0.0f, 0.0f, static_cast<float>(sc.width), static_cast<float>(sc.height),
                                  eyeInfo.eye, tint);
        }
        m_uiRenderer.EndFrame();
        m_renderer.GenerateMipmaps(sc.images[imageIndex].image, static_cast<uint32_t>(sc.width),
                                   static_cast<uint32_t>(sc.height), sc.mipLevels);

        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        CheckXr(xrReleaseSwapchainImage(sc.handle, &releaseInfo), "xrReleaseSwapchainImage (screen)");

        XrCompositionLayerQuad &quadLayer = *eyeInfo.quadLayer;
        quadLayer.layerFlags = 0;
        quadLayer.space = m_appSpace;
        quadLayer.eyeVisibility = eyeInfo.visibility;
        quadLayer.subImage.swapchain = sc.handle;
        quadLayer.subImage.imageRect.offset = {0, 0};
        quadLayer.subImage.imageRect.extent = {sc.width, sc.height};
        quadLayer.subImage.imageArrayIndex = 0;
        quadLayer.pose.orientation = orientation;

        const float ipdHalf = m_settings.useThreeDeeMode ? (m_settings.ipdOffset * 0.5f * eyeInfo.ipdSign) : 0.0f;
        quadLayer.pose.position = {basePosition.x + right.x * ipdHalf, basePosition.y + right.y * ipdHalf,
                                   basePosition.z + right.z * ipdHalf};
        quadLayer.size = {quadHeight * aspect, quadHeight};
    }
    return true;
}

bool OpenXrApp::RenderMenuLayer(XrCompositionLayerQuad &quadLayer)
{
    if (m_menuSwapchain.handle == XR_NULL_HANDLE || !m_appMenu.IsOpen())
    {
        return false;
    }

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
    m_renderer.GenerateMipmaps(m_menuSwapchain.images[imageIndex].image, static_cast<uint32_t>(m_menuSwapchain.width),
                               static_cast<uint32_t>(m_menuSwapchain.height), m_menuSwapchain.mipLevels);

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    CheckXr(xrReleaseSwapchainImage(m_menuSwapchain.handle, &releaseInfo), "xrReleaseSwapchainImage (menu)");

    // Same meters-per-pixel scale as the screen layer, so the menu doesn't
    // appear to change size just for being on its own swapchain now. Both
    // per-eye screen swapchains share the same height, so either works here.
    const float screenQuadHeight = 1.2f * m_settings.screenScale;
    const float metersPerPixel =
        m_screenSwapchainLeft.height != 0 ? screenQuadHeight / static_cast<float>(m_screenSwapchainLeft.height) : 1.0f;

    const bool followHeadActive = m_settings.followHead && m_headPoseValid;
    const XrQuaternionf orientation = ComputeScreenOrientation(m_settings, followHeadActive, m_headOrientation);
    const XrVector3f forward = QuatRotateVector(orientation, XrVector3f{0.0f, 0.0f, -1.0f});
    const float menuDistance = m_settings.screenDistance - kMenuForwardOffsetMeters;

    quadLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    quadLayer.space = m_appSpace;
    quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    quadLayer.subImage.swapchain = m_menuSwapchain.handle;
    quadLayer.subImage.imageRect.offset = {0, 0};
    quadLayer.subImage.imageRect.extent = {m_menuSwapchain.width, m_menuSwapchain.height};
    quadLayer.subImage.imageArrayIndex = 0;
    quadLayer.pose.orientation = orientation;
    // Same direction as the screen layer (both centered on the view axis)
    // but closer to the viewer - that's what actually reads as "in front of".
    quadLayer.pose.position = {forward.x * menuDistance, forward.y * menuDistance, forward.z * menuDistance};
    // metersPerPixel is calibrated against actual swapchain pixels, so use
    // the swapchain's own (physical) size here, not the logical kMenuWidth/
    // kMenuHeight - otherwise the panel would render at half its intended
    // real-world size in the headset.
    quadLayer.size = {m_menuSwapchain.width * metersPerPixel, m_menuSwapchain.height * metersPerPixel};
    return true;
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

    const bool menuButtonPressed = m_input.IsMenuButtonPressed();
    if (menuButtonPressed && !m_lastMenuButtonPressed)
        m_appMenu.ToggleOpen();
    m_lastMenuButtonPressed = menuButtonPressed;

    m_appMenu.Update(m_buttonStates, m_lastButtonStates, deltaSeconds);

    // Translated from m_buttonStates (already populated above by
    // m_input.GetButtonStates, extended in XrInput::Sync to also carry
    // per-hand stick/trigger/X/Y bits) via AppSettings::vbButtons - see
    // ButtonMapper::TranslateToVBBitmask's doc comment. Default bindings
    // (applied in Initialize when a slot has never been bound) mirror this
    // function's previous hardcoded mapping exactly: left thumbstick -> Left
    // D-Pad, right thumbstick -> Right D-Pad, A/B -> VB A/B, X/Y ->
    // Select/Start, left/right index triggers -> L/R. Only fed to the core
    // while the menu is closed, same reasoning as pc2d's Main.cpp.
    const uint32_t joypadBits = m_appMenu.IsOpen() ? 0 : ButtonMapper::TranslateToVBBitmask(m_buttonStates, m_settings.vbButtons);
    m_emulator.SetGameplayInput(joypadBits);
    m_emulator.RunFrame(deltaSeconds);

    std::vector<XrCompositionLayerBaseHeader *> layers;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
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
        // used for the Follow Head setting's slerp target, so the small
        // difference from a true head-center pose doesn't matter. Left
        // false/identity on frames without a fresh valid pose (m_headOrientation
        // then just keeps whatever it was last set to) rather than snapping
        // the screen every time tracking briefly drops.
        m_headPoseValid = posesValid && viewCountOutput > 0;
        if (m_headPoseValid)
            m_headOrientation = m_views[0].pose.orientation;

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
                CheckXr(xrAcquireSwapchainImage(sc.handle, &acquireInfo, &imageIndex), "xrAcquireSwapchainImage");

                XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                waitInfo.timeout = XR_INFINITE_DURATION;
                CheckXr(xrWaitSwapchainImage(sc.handle, &waitInfo), "xrWaitSwapchainImage");

                projectionViews[i] = XrCompositionLayerProjectionView{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
                projectionViews[i].pose = m_views[i].pose;
                projectionViews[i].fov = m_views[i].fov;
                projectionViews[i].subImage.swapchain = sc.handle;
                projectionViews[i].subImage.imageRect.offset = {0, 0};
                projectionViews[i].subImage.imageRect.extent = {sc.width, sc.height};

                m_renderer.RenderEye(sc.images[imageIndex].image, m_colorFormat, static_cast<uint32_t>(sc.width),
                                     static_cast<uint32_t>(sc.height));

                XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CheckXr(xrReleaseSwapchainImage(sc.handle, &releaseInfo), "xrReleaseSwapchainImage");
            }

            layer.space = m_appSpace;
            layer.viewCount = static_cast<uint32_t>(projectionViews.size());
            layer.views = projectionViews.data();
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));
        }
    }

    // Screen first, menu second - the compositor blends layers back-to-front
    // in submission order, and the menu should end up in front. Two screen
    // quads (left/right eye) - see RenderScreenLayer.
    XrCompositionLayerQuad screenLayerLeft{XR_TYPE_COMPOSITION_LAYER_QUAD};
    XrCompositionLayerQuad screenLayerRight{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if (RenderScreenLayer(screenLayerLeft, screenLayerRight))
    {
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&screenLayerLeft));
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&screenLayerRight));
    }
    XrCompositionLayerQuad menuLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if (RenderMenuLayer(menuLayer))
    {
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&menuLayer));
    }

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
