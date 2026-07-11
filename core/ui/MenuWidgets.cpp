#include "MenuWidgets.h"

// ---- MenuItem ----

void MenuItem::Update(uint32_t* buttonState, uint32_t* lastButtonState, float deltaSeconds) {
    if (UpdateFunction != nullptr) UpdateFunction(this, buttonState, lastButtonState);
}

int MenuItem::PressedUp() { return 0; }
int MenuItem::PressedDown() { return 0; }
int MenuItem::PressedLeft() { return 0; }
int MenuItem::PressedRight() { return 0; }
int MenuItem::PressedEnter() { return 0; }

void MenuItem::OnSelect(int direction) {
    if (OnSelectFunction != nullptr) OnSelectFunction(this, direction);
}

void MenuItem::Select() { Selected = true; }
void MenuItem::Unselect() { Selected = false; }

void MenuItem::Draw(UiRenderer& ui, float offsetX, float offsetY, float alpha) {}

// ---- MenuLabel ----

MenuLabel::MenuLabel(UiRenderer& ui, UiFontHandle font, const std::string& text, int posX, int posY, int width, int height,
                     XrColor4f color)
    : m_ui(&ui), m_font(font), m_containerX(posX), m_containerY(posY), m_containerWidth(width), m_containerHeight(height) {
    Color = color;
    SetText(text);
}

void MenuLabel::SetText(const std::string& newText) {
    m_text = newText;
    // Center text within the label's container, same as the original.
    const float textWidth = m_ui->GetTextWidth(m_font, newText);
    PosX = m_containerX + m_containerWidth / 2 - static_cast<int>(textWidth) / 2;
    PosY = m_containerY + m_containerHeight / 2 - m_ui->GetFontPHeight(m_font) / 2 - m_ui->GetFontPStart(m_font);
}

void MenuLabel::Draw(UiRenderer& ui, float offsetX, float offsetY, float alpha) {
    if (!Visible) return;
    ui.DrawText(m_font, m_text, PosX + offsetX + (Selected ? 5 : 0), PosY + offsetY, 1.0f, Color);
}

// ---- MenuButton ----

MenuButton::MenuButton(UiRenderer& ui, UiFontHandle font, const std::string& text, int posX, int posY, int width, int height,
                       std::function<void(MenuItem*)> pressFunction, std::function<void(MenuItem*)> leftFunction,
                       std::function<void(MenuItem*)> rightFunction)
    : m_ui(&ui), m_font(font), m_pressFunction(pressFunction), m_leftFunction(leftFunction), m_rightFunction(rightFunction) {
    PosX = posX;
    PosY = posY + static_cast<int>(height / 2.0f - ui.GetFontPHeight(font) / 2.0f) - ui.GetFontPStart(font);
    m_containerWidth = width;
    Selectable = true;
    SetText(text);
}

MenuButton::MenuButton(UiRenderer& ui, UiFontHandle font, const std::string& text, int posX, int posY,
                       std::function<void(MenuItem*)> pressFunction, std::function<void(MenuItem*)> leftFunction,
                       std::function<void(MenuItem*)> rightFunction)
    : m_ui(&ui), m_font(font), m_pressFunction(pressFunction), m_leftFunction(leftFunction), m_rightFunction(rightFunction) {
    PosX = posX;
    PosY = posY;
    Selectable = true;
    SetText(text);
}

void MenuButton::SetText(const std::string& newText) {
    Text = newText;
    if (m_containerWidth > 0) {
        const float textWidth = m_ui->GetTextWidth(m_font, newText);
        m_offsetX = m_containerWidth / 2 - static_cast<int>(textWidth) / 2;
    }
}

int MenuButton::PressedLeft() {
    if (m_leftFunction != nullptr) {
        m_leftFunction(this);
        return 1;
    }
    return 0;
}

int MenuButton::PressedRight() {
    if (m_rightFunction != nullptr) {
        m_rightFunction(this);
        return 1;
    }
    return 0;
}

int MenuButton::PressedEnter() {
    if (m_pressFunction != nullptr) {
        m_pressFunction(this);
        return 1;
    }
    return 0;
}

void MenuButton::Draw(UiRenderer& ui, float offsetX, float offsetY, float alpha) {
    if (!Visible) return;
    ui.DrawText(m_font, Text, PosX + (Selected ? 5 : 0) + offsetX + m_offsetX, PosY + offsetY, 1.0f,
               Selected ? SelectionColor : Color);
}

