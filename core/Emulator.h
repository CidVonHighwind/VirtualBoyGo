#pragma once

#include "ui/UiRenderer.h"

#include <cstdint>

// Stand-in for the eventual Virtual Boy emulator core. Right now it just
// loads a static test image ("game_image.png") as the "game screen", but
// DrawScreen's call shape (a single pixel-perfect stretched texture draw)
// is exactly what the real core will keep once it's uploading actual
// per-frame emulated frames instead of a static PNG - callers on every
// platform (OpenXrApp, pc2d) go through this same interface so none of them
// need to change when that swap happens.
class Emulator {
   public:
    // Screens are shown at this fixed integer upscale (pixel-perfect,
    // nearest-neighbor - see UiRenderer::LoadImage) everywhere the emulator
    // screen is displayed, so PC2D and the headset builds look consistent.
    static constexpr int kScale = 3;

    void Initialize(UiRenderer& ui);

    bool HasScreen() const { return m_screenTexture.IsValid(); }
    uint32_t GetScreenWidth() const { return m_screenWidth; }
    uint32_t GetScreenHeight() const { return m_screenHeight; }

    // Stretches the current screen into the given destination rect (in
    // target pixels) - callers typically size that rect to
    // GetScreenWidth/Height() * kScale for a pixel-perfect look.
    void DrawScreen(UiRenderer& ui, float x, float y, float w, float h) const;

   private:
    UiImageHandle m_screenTexture;
    uint32_t m_screenWidth = 0;
    uint32_t m_screenHeight = 0;
};
