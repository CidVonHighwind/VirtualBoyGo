#pragma once

#include <volk.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <memory>
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
// Glyphs are baked on demand, not just once at LoadFont time: LoadFont only
// eagerly bakes ASCII 32-126 (every literal string already in this codebase,
// e.g. the header title), but arbitrary text this app doesn't control the
// content of - ROM file names read off disk, chiefly - can contain any
// Unicode codepoint. EnsureGlyphsForText grows the atlas (CPU-side buffer +
// GPU texture) to cover whatever a caller is about to display. Call it when
// text content is *set* (MenuList::AddEntry, MenuLabel/MenuButton::SetText
// already do), not while drawing - growing the atlas destroys/recreates its
// GPU image, which is only safe outside an active BeginFrame/EndFrame render
// pass. DrawText/GetTextWidth are pure lookups and silently skip a glyph
// that was never baked, same as always.
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

    // Per-font GPU resources + character map + on-demand baking state.
    struct Font
    {
        int fontSize = 0;      // physical FreeType pixel height
        float renderScale = 1.0f;
        float offsetY = 0; // baseline offset from top of glyph cell
        float pHeight = 0; // height of the capital 'P' glyph
        float pStart = 0;  // gap above 'P' (used to vertically center header text)
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkSampler sampler{VK_NULL_HANDLE};
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
        std::unordered_map<char32_t, Character> characters;

        // On-demand baking state - kept alive for the Font's whole lifetime
        // (not just during LoadFont) so EnsureGlyphsForText can rasterize
        // and pack more glyphs in later. ttfBytes is a private copy: FreeType
        // keeps a pointer into whatever buffer FT_New_Memory_Face was given,
        // so it must outlive ftFace - which is exactly why m_fonts stores
        // unique_ptr<Font> instead of Font directly (see m_fonts): a Font
        // relocating (e.g. a plain vector<Font> reallocating as more fonts
        // load) would move/copy ttfBytes to a new address without
        // FreeType's internal pointer following it, corrupting ftFace.
        std::vector<uint8_t> ttfBytes;
        FT_Face ftFace = nullptr;
        std::vector<uint8_t> atlasPixels; // R8, atlasWidth*atlasHeight
        int atlasWidth = 0, atlasHeight = 0;
        int packX = 0, packY = 0, packRowHeight = 0; // shelf-packing cursor
    };

    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                    VkCommandBuffer commandBuffer, VkDescriptorPool descriptorPool,
                    VkDescriptorSetLayout descriptorSetLayout);
    void Shutdown();

    // Bakes a FreeType glyph atlas (initially just ASCII 32-126) and uploads
    // it as an R8_UNORM texture. pixelHeight is the *physical* rasterization
    // size (crisp glyph detail); renderScale (default 1, meaning "pixelHeight
    // is already logical") then divides every layout metric (advance,
    // bearing, width/height, pHeight/pStart) by that same amount, so callers
    // that draw in a smaller logical coordinate space scaled up at render
    // time (see AppMenuLayout.h's kMenuScale) still get metrics consistent
    // with that logical space - only the underlying atlas texture has the
    // extra pixel density. UV rects are already resolution-independent
    // (0..1) and need no scaling.
    UiFontHandle LoadFont(const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale = 1.0f);

    // Destroys and re-bakes a font in place at a new pixelHeight/renderScale
    // (e.g. after a window resize changes the ideal integer scale) - the
    // handle stays valid, and its descriptor set is reused (just re-pointed
    // at the new atlas) rather than reallocated. Re-bakes every codepoint
    // the font had ever been asked for (not just the initial ASCII set), so
    // glyphs added on demand (see EnsureGlyphsForText) survive a rescale.
    void RebakeFont(UiFontHandle handle, const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale = 1.0f);

    // Makes sure every codepoint in utf8Text has a baked glyph, rasterizing
    // and packing (growing the atlas if needed) whatever's missing. Call
    // this wherever text content is set, not from inside a render pass (see
    // the class comment) - MenuList::AddEntry and MenuLabel/MenuButton::
    // SetText already do this for you.
    void EnsureGlyphsForText(UiFontHandle handle, const std::string &utf8Text);

    float GetTextWidth(UiFontHandle handle, const std::string &text) const;
    float GetFontPHeight(UiFontHandle handle) const;
    float GetFontPStart(UiFontHandle handle) const;

    // Raw access for UiRenderer's DrawText (needs Character + descriptorSet).
    const Font &Get(UiFontHandle handle) const { return *m_fonts[handle.id]; }

private:
    // Rasterizes one codepoint via FreeType and packs it into font's atlas
    // (shelf-packing; grows atlasHeight via GrowAtlasHeight if it doesn't
    // fit). A codepoint the font has no glyph for gets a zero-size entry
    // recorded so lookups don't retry it every time. Pure CPU-side work
    // (plus updating font.characters) - no Vulkan calls.
    void RasterizeAndPackGlyph(Font &font, char32_t codepoint);
    // Doubles atlasHeight (repeatedly) until at least minHeight, copying
    // existing pixel data and rescaling already-baked glyphs' v0/v1 (their
    // pixel position is unchanged, but the denominator they're normalized
    // against just changed).
    void GrowAtlasHeight(Font &font, int minHeight);
    // (Re)creates font's GPU image/view/sampler from its current
    // atlasPixels/atlasWidth/atlasHeight - does not touch descriptorSet.
    void UploadGlyphAtlas(Font &font);
    void DestroyFontImageResources(Font &font);
    // Points font.descriptorSet (already allocated) at its current
    // image/sampler - called after UploadGlyphAtlas whenever the underlying
    // image was just (re)created.
    void UpdateFontDescriptor(Font &font);

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkQueue m_queue{VK_NULL_HANDLE};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};

    FT_Library m_ftLibrary{nullptr}; // shared by every Font's ftFace

    // unique_ptr, not a plain Font, so a Font's heap address never changes
    // for its whole lifetime even as this vector grows - see the ftFace
    // comment on Font::ttfBytes above for why that matters.
    std::vector<std::unique_ptr<Font>> m_fonts;
};
