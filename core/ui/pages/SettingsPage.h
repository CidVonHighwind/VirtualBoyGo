#pragma once
#include "../MenuPage.h"

#include <memory>

class MenuList;
struct AppSettings;

// Settings: Menu Button Mapping, Emulator Button Mapping, Move Screen,
// Follow Head toggle, 3D/2D mode, IPD offset, Color Palette, R/G/B custom
// tint, Version label. Every change autosaves immediately (see
// RefreshLabels) - there's no explicit Save action or in-list Back entry;
// Back is the bottom-bar B hint (see MenuPage::HasBackAction).
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
    static constexpr int kFollowHeadIndex = 4;
    static constexpr int kThreeDeeModeIndex = 6;
    static constexpr int kIpdIndex = 7;
    // 9 is "Color Palette" - its label never changes (see RefreshLabels).
    static constexpr int kColorRIndex = 10;
    static constexpr int kColorGIndex = 11;
    static constexpr int kColorBIndex = 12;
#if defined(__ANDROID__)
    // 13 is the spacer ahead of it.
    static constexpr int kChangeRomsFolderIndex = 14;
#endif

    static constexpr float kIpdStep = 1.0f / 256.0f; // matches FrontendGo's IPD_STEP_SIZE
    static constexpr float kIpdMin = -0.125f;
    static constexpr float kIpdMax = 0.125f;
    static constexpr float kColorStep = 0.05f; // matches FrontendGo's COLOR_STEP_SIZE

    void ToggleFollowHead();
    void ToggleThreeDeeMode();
    void ChangeIpd(int delta);
    void ChangePalette(int delta);
    void ChangeColorChannel(float AppSettings::*channel, float delta);
    void RefreshLabels();
#if defined(__ANDROID__)
    void RequestChangeRomsFolder();
#endif

    std::shared_ptr<MenuList> m_list;
    AppSettings *m_settings = nullptr;
};
