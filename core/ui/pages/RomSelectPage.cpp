#include "RomSelectPage.h"
#include "../../Emulator.h"
#include "../../RomScanner.h"
#include "../AppMenu.h"
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
        Emulator *emulator = resources.emulator;
        AppMenu *appMenu = resources.appMenu;
        for (const RomEntry &rom : roms)
        {
            const std::string romPath = rom.fullPath;
            list->AddEntry(rom.name, [this, emulator, appMenu, romPath](MenuItem *) {
                if (emulator && emulator->LoadRom(romPath) && appMenu)
                    appMenu->Hide(); // go straight to the game instead of back to the menu
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
