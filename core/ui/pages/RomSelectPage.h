#pragma once
#include "../MenuPage.h"

// ROM selection browser - one row per ROM found by ScanRoms(), picking a
// ROM loads it and returns to MainPage. Shows "(No ROMs found)" if empty.
class RomSelectPage : public MenuPage
{
public:
    MenuPage *mainPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
};
