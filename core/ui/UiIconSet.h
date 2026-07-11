#pragma once

#include "UiRenderer.h"
#include "generated_icons/UiIconAtlas.h"

// Loads the packed icon atlas (assets/icons/icons_atlas.png, built by
// tools/pack_icon_atlas.py) and draws individual icons out of it via
// UiRenderer::DrawImageRegion. One shared texture for every menu icon
// instead of one texture per icon.
class UiIconSet
{
public:
    void Load(UiRenderer &ui);

    void Draw(UiRenderer &ui, UiIconId id, float x, float y, float size, float alpha = 1.0f) const;

private:
    UiImageHandle m_atlas;
};
