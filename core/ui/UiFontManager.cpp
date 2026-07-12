#include "UiFontManager.h"
#include "UiTextUtils.h"
#include "UiVulkanUtils.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
    void CheckVk(VkResult result, const char *what)
    {
        if (result != VK_SUCCESS)
            throw std::runtime_error(std::string("Vulkan call failed: ") + what + " (" + std::to_string(result) + ")");
    }

    constexpr char32_t kInitialCodepointFirst = 32;
    constexpr char32_t kInitialCodepointLast = 126; // printable ASCII - every literal string in this codebase
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

    if (FT_Init_FreeType(&m_ftLibrary))
        throw std::runtime_error("UiFontManager: FT_Init_FreeType failed");
}

void UiFontManager::Shutdown()
{
    for (auto &fontPtr : m_fonts)
    {
        DestroyFontImageResources(*fontPtr);
        if (fontPtr->ftFace)
            FT_Done_Face(fontPtr->ftFace);
    }
    m_fonts.clear();

    if (m_ftLibrary)
    {
        FT_Done_FreeType(m_ftLibrary);
        m_ftLibrary = nullptr;
    }
}

void UiFontManager::RasterizeAndPackGlyph(Font &font, char32_t codepoint)
{
    if (font.characters.find(codepoint) != font.characters.end())
        return;

    // FT_Load_Char "succeeds" even for a codepoint this font has no glyph
    // for - it silently falls back to glyph index 0 (.notdef), which can
    // have a nonsensical advance width, wrecking the rest of the line's
    // layout. Resolve the index ourselves first so a genuinely unsupported
    // codepoint (e.g. CJK in a Latin-only font) degrades to an invisible
    // zero-advance placeholder instead.
    const FT_UInt glyphIndex = FT_Get_Char_Index(font.ftFace, codepoint);
    if (glyphIndex == 0 || FT_Load_Glyph(font.ftFace, glyphIndex, FT_LOAD_RENDER))
    {
        // No glyph for this codepoint in this font - record a zero-size
        // entry so callers don't re-attempt the (failing) lookup every time
        // this codepoint shows up again.
        font.characters[codepoint] = Character{};
        return;
    }

    const FT_Bitmap &bitmap = font.ftFace->glyph->bitmap;

    // Shelf-pack: wrap to a new row if this glyph doesn't fit the current
    // one, grow the atlas taller if it doesn't fit at all.
    if (font.packX + static_cast<int>(bitmap.width) + 1 > font.atlasWidth)
    {
        font.packX = 1;
        font.packY += font.packRowHeight + 2;
        font.packRowHeight = 0;
    }

    if (font.packY + static_cast<int>(bitmap.rows) + 1 > font.atlasHeight)
        GrowAtlasHeight(font, font.packY + static_cast<int>(bitmap.rows) + 1);

    for (unsigned int row = 0; row < bitmap.rows; ++row)
    {
        for (unsigned int col = 0; col < bitmap.width; ++col)
        {
            const int dstX = font.packX + static_cast<int>(col);
            const int dstY = font.packY + static_cast<int>(row);

            if (dstX < 0 || dstX >= font.atlasWidth || dstY < 0 || dstY >= font.atlasHeight)
                continue;

            font.atlasPixels[static_cast<size_t>(dstY) * font.atlasWidth + dstX] =
                bitmap.buffer[row * bitmap.pitch + col];
        }
    }

    Character character{};
    character.u0 = static_cast<float>(font.packX) / font.atlasWidth;
    character.v0 = static_cast<float>(font.packY) / font.atlasHeight;
    character.u1 = static_cast<float>(font.packX + bitmap.width) / font.atlasWidth;
    character.v1 = static_cast<float>(font.packY + bitmap.rows) / font.atlasHeight;
    character.width = static_cast<float>(bitmap.width) / font.renderScale;
    character.height = static_cast<float>(bitmap.rows) / font.renderScale;
    character.bearingX = static_cast<float>(font.ftFace->glyph->bitmap_left) / font.renderScale;
    character.bearingY = static_cast<float>(font.ftFace->glyph->bitmap_top) / font.renderScale;
    character.advance = static_cast<float>(font.ftFace->glyph->advance.x >> 6) / font.renderScale;
    font.characters[codepoint] = character;

    font.packX += static_cast<int>(bitmap.width) + 2;
    font.packRowHeight = std::max(font.packRowHeight, static_cast<int>(bitmap.rows));
}

