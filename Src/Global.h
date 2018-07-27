#ifndef ANDROID_RESOURCES_H
#define ANDROID_RESOURCES_H

#include "MenuHelper.h"

extern const std::string STR_HEADER, STR_VERSION;

extern const int HEADER_HEIGHT, BOTTOM_HEIGHT, MENU_WIDTH, MENU_HEIGHT;

extern int menuItemSize, saveSlot;
extern bool menuOpen, loadedRom, followHead;

extern Menu *currentMenu;

extern ovrVector4f sliderColor, textColorVersion, headerTextColor, MenuBackgroundColor,
    MenuBackgroundOverlayColor, MenuBackgroundOverlayColorLight,
    BatteryBackgroundColor, textColor, textSelectionColor;

extern int batterColorLevels;
extern ovrVector4f BatteryColors[];

extern FontManager::RenderFont fontHeader, fontBattery, fontTime, fontMenu, fontList, fontBottom,
    fontSlot,
    fontSmall;

extern GLuint textureBackgroundId, textureIdMenu, textureHeaderIconId, textureGbIconId,
    textureGbcIconId, textureVbIconId,
    textureSaveIconId,
    textureLoadIconId, textureWhiteId, textureResumeId, textureSettingsId, texuterLeftRightIconId,
    textureUpDownIconId, textureResetIconId, textureSaveSlotIconId, textureLoadRomIconId,
    textureBackIconId, textureMoveIconId, textureDistanceIconId, textureResetViewIconId,
    textureScaleIconId, textureMappingIconId, texturePaletteIconId, textureButtonAIconId,
    textureButtonBIconId, textureButtonXIconId, textureButtonYIconId, textureFollowHeadIconId,
    textureDMGIconId, textureExitIconId, threedeeIconId, twodeeIconId, mappingLeftDownId, mappingLeftUpId, mappingLeftLeftId, mappingLeftRightId,
    mappingRightDownId, mappingRightUpId, mappingRightLeftId, mappingRightRightId, mappingStartId, mappingSelectId, mappingTriggerLeft, mappingTriggerRight;

void SaveSettings();

void ResetMenuState();

#endif //ANDROID_RESOURCES_H
