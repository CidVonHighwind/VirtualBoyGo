#include "MenuWidgets.h"
#include "pages/AppMenuLayout.h"

#include <algorithm>

// ---- MenuItem ----

void MenuItem::Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds)
{
    if (UpdateFunction != nullptr)
        UpdateFunction(this, buttonState, lastButtonState);
}

int MenuItem::PressedUp() { return 0; }
int MenuItem::PressedDown() { return 0; }
int MenuItem::PressedLeft() { return 0; }
int MenuItem::PressedRight() { return 0; }
int MenuItem::PressedEnter() { return 0; }

void MenuItem::OnSelect(int direction)
{
    if (OnSelectFunction != nullptr)
        OnSelectFunction(this, direction);
}

void MenuItem::Select() { Selected = true; }
void MenuItem::Unselect() { Selected = false; }

void MenuItem::Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha) {}

// ---- MenuLabel ----

MenuLabel::MenuLabel(UiRenderer &ui, UiFontHandle font, const std::string &text, float posX, float posY, float width, float height,
                     XrColor4f color)
    : m_ui(&ui), m_font(font), m_containerX(posX), m_containerY(posY), m_containerWidth(width), m_containerHeight(height)
{
    Color = color;
    SetText(text);
}

void MenuLabel::SetText(const std::string &newText)
{
    m_text = newText;
    // Bake any glyphs this text needs that aren't already in the atlas
    // (e.g. non-ASCII characters) before measuring/drawing - safe to do here
    // (not mid-frame), see UiFontManager::EnsureGlyphsForText.
    m_ui->EnsureGlyphsForText(m_font, newText);
    // Center text within the label's container, same as the original.
    const float textWidth = m_ui->GetTextWidth(m_font, newText);
    PosX = m_containerX + m_containerWidth / 2.0f - textWidth / 2.0f;
    PosY = m_containerY + m_containerHeight / 2.0f - m_ui->GetFontPHeight(m_font) / 2.0f - m_ui->GetFontPStart(m_font);
}

void MenuLabel::Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha)
{
    if (!Visible)
        return;
    XrColor4f c = Color;
    c.a *= alpha;
    ui.DrawText(m_font, m_text, PosX + offsetX + (Selected ? 2.5f : 0.0f), PosY + offsetY, 1.0f, c);
}

// ---- MenuButton ----

MenuButton::MenuButton(UiRenderer &ui, UiFontHandle font, const std::string &text, float posX, float posY, float width, float height,
                       std::function<void(MenuItem *)> pressFunction, std::function<void(MenuItem *)> leftFunction,
                       std::function<void(MenuItem *)> rightFunction)
    : m_ui(&ui), m_font(font), m_pressFunction(pressFunction), m_leftFunction(leftFunction), m_rightFunction(rightFunction)
{
    PosX = posX;
    PosY = posY + (height / 2.0f - ui.GetFontPHeight(font) / 2.0f) - ui.GetFontPStart(font);
    m_containerWidth = width;
    Selectable = true;
    SetText(text);
}

MenuButton::MenuButton(UiRenderer &ui, UiFontHandle font, const std::string &text, float posX, float posY,
                       std::function<void(MenuItem *)> pressFunction, std::function<void(MenuItem *)> leftFunction,
                       std::function<void(MenuItem *)> rightFunction)
    : m_ui(&ui), m_font(font), m_pressFunction(pressFunction), m_leftFunction(leftFunction), m_rightFunction(rightFunction)
{
    PosX = posX;
    PosY = posY;
    Selectable = true;
    SetText(text);
}

void MenuButton::SetText(const std::string &newText)
{
    Text = newText;
    m_ui->EnsureGlyphsForText(m_font, newText); // see MenuLabel::SetText
    if (m_containerWidth > 0)
    {
        const float textWidth = m_ui->GetTextWidth(m_font, newText);
        m_offsetX = m_containerWidth / 2.0f - textWidth / 2.0f;
    }
}

int MenuButton::PressedLeft()
{
    if (m_leftFunction != nullptr)
    {
        m_leftFunction(this);
        return 1;
    }
    return 0;
}

int MenuButton::PressedRight()
{
    if (m_rightFunction != nullptr)
    {
        m_rightFunction(this);
        return 1;
    }
    return 0;
}

