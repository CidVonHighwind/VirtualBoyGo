#pragma once
#include "../MenuPage.h"

// Main in-game menu: Resume, Reset, Save Slot, Save, Load, Load ROM,
// Reset View, Settings, Exit.
// Navigation out: Load ROM → RomSelectPage, Settings → SettingsPage.
class MainPage : public MenuPage
{
public:
    // Pointers to neighbouring pages - set by AppMenu before Init().
    MenuPage *romSelectPage = nullptr;
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
};
