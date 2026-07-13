#pragma once

#include "ButtonMapping.h"
#include "UiIconSet.h"
#include "UiRenderer.h"

#include <openxr/openxr.h> // for XrColor4f

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Ported from FrontendGo's MenuHelper.h/.cpp - the navigation/selection
// state machine (Menu::Update/MoveSelection/ButtonPressed) is carried over
// near-verbatim (it only ever touched the abstract uint[3] button-bitmask
// model, never VrApi/GL directly). The draw methods are rewritten against
// UiRenderer instead of DrawHelper/FontManager. The original's standalone
// MenuImage widget and its list widget (MenuList<T>, dead/unfinished code -
// its draw methods were declared but never defined) are dropped; MenuList
// below is a new fixed-rect scrolling list, not a port. Its rows can carry
// an optional UiIconId (see AddEntry) - the closest equivalent of the
// original's per-MenuButton IconId.
class MenuItem
{
public:
    bool Selectable = false;
    bool Selected = false;
    bool Visible = true;
    float PosX = 100, PosY = 100;

    float ScrollDelay = 0.3f;
    float ScrollTimeV = 0.075f;
    float ScrollTimeH = 0.075f;

    int Tag = 0;
    int Tag2 = 0;

    XrColor4f Color{1, 1, 1, 1};
    XrColor4f SelectionColor{1, 0.85f, 0.1f, 1};

    std::function<void(MenuItem *item, int direction)> OnSelectFunction;
    std::function<void(MenuItem *item, uint32_t *buttonState, uint32_t *lastButtonState)> UpdateFunction;

    MenuItem() = default;
    virtual ~MenuItem() = default;

    virtual void Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds);

    virtual int PressedUp();
    virtual int PressedDown();
    virtual int PressedLeft();
    virtual int PressedRight();
    virtual int PressedEnter();

    virtual void OnSelect(int direction);
    virtual void Select();
    virtual void Unselect();

    // Restores the item's internal cursor to its start - e.g. MenuList's
    // scroll position - so re-entering a page doesn't leave it wherever the
    // user last left it. No-op for items without one of their own.
    virtual void ResetSelection() {}

    virtual void Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha);
};

class Menu
{
public:
    std::vector<std::shared_ptr<MenuItem>> MenuItems;

    int CurrentSelection = 0;
    float buttonDownCount = 0;

    // Was ovrVirtualBoyGo::global.SwappSelectBackButton in the original -
    // trimmed to a per-menu setting for this pass (no settings/resource
    // registry ported yet).
    bool SwapSelectBackButton = false;

    std::function<void()> BackPress;

    void Init();

    bool ButtonPressed(uint32_t *buttonState, uint32_t *lastButtonState, uint32_t device, uint32_t button);

    void MoveSelection(int dir, bool onSelect);

    void Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds);

    void Draw(UiRenderer &ui, int transitionDirX, int transitionDirY, float moveProgress, float moveDist, float fadeProgress);

    // Resets the top-level cursor to the first selectable item and resets
    // every item's own internal selection (e.g. MenuList's scroll cursor).
    void ResetSelection();
};

class MenuLabel : public MenuItem
{
public:
    MenuLabel(UiRenderer &ui, UiFontHandle font, const std::string &text, float posX, float posY, float width, float height,
              XrColor4f color);

    void SetText(const std::string &newText);

    void Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha) override;

private:
    UiRenderer *m_ui;
    UiFontHandle m_font;
    float m_containerX, m_containerY, m_containerWidth, m_containerHeight;
    std::string m_text;
};

class MenuButton : public MenuItem
{
public:
    std::string Text;

    MenuButton(UiRenderer &ui, UiFontHandle font, const std::string &text, float posX, float posY, float width, float height,
               std::function<void(MenuItem *item)> pressFunction, std::function<void(MenuItem *item)> leftFunction = nullptr,
               std::function<void(MenuItem *item)> rightFunction = nullptr);

    // Left-aligned, no vertical/horizontal centering - matches the
    // original's main-menu-page constructor (MenuHelper.cpp's 5-posarg
    // MenuButton ctor), used for plain top-to-bottom stacked menu lists.
    MenuButton(UiRenderer &ui, UiFontHandle font, const std::string &text, float posX, float posY,
               std::function<void(MenuItem *item)> pressFunction, std::function<void(MenuItem *item)> leftFunction = nullptr,
               std::function<void(MenuItem *item)> rightFunction = nullptr);

    void SetText(const std::string &newText);

    int PressedLeft() override;
    int PressedRight() override;
    int PressedEnter() override;

    void Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha) override;

private:
    UiRenderer *m_ui;
    UiFontHandle m_font;
    std::function<void(MenuItem *item)> m_pressFunction;
    std::function<void(MenuItem *item)> m_leftFunction;
    std::function<void(MenuItem *item)> m_rightFunction;
    float m_containerWidth = 0;
    float m_offsetX = 0;
};

// A vertically scrollable list that fills a fixed content rect. Handles its
// own up/down selection and draws a scrollbar when entries don't fit.
// Pages add entries via AddEntry() without managing Y positions at all.
class MenuList : public MenuItem
{
public:
    // posX/posY/width/height define the bounding rect the list fills.
    // icons may be null for pages that don't pass any entries an icon.
    MenuList(UiRenderer &ui, UiFontHandle font, float posX, float posY, float width, float height, float itemHeight,
             const UiIconSet *icons = nullptr);

    struct Entry
    {
        std::string text;
        std::function<void(MenuItem *)> pressFunction;
        std::function<void(MenuItem *)> leftFunction;
        std::function<void(MenuItem *)> rightFunction;
        UiIconId icon = UiIconId::None;
    };

    void AddEntry(const std::string &text,
                  std::function<void(MenuItem *)> press = nullptr,
                  std::function<void(MenuItem *)> left = nullptr,
                  std::function<void(MenuItem *)> right = nullptr,
                  UiIconId icon = UiIconId::None);

    int GetSelectedIndex() const { return m_selectedIndex; }

    int PressedUp() override;
    int PressedDown() override;
    int PressedLeft() override;
    int PressedRight() override;
    int PressedEnter() override;

    void ResetSelection() override
    {
        m_selectedIndex = 0;
        m_firstVisible = 0;
    }

    void Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha) override;

private:
    int maxVisible() const;
    bool needsScrollbar() const;

    UiRenderer *m_ui;
    UiFontHandle m_font;
    const UiIconSet *m_icons;
    float m_posX, m_posY, m_width, m_height;
    float m_itemHeight;
    static constexpr float kIconSize = 10.0f;
    static constexpr float kIconTextGap = 4.0f;
    int m_selectedIndex = 0;
    int m_firstVisible = 0;
    float m_textRowOffset = 0; // baseline-centering offset within each slot, baked at init
    std::vector<Entry> m_entries;

    static constexpr float kScrollbarWidth = 2.0f;
    static constexpr float kScrollbarGap = 2.0f;
};
