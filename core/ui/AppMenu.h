#pragma once

#include "MenuWidgets.h"
#include "UiRenderer.h"

#include <cstdint>
#include <vector>

// Trimmed replacement for FrontendGo's MenuGo - just a main menu page for
// this first vertical-slice pass (no ROM list/settings/button-remap pages
// yet). Owns the menu's Vulkan-independent resolution (matches the
// original's MENU_WIDTH/MENU_HEIGHT, which lived on Emulator in the old
// project) and builds/updates/draws the widget tree from core/ui/MenuWidgets.
//
// Renders in two steps: RenderToBuffer() draws all of the menu's content
// (background, header, text, buttons) into its own offscreen texture, then
// Draw() composites that *finished* buffer onto the real target as a single
// rounded-corner-masked quad - the mask applies to the whole panel at once,
// not to each shape inside it separately.
class AppMenu
{
public:
    static constexpr int kMenuWidth = 640;
    static constexpr int kMenuHeight = 480;
    static constexpr int kHeaderHeight = 75;
    static constexpr int kBottomHeight = 30;
    static constexpr float kPanelCornerRadiusPx = 8.0f;

    // Loads its own fonts (assets/fonts/VirtualLogo.ttf for the header,
    // Roboto-Regular.ttf for menu items) via AssetLoader rather than taking
    // font bytes from the caller. targetFormat must match whatever format
    // the real target(s) passed to Draw() use (see
    // UiRenderer::CreateRenderTexture).
    void Initialize(UiRenderer &ui, VkFormat targetFormat);

    void Update(uint32_t buttonStates[3], uint32_t lastButtonStates[3], float deltaSeconds);

    // Renders the menu's content into its own offscreen buffer. Call this
    // once per frame; it manages its own UiRenderer::BeginOffscreenFrame/
    // EndFrame pair internally, independent of the real target's
    // BeginFrame/EndFrame.
    void RenderToBuffer(UiRenderer &ui);

    // Composites the buffer rendered by RenderToBuffer() onto whatever's
    // currently the active BeginFrame target, at (x,y), masked to rounded
    // corners. Call after RenderToBuffer(), between the real target's own
    // BeginFrame/EndFrame.
    void Draw(UiRenderer &ui, float x, float y);

    // Clear color for the real target's BeginFrame (not the offscreen
    // buffer, which clears to transparent internally) - opaque, since it's
    // just the base canvas before anything (e.g. a game layer) is drawn.
    XrColor4f GetBackgroundColor() const;

private:
    void SetUpMenu(UiRenderer &ui);
    void RenderContent(UiRenderer &ui);

    UiFontHandle m_titleFont;
    UiFontHandle m_menuFont;
    Menu m_mainMenu;
    UiImageHandle m_offscreenTexture;
};
