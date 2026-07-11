#include "Emulator.h"
#include "AssetLoader.h"

void Emulator::Initialize(UiRenderer& ui) {
    const std::vector<uint8_t> imageBytes = LoadAssetBytes("game_image.png");
    if (imageBytes.empty()) return;
    m_screenTexture = ui.LoadImage(imageBytes, m_screenWidth, m_screenHeight);
}

void Emulator::DrawScreen(UiRenderer& ui, float x, float y, float w, float h) const {
    if (!m_screenTexture.IsValid()) return;
    ui.DrawImage(m_screenTexture, x, y, w, h);
}
