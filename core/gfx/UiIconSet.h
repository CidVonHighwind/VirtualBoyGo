#pragma once

#include "gfx/UiRenderer.h"
#include "gfx/generated_icons/UiIconAtlas.h"

#include <array>

// Loads the scale-specific packed icon atlases built by
// tools/pack_icon_atlas.py and draws individual icons via one shared UV grid.
class UiIconSet
{
public:
    void Load(UiRenderer &ui, float menuScale);
    void SetMenuScale(UiRenderer &ui, float menuScale);

    void Draw(UiRenderer &ui, UiIconId id, float x, float y, float size, float alpha = 1.0f,
              const XrColor4f &tint = XrColor4f{1.0f, 1.0f, 1.0f, 1.0f}) const;

private:
    std::array<UiImageHandle, 6> m_atlases;
    size_t m_activeAtlas = 0;

    void EnsureAtlasLoaded(UiRenderer &ui, size_t index);
};
