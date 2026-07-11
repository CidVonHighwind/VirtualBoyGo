#include "AppMenu.h"
#include "AssetLoader.h"

#include <cmath>

namespace
{
    constexpr XrColor4f kClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    constexpr XrColor4f kBodyColor = {0.2f, 0.2f, 0.2f, 0.975f};
    constexpr XrColor4f kOverlayColor = {0.35f, 0.35f, 0.35f, 0.98f};
    constexpr XrColor4f kHeaderTextColor = {0.9f, 0.1f, 0.1f, 1.0f};
    constexpr XrColor4f kHeaderTextBackColor = {0.0f, 0.0f, 0.0f, 0.45f};
    constexpr int kHeaderFontSize = 65;
} // namespace

// -----------------------------------------------------------------------
// Initialise

void AppMenu::Initialize(UiRenderer &ui, VkFormat targetFormat)
{
    const std::vector<uint8_t> headerFontBytes = LoadAssetBytes("fonts/VirtualLogo.ttf");
    const std::vector<uint8_t> menuFontBytes = LoadAssetBytes("fonts/Roboto-Regular.ttf");
    m_titleFont = ui.LoadFont(headerFontBytes, kHeaderFontSize);
    m_menuFont = ui.LoadFont(menuFontBytes, kMenuFontSize);
    m_offscreenTexture = ui.CreateRenderTexture(kMenuWidth, kMenuHeight, targetFormat);

    InitPages(ui);
    m_currentPage = &m_mainPage;
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
    wireNavigate(m_menuButtonMapPage);
    wireNavigate(m_emulatorButtonMapPage);
    wireNavigate(m_moveScreenPage);

    // Cross-page links
    m_mainPage.romSelectPage = &m_romSelectPage;
    m_mainPage.settingsPage = &m_settingsPage;

    m_settingsPage.mainPage = &m_mainPage;
    m_settingsPage.menuButtonMapPage = &m_menuButtonMapPage;
    m_settingsPage.emulatorButtonMapPage = &m_emulatorButtonMapPage;
    m_settingsPage.moveScreenPage = &m_moveScreenPage;

    m_romSelectPage.mainPage = &m_mainPage;
    m_menuButtonMapPage.settingsPage = &m_settingsPage;
    m_emulatorButtonMapPage.settingsPage = &m_settingsPage;
    m_moveScreenPage.settingsPage = &m_settingsPage;

    // Init all pages
    m_mainPage.Init(ui, m_menuFont);
    m_settingsPage.Init(ui, m_menuFont);
    m_romSelectPage.Init(ui, m_menuFont);
    m_menuButtonMapPage.Init(ui, m_menuFont);
    m_emulatorButtonMapPage.Init(ui, m_menuFont);
    m_moveScreenPage.Init(ui, m_menuFont);
}

// -----------------------------------------------------------------------
// Navigation

void AppMenu::StartTransition(MenuPage *target, int dir)
{
    if (!target || m_nextPage)
        return; // ignore if already transitioning
    m_nextPage = target;
    m_transitionDir = dir;
    m_transitionState = 1.0f;
}

// -----------------------------------------------------------------------
// Update

void AppMenu::Update(uint32_t buttonStates[3], uint32_t lastButtonStates[3], float deltaSeconds)
{
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

    if (m_currentPage)
        m_currentPage->Update(buttonStates, lastButtonStates, deltaSeconds);
}

// -----------------------------------------------------------------------
// Rendering

XrColor4f AppMenu::GetBackgroundColor() const { return kClearColor; }

void AppMenu::RenderContent(UiRenderer &ui)
{
    // Background regions
    ui.DrawQuad(0, 0, kMenuWidth, kHeaderHeight, kOverlayColor);
    ui.DrawQuad(0, kHeaderHeight, kMenuWidth, kMenuHeight - kHeaderHeight - kBottomHeight, kBodyColor);
    ui.DrawQuad(0, kMenuHeight - kBottomHeight, kMenuWidth, kBottomHeight, kOverlayColor);

    // Centred header title
    const int headerTextY = kHeaderHeight / 2 - ui.GetFontPHeight(m_titleFont) / 2 - ui.GetFontPStart(m_titleFont);
    const float headerTextX = (kMenuWidth - ui.GetTextWidth(m_titleFont, "VirtualBoyGo")) / 2.0f;
    ui.DrawText(m_titleFont, "VirtualBoyGo", headerTextX + 1, static_cast<float>(headerTextY) + 1, 1.0f, kHeaderTextBackColor);
    ui.DrawText(m_titleFont, "VirtualBoyGo", headerTextX, static_cast<float>(headerTextY), 1.0f, kHeaderTextColor);

    // Draw current page + next page (sliding in/out).
    // Matches the reference (FrontendGo MenuGo::DrawMenu):
    //   - current page starts at its natural position (offset 0) and slides away
    //   - next page starts displaced by dist and slides in to offset 0
    //   - dist is small (75px) so neither page ever leaves the visible area
    if (m_transitionState > 0.0f && m_nextPage)
    {
        const float rawProgress = m_transitionState; // 1.0 -> 0.0
        const float eased = std::sinf(rawProgress * (3.14159265f / 2.0f));
        const int dist = 75;

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
    ui.BeginOffscreenFrame(m_offscreenTexture, XrColor4f{0.0f, 0.0f, 0.0f, 0.0f});
    RenderContent(ui);
    ui.EndFrame();
}

void AppMenu::Draw(UiRenderer &ui, float x, float y)
{
    ui.DrawImageRounded(m_offscreenTexture, x, y, kMenuWidth, kMenuHeight, kPanelCornerRadiusPx);
}
