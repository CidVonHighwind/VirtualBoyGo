#include "EmulatorButtonMapPage.h"
#include "../../Emulator.h"
#include "../../Settings.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

namespace
{
struct ButtonRow
{
    const char *name;
    UiIconId icon;
    uint32_t vbBit; // VBButtonBit constant - index into AppSettings::vbButtons
};

// Matches the reference's button_icons[] (Emulator.cpp) - a fixed icon per
// VB button, keyed by the VB button itself rather than whatever physical
// input it's currently bound to. The Map* icons already existed in the
// atlas (packed from reference/VirtualBoyGoMaster/assets/icons/mapping/*)
// but were never wired up here until now.
const ButtonRow kButtons[] = {
    {"A", UiIconId::ButtonA, VBButtonBit::A},
    {"B", UiIconId::ButtonB, VBButtonBit::B},
    {"L", UiIconId::MapTriggerLeft, VBButtonBit::L},
    {"R", UiIconId::MapTriggerRight, VBButtonBit::R},
    {"Up", UiIconId::MapLeftUp, VBButtonBit::LeftUp},
    {"Down", UiIconId::MapLeftDown, VBButtonBit::LeftDown},
    {"Left", UiIconId::MapLeftLeft, VBButtonBit::LeftLeft},
    {"Right", UiIconId::MapLeftRight, VBButtonBit::LeftRight},
    {"R-Up", UiIconId::MapRightUp, VBButtonBit::RightUp},
    {"R-Down", UiIconId::MapRightDown, VBButtonBit::RightDown},
    {"R-Left", UiIconId::MapRightLeft, VBButtonBit::RightLeft},
    {"R-Right", UiIconId::MapRightRight, VBButtonBit::RightRight},
    {"Start", UiIconId::MapStart, VBButtonBit::Start},
    {"Select", UiIconId::MapSelect, VBButtonBit::Select},
};
constexpr int kButtonCount = static_cast<int>(sizeof(kButtons) / sizeof(kButtons[0]));

std::string FormatBinding(const char *name, const ButtonMapper::MappedButton &b)
{
    if (!b.IsSet)
        return std::string(name) + ": [Unset]";
    return std::string(name) + ": " + ButtonMapper::MapButtonStr[b.InputDevice * 32 + b.ButtonIndex];
}
} // namespace

void EmulatorButtonMapPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_settings = resources.settings;

    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    for (int i = 0; i < kButtonCount; ++i)
    {
        const ButtonRow &row = kButtons[i];
        m_rowEntries.push_back(list->AddEntry(std::string(row.name) + ": [Unset]",
            [this, i, vbBit = row.vbBit](MenuItem *) { StartCapture(i, vbBit); },
            nullptr, nullptr, row.icon));
    }

    list->AddSpacer(kMenuSpacerSize);

    list->AddEntry("Reset Mapping", [this](MenuItem *) { ResetMapping(); }, nullptr, nullptr, UiIconId::ResetView);

    m_menu.MenuItems.push_back(list);
    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();

    RefreshLabels();
}

void EmulatorButtonMapPage::StartCapture(int rowIndex, uint32_t vbBit)
{
    if (!m_settings)
        return;

    m_rowEntries[rowIndex]->SetText(std::string(kButtons[rowIndex].name) + ": press a button...");

    // Two-phase: first wait for every button already held (the one that
    // triggered this row's select) to be released, then watch for the next
    // fresh press - otherwise the very select press that opened this
    // capture would immediately "bind" itself.
    auto waitingForRelease = std::make_shared<bool>(true);
    SetCaptureHook([this, rowIndex, vbBit, waitingForRelease](uint32_t *buttonState, uint32_t *lastButtonState) -> bool
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
                    m_settings->vbButtons[vbBit] = {true, device, static_cast<int>(bit)};
                    RefreshLabels();
                    SetCaptureHook(nullptr);
                    return false;
                }
            }
        }
        return true;
    });
}

void EmulatorButtonMapPage::ResetMapping()
{
    if (!m_settings)
        return;
    for (auto &binding : m_settings->vbButtons)
        binding.IsSet = false;
    RefreshLabels();
}

void EmulatorButtonMapPage::RefreshLabels()
{
    if (!m_settings)
        return;
    for (int i = 0; i < kButtonCount; ++i)
        m_rowEntries[i]->SetText(FormatBinding(kButtons[i].name, m_settings->vbButtons[kButtons[i].vbBit]));

    m_settings->Save(); // always-on autosave - no explicit save action anywhere in the menu anymore
}
