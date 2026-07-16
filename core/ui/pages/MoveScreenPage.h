#pragma once
#include "../MenuPage.h"

#include <memory>

class MenuList;
struct AppSettings;

// Adjust Screen: screen placement and view mode - position (Yaw/Pitch/Roll/
// Distance/Scale + Reset Values), Follow Head, 3D Screen, Screen: Flat/Curved,
// and IPD offset. Back is the bottom-bar B hint, not an in-list entry (see
// MenuPage::HasBackAction).
// These only matter on the OpenXR path (they feed OpenXrApp's screen quad
// pose); pc2d's flat debug window has no 3D screen to move.
class MoveScreenPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    static constexpr float kYawPitchStep = 1.0f;     // degrees
    static constexpr float kYawPitchMin = -25.0f;    // degrees
    static constexpr float kYawPitchMax = 25.0f;     // degrees
    static constexpr float kRollStep = 5.0f;         // degrees
    static constexpr float kDistanceStep = 0.1f;     // meters
    static constexpr float kScaleStep = 0.1f;
    static constexpr float kIpdStep = 1.0f / 256.0f; // matches FrontendGo's IPD_STEP_SIZE
    static constexpr float kIpdMin = -0.5f;
    static constexpr float kIpdMax = 0.5f;

    static constexpr float kDefaultYaw = 0.0f;
    static constexpr float kDefaultPitch = 0.0f;
    static constexpr float kDefaultRoll = 0.0f;
    static constexpr float kDefaultDistance = 2.2f;
    static constexpr float kDefaultScale = 1.0f;

    void ChangeYaw(float delta);
    void ChangePitch(float delta);
    void ChangeRoll(float delta);
    void ChangeDistance(float delta);
    void ChangeScale(float delta);
    // Select's press action on Yaw/Pitch/Roll/Distance/Scale - resets just
    // that one field to its default instead of advancing it (unlike the
    // other rows, where press acts like Right - see Init).
    void ResetToDefault(float AppSettings::*field, float defaultValue);
    void ResetView();
    // direction +1/-1 cycles FollowHeadMode forward/backward, wrapping
    // Off<->Smooth<->Instant.
    void CycleFollowHeadMode(int direction);
    void ToggleThreeDeeMode();
    void ToggleCurvedScreen();
    void ChangeIpd(int delta);
    void RefreshLabels();

    std::shared_ptr<MenuList::Entry> m_yawEntry;
    std::shared_ptr<MenuList::Entry> m_pitchEntry;
    std::shared_ptr<MenuList::Entry> m_rollEntry;
    std::shared_ptr<MenuList::Entry> m_distanceEntry;
    std::shared_ptr<MenuList::Entry> m_scaleEntry;
    std::shared_ptr<MenuList::Entry> m_followHeadEntry;
    std::shared_ptr<MenuList::Entry> m_threeDeeEntry;
    std::shared_ptr<MenuList::Entry> m_curvedScreenEntry;
    std::shared_ptr<MenuList::Entry> m_ipdEntry;
    AppSettings *m_settings = nullptr;
};
