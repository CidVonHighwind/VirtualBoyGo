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

    list->AddEntry("Yaw: 0 deg", nullptr,
        [this](MenuItem *) { ChangeYaw(-kRotationStep); }, [this](MenuItem *) { ChangeYaw(kRotationStep); }, UiIconId::LeftRight);
    list->AddEntry("Pitch: 0 deg", nullptr,
        [this](MenuItem *) { ChangePitch(-kRotationStep); }, [this](MenuItem *) { ChangePitch(kRotationStep); }, UiIconId::UpDown);
    list->AddEntry("Roll: 0 deg", nullptr,
        [this](MenuItem *) { ChangeRoll(-kRotationStep); }, [this](MenuItem *) { ChangeRoll(kRotationStep); }, UiIconId::Reset);
    list->AddEntry("Distance: 2.20", nullptr,
        [this](MenuItem *) { ChangeDistance(-kDistanceStep); }, [this](MenuItem *) { ChangeDistance(kDistanceStep); }, UiIconId::Distance);
    list->AddEntry("Scale: 1.00x", nullptr,
        [this](MenuItem *) { ChangeScale(-kScaleStep); }, [this](MenuItem *) { ChangeScale(kScaleStep); }, UiIconId::Scale);
    list->AddEntry("Reset View", [this](MenuItem *) { ResetView(); }, nullptr, nullptr, UiIconId::ResetView);
    list->AddEntry("Back", [this](MenuItem *) { if (settingsPage) Navigate(settingsPage, -1); }, nullptr, nullptr, UiIconId::Back);

    m_menu.MenuItems.push_back(list);
    m_list = list;

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
    if (!m_settings || !m_list)
        return;

    m_list->SetEntryText(kYawIndex, FormatDeg("Yaw: ", m_settings->screenYaw));
    m_list->SetEntryText(kPitchIndex, FormatDeg("Pitch: ", m_settings->screenPitch));
    m_list->SetEntryText(kRollIndex, FormatDeg("Roll: ", m_settings->screenRoll));
    m_list->SetEntryText(kDistanceIndex, FormatValue("Distance: ", m_settings->screenDistance));
    m_list->SetEntryText(kScaleIndex, FormatValue("Scale: ", m_settings->screenScale, "x"));
}
