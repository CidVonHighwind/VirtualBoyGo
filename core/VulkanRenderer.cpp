#include "VulkanRenderer.h"

#include "XrMath.h"

#include "generated_shaders/cube.vert.h"
#include "generated_shaders/cube.frag.h"
#include "generated_shaders/textured_quad.vert.h"
#include "generated_shaders/textured_quad.frag.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "third_party/stb_image.h"

#include <array>
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

// Position (vec3) + color (vec3) per vertex, 6 faces * 2 tris * 3 verts.
// A simple unit cube centered at the origin, no culling (kept simple).
// clang-format off
const float kCubeVertices[] = {
    // -X (red)
    -0.5f,-0.5f,-0.5f, 1,0,0,  -0.5f,-0.5f, 0.5f, 1,0,0,  -0.5f, 0.5f, 0.5f, 1,0,0,
    -0.5f,-0.5f,-0.5f, 1,0,0,  -0.5f, 0.5f, 0.5f, 1,0,0,  -0.5f, 0.5f,-0.5f, 1,0,0,
    // +X (cyan)
     0.5f,-0.5f,-0.5f, 0,1,1,   0.5f, 0.5f, 0.5f, 0,1,1,   0.5f,-0.5f, 0.5f, 0,1,1,
     0.5f,-0.5f,-0.5f, 0,1,1,   0.5f, 0.5f,-0.5f, 0,1,1,   0.5f, 0.5f, 0.5f, 0,1,1,
    // -Y (green)
    -0.5f,-0.5f,-0.5f, 0,1,0,   0.5f,-0.5f, 0.5f, 0,1,0,  -0.5f,-0.5f, 0.5f, 0,1,0,
    -0.5f,-0.5f,-0.5f, 0,1,0,   0.5f,-0.5f,-0.5f, 0,1,0,   0.5f,-0.5f, 0.5f, 0,1,0,
    // +Y (magenta)
    -0.5f, 0.5f,-0.5f, 1,0,1,  -0.5f, 0.5f, 0.5f, 1,0,1,   0.5f, 0.5f, 0.5f, 1,0,1,
    -0.5f, 0.5f,-0.5f, 1,0,1,   0.5f, 0.5f, 0.5f, 1,0,1,   0.5f, 0.5f,-0.5f, 1,0,1,
    // -Z (yellow)
    -0.5f,-0.5f,-0.5f, 1,1,0,  -0.5f, 0.5f,-0.5f, 1,1,0,   0.5f, 0.5f,-0.5f, 1,1,0,
    -0.5f,-0.5f,-0.5f, 1,1,0,   0.5f, 0.5f,-0.5f, 1,1,0,   0.5f,-0.5f,-0.5f, 1,1,0,
    // +Z (blue)
    -0.5f,-0.5f, 0.5f, 0,0,1,   0.5f, 0.5f, 0.5f, 0,0,1,  -0.5f, 0.5f, 0.5f, 0,0,1,
    -0.5f,-0.5f, 0.5f, 0,0,1,   0.5f,-0.5f, 0.5f, 0,0,1,   0.5f, 0.5f, 0.5f, 0,0,1,
};
// clang-format on

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

    CreateCubeVertexBuffer();
}

