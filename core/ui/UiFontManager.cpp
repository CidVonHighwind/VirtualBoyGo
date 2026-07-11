#include "UiFontManager.h"
#include "UiVulkanUtils.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstring>
#include <stdexcept>

namespace
{
    void CheckVk(VkResult result, const char *what)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error(std::string("Vulkan call failed: ") + what + " (" + std::to_string(result) + ")");
    }
} // namespace

void UiFontManager::Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                               VkCommandBuffer commandBuffer, VkDescriptorPool descriptorPool,
                               VkDescriptorSetLayout descriptorSetLayout)
{
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_queue = queue;
    m_commandBuffer = commandBuffer;
    m_descriptorPool = descriptorPool;
    m_descriptorSetLayout = descriptorSetLayout;
}

void UiFontManager::Shutdown()
{
    for (Font &font : m_fonts)
    {
        if (font.sampler != VK_NULL_HANDLE)
            vkDestroySampler(m_device, font.sampler, nullptr);
        if (font.view != VK_NULL_HANDLE)
            vkDestroyImageView(m_device, font.view, nullptr);
        if (font.image != VK_NULL_HANDLE)
            vkDestroyImage(m_device, font.image, nullptr);
        if (font.memory != VK_NULL_HANDLE)
            vkFreeMemory(m_device, font.memory, nullptr);
    }
    m_fonts.clear();
}

void UiFontManager::BakeGlyphAtlas(const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale,
                                   Font &outFont, std::vector<uint8_t> &outAtlas, int &outTextureWidth,
                                   int &outTextureHeight)
{
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
        throw std::runtime_error("UiFontManager: FT_Init_FreeType failed");

    FT_Face face;
    if (FT_New_Memory_Face(ft, ttfBytes.data(), static_cast<FT_Long>(ttfBytes.size()), 0, &face))
    {
        FT_Done_FreeType(ft);
        throw std::runtime_error("UiFontManager: FT_New_Memory_Face failed");
    }
    FT_Set_Pixel_Sizes(face, 0, pixelHeight);

    outFont.fontSize = pixelHeight;

    const int textureWidth = 30 * pixelHeight;
    const int textureHeight = 8 * pixelHeight;
    outTextureWidth = textureWidth;
    outTextureHeight = textureHeight;
    outAtlas.assign(static_cast<size_t>(textureWidth) * textureHeight, 0);

    int posX = 1;
    int posY = 1;
    int offsetYPhysical = 0; // accumulated in physical px, divided by renderScale once below

    for (unsigned char c = 32; c < 192; ++c)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            continue;

        const FT_Bitmap &bitmap = face->glyph->bitmap;

        float descent = 0.0f;
        if (descent < static_cast<float>(bitmap.rows) - face->glyph->bitmap_top)
            descent = static_cast<float>(bitmap.rows) - face->glyph->bitmap_top;

        float ascentCalc = (face->glyph->bitmap_top < static_cast<int>(bitmap.rows))
                               ? static_cast<float>(bitmap.rows)
                               : static_cast<float>(face->glyph->bitmap_top);
        float ascent = 0.0f;
        if (ascent < ascentCalc - descent)
            ascent = ascentCalc - descent;
        if (offsetYPhysical < static_cast<int>(ascent))
            offsetYPhysical = static_cast<int>(ascent);

        if (posX + static_cast<int>(bitmap.width) > textureWidth)
        {
            posX = 0;
            posY += pixelHeight + pixelHeight / 2;
        }

        for (unsigned int row = 0; row < bitmap.rows; ++row)
        {
            for (unsigned int col = 0; col < bitmap.width; ++col)
            {
                const int dstX = posX + static_cast<int>(col);
                const int dstY = posY + static_cast<int>(row);
                if (dstX < 0 || dstX >= textureWidth || dstY < 0 || dstY >= textureHeight)
                    continue;
                outAtlas[static_cast<size_t>(dstY) * textureWidth + dstX] = bitmap.buffer[row * bitmap.pitch + col];
            }
        }

        Character character{};
        character.u0 = static_cast<float>(posX) / textureWidth;
        character.v0 = static_cast<float>(posY) / textureHeight;
        character.u1 = static_cast<float>(posX + bitmap.width) / textureWidth;
        character.v1 = static_cast<float>(posY + bitmap.rows) / textureHeight;
        character.width = static_cast<float>(bitmap.width) / renderScale;
        character.height = static_cast<float>(bitmap.rows) / renderScale;
        character.bearingX = static_cast<float>(face->glyph->bitmap_left) / renderScale;
        character.bearingY = static_cast<float>(face->glyph->bitmap_top) / renderScale;
        character.advance = static_cast<float>(face->glyph->advance.x >> 6) / renderScale;
        outFont.characters[static_cast<char>(c)] = character;

        posX += static_cast<int>(bitmap.width) + 2;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    outFont.offsetY = static_cast<float>(offsetYPhysical) / renderScale;

    auto pIt = outFont.characters.find('P');
    if (pIt != outFont.characters.end())
    {
        outFont.pHeight = pIt->second.height;
        outFont.pStart = outFont.offsetY - pIt->second.bearingY;
    }
}

