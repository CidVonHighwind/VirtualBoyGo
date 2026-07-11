#include "EmulatorButtonMapPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void EmulatorButtonMapPage::Init(UiRenderer &ui, UiFontHandle menuFont)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    static const char *kButtonNames[] = {
        "A", "B", "L", "R",
        "Up", "Down", "Left", "Right",
        "R-Up", "R-Down", "R-Left", "R-Right",
        "Start", "Select"
    };
    for (const char *name : kButtonNames)
        list->AddEntry(std::string(name) + ": [Unset]",
            [](MenuItem *) { /* TODO: open mapping overlay */ },
            [](MenuItem *) { /* TODO: cycle left */ },
            [](MenuItem *) { /* TODO: cycle right */ });

    list->AddEntry("Reset Mapping", [](MenuItem *) { /* TODO */ });
    list->AddEntry("Back", [this](MenuItem *) { if (settingsPage) Navigate(settingsPage, -1); });

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();
}
