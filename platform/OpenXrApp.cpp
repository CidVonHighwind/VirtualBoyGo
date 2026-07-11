#include "OpenXrApp.h"
#include "AssetLoader.h"

#include <openxr/openxr_platform.h>

#include <cstring>
#include <stdexcept>

namespace {

void CheckXr(XrResult result, const char* what) {
    if (XR_FAILED(result)) {
        throw std::runtime_error(std::string("OpenXR call failed: ") + what + " (" + std::to_string(result) + ")");
    }
}

}  // namespace

void OpenXrApp::Initialize(const InitInfo& info) {
    CreateInstance(info);
    InitializeSystem();
    m_renderer.CreateDevice(m_instance, m_systemId);

    const std::vector<uint8_t> imageBytes = LoadAssetBytes("test_image.jpg");
    if (!imageBytes.empty()) {
        m_testImageLoaded = m_renderer.LoadTestImage(imageBytes, m_testImageWidth, m_testImageHeight);
    }

    InitializeSession();
    CreateSwapchains();
}

void OpenXrApp::CreateInstance(const InitInfo& info) {
    std::vector<const char*> extensions = {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
#if defined(XR_USE_PLATFORM_ANDROID)
    if (info.isAndroid) {
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

void OpenXrApp::InitializeSystem() {
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

void OpenXrApp::InitializeSession() {
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

void OpenXrApp::CreateSwapchains() {
    uint32_t formatCount = 0;
    CheckXr(xrEnumerateSwapchainFormats(m_session, 0, &formatCount, nullptr), "xrEnumerateSwapchainFormats (count)");
    std::vector<int64_t> formats(formatCount);
    CheckXr(xrEnumerateSwapchainFormats(m_session, formatCount, &formatCount, formats.data()),
            "xrEnumerateSwapchainFormats");
    m_colorFormat = m_renderer.SelectSwapchainFormat(formats);

    m_swapchains.resize(m_configViews.size());
    for (size_t i = 0; i < m_configViews.size(); ++i) {
        const XrViewConfigurationView& vp = m_configViews[i];
        Swapchain& sc = m_swapchains[i];
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
                                           reinterpret_cast<XrSwapchainImageBaseHeader*>(sc.images.data())),
                "xrEnumerateSwapchainImages");
    }

    // Dedicated swapchain for the quad composition layer test - sized to
    // match the loaded test image (falls back to a square if it's missing)
    // so it renders at native resolution instead of being scaled.
    m_quadSwapchain.width = m_testImageLoaded ? static_cast<int32_t>(m_testImageWidth) : 512;
    m_quadSwapchain.height = m_testImageLoaded ? static_cast<int32_t>(m_testImageHeight) : 512;

    XrSwapchainCreateInfo quadSwapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    quadSwapchainInfo.arraySize = 1;
    quadSwapchainInfo.format = m_colorFormat;
    quadSwapchainInfo.width = m_quadSwapchain.width;
    quadSwapchainInfo.height = m_quadSwapchain.height;
    quadSwapchainInfo.mipCount = 1;
    quadSwapchainInfo.faceCount = 1;
    quadSwapchainInfo.sampleCount = 1;
    quadSwapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    CheckXr(xrCreateSwapchain(m_session, &quadSwapchainInfo, &m_quadSwapchain.handle), "xrCreateSwapchain (quad)");

    uint32_t quadImageCount = 0;
    CheckXr(xrEnumerateSwapchainImages(m_quadSwapchain.handle, 0, &quadImageCount, nullptr),
            "xrEnumerateSwapchainImages (quad count)");
    m_quadSwapchain.images.resize(quadImageCount, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    CheckXr(xrEnumerateSwapchainImages(m_quadSwapchain.handle, quadImageCount, &quadImageCount,
                                       reinterpret_cast<XrSwapchainImageBaseHeader*>(m_quadSwapchain.images.data())),
            "xrEnumerateSwapchainImages (quad)");
}

void OpenXrApp::HandleSessionStateChanged(const XrEventDataSessionStateChanged& event, bool& exitRenderLoop,
                                          bool& requestRestart) {
    m_sessionState = event.state;

    switch (m_sessionState) {
        case XR_SESSION_STATE_READY: {
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

void OpenXrApp::PollEvents(bool& exitRenderLoop, bool& requestRestart) {
    exitRenderLoop = false;
    requestRestart = false;

    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(m_instance, &event) == XR_SUCCESS) {
        switch (event.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                HandleSessionStateChanged(*reinterpret_cast<const XrEventDataSessionStateChanged*>(&event), exitRenderLoop,
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

bool OpenXrApp::RenderQuadLayer(XrCompositionLayerQuad& quadLayer) {
    if (m_quadSwapchain.handle == XR_NULL_HANDLE) {
        return false;
    }

    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t imageIndex = 0;
    CheckXr(xrAcquireSwapchainImage(m_quadSwapchain.handle, &acquireInfo, &imageIndex), "xrAcquireSwapchainImage (quad)");

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    CheckXr(xrWaitSwapchainImage(m_quadSwapchain.handle, &waitInfo), "xrWaitSwapchainImage (quad)");

    if (m_testImageLoaded) {
        m_renderer.RenderTexturedQuad(m_quadSwapchain.images[imageIndex].image, m_colorFormat,
                                      static_cast<uint32_t>(m_quadSwapchain.width),
                                      static_cast<uint32_t>(m_quadSwapchain.height));
    } else {
        // Fallback (image failed to load): a fixed, front-facing "camera" so
        // RenderEye still draws *something* distinct into this swapchain.
        XrPosef fakeEyePose{};
        fakeEyePose.orientation.w = 1.0f;
        XrFovf fakeFov{-0.6f, 0.6f, 0.6f, -0.6f};

        std::vector<DrawCube> cubes;
        DrawCube cube{};
        cube.pose.orientation.w = 1.0f;
        cube.pose.position = {0.0f, 0.0f, -1.0f};
        cube.scale = {0.4f, 0.4f, 0.4f};
        cubes.push_back(cube);

        m_renderer.RenderEye(m_quadSwapchain.images[imageIndex].image, m_colorFormat,
                             static_cast<uint32_t>(m_quadSwapchain.width), static_cast<uint32_t>(m_quadSwapchain.height),
                             fakeEyePose, fakeFov, cubes);
    }

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    CheckXr(xrReleaseSwapchainImage(m_quadSwapchain.handle, &releaseInfo), "xrReleaseSwapchainImage (quad)");

    // Sized bigger and further out so a high-res image is comfortably
    // viewable, keeping the swapchain's native aspect ratio.
    const float aspect =
        m_quadSwapchain.height != 0 ? static_cast<float>(m_quadSwapchain.width) / static_cast<float>(m_quadSwapchain.height) : 1.0f;
    const float quadHeight = 1.2f;

    quadLayer.layerFlags = 0;
    quadLayer.space = m_appSpace;
    quadLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    quadLayer.subImage.swapchain = m_quadSwapchain.handle;
    quadLayer.subImage.imageRect.offset = {0, 0};
    quadLayer.subImage.imageRect.extent = {m_quadSwapchain.width, m_quadSwapchain.height};
    quadLayer.subImage.imageArrayIndex = 0;
    quadLayer.pose.orientation.w = 1.0f;
    quadLayer.pose.position = {0.0f, 0.0f, -2.2f};
    quadLayer.size = {quadHeight * aspect, quadHeight};
    return true;
}

void OpenXrApp::RenderFrame() {
    XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    CheckXr(xrWaitFrame(m_session, &frameWaitInfo, &frameState), "xrWaitFrame");

    XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    CheckXr(xrBeginFrame(m_session, &frameBeginInfo), "xrBeginFrame");

    std::vector<XrCompositionLayerBaseHeader*> layers;
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::vector<XrCompositionLayerProjectionView> projectionViews;

    if (frameState.shouldRender) {
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

        if (posesValid) {
            projectionViews.resize(viewCountOutput);

            // Main eye buffers stay plain black - all test content lives in
            // the quad layer below, since that's the piece that actually
            // matters (the eventual menu/emulator-screen render target).
            static const std::vector<DrawCube> kNoCubes;

            for (uint32_t i = 0; i < viewCountOutput; ++i) {
                Swapchain& sc = m_swapchains[i];

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
                                     static_cast<uint32_t>(sc.height), m_views[i].pose, m_views[i].fov, kNoCubes);

                XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                CheckXr(xrReleaseSwapchainImage(sc.handle, &releaseInfo), "xrReleaseSwapchainImage");
            }

            layer.space = m_appSpace;
            layer.viewCount = static_cast<uint32_t>(projectionViews.size());
            layer.views = projectionViews.data();
            layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
        }
    }

    XrCompositionLayerQuad quadLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
    if (RenderQuadLayer(quadLayer)) {
        layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&quadLayer));
    }

    XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
    frameEndInfo.displayTime = frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
    frameEndInfo.layers = layers.data();
    CheckXr(xrEndFrame(m_session, &frameEndInfo), "xrEndFrame");
}

void OpenXrApp::Shutdown() {
    // Swapchains own Vulkan images backed by our VkDevice, so they (and the
    // session) must be torn down before the renderer destroys that device -
    // otherwise xrDestroySwapchain's driver-side vkDestroyImage call
    // segfaults on an already-destroyed device (found via a real crash on
    // Quest when losing focus).
    for (Swapchain& sc : m_swapchains) {
        if (sc.handle != XR_NULL_HANDLE) xrDestroySwapchain(sc.handle);
    }
    if (m_quadSwapchain.handle != XR_NULL_HANDLE) xrDestroySwapchain(m_quadSwapchain.handle);
    m_quadSwapchain.handle = XR_NULL_HANDLE;
    m_swapchains.clear();
    if (m_appSpace != XR_NULL_HANDLE) xrDestroySpace(m_appSpace);
    if (m_session != XR_NULL_HANDLE) xrDestroySession(m_session);
    m_renderer.Shutdown();
    if (m_instance != XR_NULL_HANDLE) xrDestroyInstance(m_instance);
    m_appSpace = XR_NULL_HANDLE;
    m_session = XR_NULL_HANDLE;
    m_instance = XR_NULL_HANDLE;
}