void VulkanRenderer::Shutdown() {
    if (m_device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(m_device);

    for (auto& [image, target] : m_renderTargets) {
        vkDestroyFramebuffer(m_device, target.framebuffer, nullptr);
        vkDestroyImageView(m_device, target.view, nullptr);
    }
    m_renderTargets.clear();

    if (m_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    if (m_texturedQuadPipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, m_texturedQuadPipeline, nullptr);
    if (m_texturedQuadPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(m_device, m_texturedQuadPipelineLayout, nullptr);
    if (m_texturedQuadDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(m_device, m_texturedQuadDescriptorSetLayout, nullptr);
    if (m_descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    if (m_testImageSampler != VK_NULL_HANDLE) vkDestroySampler(m_device, m_testImageSampler, nullptr);
    if (m_testImageView != VK_NULL_HANDLE) vkDestroyImageView(m_device, m_testImageView, nullptr);
    if (m_testImage != VK_NULL_HANDLE) vkDestroyImage(m_device, m_testImage, nullptr);
    if (m_testImageMemory != VK_NULL_HANDLE) vkFreeMemory(m_device, m_testImageMemory, nullptr);
    if (m_renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    if (m_cubeVertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(m_device, m_cubeVertexBuffer, nullptr);
    if (m_cubeVertexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(m_device, m_cubeVertexBufferMemory, nullptr);
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

uint32_t VulkanRenderer::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("VulkanRenderer: no suitable memory type");
}

void VulkanRenderer::CreateCubeVertexBuffer() {
    const VkDeviceSize size = sizeof(kCubeVertices);
    m_cubeVertexCount = static_cast<uint32_t>(size / (sizeof(float) * 6));

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CheckVk(vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_cubeVertexBuffer), "vkCreateBuffer (cube)");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_device, m_cubeVertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex =
        FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CheckVk(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_cubeVertexBufferMemory), "vkAllocateMemory (cube)");
    vkBindBufferMemory(m_device, m_cubeVertexBuffer, m_cubeVertexBufferMemory, 0);

    void* data = nullptr;
    vkMapMemory(m_device, m_cubeVertexBufferMemory, 0, size, 0, &data);
    std::memcpy(data, kCubeVertices, static_cast<size_t>(size));
    vkUnmapMemory(m_device, m_cubeVertexBufferMemory);
}

VkRenderPass VulkanRenderer::GetOrCreateRenderPass(VkFormat colorFormat) {
    if (m_renderPass != VK_NULL_HANDLE && m_renderPassFormat == colorFormat) return m_renderPass;
    if (m_renderPass != VK_NULL_HANDLE) {
        // Format changed (shouldn't normally happen) - drop everything keyed off it.
        vkDeviceWaitIdle(m_device);
        if (m_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, m_pipeline, nullptr);
        if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        if (m_texturedQuadPipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, m_texturedQuadPipeline, nullptr);
        if (m_texturedQuadPipelineLayout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(m_device, m_texturedQuadPipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        for (auto& [image, target] : m_renderTargets) {
            vkDestroyFramebuffer(m_device, target.framebuffer, nullptr);
            vkDestroyImageView(m_device, target.view, nullptr);
        }
        m_renderTargets.clear();
        m_pipeline = VK_NULL_HANDLE;
        m_texturedQuadPipeline = VK_NULL_HANDLE;
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

void VulkanRenderer::CreatePipelineIfNeeded(VkFormat colorFormat) {
    GetOrCreateRenderPass(colorFormat);
    if (m_pipeline != VK_NULL_HANDLE) return;

    VkShaderModuleCreateInfo vertModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    vertModuleInfo.codeSize = g_cube_vert_size;
    vertModuleInfo.pCode = g_cube_vert;
    VkShaderModule vertModule;
    CheckVk(vkCreateShaderModule(m_device, &vertModuleInfo, nullptr, &vertModule), "vkCreateShaderModule (vert)");

    VkShaderModuleCreateInfo fragModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    fragModuleInfo.codeSize = g_cube_frag_size;
    fragModuleInfo.pCode = g_cube_frag;
    VkShaderModule fragModule;
    CheckVk(vkCreateShaderModule(m_device, &fragModuleInfo, nullptr, &fragModule), "vkCreateShaderModule (frag)");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 6;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3};

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(float) * 16;

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;
    CheckVk(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout), "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline),
            "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
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

void VulkanRenderer::RenderEye(VkImage image, int64_t swapchainFormat, uint32_t width, uint32_t height,
                               const XrPosef& eyePose, const XrFovf& fov, const std::vector<DrawCube>& cubes) {
    const VkFormat format = static_cast<VkFormat>(swapchainFormat);
    CreatePipelineIfNeeded(format);
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

    VkViewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, {width, height}};
    vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_cubeVertexBuffer, &offset);

    const Mat4 proj = Mat4ProjectionVulkan(fov, 0.05f, 100.0f);
    const Mat4 eyeToWorld = Mat4FromQuatTranslation(eyePose.orientation, eyePose.position);
    const Mat4 view = Mat4InvertRigid(eyeToWorld);
    const Mat4 viewProj = Mat4Multiply(proj, view);

    for (const DrawCube& cube : cubes) {
        const Mat4 model = Mat4Multiply(Mat4FromQuatTranslation(cube.pose.orientation, cube.pose.position), Mat4Scale(cube.scale));
        const Mat4 mvp = Mat4Multiply(viewProj, model);
        vkCmdPushConstants(m_commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, mvp.m);
        vkCmdDraw(m_commandBuffer, m_cubeVertexCount, 1, 0, 0);
    }

    vkCmdEndRenderPass(m_commandBuffer);
    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer");

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit");
    // Simplicity over performance for this first milestone: block until the
    // eye is fully rendered before returning, instead of proper fence/
    // semaphore pipelining.
    vkQueueWaitIdle(m_queue);
}

bool VulkanRenderer::LoadTestImage(const std::vector<uint8_t>& fileBytes, uint32_t& outWidth, uint32_t& outHeight) {
    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels =
        stbi_load_from_memory(fileBytes.data(), static_cast<int>(fileBytes.size()), &width, &height, &channels, 4);
    if (!pixels) {
        return false;
    }

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;
    // sRGB, not UNORM: the swapchain target is an sRGB format (auto-encodes
    // shader output on write), and the JPEG's bytes are already sRGB-gamma
    // encoded. Sampling as UNORM (no decode) then writing to sRGB caused a
    // double gamma curve - washed-out, faded colors. Declaring the texture
    // itself sRGB makes the sampler linearize on read, round-tripping
    // correctly through the sRGB write on the other end.
    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

    // Staging buffer (host-visible) -> device-local image, one-time upload.
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkBufferCreateInfo stagingInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    stagingInfo.size = imageSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CheckVk(vkCreateBuffer(m_device, &stagingInfo, nullptr, &stagingBuffer), "vkCreateBuffer (staging)");

    VkMemoryRequirements stagingMemReq;
    vkGetBufferMemoryRequirements(m_device, stagingBuffer, &stagingMemReq);
    VkMemoryAllocateInfo stagingAllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    stagingAllocInfo.allocationSize = stagingMemReq.size;
    stagingAllocInfo.memoryTypeIndex = FindMemoryType(
        stagingMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CheckVk(vkAllocateMemory(m_device, &stagingAllocInfo, nullptr, &stagingMemory), "vkAllocateMemory (staging)");
    vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);

    void* mapped = nullptr;
    vkMapMemory(m_device, stagingMemory, 0, imageSize, 0, &mapped);
    std::memcpy(mapped, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_device, stagingMemory);
    stbi_image_free(pixels);

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CheckVk(vkCreateImage(m_device, &imageInfo, nullptr, &m_testImage), "vkCreateImage (test)");

    VkMemoryRequirements imageMemReq;
    vkGetImageMemoryRequirements(m_device, m_testImage, &imageMemReq);
    VkMemoryAllocateInfo imageAllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    imageAllocInfo.allocationSize = imageMemReq.size;
    imageAllocInfo.memoryTypeIndex = FindMemoryType(imageMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CheckVk(vkAllocateMemory(m_device, &imageAllocInfo, nullptr, &m_testImageMemory), "vkAllocateMemory (test image)");
    vkBindImageMemory(m_device, m_testImage, m_testImageMemory, 0);

    vkResetCommandBuffer(m_commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer (upload)");

    VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.image = m_testImage;
    toTransferDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toTransferDst.srcAccessMask = 0;
    toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &toTransferDst);

    VkBufferImageCopy copyRegion{};
    copyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(m_commandBuffer, stagingBuffer, m_testImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copyRegion);

    VkImageMemoryBarrier toShaderRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toShaderRead.image = m_testImage;
    toShaderRead.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &toShaderRead);

    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer (upload)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit (upload)");
    vkQueueWaitIdle(m_queue);

    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = m_testImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    CheckVk(vkCreateImageView(m_device, &viewInfo, nullptr, &m_testImageView), "vkCreateImageView (test)");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_testImageSampler), "vkCreateSampler");

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayoutInfo.bindingCount = 1;
    setLayoutInfo.pBindings = &binding;
    CheckVk(vkCreateDescriptorSetLayout(m_device, &setLayoutInfo, nullptr, &m_texturedQuadDescriptorSetLayout),
            "vkCreateDescriptorSetLayout");

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    CheckVk(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool), "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo setAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    setAllocInfo.descriptorPool = m_descriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &m_texturedQuadDescriptorSetLayout;
    CheckVk(vkAllocateDescriptorSets(m_device, &setAllocInfo, &m_texturedQuadDescriptorSet),
            "vkAllocateDescriptorSets");

    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.sampler = m_testImageSampler;
    imageDescInfo.imageView = m_testImageView;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = m_texturedQuadDescriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageDescInfo;
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

    outWidth = static_cast<uint32_t>(width);
    outHeight = static_cast<uint32_t>(height);
    return true;
}

void VulkanRenderer::CreateTexturedQuadPipelineIfNeeded(VkFormat colorFormat) {
    GetOrCreateRenderPass(colorFormat);
    if (m_texturedQuadPipeline != VK_NULL_HANDLE) return;

    VkShaderModuleCreateInfo vertModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    vertModuleInfo.codeSize = g_textured_quad_vert_size;
    vertModuleInfo.pCode = g_textured_quad_vert;
    VkShaderModule vertModule;
    CheckVk(vkCreateShaderModule(m_device, &vertModuleInfo, nullptr, &vertModule), "vkCreateShaderModule (quad vert)");

    VkShaderModuleCreateInfo fragModuleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    fragModuleInfo.codeSize = g_textured_quad_frag_size;
    fragModuleInfo.pCode = g_textured_quad_frag;
    VkShaderModule fragModule;
    CheckVk(vkCreateShaderModule(m_device, &fragModuleInfo, nullptr, &fragModule), "vkCreateShaderModule (quad frag)");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_texturedQuadDescriptorSetLayout;
    CheckVk(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_texturedQuadPipelineLayout),
            "vkCreatePipelineLayout (quad)");

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_texturedQuadPipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_texturedQuadPipeline),
            "vkCreateGraphicsPipelines (quad)");

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void VulkanRenderer::RenderTexturedQuad(VkImage image, int64_t swapchainFormat, uint32_t width, uint32_t height) {
    const VkFormat format = static_cast<VkFormat>(swapchainFormat);
    CreateTexturedQuadPipelineIfNeeded(format);
    RenderTarget& target = GetOrCreateRenderTarget(image, format, width, height);

    vkResetCommandBuffer(m_commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer (quad)");

    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBegin.renderPass = m_renderPass;
    renderPassBegin.framebuffer = target.framebuffer;
    renderPassBegin.renderArea.extent = {width, height};
    renderPassBegin.clearValueCount = 1;
    renderPassBegin.pClearValues = &clearValue;
    vkCmdBeginRenderPass(m_commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, {width, height}};
    vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_texturedQuadPipeline);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_texturedQuadPipelineLayout, 0, 1,
                            &m_texturedQuadDescriptorSet, 0, nullptr);
    vkCmdDraw(m_commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(m_commandBuffer);
    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer (quad)");

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit (quad)");
    vkQueueWaitIdle(m_queue);
}
