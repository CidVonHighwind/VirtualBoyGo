#include "menu/pages/RomSelectPage.h"
#include "emu/Emulator.h"
#include "io/Platform.h"
#include "menu/AppMenu.h"
#include "menu/MenuPage.h"
#include "menu/pages/AppMenuLayout.h"

void RomSelectPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;
    list->TintIconOnSelect = false; // cartridge glyph, not a status indicator - stays put when a row is selected

    Platform *platform = resources.platform;
    if (!platform->HasRomsFolder())
    {
        // Only reachable if "Change ROMs Folder..." (SettingsPage) cleared the
        // folder this session - re-picking needs an app restart, so just say so.
        m_pickEntry = list->AddEntry("Pick ROMs folder...", [this, platform](MenuItem *) {
            platform->RequestChangeRomsFolder();
            m_pickEntry->SetText("Folder cleared - restart the app!");
        });
    }
    else
    {
        const std::vector<RomEntry> roms = platform->ScanRoms();
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