void UiFontManager::UploadGlyphAtlas(Font &font, const std::vector<uint8_t> &atlas, int textureWidth,
                                     int textureHeight)
{
    const VkDeviceSize imageSize = atlas.size();
    constexpr VkFormat format = VK_FORMAT_R8_UNORM;

    font.image = UiUploadImage(m_device, m_physicalDevice, m_queue, m_commandBuffer,
                               atlas.data(), imageSize,
                               static_cast<uint32_t>(textureWidth), static_cast<uint32_t>(textureHeight),
                               format, VK_IMAGE_USAGE_SAMPLED_BIT, font.memory);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = font.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    CheckVk(vkCreateImageView(m_device, &viewInfo, nullptr, &font.view), "vkCreateImageView (font atlas)");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &font.sampler), "vkCreateSampler (font atlas)");
}

void UiFontManager::DestroyFontImageResources(Font &font)
{
    if (font.sampler != VK_NULL_HANDLE)
        vkDestroySampler(m_device, font.sampler, nullptr);
    if (font.view != VK_NULL_HANDLE)
        vkDestroyImageView(m_device, font.view, nullptr);
    if (font.image != VK_NULL_HANDLE)
        vkDestroyImage(m_device, font.image, nullptr);
    if (font.memory != VK_NULL_HANDLE)
        vkFreeMemory(m_device, font.memory, nullptr);

    font.image = VK_NULL_HANDLE;
    font.view = VK_NULL_HANDLE;
    font.sampler = VK_NULL_HANDLE;
    font.memory = VK_NULL_HANDLE;
}

UiFontHandle UiFontManager::LoadFont(const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale)
{
    Font font;
    std::vector<uint8_t> atlas;
    int textureWidth = 0, textureHeight = 0;
    BakeGlyphAtlas(ttfBytes, pixelHeight, renderScale, font, atlas, textureWidth, textureHeight);
    UploadGlyphAtlas(font, atlas, textureWidth, textureHeight);

    VkDescriptorSetAllocateInfo setAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    setAllocInfo.descriptorPool = m_descriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &m_descriptorSetLayout;
    CheckVk(vkAllocateDescriptorSets(m_device, &setAllocInfo, &font.descriptorSet),
            "vkAllocateDescriptorSets (font atlas)");

    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.sampler = font.sampler;
    imageDescInfo.imageView = font.view;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = font.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageDescInfo;
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

    m_fonts.push_back(std::move(font));
    return UiFontHandle{static_cast<int>(m_fonts.size()) - 1};
}

void UiFontManager::RebakeFont(UiFontHandle handle, const std::vector<uint8_t> &ttfBytes, int pixelHeight,
                               float renderScale)
{
    if (!handle.IsValid())
        return;

    Font newFont;
    std::vector<uint8_t> atlas;
    int textureWidth = 0, textureHeight = 0;
    BakeGlyphAtlas(ttfBytes, pixelHeight, renderScale, newFont, atlas, textureWidth, textureHeight);
    UploadGlyphAtlas(newFont, atlas, textureWidth, textureHeight);

    Font &font = m_fonts[handle.id];
    const VkDescriptorSet descriptorSet = font.descriptorSet; // reused, not reallocated
    DestroyFontImageResources(font);

    newFont.descriptorSet = descriptorSet;
    font = std::move(newFont);

    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.sampler = font.sampler;
    imageDescInfo.imageView = font.view;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = font.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageDescInfo;
    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

float UiFontManager::GetTextWidth(UiFontHandle handle, const std::string &text) const
{
    if (!handle.IsValid())
        return 0.0f;
    const Font &f = m_fonts[handle.id];
    float width = 0.0f;
    for (char c : text)
    {
        auto it = f.characters.find(c);
        if (it != f.characters.end())
            width += static_cast<float>(it->second.advance);
    }
    return width;
}

float UiFontManager::GetFontPHeight(UiFontHandle handle) const
{
    return handle.IsValid() ? m_fonts[handle.id].pHeight : 0.0f;
}

float UiFontManager::GetFontPStart(UiFontHandle handle) const
{
    return handle.IsValid() ? m_fonts[handle.id].pStart : 0.0f;
}
