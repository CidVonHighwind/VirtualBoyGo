#pragma once
#include "../MenuPage.h"

#include <memory>

class MenuList;
struct AppSettings;

// Screen placement: Yaw, Pitch, Roll, Distance, Scale, Reset View. Back is
// the bottom-bar B hint, not an in-list entry (see MenuPage::HasBackAction).
// Only meaningful on the OpenXR path - these values feed OpenXrApp's screen
// quad pose (see OpenXrApp::RenderScreenLayer); pc2d's flat debug window has
// no 3D screen to move.
class MoveScreenPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, const UiMenuResources &resources) override;

private:
    static constexpr float kRotationStep = 5.0f;   // degrees
    static constexpr float kDistanceStep = 0.1f;   // meters
    static constexpr float kScaleStep = 0.1f;

    void ChangeYaw(float delta);
    void ChangePitch(float delta);
    void ChangeRoll(float delta);
    void ChangeDistance(float delta);
    void ChangeScale(float delta);
    void ResetView();
    void RefreshLabels();

    std::shared_ptr<MenuList::Entry> m_yawEntry;
    std::shared_ptr<MenuList::Entry> m_pitchEntry;
    std::shared_ptr<MenuList::Entry> m_rollEntry;
    std::shared_ptr<MenuList::Entry> m_distanceEntry;
    std::shared_ptr<MenuList::Entry> m_scaleEntry;
    AppSettings *m_settings = nullptr;
};
