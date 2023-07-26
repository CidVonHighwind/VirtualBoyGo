//
// Created by Patrick on 26.10.2020.
//
#include "Global.h"
#include "TextureLoader.h"

void Global::Init(OVRFW::ovrFileSys *fileSys) {
    menuItemSize = 20;
    saveSlot = 0;

    SwappSelectBackButton = false;
    menuOpen = true;
    followHead = false;

    // load all the different font sizes
    FontManager::LoadFontFromAssets(fileSys, &fontHeader, "apk:///assets/fonts/VirtualLogo.ttf", 65);
    FontManager::LoadFont(&fontMenu, "/system/fonts/Roboto-Regular.ttf", 24);
    FontManager::LoadFont(&fontList, "/system/fonts/Roboto-Regular.ttf", 22);
    FontManager::LoadFont(&fontBottom, "/system/fonts/Roboto-Bold.ttf", 22);
    FontManager::LoadFont(&fontVersion, "/system/fonts/Roboto-Regular.ttf", 20);
    FontManager::LoadFont(&fontSlot, "/system/fonts/Roboto-Regular.ttf", 26);
    FontManager::LoadFont(&fontBattery, "/system/fonts/Roboto-Bold.ttf", 23);
    FontManager::LoadFont(&fontTime, "/system/fonts/Roboto-Bold.ttf", 25);
    FontManager::CloseFontLoader();

    // load all the textures used in the menu
    textureBackgroundId = TextureLoader::Load(fileSys, "apk:///assets/background2.dds");

    textureHeaderIconId = TextureLoader::Load(fileSys, "apk:///assets/header_icon.dds");
    textureGbIconId = TextureLoader::Load(fileSys, "apk:///assets/gb_cartridge.dds");
    textureVbIconId = TextureLoader::Load(fileSys, "apk:///assets/vb_cartridge.dds");

    textureGbcIconId = TextureLoader::Load(fileSys, "apk:///assets/gbc_cartridge.dds");
    textureSaveIconId = TextureLoader::Load(fileSys, "apk:///assets/save_icon.dds");
    textureLoadIconId = TextureLoader::Load(fileSys, "apk:///assets/load_icon.dds");
    textureResumeId = TextureLoader::Load(fileSys, "apk:///assets/resume_icon.dds");
    textureSettingsId = TextureLoader::Load(fileSys, "apk:///assets/settings_icon.dds");
    texuterLeftRightIconId = TextureLoader::Load(fileSys, "apk:///assets/leftright_icon.dds");
    textureUpDownIconId = TextureLoader::Load(fileSys, "apk:///assets/updown_icon.dds");
    textureResetIconId = TextureLoader::Load(fileSys, "apk:///assets/reset_icon.dds");
    textureSaveSlotIconId = TextureLoader::Load(fileSys, "apk:///assets/save_slot_icon.dds");
    textureLoadRomIconId = TextureLoader::Load(fileSys, "apk:///assets/rom_list_icon.dds");
    textureMoveIconId = TextureLoader::Load(fileSys, "apk:///assets/move_icon.dds");
    textureBackIconId = TextureLoader::Load(fileSys, "apk:///assets/back_icon.dds");
    textureDistanceIconId = TextureLoader::Load(fileSys, "apk:///assets/distance_icon.dds");
    textureResetViewIconId = TextureLoader::Load(fileSys, "apk:///assets/reset_view_icon.dds");
    textureScaleIconId = TextureLoader::Load(fileSys, "apk:///assets/scale_icon.dds");
    textureMappingIconId = TextureLoader::Load(fileSys, "apk:///assets/mapping_icon.dds");
    texturePaletteIconId = TextureLoader::Load(fileSys, "apk:///assets/palette_icon.dds");
    textureButtonAIconId = TextureLoader::Load(fileSys, "apk:///assets/button_a_icon.dds");
    textureButtonBIconId = TextureLoader::Load(fileSys, "apk:///assets/button_b_icon.dds");
    textureButtonXIconId = TextureLoader::Load(fileSys, "apk:///assets/button_x_icon.dds");
    textureButtonYIconId = TextureLoader::Load(fileSys, "apk:///assets/button_y_icon.dds");
    textureFollowHeadIconId = TextureLoader::Load(fileSys, "apk:///assets/follow_head_icon.dds");
    textureDMGIconId = TextureLoader::Load(fileSys, "apk:///assets/force_dmg_icon.dds");
    textureExitIconId = TextureLoader::Load(fileSys, "apk:///assets/exit_icon.dds");
    threedeeIconId = TextureLoader::Load(fileSys, "apk:///assets/3d_icon.dds");
    twodeeIconId = TextureLoader::Load(fileSys, "apk:///assets/2d_icon.dds");

    textureIpdIconId = TextureLoader::Load(fileSys, "apk:///assets/ipd_icon.dds");

    mappingLeftDownId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/left_down.dds");
    mappingLeftUpId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/left_up.dds");
    mappingLeftLeftId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/left_left.dds");
    mappingLeftRightId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/left_right.dds");

    mappingRightDownId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/right_down.dds");
    mappingRightUpId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/right_up.dds");
    mappingRightLeftId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/right_left.dds");
    mappingRightRightId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/right_right.dds");

    mappingStartId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/start.dds");
    mappingSelectId = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/select.dds");

    mappingTriggerLeft = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/trigger_left.dds");
    mappingTriggerRight = TextureLoader::Load(fileSys, "apk:///assets/icons/mapping/trigger_right.dds");

    textureWhiteId = TextureLoader::CreateWhiteTexture();
}

