#include "MenuButtonMapPage.h"
#include "../../Settings.h"
#include "../AppMenu.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

namespace
{
constexpr int kSwapIndex = 0;
constexpr int kButton1Index = 1;
constexpr int kButton2Index = 2;

std::string FormatBinding(const char *name, const ButtonMapper::MappedButton &b)
{
    if (!b.IsSet)
        return std::string(name) + ": [Unset]";
    return std::string(name) + ": " + ButtonMapper::MapButtonStr[b.InputDevice * 32 + b.ButtonIndex];
}
} // namespace

void MenuButtonMapPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_settings = resources.settings;
    m_appMenu = resources.appMenu;

    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Swap Select/Back: No", nullptr,
        [this](MenuItem *) { ToggleSwap(); }, [this](MenuItem *) { ToggleSwap(); });
    list->AddEntry("Menu Button 1: [Unset]",
        [this](MenuItem *) { StartCapture(kButton1Index, 0); }, nullptr, nullptr);
    list->AddEntry("Menu Button 2: [Unset]",
        [this](MenuItem *) { StartCapture(kButton2Index, 1); }, nullptr, nullptr);
    list->AddEntry("Back", [this](MenuItem *) { if (settingsPage) Navigate(settingsPage, -1); }, nullptr, nullptr, UiIconId::Back);

    m_menu.MenuItems.push_back(list);
    m_list = list;
    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();

    RefreshLabels();
}

void MenuButtonMapPage::ToggleSwap()
{
    if (!m_settings)
        return;
    m_settings->swapSelectBackButton = !m_settings->swapSelectBackButton;
    if (m_appMenu)
        m_appMenu->ApplyMenuButtonSettings();
    RefreshLabels();
}

void MenuButtonMapPage::StartCapture(int rowIndex, int slot)
{
    if (!m_settings || !m_list)
        return;

    m_list->SetEntryText(rowIndex, (slot == 0 ? "Menu Button 1: press a button..." : "Menu Button 2: press a button..."));

    // Same two-phase release-then-capture as EmulatorButtonMapPage - see its
    // StartCapture doc comment.
    auto waitingForRelease = std::make_shared<bool>(true);
    SetCaptureHook([this, slot, waitingForRelease](uint32_t *buttonState, uint32_t *lastButtonState) -> bool
    {
        if (*waitingForRelease)
        {
            if (!buttonState[0] && !buttonState[1] && !buttonState[2])
                *waitingForRelease = false;
            return true;
        }

        for (int device = 0; device < 3; ++device)
        {
            for (uint32_t bit = 0; bit < static_cast<uint32_t>(ButtonMapper::EmuButtonCount); ++bit)
            {
                const uint32_t mask = ButtonMapper::ButtonMapping[bit];
                if ((buttonState[device] & mask) && !(lastButtonState[device] & mask))
                {
                    ButtonMapper::MappedButton &target = (slot == 0) ? m_settings->menuButton1 : m_settings->menuButton2;
                    target = {true, device, static_cast<int>(bit)};
                    if (m_appMenu)
                        m_appMenu->ApplyMenuButtonSettings();
                    RefreshLabels();
                    SetCaptureHook(nullptr);
                    return false;
                }
            }
        }
        return true;
    });
}

void MenuButtonMapPage::RefreshLabels()
{
    if (!m_settings || !m_list)
        return;
    m_list->SetEntryText(kSwapIndex, m_settings->swapSelectBackButton ? "Swap Select/Back: Yes" : "Swap Select/Back: No");
    m_list->SetEntryText(kButton1Index, FormatBinding("Menu Button 1", m_settings->menuButton1));
    m_list->SetEntryText(kButton2Index, FormatBinding("Menu Button 2", m_settings->menuButton2));
}
