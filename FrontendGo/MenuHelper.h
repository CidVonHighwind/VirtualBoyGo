#pragma once

#include <string>
#include <vector>
#include "FontMaster.h"
#include "Appl.h"
#include "DrawHelper.h"
#include <OVR_LogUtils.h>

class MenuItem {
public:
    bool Selectable = false;
    bool Selected = false;
    bool Visible = true;
    int PosX = 100, PosY = 100;

    float ScrollDelay = 0.3;
    float ScrollTimeV = 0.075;
    float ScrollTimeH = 0.075;

    int Tag = 0;
    int Tag2 = 0;

    ovrVector4f Color;
    ovrVector4f SelectionColor;

    std::function<void(MenuItem *item, int direction)> OnSelectFunction;
    std::function<void(MenuItem *item, uint *buttonState, uint *lastButtonState)> UpdateFunction;

    MenuItem();

    virtual ~MenuItem() {}

    virtual void Update(uint *buttonState, uint *lastButtonState, float deltaSeconds);

    virtual int PressedUp();

    virtual int PressedDown();

    virtual int PressedLeft();

    virtual int PressedRight();

    virtual int PressedEnter();

    virtual void OnSelect(int direction);

    virtual void Select();

    virtual void Unselect();

    virtual void DrawText(FontManager &fontManager, float offsetX, float offsetY, float transparency);

    virtual void DrawTexture(DrawHelper &drawHelper, float offsetX, float offsetY, float transparency);
};

class Menu {
public:
    std::vector<std::shared_ptr<MenuItem>> MenuItems;

    int CurrentSelection = 0;
    float buttonDownCount = 0;

    std::function<void()> BackPress;

    void Init();

    bool ButtonPressed(uint *buttonState, uint *lastButtonState, uint device, uint button);

    void MoveSelection(int dir, bool onSelect);

    void Update(uint *buttonState, uint *lastButtonState, float deltaSeconds);

    void Draw(DrawHelper &drawHelper, FontManager &fontManager, int transitionDirX, int transitionDirY, float moveProgress, int moveDist, float fadeProgress);
};

class MenuLabel : public MenuItem {

private:
    int containerX;
    int containerY;
    int containerWidth;
    int containerHeight;

    FontManager::RenderFont *Font;

    std::string Text;

public:
    MenuLabel(FontManager::RenderFont *font, std::string text, int posX, int posY, int width, int height, ovrVector4f color);

    void SetText(std::string newText);

    virtual ~MenuLabel();

    virtual void DrawText(FontManager &fontManager, float offsetX, float offsetY, float transparency) override;
};

class MenuImage : public MenuItem {

private:
    int Width;
    int Height;

    ovrVector4f Color;
    GLuint ImageId;

public:
    MenuImage(GLuint imageId, int posX, int posY, int width, int height, ovrVector4f color);

    virtual ~MenuImage();

    virtual void DrawTexture(DrawHelper &drawHelper, float offsetX, float offsetY, float transparency) override;
};

class MenuButton : public MenuItem {
private:
    std::function<void(MenuItem *item)> PressFunction;

    std::function<void(MenuItem *item)> LeftFunction;

    std::function<void(MenuItem *item)> RightFunction;

    FontManager::RenderFont *Font;

    int ContainerWidth = 0;
    int OffsetX = 0;

public:
    GLuint IconId;

    std::string Text;

    MenuButton(FontManager::RenderFont *font, GLuint iconId, std::string text, int posX, int posY,
               std::function<void(MenuItem *item)> pressFunction, std::function<void(MenuItem *item)> leftFunction,
               std::function<void(MenuItem *item)> rightFunction);

    MenuButton(FontManager::RenderFont *font, GLuint iconId, std::string text, int posX, int posY, int width, int height,
               std::function<void(MenuItem *item)> pressFunction, std::function<void(MenuItem *item)> leftFunction,
               std::function<void(MenuItem *item)> rightFunction);

    virtual ~MenuButton();

    virtual int PressedLeft() override;

    virtual int PressedRight() override;

    virtual int PressedEnter() override;

    void SetText(std::string newText);

    virtual void DrawText(FontManager &fontManager, float offsetX, float offsetY, float transparency) override;

    virtual void DrawTexture(DrawHelper &drawHelper, float offsetX, float offsetY, float transparency) override;
};

template<class MyType>
class MenuList : public MenuItem {
private:
    FontManager::RenderFont *Font;

    std::function<void(MyType *item)> PressFunction;

    int maxListItems = 0;

    int Width = 0;
    int Height = 0;

    int scrollbarWidth = 14;
    int scrollbarHeight = 0;

    int listItemSize = 0;
    int itemOffsetY = 0;
    int listStartY = 0;

    int menuListState = 0;

    float menuListFState = 0;

public:
    std::vector<MyType> *ItemList;

    int CurrentSelection = 0;

    MenuList(FontManager::RenderFont *font, std::function<void(MyType *)> pressFunction, std::vector<MyType> *romList,
             int posX, int posY, int width, int height) {
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

    int PressedUp() {
        CurrentSelection--;
        if (CurrentSelection < 0)
            CurrentSelection = (int) (ItemList->size() - 1);
        return 1;
    }

    int PressedDown() override {
        CurrentSelection++;
        if (CurrentSelection >= ItemList->size())
            CurrentSelection = 0;
        return 1;
    }

    int PressedEnter() {
        if (ItemList->size() <= 0) return 1;

        if (PressFunction != nullptr)
            PressFunction(&ItemList->at(CurrentSelection));

        return 1;
    }

    void Update(uint *buttonState, uint *lastButtonState, float deltaSeconds) {
        // scroll the list to the current Selection
        if (CurrentSelection - 3 < menuListState)
            menuListState = CurrentSelection - 3;
        if (CurrentSelection + 4 - maxListItems > menuListState)
            menuListState = CurrentSelection + 4 - maxListItems;

        if (menuListState > (int) ItemList->size() - maxListItems)
            menuListState = (int) ItemList->size() - maxListItems;
        if (menuListState < 0)
            menuListState = 0;

        float dist = menuListState - menuListFState;
        float absDist = dist < 0 ? -dist : dist;
        float dir = dist < 0 ? -1 : 1;

        if (absDist > 10)
            absDist = 10;
        float speed = cos(MATH_FLOAT_PIOVER2 * 0.9 * (1.0 - absDist / 10.0)) * 2.0;
        menuListFState += speed * dir * (deltaSeconds * 25.0);

        // finished moving?
        if ((dir > 0 && menuListFState > menuListState) ||
            (dir < 0 && menuListFState < menuListState))
            menuListFState = menuListState;
    }

    virtual void DrawText(FontManager &fontManager, float offsetX, float offsetY, float transparency) override;

    virtual void DrawTexture(DrawHelper &drawHelper, float offsetX, float offsetY, float transparency) override;
};
