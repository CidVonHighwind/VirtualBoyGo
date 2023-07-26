#pragma once

#include "Appl.h"
#include "FontMaster.h"

class Global {
public:
    const ovrVector4f headerTextColor = {0.9f, 0.1f, 0.1f, 1.0f};
    const ovrVector4f textSelectionColor = {0.9f, 0.1f, 0.1f, 1.0f};
    const ovrVector4f textColor = {0.8f, 0.8f, 0.8f, 1.0f};
    const ovrVector4f sliderColor = {0.8f, 0.8f, 0.8f, 0.8f};
    const ovrVector4f MenuBackgroundColor = {0.2f, 0.2f, 0.2f, 0.95f};
    const ovrVector4f MenuBackgroundOverlayHeader = {0.5f, 0.5f, 0.5f, 0.15f};
    const ovrVector4f MenuBackgroundOverlayColorLight = {0.5f, 0.5f, 0.5f, 0.15f};
    const ovrVector4f MenuBackgroundOverlayColor = {0.431f, 0.412f, 0.443f, 0.75f};
    const ovrVector4f textColorBattery = {0.25f, 0.25f, 0.25f, 1.0f};
    const ovrVector4f textColorVersion = {0.8f, 0.8f, 0.8f, 1.0f};
    const ovrVector4f BatteryBackgroundColor = {0.25f, 0.25f, 0.25f, 1.0f};
    const ovrVector4f MenuBottomColor = {0.25f, 0.25f, 0.25f, 1.0f};

    FontManager::RenderFont fontHeader, fontBattery, fontTime, fontMenu, fontList, fontBottom, fontSlot, fontVersion, fontSmall;

    GLuint textureBackgroundId, textureHeaderIconId, textureGbIconId,
            textureGbcIconId, textureVbIconId,
            textureSaveIconId,
            textureLoadIconId, textureWhiteId, textureResumeId, textureSettingsId, texuterLeftRightIconId,
            textureUpDownIconId, textureResetIconId, textureSaveSlotIconId, textureLoadRomIconId,
            textureBackIconId, textureMoveIconId, textureDistanceIconId, textureResetViewIconId,
            textureScaleIconId, textureMappingIconId, texturePaletteIconId, textureButtonAIconId,
            textureButtonBIconId, textureButtonXIconId, textureButtonYIconId, textureFollowHeadIconId,
            textureDMGIconId, textureExitIconId, threedeeIconId, twodeeIconId, mappingLeftDownId, textureIpdIconId,
            mappingLeftUpId, mappingLeftLeftId, mappingLeftRightId, mappingRightDownId, mappingRightUpId,
            mappingRightLeftId, mappingRightRightId, mappingStartId, mappingSelectId, mappingTriggerLeft, mappingTriggerRight;

    std::string appStoragePath;
    std::string saveFilePath;
    std::string romFolderPath;

    int menuItemSize;
    int saveSlot;

    bool SwappSelectBackButton;
    bool menuOpen;
    bool followHead;

    void Init(OVRFW::ovrFileSys *fileSys);

    void Free();
};