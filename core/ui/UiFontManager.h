#pragma once

#include <volk.h>

#include <string>
#include <unordered_map>
#include <vector>

// Opaque handle to a loaded font (glyph atlas + metrics).
struct UiFontHandle
{
    int id = -1;
    bool IsValid() const { return id >= 0; }
};

// Owns the FreeType atlas-baking pipeline and the resulting per-font Vulkan
// textures. Extracted from UiRenderer so the font loading/querying code
// lives in one place rather than mixed in with pipelines and draw calls.
//
// Lifetime: Initialize() before any LoadFont call; Shutdown() on teardown.
// The device/queue/commandBuffer passed to Initialize() must outlive this
// object - they are borrowed from VulkanRenderer, same as UiRenderer does.
class UiFontManager
{
public:
    // Per-glyph atlas UV rect + layout metrics. width/height/bearing*/advance
    // are logical-space units (see LoadFont's renderScale) - float rather
    // than int so a renderScale that doesn't divide evenly (e.g. an odd
    // glyph bitmap size / 2) doesn't get truncated away.
    struct Character
    {
        float u0, v0, u1, v1;
        float width, height;
        float bearingX, bearingY;
        float advance;
    };

    // Per-font GPU resources + character map.
    struct Font
    {
        int fontSize = 0;
        float offsetY = 0; // baseline offset from top of glyph cell
        float pHeight = 0; // height of the capital 'P' glyph
        float pStart = 0;  // gap above 'P' (used to vertically center header text)
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkSampler sampler{VK_NULL_HANDLE};
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
        std::unordered_map<char, Character> characters;
    };

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                    VkCommandBuffer commandBuffer, VkDescriptorPool descriptorPool,
                    VkDescriptorSetLayout descriptorSetLayout);
    void Shutdown();

    // Bakes a FreeType glyph atlas and uploads it as an R8_UNORM texture.
    // pixelHeight is the *physical* rasterization size (crisp glyph detail);
    // renderScale (default 1, meaning "pixelHeight is already logical") then
    // divides every layout metric (advance, bearing, width/height, pHeight/
    // pStart) by that same amount, so callers that draw in a smaller logical
    // coordinate space scaled up at render time (see AppMenuLayout.h's
    // kMenuScale) still get metrics consistent with that logical space -
    // only the underlying atlas texture has the extra pixel density. UV
    // rects are already resolution-independent (0..1) and need no scaling.
    UiFontHandle LoadFont(const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale = 1.0f);

    // Destroys and re-bakes a font in place at a new pixelHeight/renderScale
    // (e.g. after a window resize changes the ideal integer scale) - the
    // handle stays valid, and its descriptor set is reused (just re-pointed
    // at the new atlas) rather than reallocated.
    void RebakeFont(UiFontHandle handle, const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale = 1.0f);

    float GetTextWidth(UiFontHandle handle, const std::string &text) const;
    float GetFontPHeight(UiFontHandle handle) const;
    float GetFontPStart(UiFontHandle handle) const;

    // Raw access for UiRenderer's DrawText (needs Character + descriptorSet).
    const Font &Get(UiFontHandle handle) const { return m_fonts[handle.id]; }

private:
    // Pure CPU-side FreeType baking (no Vulkan calls) - shared by LoadFont
    // and RebakeFont. outAtlas is an R8 textureWidth*textureHeight buffer.
    void BakeGlyphAtlas(const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale, Font &outFont,
                        std::vector<uint8_t> &outAtlas, int &outTextureWidth, int &outTextureHeight);
    // Uploads outAtlas as this Font's R8_UNORM image/view/sampler - does not
    // touch descriptorSet.
    void UploadGlyphAtlas(Font &font, const std::vector<uint8_t> &atlas, int textureWidth, int textureHeight);
    void DestroyFontImageResources(Font &font);

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkQueue m_queue{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};

    std::vector<Font> m_fonts;
};
