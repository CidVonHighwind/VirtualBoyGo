#pragma once
#include "../MenuPage.h"

#include <memory>

class MenuList;
struct AppSettings;

// Settings: Menu Button Mapping, Emulator Button Mapping, Move Screen,
// Follow Head toggle, 3D/2D mode, IPD offset, Color Palette, R/G/B custom
// tint, Save and Back, Version label. Rows 4-8 (3D/2D..B) mirror
// FrontendGo's Emulator::InitSettingsMenu, which sat between Follow Head and
// Save and Back there too.
class SettingsPage : public MenuPage
{
public:
    MenuPage *mainPage = nullptr;
    MenuPage *menuButtonMapPage = nullptr;
    MenuPage *emulatorButtonMapPage = nullptr;
    MenuPage *moveScreenPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    // Entry indices within m_list - used by SetEntryText to relabel a row
    // after its value changes.
    static constexpr int kFollowHeadIndex = 3;
    static constexpr int kThreeDeeModeIndex = 4;
    static constexpr int kIpdIndex = 5;
    static constexpr int kPaletteIndex = 6;
    static constexpr int kColorRIndex = 7;
    static constexpr int kColorGIndex = 8;
    static constexpr int kColorBIndex = 9;

    static constexpr float kIpdStep = 1.0f / 256.0f;  // matches FrontendGo's IPD_STEP_SIZE
    static constexpr float kIpdMin = -0.125f;
    static constexpr float kIpdMax = 0.125f;
    static constexpr float kColorStep = 0.05f; // matches FrontendGo's COLOR_STEP_SIZE

    void ToggleFollowHead();
    void ToggleThreeDeeMode();
    void ChangeIpd(int delta);
    void ChangePalette(int delta);
    void ChangeColorChannel(float AppSettings::*channel, float delta);
    void RefreshLabels();

    std::shared_ptr<MenuList> m_list;
    AppSettings *m_settings = nullptr;
};
