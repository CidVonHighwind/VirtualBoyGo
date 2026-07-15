#include "UiIconSet.h"
#include "AssetLoader.h"

namespace
{
    constexpr int kAtlasSizes[] = {10, 20, 30, 40, 50, 60};
    // MenuList draws the largest icons at 10 logical pixels. Footer hints
    // are slightly smaller at 9, so this covers every current icon use.
    constexpr float kMaxLogicalIconSize = 10.0f;
}

void UiIconSet::Load(UiRenderer &ui, float menuScale)
{
    SetMenuScale(ui, menuScale);
}

void UiIconSet::EnsureAtlasLoaded(UiRenderer &ui, size_t index)
{
    if (m_atlases[index].IsValid())
        return;

    const std::string atlasPath = "icons/icons_atlas_" + std::to_string(kAtlasSizes[index]) + ".png";
    const std::vector<uint8_t> atlasBytes = LoadAssetBytes(atlasPath.c_str());
    uint32_t width = 0, height = 0;
    m_atlases[index] = ui.LoadImage(atlasBytes, width, height);
}

void UiIconSet::SetMenuScale(UiRenderer &ui, float menuScale)
{
    // Choose the smallest source that covers the largest physical icon.
    // Downsampling from the next tier is crisp; upsampling a smaller tier is
    // avoided. Atlases are loaded lazily so unused high-resolution textures
    // consume no GPU memory.
    const float requiredPixels = kMaxLogicalIconSize * menuScale;
    m_activeAtlas = 0;
    while (m_activeAtlas + 1 < m_atlases.size() && kAtlasSizes[m_activeAtlas] < requiredPixels)
        ++m_activeAtlas;
    EnsureAtlasLoaded(ui, m_activeAtlas);
}

void UiIconSet::Draw(UiRenderer &ui, UiIconId id, float x, float y, float size, float alpha, const XrColor4f &tint) const
{
    if (id == UiIconId::None)
        return;
    const UiIconUv &uv = kIconUvs[static_cast<size_t>(id)];
    ui.DrawImageRegion(m_atlases[m_activeAtlas], x, y, size, size, uv.u0, uv.v0, uv.u1, uv.v1, alpha, tint);
}
