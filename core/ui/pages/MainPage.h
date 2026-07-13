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

    // Also refreshes the save preview - see MenuPage::ResetSelection's doc
    // comment for why this is the hook that has to carry it (this is what
    // fires when a ROM load jumps straight back to MainPage, and it's the
    // only chance to pick up that ROM's existing saves without waiting for
    // the user to touch Save/the slot stepper first).
    void ResetSelection() override;

private:
    static constexpr int kMinSaveSlot = 1;
    static constexpr int kMaxSaveSlot = 9;
    static constexpr int kSaveSlotEntryIndex = 2; // Resume, Reset Game, Save Slot, ...

    void ChangeSaveSlot(int delta);
    // Reloads the preview thumbnail (or clears it) for whatever slot
    // m_saveSlot currently is - called on Init (initial slot), whenever
    // ChangeSaveSlot/the Save button changes what's actually on disk, and
    // from ResetSelection (a ROM just loaded - see its doc comment).
    void RefreshSavePreview();

    std::shared_ptr<MenuList> m_list;
    std::shared_ptr<MenuImage> m_preview;
    Emulator *m_emulator = nullptr;
    int m_saveSlot = kMinSaveSlot;
};
