#include "MoveScreenPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void MoveScreenPage::Init(UiRenderer &ui, UiFontHandle menuFont, const UiIconSet &icons)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize, &icons);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Yaw: 0 deg",    nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ }, UiIconId::LeftRight);
    list->AddEntry("Pitch: 0 deg",  nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ }, UiIconId::UpDown);
    list->AddEntry("Roll: 0 deg",   nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ }, UiIconId::Reset);
    list->AddEntry("Distance: 5.5", nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ }, UiIconId::Distance);
    list->AddEntry("Scale: 1.0x",   nullptr, [](MenuItem *) { /* TODO: dec */ }, [](MenuItem *) { /* TODO: inc */ }, UiIconId::Scale);
    list->AddEntry("Reset View", [](MenuItem *) { /* TODO */ }, nullptr, nullptr, UiIconId::ResetView);
    list->AddEntry("Back", [this](MenuItem *) { if (settingsPage) Navigate(settingsPage, -1); }, nullptr, nullptr, UiIconId::Back);

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();
}
