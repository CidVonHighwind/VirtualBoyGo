#include <OVR_LogUtils.h>
#include <VirtualBoyGo/Src/main.h>
#include "MenuHelper.h"
#include "FontMaster.h"
#include "DrawHelper.h"
#include "Global.h"
#include "ButtonMapping.h"
#include "Menu.h"

// MenuItem

MenuItem::MenuItem() {
    Color = ovrVirtualBoyGo::global.textColor;
    SelectionColor = ovrVirtualBoyGo::global.textSelectionColor;
}

void MenuItem::OnSelect(int direction) {
    if (OnSelectFunction != nullptr) OnSelectFunction(this, direction);
}

void MenuItem::Update(uint *buttonState, uint *lastButtonState, float deltaSeconds) {
    if (UpdateFunction != nullptr) UpdateFunction(this, buttonState, lastButtonState);
}

int MenuItem::PressedUp() { return 0; }

int MenuItem::PressedDown() { return 0; }

int MenuItem::PressedLeft() { return 0; }

int MenuItem::PressedRight() { return 0; }

int MenuItem::PressedEnter() { return 0; }

void MenuItem::Select() { Selected = true; }

void MenuItem::Unselect() { Selected = false; }

void MenuItem::DrawText(FontManager &fontManager, float offsetX, float offsetY, float transparency) {}

void MenuItem::DrawTexture(DrawHelper &drawHelper, float offsetX, float offsetY, float transparency) {}

// MenuLabel

MenuLabel::MenuLabel(FontManager::RenderFont *font, std::string text, int posX, int posY, int width, int height, ovrVector4f color) {
    Font = font;
    Color = color;

    containerX = posX;
    containerY = posY;
    containerWidth = width;
    containerHeight = height;

    SetText(text);
}

MenuLabel::~MenuLabel() {}

void MenuLabel::DrawText(FontManager &fontManager, float offsetX, float offsetY, float transparency) {
    if (Visible)
        fontManager.RenderText(*Font, Text, PosX + offsetX + (Selected ? 5 : 0), PosY + offsetY, 1.0f, Color, transparency);
}

void MenuLabel::SetText(std::string newText) {
    Text = newText;
    // center text
    int textWidth = FontManager::GetWidth(*Font, newText);
    PosX = containerX + containerWidth / 2 - textWidth / 2;
    PosY = containerY + containerHeight / 2 - Font->PHeight / 2 - Font->PStart;
}

// MenuImage

void MenuImage::DrawTexture(DrawHelper &drawHelper, float offsetX, float offsetY, float transparency) {
    if (Visible)
        drawHelper.DrawTexture(ImageId, PosX + offsetX + (Selected ? 5 : 0), PosY + offsetY, Width, Height, Color, transparency);
}

MenuImage::MenuImage(GLuint imageId, int posX, int posY, int width, int height, ovrVector4f color) {
    ImageId = imageId;
    PosX = posX;
    PosY = posY;
    Width = width;
    Height = height;
    Color = color;
}

MenuImage::~MenuImage() {}

// MenuButton

MenuButton::MenuButton(FontManager::RenderFont *font, GLuint iconId, std::string text, int posX, int posY,
                       std::function<void(MenuItem *item)> pressFunction, std::function<void(MenuItem *item)> leftFunction,
                       std::function<void(MenuItem *item)> rightFunction) {
    PosX = posX;
    PosY = posY;
    IconId = iconId;
    Text = text;
    Font = font;
    PressFunction = pressFunction;
    LeftFunction = leftFunction;
    RightFunction = rightFunction;
    Selectable = true;
}

MenuButton::MenuButton(FontManager::RenderFont *font, GLuint iconId, std::string text, int posX, int posY, int width, int height,
                       std::function<void(MenuItem *item)> pressFunction, std::function<void(MenuItem *item)> leftFunction,
                       std::function<void(MenuItem *item)> rightFunction) {
    PosX = posX;
    PosY = posY + (int) (height / 2.0f - font->PHeight / 2.0f) - font->PStart;
    ContainerWidth = width;
    IconId = iconId;
    Font = font;
    PressFunction = pressFunction;
    LeftFunction = leftFunction;
    RightFunction = rightFunction;
    Selectable = true;
    SetText(text);
}

MenuButton::~MenuButton() {}

void MenuButton::DrawText(FontManager &fontManager, float offsetX, float offsetY, float transparency) {
    if (Visible)
        fontManager.RenderText(*Font, Text, PosX + (IconId > 0 ? 33 : 0) + (Selected ? 5 : 0) + offsetX + OffsetX,
                               PosY + offsetY, 1.0f, Selected ? SelectionColor : Color, transparency);
}

