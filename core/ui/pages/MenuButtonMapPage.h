#pragma once
#include "../MenuPage.h"

#include <memory>

class MenuList;
class AppMenu;
struct AppSettings;

// Menu button mapping: Swap Select/Back toggle + two extra select-button
// assignment slots (see Menu::ExtraSelectButton1/2).
class MenuButtonMapPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    void ToggleSwap();
    void StartCapture(int slot); // slot: 0 = menuButton1, 1 = menuButton2
    void RefreshLabels();

    std::shared_ptr<MenuList::Entry> m_swapEntry;
    std::shared_ptr<MenuList::Entry> m_button1Entry;
    std::shared_ptr<MenuList::Entry> m_button2Entry;
    AppSettings *m_settings = nullptr;
    AppMenu *m_appMenu = nullptr;
};
