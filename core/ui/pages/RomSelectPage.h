#pragma once
#include "../MenuPage.h"

// ROM selection browser. Placeholder for now - shows a single stub item.
// Will hold a MenuList<std::string> once ROM scanning is implemented.
class RomSelectPage : public MenuPage
{
public:
    MenuPage *mainPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
};
