#pragma once
#include "../MenuPage.h"

// Settings: Menu Button Mapping, Emulator Button Mapping, Move Screen,
// Follow Head toggle, Save and Back, Version label.
class SettingsPage : public MenuPage
{
public:
    MenuPage *mainPage = nullptr;
    MenuPage *menuButtonMapPage = nullptr;
    MenuPage *emulatorButtonMapPage = nullptr;
    MenuPage *moveScreenPage = nullptr;

    void Init(UiRenderer &ui, UiFontHandle menuFont) override;
};
