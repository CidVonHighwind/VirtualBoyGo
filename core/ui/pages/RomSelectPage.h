#pragma once
#include "../MenuPage.h"

// ROM selection browser - one row per ROM found by ScanRoms(), picking a
// ROM loads it and returns to MainPage. Shows "(No ROMs found)" if empty.
// On Android with no ROMs folder configured yet, shows a "Pick ROMs
// folder..." entry instead (see RomSelectPage.cpp).
class RomSelectPage : public MenuPage
{
public:
    MenuPage *mainPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
};