void MenuButton::DrawTexture(DrawHelper &drawHelper, float offsetX, float offsetY, float transparency) {
    if (IconId > 0 && Visible)
        drawHelper.DrawTexture(IconId, PosX + (Selected ? 5 : 0) + offsetX,
                               PosY + Font->PStart + Font->PHeight / 2 - 14 + offsetY, 28,
                               28, Selected ? SelectionColor : Color, transparency);
}

void MenuButton::SetText(std::string newText) {
    Text = newText;

    // center text
    if (ContainerWidth > 0) {
        int textWidth = FontManager::GetWidth(*Font, newText);
        OffsetX = ContainerWidth / 2 - textWidth / 2;
    }
}

int MenuButton::PressedLeft() {
    if (LeftFunction != nullptr) {
        LeftFunction(this);
        return 1;
    }
    return 0;
}

int MenuButton::PressedRight() {
    if (RightFunction != nullptr) {
        RightFunction(this);
        return 1;
    }
    return 0;
}

int MenuButton::PressedEnter() {
    if (PressFunction != nullptr) {
        PressFunction(this);
        return 1;
    }
    return 0;
}

// Menu

void ClearButtonState(uint *buttonState) {
    for (int i = 0; i < 3; ++i) {
        buttonState[i] = 0;
    }
}

void Menu::Update(uint *buttonState, uint *lastButtonState, float deltaSeconds) {
    using namespace ButtonMapper;

    MenuItems[CurrentSelection]->Unselect();

    if ((buttonState[DeviceGamepad] &
         (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right] |
          ButtonMapping[EmuButton_LeftStickUp] | ButtonMapping[EmuButton_LeftStickDown] |
          ButtonMapping[EmuButton_LeftStickLeft] | ButtonMapping[EmuButton_LeftStickRight])) ||
        (buttonState[DeviceLeftTouch] &
         (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right])) ||
        (buttonState[DeviceRightTouch] &
         (ButtonMapping[EmuButton_Up] | ButtonMapping[EmuButton_Down] | ButtonMapping[EmuButton_Left] | ButtonMapping[EmuButton_Right]))) {
        buttonDownCount += deltaSeconds;
    } else {
        buttonDownCount = 0;
    }

    for (auto &MenuItem : MenuItems) {
        MenuItem->Update(buttonState, lastButtonState, deltaSeconds);
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

    if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, ovrVirtualBoyGo::global.SwappSelectBackButton ? EmuButton_B : EmuButton_A) ||
        ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, ovrVirtualBoyGo::global.SwappSelectBackButton ? EmuButton_B : EmuButton_A)) {
        buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
        if (MenuItems[CurrentSelection]->PressedEnter() != 0) {
            ClearButtonState(buttonState);
        }
    } else if (ButtonPressed(buttonState, lastButtonState, DeviceGamepad, ovrVirtualBoyGo::global.SwappSelectBackButton ? EmuButton_A : EmuButton_B) ||
               ButtonPressed(buttonState, lastButtonState, DeviceRightTouch, ovrVirtualBoyGo::global.SwappSelectBackButton ? EmuButton_A : EmuButton_B)) {
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

void
Menu::Draw(DrawHelper &drawHelper, FontManager &fontManager, int transitionDirX, int transitionDirY, float moveProgress, int moveDist, float fadeProgress) {
    // draw the menu textures
    for (auto &MenuItem : MenuItems)
        MenuItem->DrawTexture(drawHelper, transitionDirX * moveProgress * moveDist, transitionDirY * moveProgress * moveDist, fadeProgress);

    // draw menu text
    fontManager.Begin();

    for (auto &MenuItem : MenuItems)
        MenuItem->DrawText(fontManager, transitionDirX * moveProgress * moveDist, transitionDirY * moveProgress * moveDist, fadeProgress);

    fontManager.End();
}

void Menu::MoveSelection(int dir, bool onSelect) {
    // WARNIGN: this will not work if there is nothing selectable
    // OVR_LOG("Move %i" + dir);
    do {
        CurrentSelection += dir;
        if (CurrentSelection < 0) CurrentSelection = (int) (MenuItems.size() - 1);
        if (CurrentSelection >= MenuItems.size()) CurrentSelection = 0;
    } while (!MenuItems[CurrentSelection]->Selectable);

    if (onSelect)
        MenuItems[CurrentSelection]->OnSelect(dir);
}

bool Menu::ButtonPressed(uint *buttonState, uint *lastButtonState, uint device, uint button) {
    return (buttonState[device] & ButtonMapper::ButtonMapping[button]) &&
           (!(lastButtonState[device] & ButtonMapper::ButtonMapping[button]) || buttonDownCount > MenuItems[CurrentSelection]->ScrollDelay);
}

void Menu::Init() { MenuItems[CurrentSelection]->Select(); }
