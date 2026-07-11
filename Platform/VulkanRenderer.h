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

// A single instance to draw - a colored cube at a given pose/scale.
struct DrawCube {
    XrPosef pose;
    XrVector3f scale;
};

// Owns the Vulkan instance/device (created via OpenXR's XR_KHR_vulkan_enable2
// delegated-creation calls) and renders one eye's swapchain image per call.
// Deliberately minimal: a fixed pipeline drawing colored cubes, no scene
// graph, no textures yet - this is the first cross-platform (PC + Quest)
// milestone the emulator/menu rendering will later replace.
class VulkanRenderer {
   public:
    void CreateDevice(XrInstance xrInstance, XrSystemId xrSystemId);

    // Windowed (no OpenXR) device creation, for the 2D desktop debug build.
    // VulkanRenderer stays windowing-library-agnostic: the caller creates
    // the actual window/surface (e.g. via GLFW) and passes the surface in
    // just to pick a physical device/queue that can present to it.
    VkInstance CreateInstanceStandalone(const std::vector<const char*>& instanceExtensions);
    void CreateDeviceForSurface(VkSurfaceKHR surface);

    void Shutdown();

    XrGraphicsBindingVulkan2KHR GetGraphicsBinding() const;

    VkInstance GetInstance() const { return m_instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkDevice GetDevice() const { return m_device; }
    VkQueue GetQueue() const { return m_queue; }
    uint32_t GetQueueFamilyIndex() const { return m_queueFamilyIndex; }

    int64_t SelectSwapchainFormat(const std::vector<int64_t>& runtimeFormats) const;

    void RenderEye(VkImage image, int64_t swapchainFormat, uint32_t width, uint32_t height, const XrPosef& eyePose,
                   const XrFovf& fov, const std::vector<DrawCube>& cubes);

    // Decodes an image file (JPEG/PNG via stb_image) and uploads it as a
    // sampled texture, for RenderTexturedQuad. Returns the image's
    // width/height so the caller can size a swapchain/quad layer to match.
    bool LoadTestImage(const std::vector<uint8_t>& fileBytes, uint32_t& outWidth, uint32_t& outHeight);

    // Draws the loaded test image as a fullscreen textured quad into the
    // given swapchain image - this is the composition-layer content path
    // (as opposed to RenderEye's colored-cube path used for the main eye
    // buffers).
    void RenderTexturedQuad(VkImage image, int64_t swapchainFormat, uint32_t width, uint32_t height);

   private:
    struct RenderTarget {
        VkImageView view{VK_NULL_HANDLE};
        VkFramebuffer framebuffer{VK_NULL_HANDLE};
    };

    void FinishDeviceSetup();
    void CreatePipelineIfNeeded(VkFormat colorFormat);
    void CreateTexturedQuadPipelineIfNeeded(VkFormat colorFormat);
    RenderTarget& GetOrCreateRenderTarget(VkImage image, VkFormat format, uint32_t width, uint32_t height);
    VkRenderPass GetOrCreateRenderPass(VkFormat colorFormat);
    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    void CreateCubeVertexBuffer();

    VkInstance m_instance{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_queue{VK_NULL_HANDLE};
    uint32_t m_queueFamilyIndex{0};

    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};

    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    VkFormat m_renderPassFormat{VK_FORMAT_UNDEFINED};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_pipeline{VK_NULL_HANDLE};

    VkBuffer m_cubeVertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_cubeVertexBufferMemory{VK_NULL_HANDLE};
    uint32_t m_cubeVertexCount{0};

    // Textured-quad pipeline (composition-layer test image).
    VkPipelineLayout m_texturedQuadPipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_texturedQuadPipeline{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_texturedQuadDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSet m_texturedQuadDescriptorSet{VK_NULL_HANDLE};

    VkImage m_testImage{VK_NULL_HANDLE};
    VkDeviceMemory m_testImageMemory{VK_NULL_HANDLE};
    VkImageView m_testImageView{VK_NULL_HANDLE};
    VkSampler m_testImageSampler{VK_NULL_HANDLE};

    std::unordered_map<VkImage, RenderTarget> m_renderTargets;
};
