#pragma once
#include "../MenuPage.h"

#include <memory>

class MenuList;
class MenuImage;
class Emulator;

// Main in-game menu: Resume, Reset, Save Slot, Save, Load, Load ROM,
// Settings, Exit.
// Navigation out: Load ROM → RomSelectPage, Settings → SettingsPage.
class MainPage : public MenuPage
{
public:
    // Pointers to neighbouring pages - set by AppMenu before Init().
    MenuPage *romSelectPage = nullptr;
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
    void ResetSelection() override;

    // AppMenu boots straight into RomSelectPage (nothing's loaded yet, so
    // Resume/Save/Load would be dead) rather than going through MainPage's
    // own "Load ROM" row first - call this once at boot so backing out of
    // RomSelectPage still lands on "Load ROM" instead of row 0 ("Resume"),
    // matching what a real MainPage -> Load ROM -> RomSelectPage trip would
    // have left selected.
    void SelectLoadRomEntry();

private:
    static constexpr int kMinSaveSlot = 0;
    static constexpr int kMaxSaveSlot = 9; // 10 slots total (0-9), matches FrontendGo's saveStates[10]
    static constexpr int kSaveSlotEntryIndex = 3; // Resume, Reset Game, [spacer], Save Slot, ...
    static constexpr int kLoadRomEntryIndex = 7;  // ...Load, [spacer], Load ROM, ...

    void ChangeSaveSlot(int delta);
    void RefreshSavePreview();

    std::shared_ptr<MenuList> m_list;
    std::shared_ptr<MenuImage> m_preview;
    Emulator *m_emulator = nullptr;
    int m_saveSlot = kMinSaveSlot;
};
