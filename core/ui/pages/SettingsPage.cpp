#include "SettingsPage.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

void SettingsPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Menu Button Mapping", [this](MenuItem *)
                   { if (menuButtonMapPage)    Navigate(menuButtonMapPage,    1); }, nullptr, nullptr, UiIconId::Mapping);
    list->AddEntry("Emulator Button Mapping", [this](MenuItem *)
                   { if (emulatorButtonMapPage) Navigate(emulatorButtonMapPage, 1); }, nullptr, nullptr, UiIconId::Mapping);
    list->AddEntry("Move Screen", [this](MenuItem *)
                   { if (moveScreenPage)        Navigate(moveScreenPage,        1); }, nullptr, nullptr, UiIconId::Move);
    list->AddEntry("Follow Head: No", nullptr, [](MenuItem *) { /* TODO: toggle */ }, [](MenuItem *) { /* TODO: toggle */ }, UiIconId::FollowHead);
    list->AddEntry("Save and Back", [this](MenuItem *)
                   { if (mainPage) Navigate(mainPage, -1); }, nullptr, nullptr, UiIconId::Back);

    m_menu.MenuItems.push_back(list);

    // Version string, bottom-right, small/secondary font - not part of the
    // scrollable list (non-selectable, doesn't participate in navigation).
    // MenuLabel centers text within its box, so measure the actual text
    // width and size the box to match - that's what makes it right-aligned
    // against kMenuContentX instead of just centered somewhere near it.
    const int versionWidth = static_cast<int>(ui.GetTextWidth(resources.smallFont, kVersionString)) + 1;
    constexpr int kVersionHeight = 16;
    auto versionLabel = std::make_shared<MenuLabel>(
        ui, resources.smallFont, kVersionString,
        kMenuWidth - 10 - versionWidth, kMenuHeight - kBottomHeight - kVersionHeight - 8,
        versionWidth, kVersionHeight, kMenuVersionColor);
    m_menu.MenuItems.push_back(versionLabel);

    m_menu.BackPress = [this]()
    { if (mainPage) Navigate(mainPage, -1); };
    m_menu.Init();
}
