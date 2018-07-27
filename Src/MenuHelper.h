#ifndef ANDROID_MENUHELPER_H
#define ANDROID_MENUHELPER_H

#include <string>
#include <vector>
#include "App.h"
#include "FontMaster.h"

using namespace OVR;

extern int SelectButton;
extern int BackButton;
extern bool SwappSelectBackButton;

class MenuItem {
 public:
  ovrVector4f textSelectionColor = {0.9f, 0.1f, 0.1f, 1.0f};// {0.15f, 0.8f, 0.6f, 0.8f};
  ovrVector4f textColor = {0.8f, 0.8f, 0.8f, 1.0f};
  bool Selectable = false;
  bool Selected = false;
  bool Visible = true;
  int PosX = 100, PosY = 100;
  int ScrollDelay = 15;
  int ScrollTimeV = 5;
  int ScrollTimeH = 5;
  int Tag = 0;

  void (*UpdateFunction)(MenuItem *item, uint &buttonState, uint &lastButtonState) = nullptr;

  virtual ~MenuItem() {}

  virtual void Update(uint &buttonState, uint &lastButtonState) {
    if (UpdateFunction != nullptr) UpdateFunction(this, buttonState, lastButtonState);
  }

  virtual int PressedUp() { return 0; }
  virtual int PressedDown() { return 0; }
  virtual int PressedLeft() { return 0; }
  virtual int PressedRight() { return 0; }
  virtual int PressedEnter() { return 0; }

  virtual void Select() { Selected = true; }
  virtual void Unselect() { Selected = false; }

  virtual void DrawText(float offsetX, float offsetY, float transparency) {}
  virtual void DrawTexture(float offsetX, float offsetY, float transparency) {}
};

class MenuLabel : public MenuItem {
 public:
  MenuLabel(FontManager::RenderFont *font, std::string text, int posX, int posY, int width,
            int height, ovrVector4f color) {
    Font = font;
    Color = color;

    containerX = posX;
    containerY = posY;
    containerWidth = width;
    containerHeight = height;

    SetText(text);
  }

  void SetText(std::string newText){
    Text = newText;
    // center text
    int textWidth = FontManager::GetWidth(*Font, newText);
    PosX = containerX + containerWidth / 2 - textWidth / 2;
    PosY = containerY + containerHeight / 2 - Font->PHeight / 2 - Font->PStart;
  }

  virtual ~MenuLabel() {}

  int containerX, containerY, containerWidth, containerHeight;

  FontManager::RenderFont *Font;

  ovrVector4f Color;

  std::string Text;

  virtual void DrawText(float offsetX, float offsetY, float transparency) override;
};

class MenuImage : public MenuItem {
 public:
  MenuImage(GLuint imageId, int posX, int posY, int width, int height, ovrVector4f color) {
    ImageId = imageId;
    PosX = posX;
    PosY = posY;
    Width = width;
    Height = height;
    Color = color;
  }

  ovrVector4f Color;

  GLuint ImageId;

  int Width, Height;

  virtual ~MenuImage() {}

  virtual void DrawTexture(float offsetX, float offsetY, float transparency) override;
};

class MenuButton : public MenuItem {
 public:
  MenuButton(FontManager::RenderFont *font, GLuint iconId, std::string text, int posX, int posY,
             void (*pressFunction)(MenuItem *item), void (*leftFunction)(MenuItem *item),
             void (*rightFunction)(MenuItem *item)) {
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

  MenuButton(FontManager::RenderFont *font, GLuint iconId, std::string text, int posX, int posY,
             int height, void (*pressFunction)(MenuItem *item), void (*leftFunction)(MenuItem *item),
             void (*rightFunction)(MenuItem *item)) {
    PosX = posX;
    PosY = posY + (int)(height / 2.0f - font->PHeight / 2.0f) - font->PStart;
    IconId = iconId;
    Text = text;
    Font = font;
    PressFunction = pressFunction;
    LeftFunction = leftFunction;
    RightFunction = rightFunction;
    Selectable = true;
  }

  virtual ~MenuButton() {}

  void (*PressFunction)(MenuItem *item) = nullptr;
  void (*LeftFunction)(MenuItem *item) = nullptr;
  void (*RightFunction)(MenuItem *item) = nullptr;

  FontManager::RenderFont *Font;

  GLuint IconId;

  std::string Text;

  virtual int PressedLeft() override {
    if (LeftFunction != nullptr) {
      LeftFunction(this);
      return 1;
    }
    return 0;
  }

  virtual int PressedRight() override {
    if (RightFunction != nullptr) {
      RightFunction(this);
      return 1;
    }
    return 0;
  }

  virtual int PressedEnter() override {
    if (PressFunction != nullptr) {
      PressFunction(this);
      return 1;
    }
    return 0;
  }

  virtual void DrawText(float offsetX, float offsetY, float transparency) override;

  virtual void DrawTexture(float offsetX, float offsetY, float transparency) override;
};

class MenuContainer : public MenuItem {
 public:
  MenuContainer() { Selectable = true; }

