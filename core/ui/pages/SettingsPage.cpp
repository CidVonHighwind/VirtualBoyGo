#include "SettingsPage.h"
#include "../../Settings.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

#include <cstdio>

namespace
{
std::string FormatFloat(const char *prefix, float value, const char *suffix = "")
{
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s%.3f%s", prefix, value, suffix);
    return buf;
}
} // namespace

void SettingsPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_settings = resources.settings;

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
    list->AddEntry("Follow Head: No", nullptr,
        [this](MenuItem *) { ToggleFollowHead(); }, [this](MenuItem *) { ToggleFollowHead(); }, UiIconId::FollowHead);

    // Rows below mirror FrontendGo's Emulator::InitSettingsMenu, which sat
    // between Follow Head and Save and Back there too - screen 3D/2D mode,
    // IPD (stereo eye-separation) offset, VB screen color palette + custom
    // R/G/B tint.
    list->AddEntry("3D Screen: Yes", nullptr,
        [this](MenuItem *) { ToggleThreeDeeMode(); }, [this](MenuItem *) { ToggleThreeDeeMode(); }, UiIconId::ThreeD);
    list->AddEntry("IPD offset: 0.000", [this](MenuItem *) { ChangeIpd(0); /* press resets - see ChangeIpd */ },
        [this](MenuItem *) { ChangeIpd(-1); }, [this](MenuItem *) { ChangeIpd(1); }, UiIconId::Ipd);
    list->AddEntry("Color Palette", nullptr,
        [this](MenuItem *) { ChangePalette(-1); }, [this](MenuItem *) { ChangePalette(1); }, UiIconId::Palette);
    list->AddEntry("R: 1.000", nullptr,
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorR, -kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorR, kColorStep); });
    list->AddEntry("G: 1.000", nullptr,
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorG, -kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorG, kColorStep); });
    list->AddEntry("B: 1.000", nullptr,
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorB, -kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorB, kColorStep); });

    list->AddEntry("Save and Back", [this](MenuItem *)
                   { if (m_settings) m_settings->Save(); if (mainPage) Navigate(mainPage, -1); }, nullptr, nullptr, UiIconId::Back);

    m_menu.MenuItems.push_back(list);
    m_list = list;

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

    RefreshLabels();
}

void SettingsPage::ToggleFollowHead()
{
    if (!m_settings)
        return;
    m_settings->followHead = !m_settings->followHead;
    RefreshLabels();
}

void SettingsPage::ToggleThreeDeeMode()
{
    if (!m_settings)
        return;
    m_settings->useThreeDeeMode = !m_settings->useThreeDeeMode;
    RefreshLabels();
}

void SettingsPage::ChangeIpd(int delta)
{
    if (!m_settings)
        return;
    if (delta == 0)
    {
        m_settings->ipdOffset = 0.0f; // press resets to 0, matches FrontendGo's OnClickIPD
    }
    else
    {
        m_settings->ipdOffset += delta * kIpdStep;
        if (m_settings->ipdOffset < kIpdMin) m_settings->ipdOffset = kIpdMin;
        if (m_settings->ipdOffset > kIpdMax) m_settings->ipdOffset = kIpdMax;
    }
    RefreshLabels();
}

void SettingsPage::ChangePalette(int delta)
{
    if (!m_settings)
        return;
    int index = m_settings->selectedPalette;
    if (index < 0)
        index = 0; // was on a custom (non-preset) color - start cycling from the first preset
    index = (index + delta + 11) % 11;
    m_settings->selectedPalette = index;
    m_settings->colorR = kPredefColors[index].r;
    m_settings->colorG = kPredefColors[index].g;
    m_settings->colorB = kPredefColors[index].b;
    RefreshLabels();
}

void SettingsPage::ChangeColorChannel(float AppSettings::*channel, float delta)
{
    if (!m_settings)
        return;
    float &value = m_settings->*channel;
    value += delta;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;
    m_settings->selectedPalette = -1; // diverges from whatever preset was selected, matches FrontendGo
    RefreshLabels();
}

void SettingsPage::RefreshLabels()
{
    if (!m_settings || !m_list)
        return;

    m_list->SetEntryText(kFollowHeadIndex, m_settings->followHead ? "Follow Head: Yes" : "Follow Head: No");
    m_list->SetEntryText(kThreeDeeModeIndex, m_settings->useThreeDeeMode ? "3D Screen: Yes" : "3D Screen: No");
    m_list->SetEntryText(kIpdIndex, FormatFloat("IPD offset: ", m_settings->ipdOffset));
    m_list->SetEntryText(kPaletteIndex,
                         m_settings->selectedPalette >= 0
                             ? "Color Palette: " + std::to_string(m_settings->selectedPalette + 1) + "/11"
                             : "Color Palette: Custom");
    m_list->SetEntryText(kColorRIndex, FormatFloat("R: ", m_settings->colorR));
    m_list->SetEntryText(kColorGIndex, FormatFloat("G: ", m_settings->colorG));
    m_list->SetEntryText(kColorBIndex, FormatFloat("B: ", m_settings->colorB));
}
