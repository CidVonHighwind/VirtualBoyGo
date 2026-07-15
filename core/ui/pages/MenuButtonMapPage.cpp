#include "MenuButtonMapPage.h"
#include "../../Settings.h"
#include "../AppMenu.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

namespace
{
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

    m_swapEntry = list->AddEntry("Swap Select/Back: No", [this](MenuItem *) { ToggleSwap(); }, // Select acts like Right - same toggle
        [this](MenuItem *) { ToggleSwap(); }, [this](MenuItem *) { ToggleSwap(); });
    m_button1Entry = list->AddEntry("Menu Button 1: [Unset]",
        [this](MenuItem *) { StartCapture(0); }, nullptr, nullptr);
    m_button2Entry = list->AddEntry("Menu Button 2: [Unset]",
        [this](MenuItem *) { StartCapture(1); }, nullptr, nullptr);

    m_menu.MenuItems.push_back(list);
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

void MenuButtonMapPage::StartCapture(int slot)
{
    if (!m_settings)
        return;

    const auto &entry = (slot == 0) ? m_button1Entry : m_button2Entry;
    entry->SetText(slot == 0 ? "Menu Button 1: press a button..." : "Menu Button 2: press a button...");

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
    if (!m_settings)
        return;
    m_swapEntry->SetText(m_settings->swapSelectBackButton ? "Swap Select/Back: Yes" : "Swap Select/Back: No");
    m_button1Entry->SetText(FormatBinding("Menu Button 1", m_settings->menuButton1));
    m_button2Entry->SetText(FormatBinding("Menu Button 2", m_settings->menuButton2));

    m_settings->Save(); // always-on autosave - no explicit save action anywhere in the menu anymore
}
