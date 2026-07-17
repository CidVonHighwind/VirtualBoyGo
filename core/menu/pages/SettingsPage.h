#pragma once
#include "menu/MenuPage.h"

#include <memory>

class MenuList;
struct AppSettings;
class Platform;

// Settings: Button Mapping, Adjust Screen, and the VB screen Color Palette +
// custom R/G/B tint, plus a version label (and Change ROMs Folder, on
// platforms where Platform::SupportsChangeRomsFolder() is true). Screen
// placement/view settings live in the Adjust Screen page. Every change
// autosaves immediately (see RefreshLabels); Back is the bottom-bar B hint
// (see MenuPage::HasBackAction).
class SettingsPage : public MenuPage
{
public:
    MenuPage *mainPage = nullptr;
    MenuPage *emulatorButtonMapPage = nullptr;
    MenuPage *moveScreenPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    static constexpr float kColorStep = 0.05f; // matches FrontendGo's COLOR_STEP_SIZE

    void ChangePalette(int delta);
    void ChangeColorChannel(float AppSettings::*channel, float delta);
    void RefreshLabels();
    void RequestChangeRomsFolder();

    std::shared_ptr<MenuList::Entry> m_colorREntry;
    std::shared_ptr<MenuList::Entry> m_colorGEntry;
    std::shared_ptr<MenuList::Entry> m_colorBEntry;
    std::shared_ptr<MenuList::Entry> m_changeRomsFolderEntry;
    AppSettings *m_settings = nullptr;
    Platform *m_platform = nullptr;
};
