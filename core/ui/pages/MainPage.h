#pragma once
#include "../MenuPage.h"

#include <memory>

class MenuList;
class MenuImage;
class Emulator;

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
    void ResetSelection() override;

private:
    static constexpr int kMinSaveSlot = 1;
    static constexpr int kMaxSaveSlot = 9;
    static constexpr int kSaveSlotEntryIndex = 2; // Resume, Reset Game, Save Slot, ...

    void ChangeSaveSlot(int delta);
    void RefreshSavePreview();

    std::shared_ptr<MenuList> m_list;
    std::shared_ptr<MenuImage> m_preview;
    Emulator *m_emulator = nullptr;
    int m_saveSlot = kMinSaveSlot;
};
