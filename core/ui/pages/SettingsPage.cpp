#include "SettingsPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void SettingsPage::Init(UiRenderer &ui, UiFontHandle menuFont, const UiIconSet &icons)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize, &icons);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Menu Button Mapping",     [this](MenuItem *) { if (menuButtonMapPage)    Navigate(menuButtonMapPage,    1); },
        nullptr, nullptr, UiIconId::Mapping);
    list->AddEntry("Emulator Button Mapping", [this](MenuItem *) { if (emulatorButtonMapPage) Navigate(emulatorButtonMapPage, 1); },
        nullptr, nullptr, UiIconId::Mapping);
    list->AddEntry("Move Screen",             [this](MenuItem *) { if (moveScreenPage)        Navigate(moveScreenPage,        1); },
        nullptr, nullptr, UiIconId::Move);
    list->AddEntry("Follow Head: No", nullptr,
        [](MenuItem *) { /* TODO: toggle */ },
        [](MenuItem *) { /* TODO: toggle */ },
        UiIconId::FollowHead);
    list->AddEntry("Save and Back", [this](MenuItem *) { if (mainPage) Navigate(mainPage, -1); },
        nullptr, nullptr, UiIconId::Back);

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (mainPage) Navigate(mainPage, -1); };
    m_menu.Init();
}
