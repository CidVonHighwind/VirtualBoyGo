#include "SettingsPage.h"
#include "../../AndroidRomAccess.h"
#include "../../Settings.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

#include <cstdio>

namespace
{
std::string FormatFloat(const char *prefix, float value, int precision = 3, const char *suffix = "")
{
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s%.*f%s", prefix, precision, value, suffix);
    return buf;
}

// Preview swatches drawn directly on the "Color Palette" row, immediately
// after its icon+label (not right-aligned against the row edge), showing
// the VB's 4 brightness levels (black up to the full tint) - stands in for
// the selected-palette index (nobody reads "7/11"; the colors themselves
// are what matters). Reads AppSettings live each frame, so it tracks
// palette/R/G/B edits immediately.
constexpr float kSwatchSize = 11.0f;
constexpr float kSwatchGap = 2.0f;
constexpr float kSwatchLeftGap = 6.0f;
constexpr const char *kColorPaletteLabel = "Color Palette";

// The real hardware doesn't space its 4 brightness levels evenly (0, 1/3,
// 2/3, 1) - it's levels 0x00/0x63/0x87 out of a 0xff full intensity (level 3
// is whatever colorR/G/B is currently set to, since that already stands in
// for "full brightness" everywhere else in this app - the tint applied to
// the actual game screen).
constexpr float kBrightnessLevels[4] = {0.0f, 0x63 / 255.0f, 0x87 / 255.0f, 1.0f};

void DrawColorPreview(UiRenderer &ui, UiFontHandle labelFont, AppSettings *settings, float rowX, float rowY, float rowW, float rowH, float alpha)
{
    if (!settings)
        return;

    const float labelWidth = ui.GetTextWidth(labelFont, kColorPaletteLabel);
    float x = rowX + MenuList::kIconSize + MenuList::kIconTextGap + labelWidth + kSwatchLeftGap;
    const float y = rowY + (rowH - kSwatchSize) / 2.0f;

    for (int i = 0; i < 4; ++i)
    {
        const float level = kBrightnessLevels[i];
        const XrColor4f c{settings->colorR * level, settings->colorG * level, settings->colorB * level, alpha};
        ui.DrawQuadRounded(x, y, kSwatchSize, kSwatchSize, c, 1.0f);
        x += kSwatchSize + kSwatchGap;
    }
}
} // namespace

void SettingsPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_settings = resources.settings;

    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    list->AddEntry("Button Mapping", [this](MenuItem *)
                   { if (emulatorButtonMapPage) Navigate(emulatorButtonMapPage, 1); }, nullptr, nullptr, UiIconId::Mapping);
    list->AddEntry("Adjust Screen", [this](MenuItem *)
                   { if (moveScreenPage) Navigate(moveScreenPage, 1); }, nullptr, nullptr, UiIconId::Move);

    list->AddSpacer(kMenuSpacerSize);

    UiFontHandle menuFont = resources.menuFont;
    list->AddEntry(kColorPaletteLabel, [this](MenuItem *) { ChangePalette(1); }, // Select acts like Right - advance the palette
        [this](MenuItem *) { ChangePalette(-1); }, [this](MenuItem *) { ChangePalette(1); }, UiIconId::Palette,
        [this, menuFont](UiRenderer &ui, float x, float y, float w, float h, float a) { DrawColorPreview(ui, menuFont, m_settings, x, y, w, h, a); });
    m_colorREntry = list->AddEntry("Red: 1.00", [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorR, kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorR, -kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorR, kColorStep); });
    m_colorGEntry = list->AddEntry("Green: 1.00", [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorG, kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorG, -kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorG, kColorStep); });
    m_colorBEntry = list->AddEntry("Blue: 1.00", [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorB, kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorB, -kColorStep); },
        [this](MenuItem *) { ChangeColorChannel(&AppSettings::colorB, kColorStep); });

#if defined(__ANDROID__)
    // Way back into the ROMs-folder picker (SAF, see RomScanner.h). Clears
    // the folder and asks for a restart rather than re-popping the picker
    // directly (see AndroidRomAccess::RequestChangeRomsFolder).
    list->AddSpacer(kMenuSpacerSize);
    m_changeRomsFolderEntry = list->AddEntry("Change ROMs Folder...", [this](MenuItem *) { RequestChangeRomsFolder(); },
        nullptr, nullptr, UiIconId::RomList);
#endif

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

#if defined(__ANDROID__)
void SettingsPage::RequestChangeRomsFolder()
{
    AndroidRomAccess::RequestChangeRomsFolder();
    if (m_changeRomsFolderEntry)
        m_changeRomsFolderEntry->SetText("Folder cleared - restart the app!");
}
#endif

void SettingsPage::RefreshLabels()
{
    if (!m_settings)
        return;

    // Color Palette's label stays static ("Color Palette") - its row draws
    // the actual colors via DrawColorPreview instead of a selected-index
    // number (see AddEntry's accessoryDraw above).
    // 2 decimals, not 3 - kColorStep is 0.05, so the third decimal is always
    // 0 and never actually reachable by adjusting the value.
    m_colorREntry->SetText(FormatFloat("Red: ", m_settings->colorR, 2));
    m_colorGEntry->SetText(FormatFloat("Green: ", m_settings->colorG, 2));
    m_colorBEntry->SetText(FormatFloat("Blue: ", m_settings->colorB, 2));

    m_settings->Save(); // always-on autosave - no explicit save action anywhere in the menu anymore
}
