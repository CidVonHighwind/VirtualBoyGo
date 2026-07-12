#include "OpenXrApp.h"

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

    m_appMenu.Initialize(m_uiRenderer, static_cast<VkFormat>(m_colorFormat));

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

    // Dedicated swapchain for the emulator screen's quad composition layer,
    // sized to the emulator's screen (at a fixed pixel-perfect upscale) so
    // it renders 1:1 instead of being scaled. Falls back to the menu's own
    // size if no screen is loaded.
    m_screenSwapchain.width = m_emulator.HasScreen() ? static_cast<int32_t>(m_emulator.GetScreenWidth() * Emulator::kScale)
                                                     : static_cast<int32_t>(kMenuWidth * kMenuScale);
    m_screenSwapchain.height = m_emulator.HasScreen() ? static_cast<int32_t>(m_emulator.GetScreenHeight() * Emulator::kScale)
                                                      : static_cast<int32_t>(kMenuHeight * kMenuScale);
    m_screenSwapchain.mipLevels =
        ComputeMipLevels(static_cast<uint32_t>(m_screenSwapchain.width), static_cast<uint32_t>(m_screenSwapchain.height));

    XrSwapchainCreateInfo screenSwapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    screenSwapchainInfo.arraySize = 1;
    screenSwapchainInfo.format = m_colorFormat;
    screenSwapchainInfo.width = m_screenSwapchain.width;
    screenSwapchainInfo.height = m_screenSwapchain.height;
    screenSwapchainInfo.mipCount = m_screenSwapchain.mipLevels;
    screenSwapchainInfo.faceCount = 1;
    screenSwapchainInfo.sampleCount = 1;
    screenSwapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    CheckXr(xrCreateSwapchain(m_session, &screenSwapchainInfo, &m_screenSwapchain.handle), "xrCreateSwapchain (screen)");

    uint32_t screenImageCount = 0;
    CheckXr(xrEnumerateSwapchainImages(m_screenSwapchain.handle, 0, &screenImageCount, nullptr),
            "xrEnumerateSwapchainImages (screen count)");
    m_screenSwapchain.images.resize(screenImageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    CheckXr(xrEnumerateSwapchainImages(m_screenSwapchain.handle, screenImageCount, &screenImageCount,
                                       reinterpret_cast<XrSwapchainImageBaseHeader *>(m_screenSwapchain.images.data())),
            "xrEnumerateSwapchainImages (screen)");

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
    // Distance from the viewer to the screen layer, and how much closer (in
    // meters) the menu layer floats in front of it - gives the menu a real
    // depth cue instead of sitting flush on the same plane as the screen.
    constexpr float kScreenDistanceMeters = 2.2f;
    constexpr float kMenuForwardOffsetMeters = 0.05f;
} // namespace

bool OpenXrApp::RenderScreenLayer(XrCompositionLayerQuad &quadLayer)
{
    if (m_screenSwapchain.handle == XR_NULL_HANDLE)
    {
        return false;
    }

    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t imageIndex = 0;
    CheckXr(xrAcquireSwapchainImage(m_screenSwapchain.handle, &acquireInfo, &imageIndex), "xrAcquireSwapchainImage (screen)");

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    CheckXr(xrWaitSwapchainImage(m_screenSwapchain.handle, &waitInfo), "xrWaitSwapchainImage (screen)");

    m_uiRenderer.BeginFrame(m_screenSwapchain.images[imageIndex].image, static_cast<VkFormat>(m_colorFormat),
                            static_cast<uint32_t>(m_screenSwapchain.width), static_cast<uint32_t>(m_screenSwapchain.height),
                            m_appMenu.GetBackgroundColor());
    if (m_emulator.HasScreen())
    {
        m_emulator.DrawScreen(m_uiRenderer, 0.0f, 0.0f, static_cast<float>(m_screenSwapchain.width),
                              static_cast<float>(m_screenSwapchain.height));
    }
    m_uiRenderer.EndFrame();
    m_renderer.GenerateMipmaps(m_screenSwapchain.images[imageIndex].image, static_cast<uint32_t>(m_screenSwapchain.width),
                               static_cast<uint32_t>(m_screenSwapchain.height), m_screenSwapchain.mipLevels);

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    CheckXr(xrReleaseSwapchainImage(m_screenSwapchain.handle, &releaseInfo), "xrReleaseSwapchainImage (screen)");

    // Sized so the screen is comfortably viewable, keeping the swapchain's
    // native aspect ratio.
    const float aspect = m_screenSwapchain.height != 0
                             ? static_cast<float>(m_screenSwapchain.width) / static_cast<float>(m_screenSwapchain.height)
                             : 1.0f;
    const float quadHeight = 1.2f;

    quadLayer.layerFlags = 0;
    quadLayer.space = m_appSpace;
    quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    quadLayer.subImage.swapchain = m_screenSwapchain.handle;
    quadLayer.subImage.imageRect.offset = {0, 0};
    quadLayer.subImage.imageRect.extent = {m_screenSwapchain.width, m_screenSwapchain.height};
    quadLayer.subImage.imageArrayIndex = 0;
    quadLayer.pose.orientation.w = 1.0f;
    quadLayer.pose.position = {0.0f, 0.0f, -kScreenDistanceMeters};
    quadLayer.size = {quadHeight * aspect, quadHeight};
    return true;
}

bool OpenXrApp::RenderMenuLayer(XrCompositionLayerQuad &quadLayer)
{
    if (m_menuSwapchain.handle == XR_NULL_HANDLE)
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
    // appear to change size just for being on its own swapchain now.
    const float screenQuadHeight = 1.2f;
    const float metersPerPixel =
        m_screenSwapchain.height != 0 ? screenQuadHeight / static_cast<float>(m_screenSwapchain.height) : 1.0f;

    quadLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
    quadLayer.space = m_appSpace;
    quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    quadLayer.subImage.swapchain = m_menuSwapchain.handle;
    quadLayer.subImage.imageRect.offset = {0, 0};
    quadLayer.subImage.imageRect.extent = {m_menuSwapchain.width, m_menuSwapchain.height};
    quadLayer.subImage.imageArrayIndex = 0;
    quadLayer.pose.orientation.w = 1.0f;
    // Same X/Y as the screen layer (both centered on the view axis) but
    // closer to the viewer - that's what actually reads as "in front of".
    quadLayer.pose.position = {0.0f, 0.0f, -(kScreenDistanceMeters - kMenuForwardOffsetMeters)};
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
    m_appMenu.Update(m_buttonStates, m_lastButtonStates, deltaSeconds);

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
    // in submission order, and the menu should end up in front.
    XrCompositionLayerQuad screenLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if (RenderScreenLayer(screenLayer))
    {
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader *>(&screenLayer));
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
    if (m_screenSwapchain.handle != XR_NULL_HANDLE)
        xrDestroySwapchain(m_screenSwapchain.handle);
    if (m_menuSwapchain.handle != XR_NULL_HANDLE)
        xrDestroySwapchain(m_menuSwapchain.handle);
    m_screenSwapchain.handle = XR_NULL_HANDLE;
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
