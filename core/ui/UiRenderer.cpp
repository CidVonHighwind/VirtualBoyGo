#include "UiRenderer.h"
#include "UiVulkanUtils.h"

// STB_IMAGE_IMPLEMENTATION is defined once in VulkanRenderer.cpp (same
// vbgo_app link unit) - this include just pulls in the declarations.
#include "third_party/stb_image.h"

#include <cstring>
#include <stdexcept>

namespace
{
    void CheckVk(VkResult result, const char *what)
    {
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error(std::string("Vulkan call failed: ") + what + " (" + std::to_string(result) + ")");
        }
    }

    // clang-format off
const float kUnitQuadVertices[] = {
    0, 0,  1, 0,  0, 1,
    1, 0,  1, 1,  0, 1,
};
    // clang-format on

} // namespace

void UiRenderer::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex,
                            VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_queue = queue;
    m_queueFamilyIndex = queueFamilyIndex;
    m_commandPool = commandPool;
    m_commandBuffer = commandBuffer;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = sizeof(kUnitQuadVertices);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CheckVk(vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_unitQuadVertexBuffer), "vkCreateBuffer (unit quad)");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(m_device, m_unitQuadVertexBuffer, &memReq);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = UiFindMemoryType(
        m_physicalDevice, memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    CheckVk(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_unitQuadVertexBufferMemory), "vkAllocateMemory (unit quad)");
    vkBindBufferMemory(m_device, m_unitQuadVertexBuffer, m_unitQuadVertexBufferMemory, 0);

    void *data = nullptr;
    vkMapMemory(m_device, m_unitQuadVertexBufferMemory, 0, sizeof(kUnitQuadVertices), 0, &data);
    std::memcpy(data, kUnitQuadVertices, sizeof(kUnitQuadVertices));
    vkUnmapMemory(m_device, m_unitQuadVertexBufferMemory);

    // Shared descriptor pool + layout for fonts and images (both are one
    // combined-image-sampler at binding 0).
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 16;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    CheckVk(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool), "vkCreateDescriptorPool (ui)");

    // Created eagerly (format/render-pass independent) so LoadFont can
    // allocate descriptor sets before the first BeginFrame/EnsurePipelines.
    VkDescriptorSetLayoutBinding binding0{};
    binding0.binding = 0;
    binding0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding0.descriptorCount = 1;
    binding0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo setLayoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayoutInfo.bindingCount = 1;
    setLayoutInfo.pBindings = &binding0;
    CheckVk(vkCreateDescriptorSetLayout(m_device, &setLayoutInfo, nullptr, &m_textDescriptorSetLayout),
            "vkCreateDescriptorSetLayout (ui text)");

    m_fontManager.Initialize(device, physicalDevice, queue, commandBuffer, m_descriptorPool,
                             m_textDescriptorSetLayout);
}

