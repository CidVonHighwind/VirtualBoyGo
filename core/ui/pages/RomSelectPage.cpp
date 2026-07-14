#include "RomSelectPage.h"
#include "../../AndroidRomAccess.h"
#include "../../Emulator.h"
#include "../../RomScanner.h"
#include "../AppMenu.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void RomSelectPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;
    list->TintIconOnSelect = false; // cartridge glyph, not a status indicator - stays put when a row is selected

    if (!AndroidRomAccess::HasRomsFolder())
    {
        // Only reachable if "Change ROMs Folder..." (SettingsPage) cleared the
        // folder this session - re-picking needs an app restart, so just say so.
        MenuList *listPtr = list.get();
        list->AddEntry("Pick ROMs folder...", [listPtr](MenuItem *) {
            AndroidRomAccess::RequestChangeRomsFolder();
            listPtr->SetEntryText(0, "Folder cleared - restart the app!");
        });
    }
    else
    {
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
                const std::string romName = rom.name;
                list->AddEntry(rom.name, [this, emulator, appMenu, romPath, romName](MenuItem *) {
                    if (emulator && emulator->LoadRom(romPath, romName) && appMenu)
                        appMenu->Hide(); // go straight to the game instead of back to the menu
                    if (mainPage)
                        Navigate(mainPage, -1);
                }, nullptr, nullptr, UiIconId::VbCartridge);
            }
        }
    }

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]()
    { if (mainPage) Navigate(mainPage, -1); };
    m_menu.Init();
}
