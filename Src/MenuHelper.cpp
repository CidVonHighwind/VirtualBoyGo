#include "MenuHelper.h"
#include "FontMaster.h"
#include "DrawHelper.h"
#include "Global.h"

int SelectButton = BUTTON_A;
int BackButton = BUTTON_B;
bool SwappSelectBackButton = false;

void MenuLabel::DrawText(float offsetX, float offsetY, float transparency) {
  if (Visible)
    FontManager::RenderText(*Font, Text, PosX + offsetX + (Selected ? 5 : 0), PosY + offsetY, 1.0f, Color,
                            transparency);
}

void MenuImage::DrawTexture(float offsetX, float offsetY, float transparency) {
  if (Visible)
    DrawHelper::DrawTexture(ImageId, PosX + offsetX + (Selected ? 5 : 0), PosY + offsetY, Width, Height,
                            Color, transparency);
}

void MenuButton::DrawText(float offsetX, float offsetY, float transparency) {
  if (Visible)
    FontManager::RenderText(*Font, Text, PosX + 33 + (Selected ? 5 : 0) + offsetX, PosY + offsetY, 1.0f,
                            Selected ? textSelectionColor : textColor, transparency);
}

void MenuButton::DrawTexture(float offsetX, float offsetY, float transparency) {
  if (IconId > 0 && Visible)
    DrawHelper::DrawTexture(IconId, PosX + (Selected ? 5 : 0) + offsetX,
                            PosY + Font->PStart + Font->PHeight / 2 - 14 + offsetY, 28, 28, //  + Font->FontSize / 2 - 14
                            Selected ? textSelectionColor : textColor, transparency);
}

void MenuContainer::DrawText(float offsetX, float offsetY, float transparency) {
  for (int i = 0; i < MenuItems.size(); ++i) {
    MenuItems.at(i)->DrawText(offsetX, offsetY, transparency);
  }
}

void MenuContainer::DrawTexture(float offsetX, float offsetY, float transparency) {
  for (int i = 0; i < MenuItems.size(); ++i) {
    MenuItems.at(i)->DrawTexture(offsetX, offsetY, transparency);
  }
}