  virtual ~MenuContainer() {}

  std::vector<MenuItem *> MenuItems;

  virtual int PressedLeft() override { return MenuItems.at(0)->PressedLeft(); }

  virtual int PressedRight() override { return MenuItems.at(0)->PressedRight(); }

  virtual int PressedEnter() override { return MenuItems.at(0)->PressedEnter(); }

  virtual void DrawText(float offsetX, float offsetY, float transparency) override;

  virtual void DrawTexture(float offsetX, float offsetY, float transparency) override;

  virtual void Select() override {
    for (int i = 0; i < MenuItems.size(); ++i) {
      MenuItems.at(i)->Select();
    }
  }

  virtual void Unselect() override {
    for (int i = 0; i < MenuItems.size(); ++i) {
      MenuItems.at(i)->Unselect();
    }
  }
};

template <class MyType>
class MenuList : public MenuItem {
 public:
  MenuList(FontManager::RenderFont *font, void (*pressFunction)(MyType *item),
           std::vector<MyType> *romList, int posX, int posY, int width, int height) {
    Font = font;
    PressFunction = pressFunction;
    ItemList = romList;

    PosX = posX;
    PosY = posY;
    Width = width;
    Height = height;

    listItemSize = (font->FontSize + 8);
    itemOffsetY = 4;
    maxListItems = height / listItemSize;
    scrollbarHeight = height;
    listStartY = posY + (scrollbarHeight - (maxListItems * listItemSize)) / 2;
    Selectable = true;
  }

  virtual ~MenuList() {}

  FontManager::RenderFont *Font;

  void (*PressFunction)(MyType *item) = nullptr;

  std::vector<MyType> *ItemList;

  int maxListItems = 0;
  int Width = 0, Height = 0;
  int scrollbarWidth = 14, scrollbarHeight = 0;
  int listItemSize = 0;
  int itemOffsetY = 0;
  int listStartY = 0;

  int CurrentSelection = 0;
  int menuListState = 0;

  float menuListFState = 0;

  virtual int PressedUp() override {
    CurrentSelection--;
    if (CurrentSelection < 0) CurrentSelection = (int)(ItemList->size() - 1);
    return 1;
  }

  virtual int PressedDown() override {
    CurrentSelection++;
    if (CurrentSelection >= ItemList->size()) CurrentSelection = 0;
    return 1;
  }

  virtual int PressedEnter() override {
    if (ItemList->size() <= 0) return 1;

    if (PressFunction != nullptr) PressFunction(&ItemList->at(CurrentSelection));

    return 1;
  }

  virtual void Update(uint &buttonState, uint &lastButtonState) override {
    // scroll the list to the current Selection
    if (CurrentSelection - 2 < menuListState && menuListState > 0) {
      menuListState--;
    }
    if (CurrentSelection + 2 >= menuListState + maxListItems &&
        menuListState + maxListItems < ItemList->size()) {
      menuListState++;
    }

    float dist = menuListState - menuListFState;

    if(dist < -0.0125f){
      if(dist < -0.25f)
        menuListFState += dist * 0.1f + 0.00625f;
      else
        menuListFState -= ((dist * 8) * (dist * 8)) * 0.00625f + 0.00625f;
    }
    else if(dist > 0.0125f) {
      if(dist > 0.25f)
        menuListFState += dist * 0.1f + 0.00625f;
      else
        menuListFState += ((dist * 8) * (dist * 8)) * 0.00625f + 0.00625f;
    } else
      menuListFState = menuListState;
  }

  /*
  virtual int PressedUp() override {
    ListItems[CurrentSelection]->Selected = false;

    CurrentSelection--;
    if (CurrentSelection < 0) {
      CurrentSelection = (int)(ListItems.size() - 1);
    }

    ListItems[CurrentSelection]->Selected = true;
  }

  virtual int PressedDown() override {
    ListItems[CurrentSelection]->Selected = false;

    CurrentSelection++;
    if (CurrentSelection >= ListItems.size()) {
      CurrentSelection = 0;
    }

    ListItems[CurrentSelection]->Selected = true;
  }
   */

