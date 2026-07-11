#pragma once

#include "MenuPage.h"
#include "pages/AppMenuLayout.h"
#include "pages/MainPage.h"
#include "pages/SettingsPage.h"
#include "pages/RomSelectPage.h"
#include "pages/MenuButtonMapPage.h"
#include "pages/EmulatorButtonMapPage.h"
#include "pages/MoveScreenPage.h"
#include "UiRenderer.h"

#include <cstdint>

// Top-level menu system. Owns all menu pages and drives the slide transition
// between them. Renders the active page into an offscreen buffer each frame,
// then composites it onto the real target with rounded corners.
//
// Transition model (ported from FrontendGo`s MenuGo):
//   StartTransition(target, dir)  ->  m_transitionState slides 1->0 over
//   kTransitionSpeed seconds using a sine-eased progress value.
//   dir: +1 = target slides in from right, -1 = from left.
class AppMenu
{
public:
    // Layout constants are in pages/AppMenuLayout.h (shared with page files).
    static constexpr float kPanelCornerRadiusPx = 8.0f;
    static constexpr float kTransitionSpeed = 0.15f;

    void Initialize(UiRenderer &ui, VkFormat targetFormat);
    void Update(uint32_t buttonStates[3], uint32_t lastButtonStates[3], float deltaSeconds);
    void RenderToBuffer(UiRenderer &ui);
    void Draw(UiRenderer &ui, float x, float y);

    XrColor4f GetBackgroundColor() const;

    // Battery indicator drawn top-right of the header, ported from
    // FrontendGo's MenuGo::DrawMenu (the coloured fill block + "N%" text).
    // percent: 0-100 shows it: any value outside that range (default -1)
    // hides it entirely - no platform in this rework can read a real
    // battery level yet, so callers opt in explicitly instead of the
    // indicator silently showing a stale/fake reading. Nothing calls this
    // on the OpenXR path (desktop VR or Android) today, so it stays hidden
    // there; the flat 2D debug build cycles a fake value through it instead.
    void SetBatteryPercent(int percent) { m_batteryPercent = percent; }

private:
    void InitPages(UiRenderer &ui);
    void StartTransition(MenuPage *target, int dir);
    void RenderContent(UiRenderer &ui);

    MainPage m_mainPage;
    SettingsPage m_settingsPage;
    RomSelectPage m_romSelectPage;
    MenuButtonMapPage m_menuButtonMapPage;
    EmulatorButtonMapPage m_emulatorButtonMapPage;
    MoveScreenPage m_moveScreenPage;

    MenuPage *m_currentPage = nullptr;
    MenuPage *m_nextPage = nullptr;
    float m_transitionState = 0.0f;
    int m_transitionDir = 1;

    UiFontHandle m_titleFont;
    UiIconSet m_icons;
    UiMenuResources m_resources; // menuFont/smallFont/&m_icons - see UiMenuResources.h
    UiImageHandle m_offscreenTexture;

    int m_batteryPercent = -1;
};
