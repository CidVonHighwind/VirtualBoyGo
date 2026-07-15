#include "MoveScreenPage.h"
#include "../../Settings.h"
#include "../MenuPage.h"
#include "AppMenuLayout.h"

#include <cstdio>

namespace
{
std::string FormatDeg(const char *prefix, float value)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s%.0f deg", prefix, value);
    return buf;
}
std::string FormatValue(const char *prefix, float value, const char *suffix = "")
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s%.2f%s", prefix, value, suffix);
    return buf;
}
} // namespace

void MoveScreenPage::Init(UiRenderer &ui, const UiMenuResources &resources)
{
    m_settings = resources.settings;

    auto list = std::make_shared<MenuList>(ui, resources.menuFont, kMenuContentX, kMenuContentY, kListWidth, kListHeight,
                                           kMenuItemSize, resources.icons);
    list->Color          = kMenuTextColor;
    list->SelectionColor = kMenuSelectionColor;

    // Select acts like Right on every adjustable row below - advances the
    // value the same as pressing right, rather than doing nothing.
    m_yawEntry = list->AddEntry("Yaw: 0 deg", [this](MenuItem *) { ChangeYaw(kRotationStep); },
        [this](MenuItem *) { ChangeYaw(-kRotationStep); }, [this](MenuItem *) { ChangeYaw(kRotationStep); }, UiIconId::LeftRight);
    m_pitchEntry = list->AddEntry("Pitch: 0 deg", [this](MenuItem *) { ChangePitch(kRotationStep); },
        [this](MenuItem *) { ChangePitch(-kRotationStep); }, [this](MenuItem *) { ChangePitch(kRotationStep); }, UiIconId::UpDown);
    m_rollEntry = list->AddEntry("Roll: 0 deg", [this](MenuItem *) { ChangeRoll(kRotationStep); },
        [this](MenuItem *) { ChangeRoll(-kRotationStep); }, [this](MenuItem *) { ChangeRoll(kRotationStep); }, UiIconId::Reset);

    list->AddSpacer(kMenuSpacerSize);

    m_distanceEntry = list->AddEntry("Distance: 2.20", [this](MenuItem *) { ChangeDistance(kDistanceStep); },
        [this](MenuItem *) { ChangeDistance(-kDistanceStep); }, [this](MenuItem *) { ChangeDistance(kDistanceStep); }, UiIconId::Distance);
    m_scaleEntry = list->AddEntry("Scale: 1.00x", [this](MenuItem *) { ChangeScale(kScaleStep); },
        [this](MenuItem *) { ChangeScale(-kScaleStep); }, [this](MenuItem *) { ChangeScale(kScaleStep); }, UiIconId::Scale);

    list->AddSpacer(kMenuSpacerSize);

    list->AddEntry("Reset View", [this](MenuItem *) { ResetView(); }, nullptr, nullptr, UiIconId::ResetView);

    m_menu.MenuItems.push_back(list);

    m_menu.BackPress = [this]() { if (settingsPage) Navigate(settingsPage, -1); };
    m_menu.Init();

    RefreshLabels();
}

void MoveScreenPage::ChangeYaw(float delta)
{
    if (!m_settings) return;
    m_settings->screenYaw += delta;
    RefreshLabels();
}

void MoveScreenPage::ChangePitch(float delta)
{
    if (!m_settings) return;
    m_settings->screenPitch += delta;
    RefreshLabels();
}

void MoveScreenPage::ChangeRoll(float delta)
{
    if (!m_settings) return;
    m_settings->screenRoll += delta;
    RefreshLabels();
}

void MoveScreenPage::ChangeDistance(float delta)
{
    if (!m_settings) return;
    m_settings->screenDistance += delta;
    if (m_settings->screenDistance < 0.5f) m_settings->screenDistance = 0.5f;
    RefreshLabels();
}

void MoveScreenPage::ChangeScale(float delta)
{
    if (!m_settings) return;
    m_settings->screenScale += delta;
    if (m_settings->screenScale < 0.1f) m_settings->screenScale = 0.1f;
    RefreshLabels();
}

void MoveScreenPage::ResetView()
{
    if (!m_settings) return;
    m_settings->screenYaw = 0.0f;
    m_settings->screenPitch = 0.0f;
    m_settings->screenRoll = 0.0f;
    m_settings->screenDistance = 2.2f;
    m_settings->screenScale = 1.0f;
    RefreshLabels();
}

void MoveScreenPage::RefreshLabels()
{
    if (!m_settings)
        return;

    m_yawEntry->SetText(FormatDeg("Yaw: ", m_settings->screenYaw));
    m_pitchEntry->SetText(FormatDeg("Pitch: ", m_settings->screenPitch));
    m_rollEntry->SetText(FormatDeg("Roll: ", m_settings->screenRoll));
    m_distanceEntry->SetText(FormatValue("Distance: ", m_settings->screenDistance));
    m_scaleEntry->SetText(FormatValue("Scale: ", m_settings->screenScale, "x"));

    m_settings->Save(); // always-on autosave - no explicit save action anywhere in the menu anymore
}
