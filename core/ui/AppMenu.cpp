#include "AppMenu.h"
#include "AssetLoader.h"

namespace
{
    // Opaque - just the base canvas clear before anything else is drawn
    // (the game layer, when present, gets drawn on top of this and covers
    // most/all of it; any letterboxed edges show this color).
    constexpr XrColor4f kClearColor{0.0f, 0.0f, 0.0f, 1.0f};

    // Matches the original's Global.h colors 1:1 (MenuBackgroundColor,
    // MenuBackgroundOverlayHeader/Light, headerTextColor, textColor,
    // textSelectionColor). Drawn as a real blended quad (not the frame's
    // clear) so alpha < 1 actually shows whatever's underneath - the game
    // layer on PC2D, or just kClearColor elsewhere.
    constexpr float lightGray = 0.35f;
    constexpr float darkGray = 0.2f;
    constexpr XrColor4f kBodyColor{darkGray, darkGray, darkGray, 0.975f};
    constexpr XrColor4f kOverlayColor{lightGray, lightGray, lightGray, 0.98f};

    constexpr XrColor4f kHeaderTextColor{0.9f, 0.1f, 0.1f, 1.0f};
    constexpr XrColor4f kHeaderTextBackColor{0.0f, 0.0f, 0.0f, 0.45f};

    constexpr XrColor4f kMenuTextColor{0.8f, 0.8f, 0.8f, 1.0f};
    constexpr XrColor4f kMenuSelectionColor{0.9f, 0.1f, 0.1f, 1.0f};

    // Matches the original's fontHeader (VirtualLogo.ttf) and fontMenu
    // (Roboto-Regular.ttf) sizes.
    constexpr int kHeaderFontSize = 65;
    constexpr int kMenuFontSize = 24;
} // namespace

void AppMenu::Initialize(UiRenderer &ui, VkFormat targetFormat)
{
    const std::vector<uint8_t> headerFontBytes = LoadAssetBytes("fonts/VirtualLogo.ttf");
    const std::vector<uint8_t> menuFontBytes = LoadAssetBytes("fonts/Roboto-Regular.ttf");
    m_titleFont = ui.LoadFont(headerFontBytes, kHeaderFontSize);
    m_menuFont = ui.LoadFont(menuFontBytes, kMenuFontSize);
    m_offscreenTexture = ui.CreateRenderTexture(kMenuWidth, kMenuHeight, targetFormat);
    SetUpMenu(ui);
    m_mainMenu.Init();
}

void AppMenu::SetUpMenu(UiRenderer &ui)
{
    // Placeholder buttons - no emulator/ROM logic exists yet, so these are
    // inert for this pass. Matches the original's main-menu-page layout
    // (SetUpMenu in Menu.cpp): left-aligned at (20, HEADER_HEIGHT + 20),
    // stacked by menuItemSize = fontMenu.FontSize + 4 apart.
    const int posX = 20;
    const int menuItemSize = kMenuFontSize + 4;
    int posY = kHeaderHeight + 20;

    auto addButton = [&](const std::string &text)
    {
        auto button = std::make_shared<MenuButton>(ui, m_menuFont, text, posX, posY, nullptr);
        button->Color = kMenuTextColor;
        button->SelectionColor = kMenuSelectionColor;
        m_mainMenu.MenuItems.push_back(button);
        posY += menuItemSize;
    };

    addButton("Resume");
    addButton("Load ROM");
    addButton("Settings");
    addButton("Exit");
}

XrColor4f AppMenu::GetBackgroundColor() const { return kClearColor; }

void AppMenu::Update(uint32_t buttonStates[3], uint32_t lastButtonStates[3], float deltaSeconds)
{
    m_mainMenu.Update(buttonStates, lastButtonStates, deltaSeconds);
}

void AppMenu::RenderContent(UiRenderer &ui)
{
    // Plain sharp-cornered shapes throughout - rounding happens once, at
    // the whole-buffer level, in Draw()'s compositing step below.
    ui.DrawQuad(0, 0, kMenuWidth, kHeaderHeight, kOverlayColor);
    ui.DrawQuad(0, kHeaderHeight, kMenuWidth, kMenuHeight - kHeaderHeight - kBottomHeight, kBodyColor);
    ui.DrawQuad(0, kMenuHeight - kBottomHeight, kMenuWidth, kBottomHeight, kOverlayColor);

    const int headerTextY = kHeaderHeight / 2 - ui.GetFontPHeight(m_titleFont) / 2 - ui.GetFontPStart(m_titleFont);
    const float headerTextX = (kMenuWidth - ui.GetTextWidth(m_titleFont, "VirtualBoyGo")) / 2.0f;
    ui.DrawText(m_titleFont, "VirtualBoyGo", headerTextX + 1, static_cast<float>(headerTextY) + 1, 1.0f, kHeaderTextBackColor);
    ui.DrawText(m_titleFont, "VirtualBoyGo", headerTextX, static_cast<float>(headerTextY), 1.0f, kHeaderTextColor);

    m_mainMenu.Draw(ui, 0, 0, 0, 0, 1.0f);
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
