#include "EmulatorButtonMapPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void EmulatorButtonMapPage::Init(UiRenderer &ui, UiFontHandle menuFont, const UiIconSet &icons)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize, &icons);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    // Only A/B have a fixed icon (matches the original) - the rest were
    // picked dynamically from the emulator's own button_icons there, which
    // isn't wired up yet, so they stay icon-less for now.
    static const struct { const char *name; UiIconId icon; } kButtons[] = {
        {"A", UiIconId::ButtonA}, {"B", UiIconId::ButtonB}, {"L", UiIconId::None}, {"R", UiIconId::None},
        {"Up", UiIconId::None}, {"Down", UiIconId::None}, {"Left", UiIconId::None}, {"Right", UiIconId::None},
        {"R-Up", UiIconId::None}, {"R-Down", UiIconId::None}, {"R-Left", UiIconId::None}, {"R-Right", UiIconId::None},
        {"Start", UiIconId::None}, {"Select", UiIconId::None}
    };
    for (const auto &button : kButtons)
        list->AddEntry(std::string(button.name) + ": [Unset]",
            [](MenuItem *) { /* TODO: open mapping overlay */ },
            [](MenuItem *) { /* TODO: cycle left */ },
            [](MenuItem *) { /* TODO: cycle right */ },
            button.icon);

    list->AddEntry("Reset Mapping", [](MenuItem *) { /* TODO */ }, nullptr, nullptr, UiIconId::ResetView);
    list->AddEntry("Back", [this](MenuItem *) { if (settingsPage) Navigate(settingsPage, -1); }, nullptr, nullptr, UiIconId::Back);

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();
}