void UiRenderer::Shutdown()
{
    if (m_device == VK_NULL_HANDLE)
        return;
    vkDeviceWaitIdle(m_device);

    for (auto &[image, target] : m_renderTargets)
    {
        vkDestroyFramebuffer(m_device, target.framebuffer, nullptr);
        vkDestroyImageView(m_device, target.view, nullptr);
    }
    m_renderTargets.clear();

    m_fontManager.Shutdown();

    for (Image &image : m_images)
    {
        if (image.sampler != VK_NULL_HANDLE)
            vkDestroySampler(m_device, image.sampler, nullptr);
        if (image.view != VK_NULL_HANDLE)
            vkDestroyImageView(m_device, image.view, nullptr);
        if (image.image != VK_NULL_HANDLE)
            vkDestroyImage(m_device, image.image, nullptr);
        if (image.memory != VK_NULL_HANDLE)
            vkFreeMemory(m_device, image.memory, nullptr);
    }
    m_images.clear();

    if (m_descriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    if (m_textDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(m_device, m_textDescriptorSetLayout, nullptr);
    if (m_imageRoundedPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_device, m_imageRoundedPipeline, nullptr);
    if (m_imagePipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_device, m_imagePipeline, nullptr);
    if (m_textPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_device, m_textPipeline, nullptr);
    if (m_textPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(m_device, m_textPipelineLayout, nullptr);
    if (m_solidPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_device, m_solidPipeline, nullptr);
    if (m_solidPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(m_device, m_solidPipelineLayout, nullptr);
    if (m_offscreenRenderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
    if (m_renderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    if (m_unitQuadVertexBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_device, m_unitQuadVertexBuffer, nullptr);
    if (m_unitQuadVertexBufferMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, m_unitQuadVertexBufferMemory, nullptr);

    m_device = VK_NULL_HANDLE;
}

// --- Font pass-through ---

UiFontHandle UiRenderer::LoadFont(const std::vector<uint8_t> &ttfBytes, int pixelHeight)
{
    return m_fontManager.LoadFont(ttfBytes, pixelHeight);
}

float UiRenderer::GetTextWidth(UiFontHandle font, const std::string &text) const
{
    return m_fontManager.GetTextWidth(font, text);
}

int UiRenderer::GetFontPHeight(UiFontHandle font) const { return m_fontManager.GetFontPHeight(font); }
int UiRenderer::GetFontPStart(UiFontHandle font) const { return m_fontManager.GetFontPStart(font); }

// --- Image loading ---

UiImageHandle UiRenderer::LoadImage(const std::vector<uint8_t> &fileBytes, uint32_t &outWidth, uint32_t &outHeight)
{
    int width = 0, height = 0, channels = 0;
    stbi_uc *pixels =
        stbi_load_from_memory(fileBytes.data(), static_cast<int>(fileBytes.size()), &width, &height, &channels, 4);
    if (!pixels)
        return UiImageHandle{};

    Image image;
    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);

    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;
    // sRGB - source pixels are already gamma-encoded; _SRGB tells the sampler
    // to linearize on read so colours match what was authored (no double-gamma).
    const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    image.format = format;

    image.image = UiUploadImage(m_device, m_physicalDevice, m_queue, m_commandBuffer,
                                pixels, imageSize, image.width, image.height,
                                format, VK_IMAGE_USAGE_SAMPLED_BIT, image.memory);
    stbi_image_free(pixels);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    CheckVk(vkCreateImageView(m_device, &viewInfo, nullptr, &image.view), "vkCreateImageView (ui image)");

    // LINEAR sampler - ui_image.frag does its own texel-snapping (SamplePixelPerfectAA)
    // for stable edges under non-integer scaling / VR head movement.
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &image.sampler), "vkCreateSampler (ui image)");

    VkDescriptorSetAllocateInfo setAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    setAllocInfo.descriptorPool = m_descriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &m_textDescriptorSetLayout;
    CheckVk(vkAllocateDescriptorSets(m_device, &setAllocInfo, &image.descriptorSet),
            "vkAllocateDescriptorSets (ui image)");

    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.sampler = image.sampler;
    imageDescInfo.imageView = image.view;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = image.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageDescInfo;
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

    outWidth = image.width;
    outHeight = image.height;

    m_images.push_back(image);
    return UiImageHandle{static_cast<int>(m_images.size()) - 1};
}

UiImageHandle UiRenderer::CreateRenderTexture(uint32_t width, uint32_t height, VkFormat format)
{
    Image image;
    image.width = width;
    image.height = height;
    image.format = format;

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CheckVk(vkCreateImage(m_device, &imageInfo, nullptr, &image.image), "vkCreateImage (render texture)");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_device, image.image, &memReq);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex =
        UiFindMemoryType(m_physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CheckVk(vkAllocateMemory(m_device, &allocInfo, nullptr, &image.memory), "vkAllocateMemory (render texture)");
    vkBindImageMemory(m_device, image.image, image.memory, 0);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    CheckVk(vkCreateImageView(m_device, &viewInfo, nullptr, &image.view), "vkCreateImageView (render texture)");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &image.sampler), "vkCreateSampler (render texture)");

    VkDescriptorSetAllocateInfo setAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    setAllocInfo.descriptorPool = m_descriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &m_textDescriptorSetLayout;
    CheckVk(vkAllocateDescriptorSets(m_device, &setAllocInfo, &image.descriptorSet),
            "vkAllocateDescriptorSets (render texture)");

    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.sampler = image.sampler;
    imageDescInfo.imageView = image.view;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = image.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageDescInfo;
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

    m_images.push_back(image);
    return UiImageHandle{static_cast<int>(m_images.size()) - 1};
}

// --- Frame management ---

void UiRenderer::BeginFrame(VkImage image, VkFormat format, uint32_t width, uint32_t height, const XrColor4f &clearColor)
{
    EnsurePipelines(format);
    RenderTarget &target = GetOrCreateRenderTarget(image, format, width, height, m_renderPass);

    m_frameWidth = static_cast<float>(width);
    m_frameHeight = static_cast<float>(height);

    vkResetCommandBuffer(m_commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer (ui frame)");

    VkClearValue clearValue{};
    clearValue.color = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};

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

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_unitQuadVertexBuffer, &offset);
}

void UiRenderer::BeginOffscreenFrame(UiImageHandle target, const XrColor4f &clearColor)
{
    if (!target.IsValid())
        return;
    const Image &img = m_images[target.id];

    EnsurePipelines(img.format);
    GetOrCreateOffscreenRenderPass(img.format);
    RenderTarget &rt = GetOrCreateRenderTarget(img.image, img.format, img.width, img.height, m_offscreenRenderPass);

    m_frameWidth = static_cast<float>(img.width);
    m_frameHeight = static_cast<float>(img.height);

    vkResetCommandBuffer(m_commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CheckVk(vkBeginCommandBuffer(m_commandBuffer, &beginInfo), "vkBeginCommandBuffer (ui offscreen frame)");

    VkClearValue clearValue{};
    clearValue.color = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};

    VkRenderPassBeginInfo renderPassBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBegin.renderPass = m_offscreenRenderPass;
    renderPassBegin.framebuffer = rt.framebuffer;
    renderPassBegin.renderArea.extent = {img.width, img.height};
    renderPassBegin.clearValueCount = 1;
    renderPassBegin.pClearValues = &clearValue;
    vkCmdBeginRenderPass(m_commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0, 0, static_cast<float>(img.width), static_cast<float>(img.height), 0.0f, 1.0f};
    vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, {img.width, img.height}};
    vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_unitQuadVertexBuffer, &offset);
}