void Global::Free() {
    glBindTexture(GL_TEXTURE_2D, 0);

    // free the font textures
    glDeleteTextures(1, &fontHeader.textureID);
    glDeleteTextures(1, &fontMenu.textureID);
    glDeleteTextures(1, &fontList.textureID);
    glDeleteTextures(1, &fontBottom.textureID);
    glDeleteTextures(1, &fontVersion.textureID);
    glDeleteTextures(1, &fontSlot.textureID);
    glDeleteTextures(1, &fontBattery.textureID);
    glDeleteTextures(1, &fontTime.textureID);

    // free the textures
    glDeleteTextures(1, &textureBackgroundId);

    glDeleteTextures(1, &textureHeaderIconId);
    glDeleteTextures(1, &textureGbIconId);
    glDeleteTextures(1, &textureVbIconId);

    glDeleteTextures(1, &textureGbcIconId);
    glDeleteTextures(1, &textureSaveIconId);
    glDeleteTextures(1, &textureLoadIconId);
    glDeleteTextures(1, &textureResumeId);
    glDeleteTextures(1, &textureSettingsId);
    glDeleteTextures(1, &texuterLeftRightIconId);
    glDeleteTextures(1, &textureUpDownIconId);
    glDeleteTextures(1, &textureResetIconId);
    glDeleteTextures(1, &textureSaveSlotIconId);
    glDeleteTextures(1, &textureLoadRomIconId);
    glDeleteTextures(1, &textureMoveIconId);
    glDeleteTextures(1, &textureBackIconId);
    glDeleteTextures(1, &textureDistanceIconId);
    glDeleteTextures(1, &textureResetViewIconId);
    glDeleteTextures(1, &textureScaleIconId);
    glDeleteTextures(1, &textureMappingIconId);
    glDeleteTextures(1, &texturePaletteIconId);
    glDeleteTextures(1, &textureButtonAIconId);
    glDeleteTextures(1, &textureButtonBIconId);
    glDeleteTextures(1, &textureButtonXIconId);
    glDeleteTextures(1, &textureButtonYIconId);
    glDeleteTextures(1, &textureFollowHeadIconId);
    glDeleteTextures(1, &textureDMGIconId);
    glDeleteTextures(1, &textureExitIconId);
    glDeleteTextures(1, &threedeeIconId);
    glDeleteTextures(1, &twodeeIconId);

    glDeleteTextures(1, &textureIpdIconId);

    glDeleteTextures(1, &mappingLeftDownId);
    glDeleteTextures(1, &mappingLeftUpId);
    glDeleteTextures(1, &mappingLeftLeftId);
    glDeleteTextures(1, &mappingLeftRightId);

    glDeleteTextures(1, &mappingRightDownId);
    glDeleteTextures(1, &mappingRightUpId);
    glDeleteTextures(1, &mappingRightLeftId);
    glDeleteTextures(1, &mappingRightRightId);

    glDeleteTextures(1, &mappingStartId);
    glDeleteTextures(1, &mappingSelectId);

    glDeleteTextures(1, &mappingTriggerLeft);
    glDeleteTextures(1, &mappingTriggerRight);

    glDeleteTextures(1, &textureWhiteId);
}