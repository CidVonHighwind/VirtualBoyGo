#include "MainPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void MainPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Resume", nullptr, nullptr, nullptr, UiIconId::Resume);
    list->AddEntry("Reset Game", nullptr, nullptr, nullptr, UiIconId::Reset);
    list->AddEntry("Save Slot: 1", nullptr,
        [](MenuItem *) { /* TODO: decrement slot */ },
        [](MenuItem *) { /* TODO: increment slot */ },
        UiIconId::SaveSlot);
    list->AddEntry("Save", nullptr, nullptr, nullptr, UiIconId::Save);
    list->AddEntry("Load", nullptr, nullptr, nullptr, UiIconId::Load);
    list->AddEntry("Load ROM", [this](MenuItem *) { if (romSelectPage) Navigate(romSelectPage, 1); },
        nullptr, nullptr, UiIconId::RomList);
    list->AddEntry("Reset View", nullptr, nullptr, nullptr, UiIconId::ResetView);
    list->AddEntry("Settings",  [this](MenuItem *) { if (settingsPage)  Navigate(settingsPage,  1); },
        nullptr, nullptr, UiIconId::Settings);
    list->AddEntry("Exit", nullptr, nullptr, nullptr, UiIconId::Exit);

    m_menu.MenuItems.push_back(list);
    m_menu.Init();
}
