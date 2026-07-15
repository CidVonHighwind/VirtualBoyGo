#pragma once

#include <volk.h>

#include <openxr/openxr.h> // for XrColor4f

#include "UiFontManager.h" // also provides UiFontHandle

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Opaque handle to a loaded background/game image (see LoadImage/DrawImage).
struct UiImageHandle
{
    int id = -1;
    bool IsValid() const { return id >= 0; }
};

// Vulkan replacement for FrontendGo's DrawHelper (solid-color quads) +
// FontManager (FreeType-baked text). Ported from the original's call shape
// (see MenuHelper.cpp's DrawTexture/RenderText calls) but rendering via
// Vulkan pipelines instead of raw GL. Shares its Vulkan device/queue/command
// buffer with VulkanRenderer (via its public getters) rather than owning a
// second device - see VulkanRenderer::GetCommandPool/GetCommandBuffer.
//
// No textured icons in this pass (see rework plan) - DrawQuad is
// solid-color only, matching TextureLoader's original white-1x1-texture
// fallback used for button backgrounds.
class UiRenderer
{
public:
    void Initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex,
                    VkCommandPool commandPool, VkCommandBuffer commandBuffer);
    void Shutdown();

    // Bakes a glyph atlas via FreeType (ported from FontMaster.cpp's
    // LoadFont) and uploads it as an R8 Vulkan texture. See
    // UiFontManager::LoadFont for renderScale's meaning.
    UiFontHandle LoadFont(const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale = 1.0f);
    // See UiFontManager::RebakeFont.
    void RebakeFont(UiFontHandle font, const std::vector<uint8_t> &ttfBytes, int pixelHeight, float renderScale = 1.0f);
    // See UiFontManager::EnsureGlyphsForText - call this wherever text
    // content is set (not while drawing) so arbitrary Unicode text (e.g. ROM
    // file names) has baked glyphs by the time DrawText/GetTextWidth need them.
    void EnsureGlyphsForText(UiFontHandle font, const std::string &utf8Text);
    float GetTextWidth(UiFontHandle font, const std::string &text) const;
    float GetFontPHeight(UiFontHandle font) const;
    float GetFontPStart(UiFontHandle font) const;

    // Decodes an image file (JPEG/PNG via stb_image) and uploads it as an
    // sRGB-format sampled texture with a LINEAR sampler, for pixel-perfect
    // upscaling (e.g. a low-res emulated game screen) via DrawImage -
    // ui_image.frag does its own texel-snapping in the shader
    // (SamplePixelPerfectAA) rather than relying on NEAREST filtering, for
    // softer/more stable edges under non-integer scaling or subpixel motion
    // (e.g. VR head movement). Returns the image's native pixel size so the
    // caller can do its own integer-scale/letterbox math.
    UiImageHandle LoadImage(const std::vector<uint8_t> &fileBytes, uint32_t &outWidth, uint32_t &outHeight);

    // Creates an empty, device-local color-attachment+sampled texture -
    // render into it via BeginOffscreenFrame/EndFrame, then composite the
    // finished result onto a real target with DrawImageRounded. format must
    // match whatever format the real target(s) use (see EnsurePipelines -
    // pipelines are tied to one format for the process lifetime).
    UiImageHandle CreateRenderTexture(uint32_t width, uint32_t height, VkFormat format);

    // Destroys and recreates a CreateRenderTexture texture at a new size in
    // place - the handle stays valid (same index), and its descriptor set is
    // reused (just re-pointed at the new image) rather than reallocated,
    // since the descriptor pool isn't created with FREE_DESCRIPTOR_SET_BIT.
    // For resizable-window UIs that need to re-render their offscreen buffer
    // at a new physical resolution (e.g. after a window resize changes the
    // ideal integer scale) without leaking a fresh image+descriptor set
    // every time.
    void ResizeRenderTexture(UiImageHandle handle, uint32_t width, uint32_t height);

    // Creates a device-local sampled texture plus a persistent, permanently-
    // mapped host-visible staging buffer sized for one full frame of pixels
    // - for content that's re-uploaded wholesale every frame (e.g. an
    // emulator's video output), where UiUploadImage's destroy-and-recreate-
    // the-whole-VkImage-plus-blocking-queue-wait pattern (fine for a one-off
    // font atlas rebake) would be far too slow at ~50Hz. width/height should
    // be the largest frame size ever expected - UpdateStreamingImage always
    // uploads exactly that many pixels; draw a UV-cropped sub-rect (see
    // DrawImageRegion) for frames smaller than that.
    UiImageHandle CreateStreamingImage(uint32_t width, uint32_t height, VkFormat format);
    // Copies width*height*(format's bytes-per-pixel) bytes from pixels into
    // the image created by CreateStreamingImage and makes them visible for
    // sampling - a memcpy into the persistent staging buffer plus a single
    // vkCmdCopyBufferToImage on the shared command buffer, no image
    // recreation. dataSize must match what CreateStreamingImage sized the
    // staging buffer for (asserted via truncation, not a hard error).
    void UpdateStreamingImage(UiImageHandle handle, const void *pixels, size_t dataSize);

    // Begins recording into the given target image (e.g. a quad
    // composition-layer swapchain image). All Draw* calls happen between
    // BeginFrame/EndFrame.
    void BeginFrame(VkImage image, VkFormat format, uint32_t width, uint32_t height, const XrColor4f &clearColor);

    // Like BeginFrame, but targets a texture created by CreateRenderTexture
    // and leaves it in a shader-readable layout when the render pass ends
    // (ready for DrawImageRounded), rather than a presentable/composable
    // one. Close out with the ordinary EndFrame() - which render pass gets
    // ended is implicit in what BeginFrame/BeginOffscreenFrame began.
    //
    // logicalWidth/logicalHeight (both 0 by default) let a caller draw in a
    // smaller virtual coordinate space than the target's actual pixel size,
    // e.g. lay out a menu in 320x240 units but render it into a crisp 640x480
    // texture: the viewport/scissor still cover the full physical target (no
    // upscaling/blur - text and shapes rasterize at native resolution), only
    // the NDC divisor (screenSizePx in the shader) uses the smaller logical
    // size, so posPx/sizePx passed to Draw* calls are logical-space units
    // that land scaled-up on the physical target. Leave both 0 to use the
    // target's own pixel size for both (the ordinary 1:1 behaviour).
    void BeginOffscreenFrame(UiImageHandle target, const XrColor4f &clearColor,
                             float logicalWidth = 0.0f, float logicalHeight = 0.0f);

    // cornerRadiusPx > 0 rounds all four corners (antialiased); 0 (default)
    // is a plain sharp-cornered rect.
    void DrawQuad(float x, float y, float w, float h, const XrColor4f &color);
    void DrawQuadRounded(float x, float y, float w, float h, const XrColor4f &color, float cornerRadiusPx);
    void DrawText(UiFontHandle font, const std::string &text, float x, float y, float scale, const XrColor4f &color);
    // Stretches the whole image into the given destination rect (in target
    // pixels). Pass an already integer-scaled rect for a pixel-perfect look
    // (the sampler is NEAREST, so no blurring occurs either way). tint
    // multiplies the sampled rgb (white = no-op) - e.g. Emulator::DrawScreen's
    // VB color palette.
    void DrawImage(UiImageHandle image, float x, float y, float w, float h,
                   const XrColor4f &tint = XrColor4f{1.0f, 1.0f, 1.0f, 1.0f});
    // Like DrawImage, but samples only the given sub-rect of the image (UVs
    // in 0..1 image space) - for drawing one icon out of a shared atlas
    // texture instead of the whole image, or a UV-cropped region of a larger
    // texture (see Emulator::DrawScreen). tint multiplies the sampled rgb
    // (white = no-op, the default every caller but DrawScreen uses) in
    // addition to the alpha fade.
    void DrawImageRegion(UiImageHandle image, float x, float y, float w, float h,
                         float u0, float v0, float u1, float v1, float alpha = 1.0f,
                         const XrColor4f &tint = XrColor4f{1.0f, 1.0f, 1.0f, 1.0f});
    // Like DrawImage, but masks the sampled texture to rounded corners - the
    // intended way to composite a whole pre-rendered buffer (e.g. an
    // offscreen-rendered AppMenu) as a single rounded panel, instead of
    // rounding each shape inside it separately.
    void DrawImageRounded(UiImageHandle image, float x, float y, float w, float h, float cornerRadiusPx, float alpha = 1.0f);
    void EndFrame();

    // Drops all cached per-image framebuffers (see GetOrCreateRenderTarget).
    // Call this after destroying/recreating a swapchain (e.g. on window
    // resize) - the old swapchain's VkImage handles become invalid, and a
    // new swapchain can be given the *same* handle values by the driver, in
    // which case the stale cache entry would point at a framebuffer/view
    // built from the destroyed image instead of the new one.
    void InvalidateRenderTargets();

