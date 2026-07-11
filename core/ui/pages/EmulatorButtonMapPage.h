#pragma once
#include "../MenuPage.h"

// Per-button emulator input mapping. Stub items for each of the 14
// Virtual Boy buttons; actual remapping logic is TODO.
class EmulatorButtonMapPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, UiFontHandle menuFont, const UiIconSet &icons) override;
};
