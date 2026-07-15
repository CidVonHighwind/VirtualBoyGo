#pragma once
#include "../MenuPage.h"

#include <memory>

class MenuList;
struct AppSettings;

// Adjust Screen: screen placement and view mode - position (Yaw/Pitch/Roll/
// Distance/Scale + Reset View), Follow Head, 3D Screen, and IPD offset. Back
// is the bottom-bar B hint, not an in-list entry (see MenuPage::HasBackAction).
// These only matter on the OpenXR path (they feed OpenXrApp's screen quad
// pose); pc2d's flat debug window has no 3D screen to move.
class MoveScreenPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    static constexpr float kRotationStep = 5.0f;     // degrees
    static constexpr float kDistanceStep = 0.1f;     // meters
    static constexpr float kScaleStep = 0.1f;
    static constexpr float kIpdStep = 1.0f / 256.0f; // matches FrontendGo's IPD_STEP_SIZE
    static constexpr float kIpdMin = -0.125f;
    static constexpr float kIpdMax = 0.125f;

    void ChangeYaw(float delta);
    void ChangePitch(float delta);
    void ChangeRoll(float delta);
    void ChangeDistance(float delta);
    void ChangeScale(float delta);
    void ResetView();
    void ToggleFollowHead();
    void ToggleThreeDeeMode();
    void ChangeIpd(int delta);
    void RefreshLabels();

    std::shared_ptr<MenuList::Entry> m_yawEntry;
    std::shared_ptr<MenuList::Entry> m_pitchEntry;
    std::shared_ptr<MenuList::Entry> m_rollEntry;
    std::shared_ptr<MenuList::Entry> m_distanceEntry;
    std::shared_ptr<MenuList::Entry> m_scaleEntry;
    std::shared_ptr<MenuList::Entry> m_followHeadEntry;
    std::shared_ptr<MenuList::Entry> m_threeDeeEntry;
    std::shared_ptr<MenuList::Entry> m_ipdEntry;
    AppSettings *m_settings = nullptr;
};
