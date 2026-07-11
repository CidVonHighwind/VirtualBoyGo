#pragma once
#include "../MenuPage.h"

// Screen placement: Yaw, Pitch, Roll, Distance, Scale, Reset View, Back.
class MoveScreenPage : public MenuPage
{
public:
    MenuPage *settingsPage = nullptr;

    void Init(UiRenderer &ui, UiFontHandle menuFont, const UiIconSet &icons) override;
};