int MenuButton::PressedEnter()
{
    if (m_pressFunction != nullptr)
    {
        m_pressFunction(this);
        return 1;
    }
    return 0;
}

void MenuButton::Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha)
{
    if (!Visible)
        return;
    XrColor4f c = Selected ? SelectionColor : Color;
    c.a *= alpha;
    ui.DrawText(m_font, Text, PosX + (Selected ? 2.5f : 0.0f) + offsetX + m_offsetX, PosY + offsetY, 1.0f, c);
}

// ---- MenuImage ----

namespace
{
    const std::string kMenuImageEmptyText = "Empty Slot";
}

MenuImage::MenuImage(UiRenderer &ui, UiFontHandle font, uint32_t textureWidth, uint32_t textureHeight, float posX,
                     float posY, float width, float height, std::function<XrColor4f()> tintProvider)
    : m_ui(&ui), m_font(font), m_width(width), m_height(height), m_tintProvider(std::move(tintProvider))
{
    PosX = posX;
    PosY = posY;
    m_texture = ui.CreateStreamingImage(textureWidth, textureHeight, VK_FORMAT_B8G8R8A8_UNORM);
    m_ui->EnsureGlyphsForText(m_font, kMenuImageEmptyText);
}

void MenuImage::SetImage(const std::vector<uint8_t> &rgba)
{
    m_ui->UpdateStreamingImage(m_texture, rgba.data(), rgba.size());
    m_hasImage = true;
}

void MenuImage::Clear() { m_hasImage = false; }

void MenuImage::Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha)
{
    if (!Visible)
        return;

    const float x = PosX + offsetX;
    const float y = PosY + offsetY;

    // Rounded frame, matching the header/bottom bars - hollow, not filled:
    // the body-colored quad punches the middle back out so the frame only
    // ever shows as a border, whatever's drawn on top of it.
    constexpr float kFrameThickness = 3.0f;
    constexpr float kFrameRadius = 3.0f;
    XrColor4f frameColor = kMenuOverlayColor;
    frameColor.a = 0.45f * alpha;
    ui.DrawQuadRounded(x - kFrameThickness, y - kFrameThickness, m_width + kFrameThickness * 2,
                       m_height + kFrameThickness * 2, frameColor, kFrameRadius);

    if (m_hasImage)
    {
        XrColor4f tint = m_tintProvider ? m_tintProvider() : XrColor4f{1.0f, 1.0f, 1.0f, 1.0f};
        tint.a *= alpha;
        ui.DrawImage(m_texture, x, y, m_width, m_height, tint);
        return;
    }

    const float textWidth = ui.GetTextWidth(m_font, kMenuImageEmptyText);
    const float textX = x + (m_width - textWidth) / 2.0f;
    const float textY = y + m_height / 2.0f - ui.GetFontPHeight(m_font) / 2.0f - ui.GetFontPStart(m_font);
    const XrColor4f textColor{0.7f, 0.7f, 0.7f, alpha};
    ui.DrawText(m_font, kMenuImageEmptyText, textX, textY, 1.0f, textColor);
}

// ---- Menu ----

void Menu::Init() { MenuItems[CurrentSelection]->Select(); }

void Menu::ResetSelection()
{
    MenuItems[CurrentSelection]->Unselect();

    CurrentSelection = 0;
    while (!MenuItems[CurrentSelection]->Selectable && CurrentSelection < static_cast<int>(MenuItems.size()) - 1)
        ++CurrentSelection;

    for (auto &item : MenuItems)
        item->ResetSelection();

    MenuItems[CurrentSelection]->Select();
}

bool Menu::ButtonPressed(uint32_t *buttonState, uint32_t *lastButtonState, uint32_t device, uint32_t button)
{
    return (buttonState[device] & ButtonMapper::ButtonMapping[button]) &&
           (!(lastButtonState[device] & ButtonMapper::ButtonMapping[button]) ||
            buttonDownCount > MenuItems[CurrentSelection]->ScrollDelay);
}

void Menu::MoveSelection(int dir, bool onSelect)
{
    // Will not terminate if nothing in the list is selectable.
    do
    {
        CurrentSelection += dir;
        if (CurrentSelection < 0)
            CurrentSelection = static_cast<int>(MenuItems.size()) - 1;
        if (CurrentSelection >= static_cast<int>(MenuItems.size()))
            CurrentSelection = 0;
    } while (!MenuItems[CurrentSelection]->Selectable);

    if (onSelect)
        MenuItems[CurrentSelection]->OnSelect(dir);
}

