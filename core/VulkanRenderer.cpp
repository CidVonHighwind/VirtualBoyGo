#include "VulkanRenderer.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {

void CheckVk(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string("Vulkan call failed: ") + what + " (" + std::to_string(result) + ")");
    }
}

void CheckXr(XrResult result, const char* what) {
    if (XR_FAILED(result)) {
        throw std::runtime_error(std::string("OpenXR call failed: ") + what + " (" + std::to_string(result) + ")");
    }
}

template <typename PfnT>
PfnT GetXrFn(XrInstance instance, const char* name) {
    PfnT pfn = nullptr;
    CheckXr(xrGetInstanceProcAddr(instance, name, reinterpret_cast<PFN_xrVoidFunction*>(&pfn)), name);
    return pfn;
}

}  // namespace

void VulkanRenderer::CreateDevice(XrInstance xrInstance, XrSystemId xrSystemId) {
    CheckVk(volkInitialize(), "volkInitialize");

    auto pfnGetReqs2 =
        GetXrFn<PFN_xrGetVulkanGraphicsRequirements2KHR>(xrInstance, "xrGetVulkanGraphicsRequirements2KHR");
    auto pfnCreateInstance = GetXrFn<PFN_xrCreateVulkanInstanceKHR>(xrInstance, "xrCreateVulkanInstanceKHR");
    auto pfnCreateDevice = GetXrFn<PFN_xrCreateVulkanDeviceKHR>(xrInstance, "xrCreateVulkanDeviceKHR");
    auto pfnGetDevice2 = GetXrFn<PFN_xrGetVulkanGraphicsDevice2KHR>(xrInstance, "xrGetVulkanGraphicsDevice2KHR");

    XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    CheckXr(pfnGetReqs2(xrInstance, xrSystemId, &requirements), "xrGetVulkanGraphicsRequirements2KHR");

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "VirtualBoyGo";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "VirtualBoyGo";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.pApplicationInfo = &appInfo;

    XrVulkanInstanceCreateInfoKHR xrInstanceCreateInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xrInstanceCreateInfo.systemId = xrSystemId;
    xrInstanceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrInstanceCreateInfo.vulkanCreateInfo = &instanceCreateInfo;
    xrInstanceCreateInfo.vulkanAllocator = nullptr;

    VkResult vkResult = VK_SUCCESS;
    CheckXr(pfnCreateInstance(xrInstance, &xrInstanceCreateInfo, &m_instance, &vkResult), "xrCreateVulkanInstanceKHR");
    CheckVk(vkResult, "vkCreateInstance (via xrCreateVulkanInstanceKHR)");

    volkLoadInstance(m_instance);

    XrVulkanGraphicsDeviceGetInfoKHR deviceGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    deviceGetInfo.systemId = xrSystemId;
    deviceGetInfo.vulkanInstance = m_instance;
    CheckXr(pfnGetDevice2(xrInstance, &deviceGetInfo, &m_physicalDevice), "xrGetVulkanGraphicsDevice2KHR");

    // TEMP debug: confirm which GPU the OpenXR runtime told us to use -
    // dual-GPU systems are a suspected cause of the VK_ERROR_DEVICE_LOST
    // seen under SteamVR (cross-adapter shared-texture interop breaking if
    // this doesn't match whatever GPU SteamVR's own compositor uses).
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        std::fprintf(stderr, "[VulkanRenderer] OpenXR selected GPU: \"%s\" (vendorID=0x%04x, deviceID=0x%04x)\n",
                    props.deviceName, props.vendorID, props.deviceID);

        uint32_t allDeviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &allDeviceCount, nullptr);
        std::vector<VkPhysicalDevice> allDevices(allDeviceCount);
        vkEnumeratePhysicalDevices(m_instance, &allDeviceCount, allDevices.data());
        std::fprintf(stderr, "[VulkanRenderer] All available GPUs (%u):\n", allDeviceCount);
        for (VkPhysicalDevice dev : allDevices) {
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(dev, &p);
            std::fprintf(stderr, "  - \"%s\" (vendorID=0x%04x, deviceID=0x%04x)%s\n", p.deviceName, p.vendorID,
                        p.deviceID, dev == m_physicalDevice ? "  <-- selected" : "");
        }
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_queueFamilyIndex = i;
            break;
        }
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    XrVulkanDeviceCreateInfoKHR xrDeviceCreateInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xrDeviceCreateInfo.systemId = xrSystemId;
    xrDeviceCreateInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrDeviceCreateInfo.vulkanPhysicalDevice = m_physicalDevice;
    xrDeviceCreateInfo.vulkanCreateInfo = &deviceCreateInfo;
    xrDeviceCreateInfo.vulkanAllocator = nullptr;

    CheckXr(pfnCreateDevice(xrInstance, &xrDeviceCreateInfo, &m_device, &vkResult), "xrCreateVulkanDeviceKHR");
    CheckVk(vkResult, "vkCreateDevice (via xrCreateVulkanDeviceKHR)");

    volkLoadDevice(m_device);
    FinishDeviceSetup();
}

