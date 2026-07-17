#pragma once
#include "menu/MenuPage.h"

#include <memory>

// ROM selection browser - one row per ROM found by ScanRoms(), picking a
// ROM loads it and returns to MainPage. Shows "(No ROMs found)" if empty.
// On Android with no ROMs folder configured yet, shows a "Pick ROMs
// folder..." entry instead (see RomSelectPage.cpp).
class RomSelectPage : public MenuPage
{
public:
    MenuPage *mainPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    // Android-only "Pick ROMs folder..." row, kept so its own press handler
    // can relabel it (see Init).
    std::shared_ptr<MenuList::Entry> m_pickEntry;
};
