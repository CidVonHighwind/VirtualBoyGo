#pragma once

#include <volk.h>

#if defined(__ANDROID__)
#include <jni.h>
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Owns the Vulkan instance/device (created via OpenXR's XR_KHR_vulkan_enable2
// delegated-creation calls). The actual UI/menu/emulator-screen content is
// drawn by UiRenderer (which reuses this device's command buffer) - this
// class is left owning just device/instance setup plus RenderEye, which
// clears the main projection-layer eye buffers to black (all real content
// lives in UiRenderer-drawn composition-layer quads instead).
class VulkanRenderer
{
public:
    void CreateDevice(XrInstance xrInstance, XrSystemId xrSystemId);

    // Windowed (no OpenXR) device creation, for the 2D desktop debug build.
    // VulkanRenderer stays windowing-library-agnostic: the caller creates
    // the actual window/surface (e.g. via GLFW) and passes the surface in
    // just to pick a physical device/queue that can present to it.
    VkInstance CreateInstanceStandalone(const std::vector<const char *> &instanceExtensions);
    void CreateDeviceForSurface(VkSurfaceKHR surface);

    void Shutdown();

    XrGraphicsBindingVulkan2KHR GetGraphicsBinding() const;

    VkInstance GetInstance() const { return m_instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkDevice GetDevice() const { return m_device; }
    VkQueue GetQueue() const { return m_queue; }
    uint32_t GetQueueFamilyIndex() const { return m_queueFamilyIndex; }
    // Shared with UiRenderer, which reuses this device's single command
    // buffer for its own (synchronous, blocking) draw submissions rather
    // than owning a second pool/buffer.
    VkCommandPool GetCommandPool() const { return m_commandPool; }
    VkCommandBuffer GetCommandBuffer() const { return m_commandBuffer; }

    int64_t SelectSwapchainFormat(const std::vector<int64_t> &runtimeFormats) const;

    // Clears a main projection-layer eye buffer to black. Real content
    // (menu, emulator screen) is rendered separately by UiRenderer into its
    // own composition-layer quad swapchains.
    void RenderEye(VkImage image, int64_t swapchainFormat, uint32_t width, uint32_t height);

    // Blits mip 0 (already rendered - e.g. by UiRenderer::BeginFrame/
    // EndFrame - and left in COLOR_ATTACHMENT_OPTIMAL) down into the rest
    // of the image's mip chain. The OpenXR compositor does its own
    // resampling of composition-layer swapchains onto the eye buffers, a
    // step entirely outside our own rendering - without a mip chain, that
    // resampling aliases/shimmers whenever the layer is minified (viewed
    // smaller than its native resolution, e.g. far away), regardless of how
    // carefully we sampled while drawing mip 0 ourselves. mipLevels must
    // match what the image was actually created with (XrSwapchainCreateInfo
    // ::mipCount). Currently unused - swapchain mips are disabled while
    // SteamVR stability is being established; re-test with mips before
    // re-enabling.
    void GenerateMipmaps(VkImage image, uint32_t width, uint32_t height, uint32_t mipLevels);

private:
    struct RenderTarget
    {
        VkImageView view{VK_NULL_HANDLE};
        VkFramebuffer framebuffer{VK_NULL_HANDLE};
    };

    void FinishDeviceSetup();
    RenderTarget &GetOrCreateRenderTarget(VkImage image, VkFormat format, uint32_t width, uint32_t height);
    VkRenderPass GetOrCreateRenderPass(VkFormat colorFormat);
    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

    VkInstance m_instance{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_queue{VK_NULL_HANDLE};
    uint32_t m_queueFamilyIndex{0};

    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};

    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    VkFormat m_renderPassFormat{VK_FORMAT_UNDEFINED};

    std::unordered_map<VkImage, RenderTarget> m_renderTargets;
};
