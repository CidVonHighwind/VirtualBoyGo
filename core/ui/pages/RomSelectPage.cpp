#include "RomSelectPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void RomSelectPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    // No icon-bearing entries yet - still a placeholder page.
    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize);
    list->Color = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    // Placeholder until ROM scanning is wired up
    list->AddEntry("(No ROMs found)");
    // TEMP unicode glyph test
    list->AddEntry(u8"Jörg's Ünïcode Café.vb");
    list->AddEntry(u8"日本語のファイル名テスト");
    list->AddEntry(u8"Prîncé – Spéçîál Édïtïon™ ©®€.vb");

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]()
    { if (mainPage) Navigate(mainPage, -1); };
    m_menu.Init();
}
