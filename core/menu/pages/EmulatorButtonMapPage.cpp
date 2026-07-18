#include "menu/pages/EmulatorButtonMapPage.h"
#include "emu/Emulator.h"
#include "io/Platform.h"
#include "io/Settings.h"
#include "menu/MenuPage.h"
#include "menu/pages/AppMenuLayout.h"

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
// atlas (packed from reference/VirtualBoyGoMaster/assets/icons/mapping/*).
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

// Just the physical-input name, or "-" when unbound - the row's icon already
// says which VB button it is (matches the reference's SetMappingText).
std::string BindingStr(const ButtonMapper::MappedButton &b, ButtonMappingProfile profile)
{
    if (!b.IsSet)
        return "-";
    if (b.InputDevice == ButtonMapper::DeviceKeyboard)
    {
        if (b.ButtonIndex >= 'A' && b.ButtonIndex <= 'Z')
            return std::string(1, static_cast<char>(b.ButtonIndex));
        if (b.ButtonIndex >= '0' && b.ButtonIndex <= '9')
            return std::string(1, static_cast<char>(b.ButtonIndex));
        switch (b.ButtonIndex)
        {
        case 32: return "Space";
        case 256: return "Escape";
        case 257: return "Enter";
        case 258: return "Tab";
        case 259: return "Backspace";
        case 260: return "Insert";
        case 261: return "Delete";
        case 262: return "Right Arrow";
        case 263: return "Left Arrow";
        case 264: return "Down Arrow";
        case 265: return "Up Arrow";
        case 266: return "Page Up";
        case 267: return "Page Down";
        case 268: return "Home";
        case 269: return "End";
        default:
            if (b.ButtonIndex >= 290 && b.ButtonIndex <= 314)
                return "F" + std::to_string(b.ButtonIndex - 289);
            return "Key " + std::to_string(b.ButtonIndex);
        }
    }
    return ButtonMapper::MapButtonStr[b.InputDevice * 32 + b.ButtonIndex];
}
} // namespace

void EmulatorButtonMapPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_settings = resources.settings;
    m_platform = resources.platform;
    m_mappingProfile = resources.buttonMappingProfile;

    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;
    // The icon identifies the emulated button; it is not either of the two
    // selectable physical bindings and therefore never receives focus tint.
    list->TintIconOnSelect = false;

    // One row per VB button: its icon plus two side-by-side binding columns
    // (primary + secondary). Left/Right pick the column, pressing it rebinds
    // that slot - see StartCapture. RefreshLabels fills in the real bindings.
    for (int i = 0; i < kButtonCount; ++i)
    {
        auto entry = list->AddEntry("-", [this, i](MenuItem *) { StartCapture(i, m_list->GetActiveColumn()); },
                                    nullptr, nullptr, kButtons[i].icon);
        entry->twoColumn = true;
        m_rowEntries.push_back(entry);
    }

    list->AddSpacer(kMenuSpacerSize);

    auto resetEntry = list->AddEntry("Reset Mapping", [this](MenuItem *) { ResetMapping(); }, nullptr, nullptr,
                                     UiIconId::ResetView);
    resetEntry->centered = true;
    resetEntry->tintIconOnSelect = true;

    m_menu.MenuItems.push_back(list);
    m_list = list;
    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();

    // Fill every untouched slot from the same table used by Reset Mapping.
    // Existing user bindings are preserved.
    ApplyDefaultMapping(false);
    RefreshLabels();
}

void EmulatorButtonMapPage::StartCapture(int buttonIndex, int column)
{
    if (!m_settings)
        return;

    // Show a prompt in just the column being rebound; the other stays put.
    if (column == 0)
        m_rowEntries[buttonIndex]->SetText("press...");
    else
        m_rowEntries[buttonIndex]->SetSecondaryText("press...");
    const uint32_t vbBit = kButtons[buttonIndex].vbBit;

    // Two-phase: first wait for every button already held (the one that
    // triggered this row's select) to be released, then watch for the next
    // fresh press - otherwise the very select press that opened this
    // capture would immediately "bind" itself.
    auto waitingForRelease = std::make_shared<bool>(true);
    SetRawCaptureHook([this, vbBit, column, waitingForRelease](const ButtonMapper::MappedButton &button)
    {
        if (*waitingForRelease)
            return;
        m_settings->vbButtons[vbBit].Buttons[column] = button;
        RefreshLabels();
        SetCaptureHook(nullptr);
        SetRawCaptureHook(nullptr);
    });
    SetCaptureHook([waitingForRelease](uint32_t *buttonState, uint32_t *) -> bool
    {
        if (*waitingForRelease)
        {
            if (!buttonState[0] && !buttonState[1] && !buttonState[2])
                *waitingForRelease = false;
        }
        // This hook only suspends menu navigation and waits for release.
        // Actual bindings exclusively arrive through RawCaptureHook.
        return true;
    });
}

void EmulatorButtonMapPage::ResetMapping()
{
    if (!m_settings)
        return;

    ApplyDefaultMapping(true);
    RefreshLabels();
}

