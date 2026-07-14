#include "UiIconSet.h"
#include "AssetLoader.h"

void UiIconSet::Load(UiRenderer &ui)
{
    const std::vector<uint8_t> atlasBytes = LoadAssetBytes("icons/icons_atlas.png");
    uint32_t width = 0, height = 0;
    m_atlas = ui.LoadImage(atlasBytes, width, height);
}

void UiIconSet::Draw(UiRenderer &ui, UiIconId id, float x, float y, float size, float alpha, const XrColor4f &tint) const
{
    if (id == UiIconId::None)
        return;
    const UiIconUv &uv = kIconUvs[static_cast<size_t>(id)];
    ui.DrawImageRegion(m_atlas, x, y, size, size, uv.u0, uv.v0, uv.u1, uv.v1, alpha, tint);
}
