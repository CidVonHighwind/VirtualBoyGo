#include "MoveScreenPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void MoveScreenPage::Init(UiRenderer &ui, UiFontHandle menuFont)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Yaw: 0 deg",    nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ });
    list->AddEntry("Pitch: 0 deg",  nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ });
    list->AddEntry("Roll: 0 deg",   nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ });
    list->AddEntry("Distance: 5.5", nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ });
    list->AddEntry("Scale: 1.0x",   nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ });
    list->AddEntry("Reset View", [](MenuItem *) { /* TODO */ });
    list->AddEntry("Back", [this](MenuItem *) { if (settingsPage) Navigate(settingsPage, -1); });

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();
}