void EmulatorButtonMapPage::ApplyDefaultMapping(bool overwrite)
{
    if (!m_settings)
        return;

    if (overwrite)
        for (auto &pair : m_settings->vbButtons)
            pair = {};

    using namespace ButtonMapper;
    auto bind = [this](uint32_t vbBit, int slot, int device, uint32_t button)
    {
        ButtonMapper::MappedButton &binding = m_settings->vbButtons[vbBit].Buttons[slot];
        if (!binding.IsSet)
            binding = {true, device, static_cast<int>(button)};
    };

    // Slot 1 is the conventional gamepad layout on every platform.
    bind(VBButtonBit::A,          1, DeviceGamepad, EmuButton_A);
    bind(VBButtonBit::B,          1, DeviceGamepad, EmuButton_B);
    bind(VBButtonBit::L,          1, DeviceGamepad, EmuButton_LShoulder);
    bind(VBButtonBit::R,          1, DeviceGamepad, EmuButton_RShoulder);
    bind(VBButtonBit::LeftUp,     1, DeviceGamepad, EmuButton_Up);
    bind(VBButtonBit::LeftDown,   1, DeviceGamepad, EmuButton_Down);
    bind(VBButtonBit::LeftLeft,   1, DeviceGamepad, EmuButton_Left);
    bind(VBButtonBit::LeftRight,  1, DeviceGamepad, EmuButton_Right);
    bind(VBButtonBit::RightUp,    1, DeviceGamepad, EmuButton_RightStickUp);
    bind(VBButtonBit::RightDown,  1, DeviceGamepad, EmuButton_RightStickDown);
    bind(VBButtonBit::RightLeft,  1, DeviceGamepad, EmuButton_RightStickLeft);
    bind(VBButtonBit::RightRight, 1, DeviceGamepad, EmuButton_RightStickRight);
    bind(VBButtonBit::Start,      1, DeviceGamepad, EmuButton_Enter);
    bind(VBButtonBit::Select,     1, DeviceGamepad, EmuButton_Back);

    if (m_mappingProfile == ButtonMappingProfile::Desktop)
    {
        // Keyboard gameplay uses a slot separate from desktop menu input.
        bind(VBButtonBit::LeftUp,     0, DeviceKeyboard, 265); // GLFW_KEY_UP
        bind(VBButtonBit::LeftDown,   0, DeviceKeyboard, 264);
        bind(VBButtonBit::LeftLeft,   0, DeviceKeyboard, 263);
        bind(VBButtonBit::LeftRight,  0, DeviceKeyboard, 262);
        bind(VBButtonBit::RightUp,    0, DeviceKeyboard, 'W');
        bind(VBButtonBit::RightDown,  0, DeviceKeyboard, 'S');
        bind(VBButtonBit::RightLeft,  0, DeviceKeyboard, 'A');
        bind(VBButtonBit::RightRight, 0, DeviceKeyboard, 'D');
        bind(VBButtonBit::A,          0, DeviceKeyboard, 'X');
        bind(VBButtonBit::B,          0, DeviceKeyboard, 'Z');
        bind(VBButtonBit::L,          0, DeviceKeyboard, 'Q');
        bind(VBButtonBit::R,          0, DeviceKeyboard, 'E');
        bind(VBButtonBit::Start,      0, DeviceKeyboard, 257); // GLFW_KEY_ENTER
        bind(VBButtonBit::Select,     0, DeviceKeyboard, 259); // GLFW_KEY_BACKSPACE
    }
    else
    {
        // OpenXR controller layout mirrors the raw states produced by XrInput.
        bind(VBButtonBit::LeftUp,     0, DeviceLeftTouch, EmuButton_LeftStickUp);
        bind(VBButtonBit::LeftDown,   0, DeviceLeftTouch, EmuButton_LeftStickDown);
        bind(VBButtonBit::LeftLeft,   0, DeviceLeftTouch, EmuButton_LeftStickLeft);
        bind(VBButtonBit::LeftRight,  0, DeviceLeftTouch, EmuButton_LeftStickRight);
        bind(VBButtonBit::RightUp,    0, DeviceRightTouch, EmuButton_RightStickUp);
        bind(VBButtonBit::RightDown,  0, DeviceRightTouch, EmuButton_RightStickDown);
        bind(VBButtonBit::RightLeft,  0, DeviceRightTouch, EmuButton_RightStickLeft);
        bind(VBButtonBit::RightRight, 0, DeviceRightTouch, EmuButton_RightStickRight);
        bind(VBButtonBit::A,          0, DeviceRightTouch, EmuButton_A);
        bind(VBButtonBit::B,          0, DeviceRightTouch, EmuButton_B);
        bind(VBButtonBit::L,          0, DeviceLeftTouch, EmuButton_Trigger);
        bind(VBButtonBit::R,          0, DeviceRightTouch, EmuButton_Trigger);
        bind(VBButtonBit::Start,      0, DeviceLeftTouch, EmuButton_Y);
        bind(VBButtonBit::Select,     0, DeviceLeftTouch, EmuButton_X);
    }
}

void EmulatorButtonMapPage::RefreshLabels()
{
    if (!m_settings)
        return;
    for (int i = 0; i < kButtonCount; ++i)
    {
        const ButtonMapper::MappedButtons &pair = m_settings->vbButtons[kButtons[i].vbBit];
        m_rowEntries[i]->SetText(BindingStr(pair.Buttons[0], m_mappingProfile));
        m_rowEntries[i]->SetSecondaryText(BindingStr(pair.Buttons[1], m_mappingProfile));
    }

    m_settings->Save(*m_platform); // always-on autosave - no explicit save action anywhere in the menu anymore
}
