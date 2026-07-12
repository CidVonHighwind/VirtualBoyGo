#include "RomSelectPage.h"
#include "../../RomScanner.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void RomSelectPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    // No icon-bearing entries yet - still a placeholder page.
    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize);
    list->Color = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    const std::vector<RomEntry> roms = ScanRoms();
    if (roms.empty())
    {
        list->AddEntry("(No ROMs found)");
    }
    else
    {
        for (const RomEntry &rom : roms)
        {
            // TODO: actually load romPath's bytes into the emulator core
            // once one exists (see core/Emulator.h) - for now selecting a
            // ROM just returns to the main page, same as pressing Back.
            const std::string romPath = rom.fullPath;
            list->AddEntry(rom.name, [this, romPath](MenuItem *) {
                (void)romPath;
                if (mainPage)
                    Navigate(mainPage, -1);
            });
        }
    }

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]()
    { if (mainPage) Navigate(mainPage, -1); };
    m_menu.Init();
}
