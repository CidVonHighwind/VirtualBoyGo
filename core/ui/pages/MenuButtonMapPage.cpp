#include "MenuButtonMapPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void MenuButtonMapPage::Init(UiRenderer &ui, UiFontHandle menuFont)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Swap Select/Back: No", nullptr,
        [](MenuItem *) { /* TODO: toggle */ },
        [](MenuItem *) { /* TODO: toggle */ });
    list->AddEntry("Menu Button 1: [Unset]",
        [](MenuItem *) { /* TODO: open mapping overlay */ },
        [](MenuItem *) { /* TODO: cycle left */ },
        [](MenuItem *) { /* TODO: cycle right */ });
    list->AddEntry("Menu Button 2: [Unset]",
        [](MenuItem *) { /* TODO: open mapping overlay */ },
        [](MenuItem *) { /* TODO: cycle left */ },
        [](MenuItem *) { /* TODO: cycle right */ });
    list->AddEntry("Back", [this](MenuItem *) { if (settingsPage) Navigate(settingsPage, -1); });

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();
}
