#include "SettingsPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void SettingsPage::Init(UiRenderer &ui, UiFontHandle menuFont)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Menu Button Mapping",     [this](MenuItem *) { if (menuButtonMapPage)    Navigate(menuButtonMapPage,    1); });
    list->AddEntry("Emulator Button Mapping", [this](MenuItem *) { if (emulatorButtonMapPage) Navigate(emulatorButtonMapPage, 1); });
    list->AddEntry("Move Screen",             [this](MenuItem *) { if (moveScreenPage)        Navigate(moveScreenPage,        1); });
    list->AddEntry("Follow Head: No", nullptr,
        [](MenuItem *) { /* TODO: toggle */ },
        [](MenuItem *) { /* TODO: toggle */ });
    list->AddEntry("Save and Back", [this](MenuItem *) { if (mainPage) Navigate(mainPage, -1); });

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (mainPage) Navigate(mainPage, -1); };
    m_menu.Init();
}