void UiFontManager::GrowAtlasHeight(Font &font, int minHeight)
{
    int newHeight = font.atlasHeight > 0 ? font.atlasHeight : 1;
    while (newHeight < minHeight)
        newHeight *= 2;

    std::vector<uint8_t> newPixels(static_cast<size_t>(font.atlasWidth) * newHeight, 0);
    for (int y = 0; y < font.atlasHeight; ++y)
    {
        std::memcpy(&newPixels[static_cast<size_t>(y) * font.atlasWidth],
                    &font.atlasPixels[static_cast<size_t>(y) * font.atlasWidth], font.atlasWidth);
    }

    // Existing glyphs' pixel position doesn't move, but v0/v1 are normalized
    // by atlasHeight, which just changed - rescale them to match.
    const float ratio = static_cast<float>(font.atlasHeight) / static_cast<float>(newHeight);
    for (auto &[codepoint, character] : font.characters)
    {
        character.v0 *= ratio;
        character.v1 *= ratio;
    }

    font.atlasPixels = std::move(newPixels);
    font.atlasHeight = newHeight;
}

void UiFontManager::UploadGlyphAtlas(Font &font)
{
    const VkDeviceSize imageSize = font.atlasPixels.size();
    constexpr VkFormat format = VK_FORMAT_R8_UNORM;

    font.image = UiUploadImage(m_device, m_physicalDevice, m_queue, m_commandBuffer,
                               font.atlasPixels.data(), imageSize,
                               static_cast<uint32_t>(font.atlasWidth), static_cast<uint32_t>(font.atlasHeight),
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

void UiFontManager::UpdateFontDescriptor(Font &font)
{
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

UiFontHandle UiFontManager::LoadFont(const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale)
{
    auto fontPtr = std::make_unique<Font>();
    Font &font = *fontPtr;
    font.fontSize = pixelHeight;
    font.renderScale = renderScale;
    font.ttfBytes = ttfBytes; // owned copy - FT_Face keeps a pointer into this

    if (FT_New_Memory_Face(m_ftLibrary, font.ttfBytes.data(), static_cast<FT_Long>(font.ttfBytes.size()), 0,
                           &font.ftFace))
        throw std::runtime_error("UiFontManager: FT_New_Memory_Face failed");
    FT_Set_Pixel_Sizes(font.ftFace, 0, pixelHeight);
    // Fixed once from the face's own metrics rather than accumulated from
    // whichever glyphs happen to be baked - see the Font::offsetY comment
    // for why growing it later (e.g. from a tall on-demand glyph) would
    // desync every already-laid-out row's text position.
    font.offsetY = static_cast<float>(font.ftFace->size->metrics.ascender >> 6) / font.renderScale;

    font.atlasWidth = 30 * pixelHeight;
    font.atlasHeight = 8 * pixelHeight;
    font.atlasPixels.assign(static_cast<size_t>(font.atlasWidth) * font.atlasHeight, 0);
    font.packX = 1;
    font.packY = 1;

    for (char32_t c = kInitialCodepointFirst; c <= kInitialCodepointLast; ++c)
        RasterizeAndPackGlyph(font, c);

    auto pIt = font.characters.find(U'P');
    if (pIt != font.characters.end())
    {
        font.pHeight = pIt->second.height;
        font.pStart = font.offsetY - pIt->second.bearingY;
    }

    UploadGlyphAtlas(font);

    VkDescriptorSetAllocateInfo setAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    setAllocInfo.descriptorPool = m_descriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &m_descriptorSetLayout;
    CheckVk(vkAllocateDescriptorSets(m_device, &setAllocInfo, &font.descriptorSet),
            "vkAllocateDescriptorSets (font atlas)");

    UpdateFontDescriptor(font);

    m_fonts.push_back(std::move(fontPtr));
    return UiFontHandle{static_cast<int>(m_fonts.size()) - 1};
}

void UiFontManager::RebakeFont(UiFontHandle handle, const std::vector<uint8_t> &ttfBytes, int pixelHeight,
                               float renderScale)
{
    if (!handle.IsValid())
        return;
    Font &oldFont = *m_fonts[handle.id];

    // Carry over every codepoint this font was ever asked for - not just the
    // initial ASCII set - so glyphs added on demand (EnsureGlyphsForText)
    // survive a rescale instead of quietly reverting to tofu.
    std::vector<char32_t> codepoints;
    codepoints.reserve(oldFont.characters.size());
    for (const auto &[codepoint, character] : oldFont.characters)
        codepoints.push_back(codepoint);

    auto newFontPtr = std::make_unique<Font>();
    Font &newFont = *newFontPtr;
    newFont.fontSize = pixelHeight;
    newFont.renderScale = renderScale;
    newFont.ttfBytes = ttfBytes;

    if (FT_New_Memory_Face(m_ftLibrary, newFont.ttfBytes.data(), static_cast<FT_Long>(newFont.ttfBytes.size()), 0,
                           &newFont.ftFace))
        throw std::runtime_error("UiFontManager: FT_New_Memory_Face failed");
    FT_Set_Pixel_Sizes(newFont.ftFace, 0, pixelHeight);
    newFont.offsetY = static_cast<float>(newFont.ftFace->size->metrics.ascender >> 6) / newFont.renderScale;

    newFont.atlasWidth = 30 * pixelHeight;
    newFont.atlasHeight = 8 * pixelHeight;
    newFont.atlasPixels.assign(static_cast<size_t>(newFont.atlasWidth) * newFont.atlasHeight, 0);
    newFont.packX = 1;
    newFont.packY = 1;

    for (char32_t c : codepoints)
        RasterizeAndPackGlyph(newFont, c);

    auto pIt = newFont.characters.find(U'P');
    if (pIt != newFont.characters.end())
    {
        newFont.pHeight = pIt->second.height;
        newFont.pStart = newFont.offsetY - pIt->second.bearingY;
    }

    UploadGlyphAtlas(newFont);

    const VkDescriptorSet descriptorSet = oldFont.descriptorSet; // reused, not reallocated
    DestroyFontImageResources(oldFont);
    if (oldFont.ftFace)
        FT_Done_Face(oldFont.ftFace);

    newFont.descriptorSet = descriptorSet;
    m_fonts[handle.id] = std::move(newFontPtr);

    UpdateFontDescriptor(*m_fonts[handle.id]);
}

void UiFontManager::EnsureGlyphsForText(UiFontHandle handle, const std::string &utf8Text)
{
    if (!handle.IsValid())
        return;
    Font &font = *m_fonts[handle.id];

    bool addedAny = false;
    for (size_t i = 0; i < utf8Text.size();)
    {
        const char32_t codepoint = UiDecodeUtf8(utf8Text, i);
        if (font.characters.find(codepoint) != font.characters.end())
            continue;
        RasterizeAndPackGlyph(font, codepoint);
        addedAny = true;
    }

    if (!addedAny)
        return;

    // The atlas texture changed - rebuild it and re-point the (already
    // allocated) descriptor set at the new image, same in-place-reuse
    // pattern as RebakeFont.
    DestroyFontImageResources(font);
    UploadGlyphAtlas(font);
    UpdateFontDescriptor(font);
}

float UiFontManager::GetTextWidth(UiFontHandle handle, const std::string &text) const
{
    if (!handle.IsValid())
        return 0.0f;
    const Font &f = *m_fonts[handle.id];
    float width = 0.0f;
    for (size_t i = 0; i < text.size();)
    {
        const char32_t codepoint = UiDecodeUtf8(text, i);
        auto it = f.characters.find(codepoint);
        if (it != f.characters.end())
            width += it->second.advance;
    }
    return width;
}

float UiFontManager::GetFontPHeight(UiFontHandle handle) const
{
    return handle.IsValid() ? m_fonts[handle.id]->pHeight : 0.0f;
}

float UiFontManager::GetFontPStart(UiFontHandle handle) const
{
    return handle.IsValid() ? m_fonts[handle.id]->pStart : 0.0f;
}

void UiFontManager::DebugDumpAtlas(UiFontHandle handle, const char *path) const
{
    if (!handle.IsValid())
        return;
    const Font &font = *m_fonts[handle.id];
    std::FILE *f = std::fopen(path, "wb");
    if (!f)
        return;
    std::fprintf(f, "P5\n%d %d\n255\n", font.atlasWidth, font.atlasHeight);
    std::fwrite(font.atlasPixels.data(), 1, font.atlasPixels.size(), f);
    std::fclose(f);
}