void Menu::Update(uint32_t *buttonState, uint32_t *lastButtonState, float deltaSeconds)
{
    using namespace ButtonMapper;

    if (CaptureHook && CaptureHook(buttonState, lastButtonState))
        return; // still waiting for a button press - normal navigation suspended

    MenuItems[CurrentSelection]->Unselect();

    if ((buttonState[DeviceGamepad] &
         (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right] |
          ButtonMapping[EmuButton_LeftStickUp] | ButtonMapping[EmuButton_LeftStickDown] | ButtonMapping[EmuButton_LeftStickLeft] |
          ButtonMapping[EmuButton_LeftStickRight])) ||
        (buttonState[DeviceLeftTouch] &
         (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right])) ||
        (buttonState[DeviceRightTouch] &
         (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right])))
    {
        buttonDownCount += deltaSeconds;
    }
    else
    {
        buttonDownCount = 0;
    }

    for (auto &item : MenuItems)
    {
        item->Update(buttonState, lastButtonState, deltaSeconds);
    }

    // Left/Right adjust the selected item's own value (e.g. save slot +/-).
    // No ClearButtonState - clearing would make the held key look freshly
    // pressed every frame, breaking the ScrollDelay repeat throttle.
    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_Left) ||
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_LeftStickLeft) ||
        ButtonPressed(buttonState, lastButtonState, DeviceLeftTouch, EmuButton_Left) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, EmuButton_Left))
    {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
        MenuItems[CurrentSelection]->PressedLeft();
    }

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_Right) ||
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_LeftStickRight) ||
        ButtonPressed(buttonState, lastButtonState, DeviceLeftTouch, EmuButton_Right) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, EmuButton_Right))
    {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
        MenuItems[CurrentSelection]->PressedRight();
    }

    bool selectPressed =
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, SwapSelectBackButton ? EmuButton_B : EmuButton_A) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, SwapSelectBackButton ? EmuButton_B : EmuButton_A);
    if (!selectPressed && ExtraSelectButton1.IsSet)
        selectPressed = ButtonPressed(buttonState, lastButtonState, static_cast<uint32_t>(ExtraSelectButton1.InputDevice),
                                      static_cast<uint32_t>(ExtraSelectButton1.ButtonIndex));
    if (!selectPressed && ExtraSelectButton2.IsSet)
        selectPressed = ButtonPressed(buttonState, lastButtonState, static_cast<uint32_t>(ExtraSelectButton2.InputDevice),
                                      static_cast<uint32_t>(ExtraSelectButton2.ButtonIndex));

    if (selectPressed)
    {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
        MenuItems[CurrentSelection]->PressedEnter();
    }
    else if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, SwapSelectBackButton ? EmuButton_A : EmuButton_B) ||
             ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, SwapSelectBackButton ? EmuButton_A : EmuButton_B))
    {
        if (BackPress != nullptr)
            BackPress();
    }

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_Up) ||
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_LeftStickUp) ||
        ButtonPressed(buttonState, lastButtonState, DeviceLeftTouch, EmuButton_Up) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, EmuButton_Up))
    {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeV;
        if (MenuItems[CurrentSelection]->PressedUp() == 0)
        {
            MoveSelection(-1, true);
        }
    }

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_Down) ||
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_LeftStickDown) ||
        ButtonPressed(buttonState, lastButtonState, DeviceLeftTouch, EmuButton_Down) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, EmuButton_Down))
    {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeV;
        if (MenuItems[CurrentSelection]->PressedDown() == 0)
        {
            MoveSelection(1, true);
        }
    }

    MenuItems[CurrentSelection]->Select();
}

void Menu::Draw(UiRenderer &ui, int transitionDirX, int transitionDirY, float moveProgress, float moveDist, float fadeProgress)
{
    for (auto &item : MenuItems)
    {
        item->Draw(ui, transitionDirX * moveProgress * moveDist, transitionDirY * moveProgress * moveDist, fadeProgress);
    }
}

// ---- MenuList ----

