#include "MainPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void MainPage::Init(UiRenderer &ui, UiFontHandle menuFont)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Resume");
    list->AddEntry("Reset Game");
    list->AddEntry("Save Slot: 1", nullptr,
        [](MenuItem *) { /* TODO: decrement slot */ },
        [](MenuItem *) { /* TODO: increment slot */ });
    list->AddEntry("Save");
    list->AddEntry("Load");
    list->AddEntry("Load ROM", [this](MenuItem *) { if (romSelectPage) Navigate(romSelectPage, 1); });
    list->AddEntry("Reset View");
    list->AddEntry("Settings",  [this](MenuItem *) { if (settingsPage)  Navigate(settingsPage,  1); });
    list->AddEntry("Exit");

    m_menu.MenuItems.push_back(list);
    m_menu.Init();
}