void UiRenderer::SetViewportRegion(float x, float y, float w, float h)
{
    VkViewport viewport{x, y, w, h, 0.0f, 1.0f};
    vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{{static_cast<int32_t>(x), static_cast<int32_t>(y)},
                     {static_cast<uint32_t>(w), static_cast<uint32_t>(h)}};
    vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    m_frameWidth = w;
    m_frameHeight = h;
}

void UiRenderer::EndFrame()
{
    vkCmdEndRenderPass(m_commandBuffer);
    CheckVk(vkEndCommandBuffer(m_commandBuffer), "vkEndCommandBuffer (ui frame)");

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    CheckVk(vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit (ui frame)");
    vkQueueWaitIdle(m_queue);
}

// --- Draw calls ---

void UiRenderer::DrawUnitQuad(VkPipeline pipeline, VkPipelineLayout layout, VkDescriptorSet descriptorSet,
                              float x, float y, float w, float h,
                              float u0, float v0, float u1, float v1,
                              const XrColor4f &color, float cornerRadiusPx)
{
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    if (descriptorSet != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);

    PushConstants pc{};
    pc.posPx[0] = x;
    pc.posPx[1] = y;
    pc.sizePx[0] = w;
    pc.sizePx[1] = h;
    pc.uvRect[0] = u0;
    pc.uvRect[1] = v0;
    pc.uvRect[2] = u1;
    pc.uvRect[3] = v1;
    pc.color[0] = color.r;
    pc.color[1] = color.g;
    pc.color[2] = color.b;
    pc.color[3] = color.a;
    pc.screenSizePx[0] = m_frameWidth;
    pc.screenSizePx[1] = m_frameHeight;
    pc.cornerRadiusPx = cornerRadiusPx;
    vkCmdPushConstants(m_commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

    vkCmdDraw(m_commandBuffer, 6, 1, 0, 0);
}

void UiRenderer::DrawQuad(float x, float y, float w, float h, const XrColor4f &color)
{
    DrawUnitQuad(m_solidPipeline, m_solidPipelineLayout, VK_NULL_HANDLE, x, y, w, h, 0, 0, 1, 1, color, 0.0f);
}

void UiRenderer::DrawQuadRounded(float x, float y, float w, float h, const XrColor4f &color, float cornerRadiusPx)
{
    DrawUnitQuad(m_solidPipeline, m_solidPipelineLayout, VK_NULL_HANDLE, x, y, w, h, 0, 0, 1, 1, color, cornerRadiusPx);
}

void UiRenderer::DrawText(UiFontHandle fontHandle, const std::string &text, float x, float y, float scale,
                          const XrColor4f &color)
{
    if (!fontHandle.IsValid())
        return;
    const UiFontManager::Font &f = m_fontManager.Get(fontHandle);

    float cursorX = x;
    for (char c : text)
    {
        auto it = f.characters.find(c);
        if (it == f.characters.end())
            continue;
        const UiFontManager::Character &ch = it->second;

        const float xpos = cursorX + ch.bearingX * scale;
        const float ypos = y + f.offsetY - ch.bearingY;
        const float cw = ch.width * scale;
        const float ch_h = ch.height * scale;

        DrawUnitQuad(m_textPipeline, m_textPipelineLayout, f.descriptorSet,
                     xpos, ypos, cw, ch_h, ch.u0, ch.v0, ch.u1, ch.v1, color);

        cursorX += ch.advance * scale;
    }
}

void UiRenderer::DrawImage(UiImageHandle imageHandle, float x, float y, float w, float h)
{
    if (!imageHandle.IsValid())
        return;
    const Image &img = m_images[imageHandle.id];
    DrawUnitQuad(m_imagePipeline, m_textPipelineLayout, img.descriptorSet,
                 x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, XrColor4f{1.0f, 1.0f, 1.0f, 1.0f});
}

void UiRenderer::DrawImageRegion(UiImageHandle imageHandle, float x, float y, float w, float h,
                                 float u0, float v0, float u1, float v1, float alpha)
{
    if (!imageHandle.IsValid())
        return;
    const Image &img = m_images[imageHandle.id];
    DrawUnitQuad(m_imagePipeline, m_textPipelineLayout, img.descriptorSet,
                 x, y, w, h, u0, v0, u1, v1, XrColor4f{1.0f, 1.0f, 1.0f, alpha});
}

void UiRenderer::DrawImageRounded(UiImageHandle imageHandle, float x, float y, float w, float h, float cornerRadiusPx)
{
    if (!imageHandle.IsValid())
        return;
    const Image &img = m_images[imageHandle.id];
    DrawUnitQuad(m_imageRoundedPipeline, m_textPipelineLayout, img.descriptorSet,
                 x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, XrColor4f{1.0f, 1.0f, 1.0f, 1.0f}, cornerRadiusPx);
}