MenuList::MenuList(UiRenderer &ui, UiFontHandle font, float posX, float posY, float width, float height, float itemHeight,
                   const UiIconSet *icons)
    : m_ui(&ui), m_font(font), m_icons(icons), m_posX(posX), m_posY(posY), m_width(width), m_height(height)
{
    m_itemHeight = itemHeight;
    // Bake the per-row baseline offset so text sits centred within its slot.
    m_textRowOffset = m_itemHeight / 2.0f - ui.GetFontPHeight(font) / 2.0f - ui.GetFontPStart(font);
    Selectable = true;
}

void MenuList::AddEntry(const std::string &text,
                        std::function<void(MenuItem *)> press,
                        std::function<void(MenuItem *)> left,
                        std::function<void(MenuItem *)> right,
                        UiIconId icon,
                        AccessoryDrawFn accessoryDraw)
{
    // Row text isn't known upfront (ROM file names, etc.) - bake whatever
    // glyphs it needs now, not while drawing. See MenuLabel::SetText.
    m_ui->EnsureGlyphsForText(m_font, text);
    m_entries.push_back({text, press, left, right, icon, std::move(accessoryDraw)});
}

void MenuList::AddSpacer(float height)
{
    Entry entry;
    entry.isSpacer = true;
    entry.height = height;
    m_entries.push_back(entry);
}

void MenuList::SetEntryText(int index, const std::string &text)
{
    m_ui->EnsureGlyphsForText(m_font, text);
    m_entries[index].text = text;
}

float MenuList::rowHeight(int index) const { return m_entries[index].isSpacer ? m_entries[index].height : m_itemHeight; }

// How many entries starting at `first` fit within the list's height, packing
// by each row's own height rather than assuming a uniform m_itemHeight.
int MenuList::maxVisibleFrom(int first) const
{
    float used = 0;
    int count = 0;
    for (int i = first; i < (int)m_entries.size(); ++i)
    {
        const float h = rowHeight(i);
        if (count > 0 && used + h > m_height)
            break;
        used += h;
        ++count;
    }
    return count;
}

bool MenuList::needsScrollbar() const { return maxVisibleFrom(0) < (int)m_entries.size(); }

void MenuList::ResetSelection()
{
    m_selectedIndex = 0;
    while (m_selectedIndex < (int)m_entries.size() - 1 && m_entries[m_selectedIndex].isSpacer)
        ++m_selectedIndex;
    m_firstVisible = 0;
}

void MenuList::SelectIndex(int index)
{
    if (index < 0 || index >= (int)m_entries.size() || m_entries[index].isSpacer)
        return;

    m_selectedIndex = index;
    if (m_selectedIndex < m_firstVisible)
        m_firstVisible = m_selectedIndex;
    else if (m_selectedIndex >= m_firstVisible + maxVisibleFrom(m_firstVisible))
        m_firstVisible = m_selectedIndex - maxVisibleFrom(m_firstVisible) + 1;
}

int MenuList::PressedUp()
{
    if (m_entries.empty())
        return 0;
    const int start = m_selectedIndex;
    do
    {
        if (m_selectedIndex > 0)
            --m_selectedIndex;
        else
            m_selectedIndex = (int)m_entries.size() - 1; // wrap to bottom
    } while (m_entries[m_selectedIndex].isSpacer && m_selectedIndex != start);

    if (m_selectedIndex < m_firstVisible)
        m_firstVisible = m_selectedIndex;
    else if (m_selectedIndex >= m_firstVisible + maxVisibleFrom(m_firstVisible))
        m_firstVisible = m_selectedIndex - maxVisibleFrom(m_firstVisible) + 1;
    return 1;
}

int MenuList::PressedDown()
{
    if (m_entries.empty())
        return 0;
    const int start = m_selectedIndex;
    do
    {
        if (m_selectedIndex < (int)m_entries.size() - 1)
            ++m_selectedIndex;
        else
            m_selectedIndex = 0; // wrap to top
    } while (m_entries[m_selectedIndex].isSpacer && m_selectedIndex != start);

    if (m_selectedIndex < m_firstVisible)
        m_firstVisible = m_selectedIndex;
    else if (m_selectedIndex >= m_firstVisible + maxVisibleFrom(m_firstVisible))
        m_firstVisible = m_selectedIndex - maxVisibleFrom(m_firstVisible) + 1;
    return 1;
}

int MenuList::PressedLeft()
{
    if (m_entries.empty())
        return 0;
    const auto &entry = m_entries[m_selectedIndex];
    if (entry.leftFunction)
    {
        entry.leftFunction(this);
        return 1;
    }
    return 0;
}