VkInstance VulkanRenderer::CreateInstanceStandalone(const std::vector<const char*>& instanceExtensions) {
    CheckVk(volkInitialize(), "volkInitialize");

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "VirtualBoyGo";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "VirtualBoyGo";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.empty() ? nullptr : instanceExtensions.data();

    CheckVk(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance), "vkCreateInstance (standalone)");
    volkLoadInstance(m_instance);
    return m_instance;
}

void VulkanRenderer::CreateDeviceForSurface(VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    if (devices.empty()) {
        throw std::runtime_error("VulkanRenderer: no Vulkan physical devices found");
    }

    for (VkPhysicalDevice candidate : devices) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, queueFamilies.data());
        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &presentSupport);
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                m_physicalDevice = candidate;
                m_queueFamilyIndex = i;
                break;
            }
        }
        if (m_physicalDevice != VK_NULL_HANDLE) break;
    }
    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanRenderer: no Vulkan device with graphics+present support for this surface");
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
    CheckVk(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device), "vkCreateDevice (standalone)");

    volkLoadDevice(m_device);
    FinishDeviceSetup();
}

void VulkanRenderer::FinishDeviceSetup() {
    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;
    CheckVk(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocInfo.commandPool = m_commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    CheckVk(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_commandBuffer), "vkAllocateCommandBuffers");
}

