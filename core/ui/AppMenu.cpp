#include "AppMenu.h"
#include "AssetLoader.h"
#include "../Settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace
{
    constexpr XrColor4f kClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr XrColor4f kHeaderTextColor = {0.9f, 0.1f, 0.1f, 1.0f};
    constexpr XrColor4f kHeaderTextBackColor = {0.0f, 0.0f, 0.0f, 0.45f};
    // Font pixel sizes are inherently integer (FreeType rasterizes whole
    // pixels only) - 65/2 doesn't land on a whole number like the other
    // logical-space constants do, so this one is just rounded. Still
    // rasterizes crisp: this is a font *size* fed to FreeType, unrelated to
    // kMenuScale's physical-vs-logical pixel mapping.
    constexpr int kHeaderFontSize = 33;

    // Clock + battery indicator, ported from FrontendGo's Menu.cpp
    // (SetTimeString/BatteryColors/DrawMenu's battery block) - two rows
    // stacked in the header's top-right corner, both right-aligned to the
    // same margin. The clock needs no platform hook (std::time works
    // everywhere); the battery does (see AppMenu::SetBatteryPercent).
    constexpr float kHeaderRightMargin = 7.5f;
    constexpr float kTimeRowCenterY = kHeaderHeight / 2.0f - 5.5f;
    constexpr float kBatteryRowCenterY = kHeaderHeight / 2.0f + 5.5f;

    constexpr float kBatteryBlockWidth = 5.0f;
    constexpr float kBatteryBlockHeight = 8.0f;
    constexpr float kBatteryPadding = 1.0f;
    constexpr float kBatteryCornerRadiusPx = 1.5f;
    constexpr float kBatteryCornerInsideRadiusPx = 1.0f;
    constexpr XrColor4f kBatteryBackgroundColor = {0.25f, 0.25f, 0.25f, 1.0f};
    // The gradient walks red -> orange -> yellow -> green as the level
    // rises; two flat plateaus (indices 3-4 and 5-6) are intentional,
    // matching the original.
    constexpr int kBatteryColorCount = 5;
    constexpr XrColor4f kBatteryColors[] = {
        {0.745f, 0.114f, 0.176f, 1.0f},
        {0.92f, 0.361f, 0.176f, 1.0f},
        {0.976f, 0.69f, 0.255f, 1.0f},
        {0.545f, 0.769f, 0.247f, 1.0f},
        {0.545f, 0.769f, 0.247f, 1.0f},
        {0.0f, 0.78f, 0.078f, 1.0f},
        {0.0f, 0.78f, 0.078f, 1.0f},
    };

    XrColor4f Lerp(const XrColor4f &a, const XrColor4f &b, float t)
    {
        return {a.r * (1 - t) + b.r * t, a.g * (1 - t) + b.g * t, a.b * (1 - t) + b.b * t, a.a * (1 - t) + b.a * t};
    }

    XrColor4f BatteryColorForPercent(int percent)
    {
        const float step = 100.0f / kBatteryColorCount;
        const float colorState = std::fmod(static_cast<float>(percent), step) / step;
        const int currentColor = static_cast<int>(percent / step);
        return Lerp(kBatteryColors[currentColor], kBatteryColors[currentColor + 1], colorState);
    }

    std::string CurrentTimeString()
    {
        const std::time_t t = std::time(nullptr);
        std::tm tmv{};
#if defined(_WIN32)
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
        char buf[6]; // "HH:MM\0"
        std::snprintf(buf, sizeof(buf), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        return buf;
    }
} // namespace

// -----------------------------------------------------------------------
// Initialise

void AppMenu::Initialize(UiRenderer &ui, VkFormat targetFormat, Emulator &emulator, AppSettings &settings,
                         ButtonMappingProfile mappingProfile)
{
    const std::vector<uint8_t> headerFontBytes = LoadAssetBytes("fonts/VirtualLogo.ttf");
    const std::vector<uint8_t> menuFontBytes = LoadAssetBytes("fonts/Roboto-Regular.ttf");
    const std::vector<uint8_t> smallFontBytes = LoadAssetBytes("fonts/Roboto-Bold.ttf");

    // Bake glyphs at physical resolution (kMenuScale x the logical size) for
    // crisp text - see UiFontManager::LoadFont's renderScale doc comment.
    m_titleFont = ui.LoadFont(headerFontBytes, static_cast<int>(kHeaderFontSize * m_menuScale), m_menuScale);
    m_resources.menuFont = ui.LoadFont(menuFontBytes, static_cast<int>(kMenuFontSize * m_menuScale), m_menuScale);
    m_resources.smallFont = ui.LoadFont(smallFontBytes, static_cast<int>(kSmallFontSize * m_menuScale), m_menuScale);

    m_icons.Load(ui, m_menuScale);
    m_resources.icons = &m_icons;
    m_resources.emulator = &emulator;
    m_resources.appMenu = this;
    m_resources.settings = &settings;
    m_resources.buttonMappingProfile = mappingProfile;
    // Physical pixel size - kMenuWidth/kMenuHeight are logical units (see
    // AppMenuLayout.h); RenderToBuffer maps them onto this full-resolution
    // texture via BeginOffscreenFrame's logicalWidth/logicalHeight, so the
    // menu rasterizes crisp at m_menuScale regardless of how small the
    // logical layout numbers are.
    m_offscreenTexture = ui.CreateRenderTexture(static_cast<uint32_t>(kMenuWidth * m_menuScale),
                                                static_cast<uint32_t>(kMenuHeight * m_menuScale), targetFormat);

    InitPages(ui);
    // Start on ROM selection, not MainPage - nothing's loaded yet at boot,
    // so Resume/Save/Load would just be dead buttons; picking a ROM already
    // navigates back to MainPage afterward (see RomSelectPage::Init).
    m_currentPage = &m_romSelectPage;
    // Pre-select MainPage's "Load ROM" row so backing out of RomSelectPage
    // lands there instead of row 0 - see SelectLoadRomEntry's doc comment.
    m_mainPage.SelectLoadRomEntry();
}

void AppMenu::SetMenuScale(UiRenderer &ui, float scale)
{
    if (scale == m_menuScale)
        return;
    m_menuScale = scale;
    m_icons.SetMenuScale(ui, m_menuScale);

    ui.ResizeRenderTexture(m_offscreenTexture, static_cast<uint32_t>(kMenuWidth * m_menuScale),
                           static_cast<uint32_t>(kMenuHeight * m_menuScale));

    const std::vector<uint8_t> headerFontBytes = LoadAssetBytes("fonts/VirtualLogo.ttf");
    const std::vector<uint8_t> menuFontBytes = LoadAssetBytes("fonts/Roboto-Regular.ttf");
    const std::vector<uint8_t> smallFontBytes = LoadAssetBytes("fonts/Roboto-Bold.ttf");
    ui.RebakeFont(m_titleFont, headerFontBytes, static_cast<int>(kHeaderFontSize * m_menuScale), m_menuScale);
    ui.RebakeFont(m_resources.menuFont, menuFontBytes, static_cast<int>(kMenuFontSize * m_menuScale), m_menuScale);
    ui.RebakeFont(m_resources.smallFont, smallFontBytes, static_cast<int>(kSmallFontSize * m_menuScale), m_menuScale);
}

void AppMenu::InitPages(UiRenderer &ui)
{
    // Wire Navigate callbacks for all pages before calling Init() on any.
    auto wireNavigate = [&](MenuPage &page)
    {
        page.Navigate = [this](MenuPage *target, int dir)
        {
            StartTransition(target, dir);
        };
    };

    wireNavigate(m_mainPage);
    wireNavigate(m_settingsPage);
    wireNavigate(m_romSelectPage);
    wireNavigate(m_emulatorButtonMapPage);
    wireNavigate(m_moveScreenPage);

    // Cross-page links
    m_mainPage.romSelectPage = &m_romSelectPage;
    m_mainPage.settingsPage = &m_settingsPage;

    m_settingsPage.mainPage = &m_mainPage;
    m_settingsPage.emulatorButtonMapPage = &m_emulatorButtonMapPage;
    m_settingsPage.moveScreenPage = &m_moveScreenPage;

    m_romSelectPage.mainPage = &m_mainPage;
    m_emulatorButtonMapPage.settingsPage = &m_settingsPage;
    m_moveScreenPage.settingsPage = &m_settingsPage;

    // Init all pages
    m_mainPage.Init(ui, m_resources);
    m_settingsPage.Init(ui, m_resources);
    m_romSelectPage.Init(ui, m_resources);
    m_emulatorButtonMapPage.Init(ui, m_resources);
    m_moveScreenPage.Init(ui, m_resources);
}

// -----------------------------------------------------------------------
// Navigation

void AppMenu::StartTransition(MenuPage *target, int dir)
{
    if (!target || m_nextPage)
        return; // ignore if already transitioning

    if (!m_open)
    {
        // Closing (e.g. RomSelectPage hides the menu then navigates back to
        // MainPage in the same callback) - the close animation still shows
        // whatever m_currentPage is for the next ~kOpenCloseSpeed seconds
        // (see IsVisible()), so switching pages right now would fade out
        // MainPage instead of RomSelectPage - the wrong page flashing up
        // right as the menu disappears. Defer the switch instead: keep
        // showing the current page through the fade, and apply the pending
        // one only once the menu actually reopens (see Update()).
        m_pendingPage = target;
        return;
    }

    m_nextPage = target;
    m_transitionDir = dir;
    m_transitionState = 1.0f;
}

// -----------------------------------------------------------------------
// Update

void AppMenu::Update(uint32_t buttonStates[3], uint32_t lastButtonStates[3], float deltaSeconds)
{
    for (int device = 0; device < 3; ++device)
        if ((buttonStates[device] & m_suppressedSelectButtons[device]) == 0)
            m_suppressedSelectButtons[device] = 0;

    // Animate the open/close fade regardless of m_open, so Hide() eases the
    // panel out instead of popping it away the instant gameplay input
    // resumes (see IsOpen() vs IsVisible()'s doc comment).
    const float visibilityTarget = m_open ? 1.0f : 0.0f;
    if (m_visibility != visibilityTarget)
    {
        const float step = deltaSeconds / kOpenCloseSpeed;
        m_visibility = (m_visibility < visibilityTarget) ? std::min(visibilityTarget, m_visibility + step)
                                                          : std::max(visibilityTarget, m_visibility - step);
    }

    if (m_open && m_pendingPage)
    {
        // Apply a page switch deferred by StartTransition while closed (see
        // its doc comment) - now that we're open again, swap before this
        // frame renders so the reopen shows the target page directly
        // instead of briefly flashing whatever was showing when it closed.
        m_currentPage = m_pendingPage;
        m_pendingPage = nullptr;
        m_currentPage->ResetSelection();
    }

    if (!m_open)
        return; // closed - no page should react to input meant for gameplay

    if (m_transitionState > 0.0f)
    {
        m_transitionState -= deltaSeconds / kTransitionSpeed;
        if (m_transitionState <= 0.0f)
        {
            m_transitionState = 0.0f;
            m_currentPage = m_nextPage;
            m_nextPage = nullptr;
        }
        return; // don't process input during transition
    }

    const bool wasOpen = m_open;
    if (m_currentPage)
        m_currentPage->Update(buttonStates, lastButtonStates, deltaSeconds);
    if (wasOpen && !m_open)
    {
        const uint32_t selectMask = ButtonMapper::ButtonMapping[ButtonMapper::EmuButton_A];
        m_suppressedSelectButtons[ButtonMapper::DeviceGamepad] = buttonStates[ButtonMapper::DeviceGamepad] & selectMask;
        m_suppressedSelectButtons[ButtonMapper::DeviceRightTouch] = buttonStates[ButtonMapper::DeviceRightTouch] & selectMask;
    }
}

void AppMenu::ApplyGameplayInputSuppression(uint32_t buttonStates[3]) const
{
    for (int device = 0; device < 3; ++device)
        buttonStates[device] &= ~m_suppressedSelectButtons[device];
}

void AppMenu::SubmitRawMappingInput(const ButtonMapper::MappedButton &button)
{
    if (m_open && m_currentPage && m_transitionState <= 0.0f)
        m_currentPage->SubmitRawCaptureInput(button);
}

// -----------------------------------------------------------------------
// Rendering

XrColor4f AppMenu::GetBackgroundColor() const { return kClearColor; }

void AppMenu::RenderContent(UiRenderer &ui)
{
    // Background regions
    ui.DrawQuad(0, 0, kMenuWidth, kHeaderHeight, kMenuOverlayColor);
    ui.DrawQuad(0, kHeaderHeight, kMenuWidth, kMenuHeight - kHeaderHeight - kBottomHeight, kMenuBodyColor);
    ui.DrawQuad(0, kMenuHeight - kBottomHeight, kMenuWidth, kBottomHeight, kMenuOverlayColor);

    // Bottom-bar button hints ("[A] Select" / "[B] Back") - helps players
    // navigate without having to guess which button does what. "Back" only
    // shows on pages that actually have somewhere to go (MainPage is the
    // root). Right-anchored near the edge with both groups pulled close
    // together - measured off the actual text width so the gap stays tight
    // regardless of font metrics, rather than hand-picked fixed positions.
    {
        const UiIconId selectIcon = UiIconId::ButtonA;
        const UiIconId backIcon = UiIconId::ButtonB;
        const bool showBack = m_currentPage && m_currentPage->HasBackAction();

        constexpr float kHintIconSize = 9.0f;
        constexpr float kHintIconGap = 2.0f;
        constexpr float kHintRightMargin = 8.0f;
        constexpr float kHintGroupGap = 4.0f;

        const float barCenterY = kMenuHeight - kBottomHeight / 2.0f;
        const float iconY = barCenterY - kHintIconSize / 2.0f;
        const float textY = barCenterY - ui.GetFontPHeight(m_resources.smallFont) / 2.0f - ui.GetFontPStart(m_resources.smallFont);

        const float selectTextW = ui.GetTextWidth(m_resources.smallFont, "Select");
        const float selectGroupW = kHintIconSize + kHintIconGap + selectTextW;
        const float selectX = kMenuWidth - kHintRightMargin - selectGroupW;

        if (showBack)
        {
            const float backTextW = ui.GetTextWidth(m_resources.smallFont, "Back");
            const float backGroupW = kHintIconSize + kHintIconGap + backTextW;
            const float backX = selectX - kHintGroupGap - backGroupW;
            m_icons.Draw(ui, backIcon, backX, iconY, kHintIconSize);
            ui.DrawText(m_resources.smallFont, "Back", backX + kHintIconSize + kHintIconGap, textY, 1.0f, kMenuTextColor);
        }
        m_icons.Draw(ui, selectIcon, selectX, iconY, kHintIconSize);
        ui.DrawText(m_resources.smallFont, "Select", selectX + kHintIconSize + kHintIconGap, textY, 1.0f, kMenuTextColor);
    }

    // Centred header title
    const float headerTextY = kHeaderHeight / 2.0f - ui.GetFontPHeight(m_titleFont) / 2.0f - ui.GetFontPStart(m_titleFont);
    const float headerTextX = (kMenuWidth - ui.GetTextWidth(m_titleFont, "VirtualBoyGo")) / 2.0f;
    ui.DrawText(m_titleFont, "VirtualBoyGo", headerTextX + 0.5f, headerTextY + 0.5f, 1.0f, kHeaderTextBackColor);
    ui.DrawText(m_titleFont, "VirtualBoyGo", headerTextX, headerTextY, 1.0f, kHeaderTextColor);

    // Clock - always shown, needs no platform hook (std::time works
    // everywhere, unlike battery level).
    {
        const std::string timeText = CurrentTimeString();
        const float timeWidth = ui.GetTextWidth(m_resources.smallFont, timeText);
        const float timeTextY = kTimeRowCenterY - ui.GetFontPHeight(m_resources.smallFont) / 2.0f -
                                ui.GetFontPStart(m_resources.smallFont);
        ui.DrawText(m_resources.smallFont, timeText, kMenuWidth - kHeaderRightMargin - timeWidth + 0.5f, timeTextY + 0.5f, 1.0f, kHeaderTextBackColor);
        ui.DrawText(m_resources.smallFont, timeText, kMenuWidth - kHeaderRightMargin - timeWidth, timeTextY, 1.0f, kMenuTextColor);
    }

    if (m_batteryPercent >= 0 && m_batteryPercent <= 100)
    {
        const float blockX = kMenuWidth - kHeaderRightMargin - kBatteryBlockWidth;
        const float blockY = kBatteryRowCenterY - kBatteryBlockHeight / 2.0f;

        ui.DrawQuadRounded(blockX - kBatteryPadding, blockY - kBatteryPadding,
                           kBatteryBlockWidth + kBatteryPadding * 2, kBatteryBlockHeight + kBatteryPadding * 2,
                           kBatteryBackgroundColor, kBatteryCornerRadiusPx);

        const float fillHeight = m_batteryPercent / 100.0f * kBatteryBlockHeight;
        ui.DrawQuadRounded(blockX, blockY + (kBatteryBlockHeight - fillHeight), kBatteryBlockWidth, fillHeight,
                           BatteryColorForPercent(m_batteryPercent), kBatteryCornerInsideRadiusPx);

        const std::string batteryText = std::to_string(m_batteryPercent) + "%";
        const float textWidth = ui.GetTextWidth(m_resources.smallFont, batteryText);
        const float textY = kBatteryRowCenterY - ui.GetFontPHeight(m_resources.smallFont) / 2.0f -
                            ui.GetFontPStart(m_resources.smallFont);

        ui.DrawText(m_resources.smallFont, batteryText, blockX - kBatteryPadding - 2.0f - textWidth + 0.5f, textY + 0.5f, 1.0f, kHeaderTextBackColor);
        ui.DrawText(m_resources.smallFont, batteryText, blockX - kBatteryPadding - 2.0f - textWidth, textY, 1.0f, kMenuTextColor);
    }

    // Draw current page + next page (sliding in/out).
    // Matches the reference (FrontendGo MenuGo::DrawMenu):
    //   - current page starts at its natural position (offset 0) and slides away
    //   - next page starts displaced by dist and slides in to offset 0
    //   - dist is small so neither page ever leaves the visible area
    if (m_transitionState > 0.0f && m_nextPage)
    {
        const float rawProgress = m_transitionState; // 1.0 -> 0.0
        const float eased = std::sinf(rawProgress * (3.14159265f / 2.0f));
        const float dist = kTransitionSlideDistance;

        // Current page: offset ramps from 0 up to dist (slides away)
        m_currentPage->Draw(ui, -m_transitionDir, 1.0f - eased, dist, rawProgress);
        // Next page: offset ramps from dist down to 0 (slides in)
        m_nextPage->Draw(ui, m_transitionDir, eased, dist, 1.0f - rawProgress);
    }
    else if (m_currentPage)
    {
        m_currentPage->Draw(ui, 0, 0.0f, 0, 1.0f);
    }
}

void AppMenu::RenderToBuffer(UiRenderer &ui)
{
    ui.BeginOffscreenFrame(m_offscreenTexture, XrColor4f{0.0f, 0.0f, 0.0f, 0.0f}, kMenuWidth, kMenuHeight);
    RenderContent(ui);
    ui.EndFrame();
}

void AppMenu::Draw(UiRenderer &ui, float x, float y)
{
    if (m_visibility <= 0.0f)
        return; // fully closed - nothing to composite

    // Open/close animation: fades in/out while growing in from kMinScale (and
    // shrinking back down on close), eased the same way as the page-slide
    // transition (see StartTransition's doc comment) so both feel consistent.
    constexpr float kMinScale = 0.9f;
    const float eased = std::sinf(m_visibility * (3.14159265f / 2.0f));
    const float scale = kMinScale + (1.0f - kMinScale) * eased;

    // 1:1 at scale=1 - m_offscreenTexture is already the full kMenuWidth*
    // m_menuScale physical size, so no scaling happens at composite time
    // beyond the open/close animation above (see Initialize).
    // kPanelCornerRadiusPx itself DOES need scaling here though, unlike the
    // battery/scrollbar corner radii - this draw call composites onto the
    // real (physical) target, not into the logical-space offscreen buffer,
    // so nothing else scales it up automatically the way BeginOffscreenFrame's
    // logicalWidth/logicalHeight trick does for everything drawn inside RenderContent.
    const float fullW = kMenuWidth * m_menuScale;
    const float fullH = kMenuHeight * m_menuScale;
    const float w = fullW * scale;
    const float h = fullH * scale;
    ui.DrawImageRounded(m_offscreenTexture, x + (fullW - w) / 2.0f, y + (fullH - h) / 2.0f, w, h,
                        kPanelCornerRadiusPx * m_menuScale, eased);
}
