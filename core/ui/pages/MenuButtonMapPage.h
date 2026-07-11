#pragma once
#include "../MenuPage.h"

// Menu button mapping: Swap Select/Back toggle + two button assignment slots.
class MenuButtonMapPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;
};