int MenuList::PressedRight()
{
    if (m_entries.empty())
        return 0;
    const auto &entry = m_entries[m_selectedIndex];
    if (entry.rightFunction)
    {
        entry.rightFunction(this);
        return 1;
    }
    return 0;
}

int MenuList::PressedEnter()
{
    if (m_entries.empty())
        return 0;
    const auto &entry = m_entries[m_selectedIndex];
    if (entry.pressFunction)
    {
        entry.pressFunction(this);
        return 1;
    }
    return 0;
}

void MenuList::Draw(UiRenderer &ui, float offsetX, float offsetY, float alpha)
{
    if (!Visible || m_entries.empty())
        return;

    const int visible = maxVisibleFrom(m_firstVisible);

    // Centre the item block vertically around however many rows are
    // actually showing (not the list's full capacity) - a 1-entry list
    // centers that entry in the middle, not pinned to the top.
    float usedHeight = 0;
    for (int i = m_firstVisible; i < m_firstVisible + visible && i < (int)m_entries.size(); ++i)
        usedHeight += rowHeight(i);
    const float verticalPad = (m_height - usedHeight) / 2.0f;
    const float baseY = m_posY + offsetY + verticalPad;

    float rowY = baseY;
    for (int i = 0; i < visible && (m_firstVisible + i) < (int)m_entries.size(); ++i)
    {
        const int idx = m_firstVisible + i;
        const Entry &entry = m_entries[idx];
        const float h = rowHeight(idx);

        if (!entry.isSpacer)
        {
            const bool sel = (idx == m_selectedIndex);
            const float x = m_posX + offsetX + (sel ? 2.5f : 0.0f);
            const float y = rowY + m_textRowOffset;
            const float textX = (m_icons && entry.icon != UiIconId::None) ? x + kIconSize + kIconTextGap : x;

            XrColor4f c = sel ? SelectionColor : Color;
            c.a *= alpha;
            XrColor4f shadow = {0.0f, 0.0f, 0.0f, 0.45f * alpha};
            ui.DrawText(m_font, entry.text, textX + 0.5f, y + 0.5f, 1.0f, shadow);
            ui.DrawText(m_font, entry.text, textX, y, 1.0f, c);

            if (m_icons && entry.icon != UiIconId::None)
            {
                const float iconY = rowY + (h - kIconSize) / 2.0f;
                const XrColor4f iconTint = (sel && TintIconOnSelect) ? SelectionColor : XrColor4f{1.0f, 1.0f, 1.0f, 1.0f};
                m_icons->Draw(ui, entry.icon, x, iconY, kIconSize, alpha, iconTint);
            }

            if (entry.accessoryDraw)
                entry.accessoryDraw(ui, m_posX + offsetX, rowY, m_width, h, alpha);
        }

        rowY += h;
    }

    if (needsScrollbar())
    {
        const float trackX = m_posX + m_width - kScrollbarWidth;
        const int totalEntries = (int)m_entries.size();
        const float trackPad = 1.5f; // gap at top/bottom so thumb never overflows
        const float trackY = m_posY + offsetY + trackPad;
        const float trackH = m_height - trackPad * 2;
        const float radius = kScrollbarWidth / 2.0f;

        // Track
        XrColor4f trackColor{0.3f, 0.3f, 0.3f, 0.5f * alpha};
        ui.DrawQuadRounded(trackX + offsetX, trackY, kScrollbarWidth, trackH, trackColor, radius);

        // Thumb - proportional height, clamped so it never leaves the track
        const float thumbH = std::max(4.0f, trackH * visible / totalEntries);
        const float maxThumbY = trackY + trackH - thumbH;
        const float thumbY = std::min(maxThumbY,
                                      trackY + trackH * m_firstVisible / totalEntries);
        XrColor4f thumbColor{0.75f, 0.75f, 0.75f, 0.9f * alpha};
        XrColor4f shadow = {0.0f, 0.0f, 0.0f, 0.45f * alpha};
        ui.DrawQuadRounded(trackX + offsetX + 0.5f, thumbY + 0.5f, kScrollbarWidth, thumbH, shadow, radius);
        ui.DrawQuadRounded(trackX + offsetX, thumbY, kScrollbarWidth, thumbH, thumbColor, radius);
    }
}