  virtual void DrawText(float offsetX, float offsetY, float transparency) override;

  virtual void DrawTexture(float offsetX, float offsetY, float transparency) override;
};

class Menu {
 public:
  std::vector<MenuItem *> MenuItems;

  int CurrentSelection = 0;
  int buttonDownCount = 0;

 public:
  void (*BackPress)() = nullptr;

  void Init() { MenuItems[CurrentSelection]->Select(); }

  bool ButtonPressed(uint &buttonState, uint &lastButtonState, uint button) {
    return (buttonState & button) && (!(lastButtonState & button) ||
                                    buttonDownCount > MenuItems[CurrentSelection]->ScrollDelay);
  }

  void MoveSelection(int dir) {
    // WARNIGN: this will not work if there is nothing selectable
    // LOG("Move %i" + dir);
    do {
      CurrentSelection += dir;
      if (CurrentSelection < 0) CurrentSelection = (int)(MenuItems.size() - 1);
      if (CurrentSelection >= MenuItems.size()) CurrentSelection = 0;
    } while (!MenuItems[CurrentSelection]->Selectable);
  }

  void Update(uint &buttonState, uint &lastButtonState) {
    MenuItems[CurrentSelection]->Unselect();

    // could be done with a single &
    if (buttonState & BUTTON_LSTICK_UP || buttonState & BUTTON_DPAD_UP ||
        buttonState & BUTTON_LSTICK_DOWN || buttonState & BUTTON_DPAD_DOWN ||
        buttonState & BUTTON_LSTICK_LEFT || buttonState & BUTTON_DPAD_LEFT ||
        buttonState & BUTTON_LSTICK_RIGHT || buttonState & BUTTON_DPAD_RIGHT) {
      buttonDownCount++;
    } else {
      buttonDownCount = 0;
    }

    for (int i = 0; i < MenuItems.size(); ++i) {
      MenuItems[i]->Update(buttonState, lastButtonState);
    }

    if (ButtonPressed(buttonState, lastButtonState, BUTTON_LSTICK_LEFT) ||
        ButtonPressed(buttonState, lastButtonState, BUTTON_DPAD_LEFT)) {
      buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
      if (MenuItems[CurrentSelection]->PressedLeft() != 0) {
        buttonState = 0;
      }
    }

    if (ButtonPressed(buttonState, lastButtonState, BUTTON_LSTICK_RIGHT) ||
        ButtonPressed(buttonState, lastButtonState, BUTTON_DPAD_RIGHT)) {
      buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
      if (MenuItems[CurrentSelection]->PressedRight() != 0) {
        buttonState = 0;
      }
    }

    if (ButtonPressed(buttonState, lastButtonState, SelectButton)) {
      buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeH;
      if (MenuItems[CurrentSelection]->PressedEnter() != 0) {
        buttonState = 0;
      }
    }
    else if (buttonState & BackButton && !(lastButtonState & BackButton)) {
      if (BackPress != nullptr) BackPress();
    }

    if (ButtonPressed(buttonState, lastButtonState, BUTTON_LSTICK_UP) ||
        ButtonPressed(buttonState, lastButtonState, BUTTON_DPAD_UP)) {
      buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeV;
      if (MenuItems[CurrentSelection]->PressedUp() == 0) {
        MoveSelection(-1);
      }
    }

    if (ButtonPressed(buttonState, lastButtonState, BUTTON_LSTICK_DOWN) ||
        ButtonPressed(buttonState, lastButtonState, BUTTON_DPAD_DOWN)) {
      buttonDownCount -= MenuItems[CurrentSelection]->ScrollTimeV;
      if (MenuItems[CurrentSelection]->PressedDown() == 0) {
        MoveSelection(1);
      }
    }

    MenuItems[CurrentSelection]->Select();
  }

  void Draw(int transitionDirX, int transitionDirY, float moveProgress, int moveDist, float fadeProgress) {
    // draw the menu textures
    for (uint i = 0; i < MenuItems.size(); i++)
      MenuItems[i]->DrawTexture(transitionDirX * moveProgress * moveDist, transitionDirY * moveProgress * moveDist, fadeProgress);

    // draw menu text
    FontManager::Begin();
    for (uint i = 0; i < MenuItems.size(); i++)
      MenuItems[i]->DrawText(transitionDirX * moveProgress * moveDist, transitionDirY * moveProgress * moveDist, fadeProgress);
    FontManager::Close();
  }
};

#endif  // ANDROID_MENUHELPER_H