// ---- Menu ----

namespace {
void ClearButtonState(uint32_t* buttonState) {
    for (int i = 0; i < 3; ++i) buttonState[i] = 0;
}
}  // namespace

void Menu::Init() { MenuItems[CurrentSelection]->Select(); }

bool Menu::ButtonPressed(uint32_t* buttonState, uint32_t* lastButtonState, uint32_t device, uint32_t button) {
    return (buttonState[device] & ButtonMapper::ButtonMapping[button]) &&
           (!(lastButtonState[device] & ButtonMapper::ButtonMapping[button]) ||
            buttonDownCount > MenuItems[CurrentSelection]->ScrollDelay);
}

void Menu::MoveSelection(int dir, bool onSelect) {
    // Will not terminate if nothing in the list is selectable.
    do {
        CurrentSelection += dir;
        if (CurrentSelection < 0) CurrentSelection = static_cast<int>(MenuItems.size()) - 1;
        if (CurrentSelection >= static_cast<int>(MenuItems.size())) CurrentSelection = 0;
    } while (!MenuItems[CurrentSelection]->Selectable);

    if (onSelect) MenuItems[CurrentSelection]->OnSelect(dir);
}

void Menu::Update(uint32_t* buttonState, uint32_t* lastButtonState, float deltaSeconds) {
    using namespace ButtonMapper;

    MenuItems[CurrentSelection]->Unselect();

    if ((buttonState[DeviceGamepad] &
        (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right] |
         ButtonMapping[EmuButton_LeftStickUp] | ButtonMapping[EmuButton_LeftStickDown] | ButtonMapping[EmuButton_LeftStickLeft] |
         ButtonMapping[EmuButton_LeftStickRight])) ||
        (buttonState[DeviceLeftTouch] &
        (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right])) ||
        (buttonState[DeviceRightTouch] &
        (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right]))) {
        buttonDownCount += deltaSeconds;
    } else {
        buttonDownCount = 0;
    }

    for (auto& item : MenuItems) {
        item->Update(buttonState, lastButtonState, deltaSeconds);
    }

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_Left) ||
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_LeftStickLeft) ||
        ButtonPressed(buttonState, lastButtonState, DeviceLeftTouch, EmuButton_Left) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, EmuButton_Left)) {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
        if (MenuItems[CurrentSelection]->PressedLeft() != 0) {
            ClearButtonState(buttonState);
        }
    }

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_Right) ||
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_LeftStickRight) ||
        ButtonPressed(buttonState, lastButtonState, DeviceLeftTouch, EmuButton_Right) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, EmuButton_Right)) {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
        if (MenuItems[CurrentSelection]->PressedRight() != 0) {
            ClearButtonState(buttonState);
        }
    }

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, SwapSelectBackButton ? EmuButton_B : EmuButton_A) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, SwapSelectBackButton ? EmuButton_B : EmuButton_A)) {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
        if (MenuItems[CurrentSelection]->PressedEnter() != 0) {
            ClearButtonState(buttonState);
        }
    } else if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, SwapSelectBackButton ? EmuButton_A : EmuButton_B) ||
              ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, SwapSelectBackButton ? EmuButton_A : EmuButton_B)) {
        if (BackPress != nullptr) BackPress();
    }

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_Up) ||
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_LeftStickUp) ||
        ButtonPressed(buttonState, lastButtonState, DeviceLeftTouch, EmuButton_Up) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, EmuButton_Up)) {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeV;
        if (MenuItems[CurrentSelection]->PressedUp() == 0) {
            MoveSelection(-1, true);
        }
    }

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_Down) ||
        ButtonPressed(buttonState, lastButtonState, DeviceGamepad, EmuButton_LeftStickDown) ||
        ButtonPressed(buttonState, lastButtonState, DeviceLeftTouch, EmuButton_Down) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, EmuButton_Down)) {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeV;
        if (MenuItems[CurrentSelection]->PressedDown() == 0) {
            MoveSelection(1, true);
        }
    }

    MenuItems[CurrentSelection]->Select();
}

void Menu::Draw(UiRenderer& ui, int transitionDirX, int transitionDirY, float moveProgress, int moveDist, float fadeProgress) {
    for (auto& item : MenuItems) {
        item->Draw(ui, transitionDirX * moveProgress * moveDist, transitionDirY * moveProgress * moveDist, fadeProgress);
    }
}