private:
    // Push-constant layout shared by all UI pipelines (vertex stage).
    struct PushConstants
    {
        float posPx[2];
        float sizePx[2];
        float uvRect[4];
        float color[4];
        float screenSizePx[2];
        float cornerRadiusPx; // only read by ui_image_rounded.frag
        // Physical pixels per logical unit (m_pixelScale) - ui_solid.frag and
        // ui_image_rounded.frag's rounded-corner SDF antialiasing band is
        // authored as a fixed width in *physical* pixels, but dist is
        // computed in logical units (vSizePx), so it needs this to convert.
        // Without it, the AA band is 1 logical unit wide instead of 1
        // physical pixel, which is fine at scale 1 but gets visibly blurrier
        // as m_menuScale grows (a scale-4 corner would blur across 4
        // physical pixels instead of 1).
        float pixelScale;
    };

private:
    struct RenderTarget
    {
        VkImageView view{VK_NULL_HANDLE};
        VkFramebuffer framebuffer{VK_NULL_HANDLE};
    };

    struct Image
    {
        uint32_t width = 0;
        uint32_t height = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImage image{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkImageView view{VK_NULL_HANDLE};
        VkSampler sampler{VK_NULL_HANDLE};
        VkDescriptorSet descriptorSet{VK_NULL_HANDLE};

        // Only set for images created by CreateStreamingImage - see its doc
        // comment. stagingMapped stays non-null for the image's whole
        // lifetime (mapped once, not per-update).
        VkBuffer stagingBuffer{VK_NULL_HANDLE};
        VkDeviceMemory stagingMemory{VK_NULL_HANDLE};
        void *stagingMapped{nullptr};
        VkDeviceSize stagingSize{0};
        // UpdateStreamingImage needs to know the image's current layout to
        // build the right barrier (UNDEFINED only holds on the very first
        // call - every call after that starts from SHADER_READ_ONLY_OPTIMAL,
        // left there by the previous call).
        VkImageLayout currentLayout{VK_IMAGE_LAYOUT_UNDEFINED};
    };

    VkRenderPass GetOrCreateRenderPass(VkFormat format);
    VkRenderPass GetOrCreateOffscreenRenderPass(VkFormat format);
    void EnsurePipelines(VkFormat format);
    // Creates the image/memory/view/sampler for a device-local color-
    // attachment+sampled texture (shared by CreateRenderTexture and
    // ResizeRenderTexture) - does not touch img.descriptorSet.
    void CreateImageResources(Image &img, uint32_t width, uint32_t height, VkFormat format);
    // Destroys img's image/memory/view/sampler (and its render target, if
    // any) but leaves img.descriptorSet alone - the pool can't individually
    // free descriptor sets, so callers that want to keep the handle valid
    // reuse it instead (see ResizeRenderTexture).
    void DestroyImageResources(Image &img);
    RenderTarget &GetOrCreateRenderTarget(VkImage image, VkFormat format, uint32_t width, uint32_t height,
                                          VkRenderPass renderPass);
    void DrawUnitQuad(VkPipeline pipeline, VkPipelineLayout layout, VkDescriptorSet descriptorSet, float x, float y,
                      float w, float h, float u0, float v0, float u1, float v1, const XrColor4f &color,
                      float cornerRadiusPx = 0.0f);

    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkQueue m_queue{VK_NULL_HANDLE};
    uint32_t m_queueFamilyIndex{0};
    VkCommandPool m_commandPool{VK_NULL_HANDLE};     // not owned
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE}; // not owned

    VkRenderPass m_renderPass{VK_NULL_HANDLE};
    VkFormat m_renderPassFormat{VK_FORMAT_UNDEFINED};
    // finalLayout = SHADER_READ_ONLY_OPTIMAL instead of m_renderPass's
    // COLOR_ATTACHMENT_OPTIMAL - used only for BeginOffscreenFrame targets.
    // Render-pass-compatible with m_renderPass when formats match, so the
    // same pipelines (created against m_renderPass) work with either.
    VkRenderPass m_offscreenRenderPass{VK_NULL_HANDLE};
    VkFormat m_offscreenRenderPassFormat{VK_FORMAT_UNDEFINED};
    std::unordered_map<VkImage, RenderTarget> m_renderTargets;

    VkBuffer m_unitQuadVertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory m_unitQuadVertexBufferMemory{VK_NULL_HANDLE};

    VkPipelineLayout m_solidPipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_solidPipeline{VK_NULL_HANDLE};

    // Shared by both the text pipeline (R8 glyph atlas) and the image
    // pipeline (RGBA background/game images) - both are just a single
    // combined-image-sampler at binding 0, so one descriptor set layout and
    // one pipeline layout cover either.
    VkDescriptorSetLayout m_textDescriptorSetLayout{VK_NULL_HANDLE};
    VkPipelineLayout m_textPipelineLayout{VK_NULL_HANDLE};
    VkPipeline m_textPipeline{VK_NULL_HANDLE};
    VkPipeline m_imagePipeline{VK_NULL_HANDLE};
    VkPipeline m_imageRoundedPipeline{VK_NULL_HANDLE};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};

    UiFontManager m_fontManager;
    std::vector<Image> m_images;

    // Per-frame state between BeginFrame/EndFrame.
    float m_frameWidth{0};
    float m_frameHeight{0};
    // Physical pixels per logical unit for the current frame (1.0 outside
    // BeginOffscreenFrame's logical/physical split). DrawUnitQuad uses this
    // to snap every quad's edges to the physical pixel grid - a fractional
    // physical-pixel edge makes bilinear texture sampling (text glyphs,
    // images) blend the edge texel with whatever's next to it in the atlas,
    // typically blank padding, softening/clipping that edge.
    float m_pixelScale{1.0f};
};
