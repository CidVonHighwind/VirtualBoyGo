#include "RomSelectPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void RomSelectPage::Init(UiRenderer &ui, UiFontHandle menuFont)
{
    auto list = std::make_shared<MenuList>(ui, menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight, kMenuItemSize);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    // Placeholder until ROM scanning is wired up
    list->AddEntry("(No ROMs found)");

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (mainPage) Navigate(mainPage, -1); };
    m_menu.Init();
}
