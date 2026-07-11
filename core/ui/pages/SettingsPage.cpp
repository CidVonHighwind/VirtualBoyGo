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
    const float versionWidth = ui.GetTextWidth(resources.smallFont, kVersionString) + 0.5f;
    constexpr float kVersionHeight = 8.0f;
    auto versionLabel = std::make_shared<MenuLabel>(
        ui, resources.smallFont, kVersionString,
        kMenuWidth - 5.0f - versionWidth, kMenuHeight - kBottomHeight - kVersionHeight - 4.0f,
        versionWidth, kVersionHeight, kMenuVersionColor);
    m_menu.MenuItems.push_back(versionLabel);

    m_menu.BackPress = [this]()
    { if (mainPage) Navigate(mainPage, -1); };
    m_menu.Init();
}
