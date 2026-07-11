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
    // Per-glyph atlas UV rect + layout metrics.
    struct Character
    {
        float u0, v0, u1, v1;
        int width, height;
        int bearingX, bearingY;
        int advance;
    };

    // Per-font GPU resources + character map.
    struct Font
    {
        int fontSize = 0;
        int offsetY = 0; // baseline offset from top of glyph cell
        int pHeight = 0; // height of the capital 'P' glyph
        int pStart = 0;  // gap above 'P' (used to vertically center header text)
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
    UiFontHandle LoadFont(const std::vector<uint8_t> &ttfBytes, int pixelHeight);

    float GetTextWidth(UiFontHandle handle, const std::string &text) const;
    int GetFontPHeight(UiFontHandle handle) const;
    int GetFontPStart(UiFontHandle handle) const;

    // Raw access for UiRenderer's DrawText (needs Character + descriptorSet).
    const Font &Get(UiFontHandle handle) const { return m_fonts[handle.id]; }

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkQueue m_queue{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};

    std::vector<Font> m_fonts;
};