void VulkanRenderer::Shutdown() {
    if (m_device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(m_device);

    for (auto& [image, target] : m_renderTargets) {
        vkDestroyFramebuffer(m_device, target.framebuffer, nullptr);
        vkDestroyImageView(m_device, target.view, nullptr);
    }
    m_renderTargets.clear();

    if (m_renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    if (m_commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
    m_device = VK_NULL_HANDLE;
    m_instance = VK_NULL_HANDLE;
}

XrGraphicsBindingVulkan2KHR VulkanRenderer::GetGraphicsBinding() const {
    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    binding.instance = m_instance;
    binding.physicalDevice = m_physicalDevice;
    binding.device = m_device;
    binding.queueFamilyIndex = m_queueFamilyIndex;
    binding.queueIndex = 0;
    return binding;
}

int64_t VulkanRenderer::SelectSwapchainFormat(const std::vector<int64_t>& runtimeFormats) const {
    static const std::vector<int64_t> preferred = {
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
    };
    for (int64_t want : preferred) {
        for (int64_t have : runtimeFormats) {
            if (want == have) return have;
        }
    }
    return runtimeFormats.empty() ? VK_FORMAT_B8G8R8A8_UNORM : runtimeFormats[0];
}

VkRenderPass VulkanRenderer::GetOrCreateRenderPass(VkFormat colorFormat) {
    if (m_renderPass != VK_NULL_HANDLE && m_renderPassFormat == colorFormat) return m_renderPass;
    if (m_renderPass != VK_NULL_HANDLE) {
        // Format changed (shouldn't normally happen) - drop everything keyed off it.
        vkDeviceWaitIdle(m_device);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        for (auto& [image, target] : m_renderTargets) {
            vkDestroyFramebuffer(m_device, target.framebuffer, nullptr);
            vkDestroyImageView(m_device, target.view, nullptr);
        }
        m_renderTargets.clear();
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    CheckVk(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass), "vkCreateRenderPass");
    m_renderPassFormat = colorFormat;
    return m_renderPass;
}

VulkanRenderer::RenderTarget& VulkanRenderer::GetOrCreateRenderTarget(VkImage image, VkFormat format, uint32_t width,
                                                                      uint32_t height) {
    auto it = m_renderTargets.find(image);
    if (it != m_renderTargets.end()) return it->second;

    RenderTarget target;

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    CheckVk(vkCreateImageView(m_device, &viewInfo, nullptr, &target.view), "vkCreateImageView");

    VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = m_renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &target.view;
    fbInfo.width = width;
    fbInfo.height = height;
    fbInfo.layers = 1;
    CheckVk(vkCreateFramebuffer(m_device, &fbInfo, nullptr, &target.framebuffer), "vkCreateFramebuffer");

    auto [inserted, ok] = m_renderTargets.emplace(image, target);
    return inserted->second;
}

void VulkanRenderer::RenderEye(VkImage image, int64_t swapchainFormat, uint32_t width, uint32_t height) {
    const VkFormat format = static_cast<VkFormat>(swapchainFormat);
    GetOrCreateRenderPass(format);
    RenderTarget& target = GetOrCreateRenderTarget(image, format, width, height);

    vkResetCommandBuffer(m_commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBegin.renderPass = m_renderPass;
    renderPassBegin.framebuffer = target.framebuffer;
    renderPassBegin.renderArea.extent = {width, height};
    renderPassBegin.clearValueCount = 1;
    renderPassBegin.pClearValues = &clearValue;
    vkCmdBeginRenderPass(m_commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(m_commandBuffer);
    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer");

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit");
    // Simplicity over performance: block until the eye is fully rendered
    // before returning, instead of proper fence/semaphore pipelining.
    vkQueueWaitIdle(m_queue);
}

void VulkanRenderer::GenerateMipmaps(VkImage image, uint32_t width, uint32_t height, uint32_t mipLevels) {
    if (mipLevels <= 1) return;

    vkResetCommandBuffer(m_commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer (mipmaps)");

    int32_t mipWidth = static_cast<int32_t>(width);
    int32_t mipHeight = static_cast<int32_t>(height);

    for (uint32_t level = 1; level < mipLevels; ++level) {
        VkImageMemoryBarrier toSrc{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        // Mip 0 arrives in COLOR_ATTACHMENT_OPTIMAL (whatever rendered it -
        // e.g. UiRenderer::EndFrame - leaves it there); every mip after that
        // was itself just written as a blit destination in the previous
        // loop iteration.
        toSrc.oldLayout = (level == 1) ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSrc.image = image;
        toSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1};
        toSrc.srcAccessMask = (level == 1) ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
        toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(m_commandBuffer,
                             (level == 1) ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSrc);

        VkImageMemoryBarrier toDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toDst.image = image;
        toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level, 1, 0, 1};
        toDst.srcAccessMask = 0;
        toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &toDst);

        const int32_t nextWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        const int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {nextWidth, nextHeight, 1};
        vkCmdBlitImage(m_commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        // Done reading mip level-1 as a blit source - leave it in the same
        // layout the top mip normally sits in, since XR composition layer
        // swapchains aren't given any other explicit final-layout contract.
        VkImageMemoryBarrier doneWithSrc{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        doneWithSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        doneWithSrc.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        doneWithSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        doneWithSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        doneWithSrc.image = image;
        doneWithSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 1, 0, 1};
        doneWithSrc.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        doneWithSrc.dstAccessMask = 0;
        vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &doneWithSrc);

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }

    // Last mip level is still in TRANSFER_DST_OPTIMAL (never read from) -
    // bring it in line with the rest.
    VkImageMemoryBarrier lastMip{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    lastMip.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    lastMip.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    lastMip.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    lastMip.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    lastMip.image = image;
    lastMip.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mipLevels - 1, 1, 0, 1};
    lastMip.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    lastMip.dstAccessMask = 0;
    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &lastMip);

    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer (mipmaps)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit (mipmaps)");
    vkQueueWaitIdle(m_queue);
}
