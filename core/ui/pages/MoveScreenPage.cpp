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
std::string FormatFloat(const char *prefix, float value, int precision = 3, const char *suffix = "")
{
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s%.*f%s", prefix, precision, value, suffix);
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

    m_followHeadEntry = list->AddEntry("Follow Head: No", [this](MenuItem *) { ToggleFollowHead(); }, // Select acts like Right - same toggle
        [this](MenuItem *) { ToggleFollowHead(); }, [this](MenuItem *) { ToggleFollowHead(); }, UiIconId::FollowHead);
    m_threeDeeEntry = list->AddEntry("3D Screen: Yes", [this](MenuItem *) { ToggleThreeDeeMode(); }, // Select acts like Right - same toggle
        [this](MenuItem *) { ToggleThreeDeeMode(); }, [this](MenuItem *) { ToggleThreeDeeMode(); }, UiIconId::ThreeD);
    // IPD is the one exception to "Select acts like Right" - press already
    // has a distinct, meaningful action (reset to 0), so it stays that way
    // rather than doubling up with Right's step.
    m_ipdEntry = list->AddEntry("IPD offset: 0.000", [this](MenuItem *) { ChangeIpd(0); /* press resets - see ChangeIpd */ },
        [this](MenuItem *) { ChangeIpd(-1); }, [this](MenuItem *) { ChangeIpd(1); }, UiIconId::Ipd);

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

void MoveScreenPage::ToggleFollowHead()
{
    if (!m_settings) return;
    m_settings->followHead = !m_settings->followHead;
    RefreshLabels();
}

void MoveScreenPage::ToggleThreeDeeMode()
{
    if (!m_settings) return;
    m_settings->useThreeDeeMode = !m_settings->useThreeDeeMode;
    RefreshLabels();
}

void MoveScreenPage::ChangeIpd(int delta)
{
    if (!m_settings) return;
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

void MoveScreenPage::RefreshLabels()
{
    if (!m_settings)
        return;

    m_yawEntry->SetText(FormatDeg("Yaw: ", m_settings->screenYaw));
    m_pitchEntry->SetText(FormatDeg("Pitch: ", m_settings->screenPitch));
    m_rollEntry->SetText(FormatDeg("Roll: ", m_settings->screenRoll));
    m_distanceEntry->SetText(FormatValue("Distance: ", m_settings->screenDistance));
    m_scaleEntry->SetText(FormatValue("Scale: ", m_settings->screenScale, "x"));

    m_followHeadEntry->SetText(m_settings->followHead ? "Follow Head: Yes" : "Follow Head: No");
    m_threeDeeEntry->SetText(m_settings->useThreeDeeMode ? "3D Screen: Yes" : "3D Screen: No");
    m_ipdEntry->SetText(FormatFloat("IPD offset: ", m_settings->ipdOffset));

    m_settings->Save(); // always-on autosave - no explicit save action anywhere in the menu anymore
}
