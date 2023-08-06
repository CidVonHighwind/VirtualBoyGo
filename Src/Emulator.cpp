#include "Emulator.h"

#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <BeetleVBLibretroGo/mednafen/vrvb.h>
#include <OVR_LogUtils.h>

#include "Audio/OpenSLWrap.h"
#include "DrawHelper.h"
#include "FontMaster.h"
#include "LayerBuilder.h"
#include "MenuHelper.h"
#include "Global.h"

#include "main.h"

template<typename T>
std::string ToString(T value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

template<>
void MenuList<Emulator::Rom>::DrawText(FontManager &fontManager, float offsetX, float offsetY, float transparency) {
    // draw rom list
    for (uint i = (uint) menuListFState; i < menuListFState + maxListItems; i++) {
        if (i < ItemList->size()) {
            // fading in or out
            float fadeTransparency = 1;
            if (i - menuListFState < 0) {
                fadeTransparency = 1 - (menuListFState - i);
            } else if (i - menuListFState >= maxListItems - 1 && menuListFState != (int) menuListFState) {
                fadeTransparency = menuListFState - (int) menuListFState;
            }

            fontManager.RenderText(
                    *Font,
                    ItemList->at(i).RomName,
                    PosX + offsetX + scrollbarWidth + 44 + (((uint) CurrentSelection == i) ? 5 : 0),
                    listStartY + itemOffsetY + listItemSize * (i - menuListFState) + offsetY,
                    1.0f,
                    ((uint) CurrentSelection == i) ? ovrVirtualBoyGo::global.textSelectionColor : ovrVirtualBoyGo::global.textColor,
                    transparency * fadeTransparency);
        } else
            break;
    }
}

template<>
void MenuList<Emulator::Rom>::DrawTexture(DrawHelper &drawHelper, float offsetX, float offsetY, float transparency) {
    // calculate the slider position
    float scale = maxListItems / (float) ItemList->size();
    if (scale > 1) scale = 1;
    GLfloat recHeight = scrollbarHeight * scale;

    GLfloat sliderPercentage = 0;
    if (ItemList->size() > maxListItems)
        sliderPercentage = (menuListState / (float) (ItemList->size() - maxListItems));
    else
        sliderPercentage = 0;

    GLfloat recPosY = (scrollbarHeight - recHeight) * sliderPercentage;

    // slider background
    drawHelper.DrawTexture(ovrVirtualBoyGo::global.textureWhiteId, PosX + offsetX + 2, PosY + 2, scrollbarWidth - 4,
                           scrollbarHeight - 4, ovrVirtualBoyGo::global.MenuBackgroundOverlayColor, transparency);
    // slider
    drawHelper.DrawTexture(ovrVirtualBoyGo::global.textureWhiteId, PosX + offsetX, PosY + recPosY, scrollbarWidth, recHeight, ovrVirtualBoyGo::global.sliderColor,
                           transparency);

    // draw the cartridge icons
    for (uint i = (uint) menuListFState; i < menuListFState + maxListItems; i++) {
        if (i < ItemList->size()) {
            // fading in or out
            float fadeTransparency = 1;
            if (i - menuListFState < 0) {
                fadeTransparency = 1 - (menuListFState - i);
            } else if (i - menuListFState >= maxListItems - 1 && menuListFState != (int) menuListFState) {
                fadeTransparency = menuListFState - (int) menuListFState;
            }

            drawHelper.DrawTexture(ovrVirtualBoyGo::global.textureVbIconId,
                                   PosX + offsetX + scrollbarWidth + 15 - 3 + (((uint) CurrentSelection == i) ? 5 : 0),
                                   listStartY + listItemSize / 2 - 12 + listItemSize * (i - menuListFState) + offsetY, 24, 24,
                                   {1.0f, 1.0f, 1.0f, 1.0f}, transparency * fadeTransparency);
        }
    }
}

using namespace OVR;

void Emulator::InitSettingsMenu(int &posX, int &posY, Menu &settingsMenu) {
    using namespace std::placeholders;

    button_icons[0] = &ovrVirtualBoyGo::global.textureButtonAIconId;
    button_icons[1] = &ovrVirtualBoyGo::global.textureButtonBIconId;
    button_icons[2] = &ovrVirtualBoyGo::global.mappingTriggerRight;
    button_icons[3] = &ovrVirtualBoyGo::global.mappingTriggerLeft;
    button_icons[4] = &ovrVirtualBoyGo::global.mappingRightUpId;
    button_icons[5] = &ovrVirtualBoyGo::global.mappingRightRightId;
    button_icons[6] = &ovrVirtualBoyGo::global.mappingLeftRightId;
    button_icons[7] = &ovrVirtualBoyGo::global.mappingLeftLeftId;
    button_icons[8] = &ovrVirtualBoyGo::global.mappingLeftDownId;
    button_icons[9] = &ovrVirtualBoyGo::global.mappingLeftUpId;
    button_icons[10] = &ovrVirtualBoyGo::global.mappingStartId;
    button_icons[11] = &ovrVirtualBoyGo::global.mappingSelectId;
    button_icons[12] = &ovrVirtualBoyGo::global.mappingRightLeftId;
    button_icons[13] = &ovrVirtualBoyGo::global.mappingRightDownId;

    //MenuButton *curveButton =
    //    new MenuButton(&fontMenu, texturePaletteIconId, "", posX, posY += menuItemSize,
    //                   OnClickCurveScreen, nullptr, nullptr);

    int menuItemSize = ovrVirtualBoyGo::global.menuItemSize;
    screenModeButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.threedeeIconId, "", posX, posY += menuItemSize,
                                                    std::bind(&Emulator::OnClickScreenMode, this, _1),
                                                    std::bind(&Emulator::OnClickScreenMode, this, _1),
                                                    std::bind(&Emulator::OnClickScreenMode, this, _1));

    offsetButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureIpdIconId, "", posX, posY += menuItemSize,
                                                std::bind(&Emulator::OnClickResetOffset, this, _1),
                                                std::bind(&Emulator::OnClickOffsetLeft, this, _1),
                                                std::bind(&Emulator::OnClickOffsetRight, this, _1));

    paletteButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.texturePaletteIconId, "", posX,
                                                 posY += menuItemSize + 5,
                                                 std::bind(&Emulator::OnClickPrefabColorRight, this, _1),
                                                 std::bind(&Emulator::OnClickPrefabColorLeft, this, _1),
                                                 std::bind(&Emulator::OnClickPrefabColorRight, this, _1));

    rButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.texturePaletteIconId, "", posX, posY += menuItemSize,
                                           nullptr, std::bind(&Emulator::OnClickRLeft, this, _1), std::bind(&Emulator::OnClickRRight, this, _1));
    gButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.texturePaletteIconId, "", posX, posY += menuItemSize,
                                           nullptr, std::bind(&Emulator::OnClickGLeft, this, _1), std::bind(&Emulator::OnClickGRight, this, _1));
    bButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.texturePaletteIconId, "", posX, posY += menuItemSize,
                                           nullptr, std::bind(&Emulator::OnClickBLeft, this, _1), std::bind(&Emulator::OnClickBRight, this, _1));

    //settingsMenu.MenuItems.push_back(curveButton);
    settingsMenu.MenuItems.push_back(screenModeButton);
    settingsMenu.MenuItems.push_back(offsetButton);
    settingsMenu.MenuItems.push_back(paletteButton);
    settingsMenu.MenuItems.push_back(rButton);
    settingsMenu.MenuItems.push_back(gButton);
    settingsMenu.MenuItems.push_back(bButton);

    ChangeOffset(offsetButton.get(), 0);
    SetThreeDeeMode(screenModeButton.get(), useThreeDeeMode);
    ChangePalette(paletteButton.get(), 0);
}

void Emulator::OnClickRLeft(MenuItem *item) { ChangeColor((MenuButton *) item, 0, -COLOR_STEP_SIZE); }

void Emulator::OnClickRRight(MenuItem *item) { ChangeColor((MenuButton *) item, 0, COLOR_STEP_SIZE); }

void Emulator::OnClickGLeft(MenuItem *item) { ChangeColor((MenuButton *) item, 1, -COLOR_STEP_SIZE); }

void Emulator::OnClickGRight(MenuItem *item) { ChangeColor((MenuButton *) item, 1, COLOR_STEP_SIZE); }

void Emulator::OnClickBLeft(MenuItem *item) { ChangeColor((MenuButton *) item, 2, -COLOR_STEP_SIZE); }

void Emulator::OnClickBRight(MenuItem *item) { ChangeColor((MenuButton *) item, 2, COLOR_STEP_SIZE); }

void Emulator::ChangeColor(MenuButton *item, int colorIndex, float dir) {
    color[colorIndex] += dir;

    if (color[colorIndex] < 0)
        color[colorIndex] = 0;
    else if (color[colorIndex] > 1)
        color[colorIndex] = 1;

    item->Text = strColor[colorIndex] + ToString(color[colorIndex]);

    // update screen
    if (currentScreenData)
        UpdateScreen(currentScreenData);
    // update save slot color
    UpdateStateImage(ovrVirtualBoyGo::global.saveSlot);
}

void Emulator::InitRomSelectionMenu(int posX, int posY, Menu &romSelectionMenu) {
    // rom list
    romList = std::make_shared<MenuList<Rom>>(&ovrVirtualBoyGo::global.fontList, std::bind(&Emulator::OnClickRom, this, std::placeholders::_1),
                                              &romFileList, 10, HEADER_HEIGHT + 10, MENU_WIDTH - 20, (MENU_HEIGHT - HEADER_HEIGHT - BOTTOM_HEIGHT - 20));

    if (romSelection < 0 || romSelection >= romList->ItemList->size())
        romSelection = 0;

    romList->CurrentSelection = romSelection;
    romSelectionMenu.MenuItems.push_back(romList);
}

void Emulator::Free() {
    VRVB::unload_game();

    vrapi_DestroyTextureSwapChain(CylinderSwapChain);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &screenTextureId);
    glDeleteTextures(1, &stateImageId);
}

void Emulator::Init(std::string appFolderPath, LayerBuilder *_layerBuilder, DrawHelper *_drawHelper, OpenSLWrapper *openSLWrap) {
    stateFolderPath = appFolderPath + stateFilePath;

    layerBuilder = _layerBuilder;
    drawHelper = _drawHelper;

    openSlWrap = openSLWrap;

    romFileList.clear();

    // set the button mapping
    ResetButtonMapping();

    OVR_LOG("VRVB INIT w %i, %i, %i, %i", CylinderWidth, CylinderHeight, VIDEO_WIDTH, VIDEO_HEIGHT);
    // emu screen layer
    // left layer
    int cubeSizeX = CylinderWidth;
    int cubeSizeY = CylinderWidth;

    screenPosY = CylinderWidth / 2 - CylinderHeight / 2;
    OVR_LOG("screePosY %i", screenPosY);

    for (int y = 0; y < cubeSizeY; ++y) {
        for (int x = 0; x < cubeSizeX; ++x) {
            pixelData[x + y * cubeSizeX] = 0xFFFF00FF;
        }
    }
    GLfloat borderColor[] = {1.0f, 0.0f, 0.0f, 1.0f};

    glGenTextures(1, &screenTextureId);
    glBindTexture(GL_TEXTURE_2D, screenTextureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VIDEO_WIDTH, VIDEO_HEIGHT * 2 + screenborder * 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindTexture(GL_TEXTURE_2D, 0);

    {
        int borderSize = screenborder;
        // left texture
        CylinderSwapChain =
                vrapi_CreateTextureSwapChain(VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888_sRGB, CylinderWidth * 2 + borderSize * 2,
                                             TextureHeight * 2 + borderSize * 2, 1, false);
        screenTextureCylinderId = vrapi_GetTextureSwapChainHandle(CylinderSwapChain, 0);

        glBindTexture(GL_TEXTURE_2D, screenTextureCylinderId);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CylinderWidth * 2 + borderSize * 2, TextureHeight * 2 + borderSize * 2, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        //glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glBindTexture(GL_TEXTURE_2D, 0);

        // create the framebuffer for the screen texture
        glGenFramebuffers(1, &screenFramebuffer[0]);
        glBindFramebuffer(GL_FRAMEBUFFER, screenFramebuffer[0]);
        // Set "renderedTexture" as our colour attachement #0
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTextureCylinderId, 0);
        // Set the list of draw buffers.
        GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, DrawBuffers);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    OVR_LOG("INIT VRVB");
    //VRVB::Reset();
    VRVB::Init();

    VRVB::audio_cb = std::bind(&Emulator::VB_Audio_CB, this, std::placeholders::_1, std::placeholders::_2);
    VRVB::video_cb = std::bind(&Emulator::VB_VIDEO_CB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    InitStateImage();
    currentGame = new LoadedGame();
    for (int i = 0; i < 10; ++i) {
        currentGame->saveStates[i].saveImage = new uint8_t[VIDEO_WIDTH * 2 * VIDEO_HEIGHT]();
    }

    Vector3f size(5.25f, 5.25f * (VIDEO_HEIGHT / (float) VIDEO_WIDTH), 0.0f);

    SceneScreenBounds = Bounds3f(size * -0.5f, size * 0.5f);
    SceneScreenBounds.Translate(Vector3f(0.0f, 1.66f, -5.61f));
}

void Emulator::InitStateImage() {
    glGenTextures(1, &stateImageId);
    glBindTexture(GL_TEXTURE_2D, stateImageId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VIDEO_WIDTH, VIDEO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Emulator::UpdateStateImage(int saveSlot) {
    glBindTexture(GL_TEXTURE_2D, stateImageId);

    uint8_t *dataArray = currentGame->saveStates[saveSlot].saveImage;
    for (int y = 0; y < VIDEO_HEIGHT; ++y) {
        for (int x = 0; x < VIDEO_WIDTH; ++x) {
            uint8_t das = dataArray[x + y * VIDEO_WIDTH];
            stateImageData[x + y * VIDEO_WIDTH] =
                    0xFF000000 | ((int) (das * color[2]) << 16) | ((int) (das * color[1]) << 8) | (int) (das * color[0]);
        }
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, stateImageData);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Emulator::AudioFrame(unsigned short *audio, int32_t sampleCount) {
    if (!audioInit) {
        audioInit = true;
        openSlWrap->StartPlaying();
    }

    openSlWrap->SetBuffer(audio, (unsigned) (sampleCount * 2));
    // 52602
    // 877
    // OVR_LOG("VRVB audio size: %i", sampleCount);
}

void Emulator::VB_Audio_CB(int16_t *SoundBuf, int32_t SoundBufSize) {
    AudioFrame((unsigned short *) SoundBuf, SoundBufSize);
}

void Emulator::VB_VIDEO_CB(const void *data, unsigned width, unsigned height) {
    // OVR_LOG("VRVB width: %i, height: %i, %i", width, height, (((int8_t *) data)[5])); // 144 + 31 * 384
    // update the screen texture with the newly received image
    currentScreenData = data;
    UpdateScreen(data);
}

bool Emulator::StateExists(int slot) {
    std::string savePath = stateFolderPath + CurrentRom->RomName + ".state";
    if (slot > 0) savePath += ToString(slot);
    struct stat buffer;
    return (stat(savePath.c_str(), &buffer) == 0);
}

void Emulator::SaveStateImage(int slot) {
    std::string savePath = stateFolderPath + CurrentRom->RomName + ".stateimg";
    if (slot > 0) savePath += ToString(slot);

    OVR_LOG("save image of slot to %s", savePath.c_str());
    std::ofstream outfile(savePath, std::ios::trunc | std::ios::binary);
    outfile.write((const char *) currentGame->saveStates[slot].saveImage,
                  sizeof(uint8_t) * VIDEO_WIDTH * VIDEO_HEIGHT);
    outfile.close();
    OVR_LOG("finished writing save image to file");
}

bool Emulator::LoadStateImage(int slot) {
    std::string savePath = stateFolderPath + CurrentRom->RomName + ".stateimg";
    if (slot > 0) savePath += ToString(slot);

    std::ifstream file(savePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        uint8_t *data = new uint8_t[VIDEO_WIDTH * VIDEO_HEIGHT];
        file.seekg(0, std::ios::beg);
        file.read((char *) data, sizeof(uint8_t) * VIDEO_WIDTH * VIDEO_HEIGHT);
        file.close();

        memcpy(currentGame->saveStates[slot].saveImage, data,
               sizeof(uint8_t) * VIDEO_WIDTH * VIDEO_HEIGHT);

        delete[] data;

        OVR_LOG("loaded image file: %s", savePath.c_str());

        return true;
    }

    OVR_LOG("could not load image file: %s", savePath.c_str());
    return false;
}

void Emulator::LoadGame(Rom *rom) {
    // save the ram of the old rom
    SaveRam();

    OVR_LOG("LOAD VRVB ROM %s", rom->FullPath.c_str());
    std::ifstream file(rom->FullPath, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        long romBufferSize = file.tellg();
        char *memblock = new char[romBufferSize];

        file.seekg(0, std::ios::beg);
        file.read(memblock, romBufferSize);
        file.close();

        VRVB::LoadRom((const uint8_t *) memblock, (size_t) romBufferSize);

        delete[] memblock;

        CurrentRom = rom;
        OVR_LOG("finished loading rom %ld", romBufferSize);

        OVR_LOG("start loading ram");
        LoadRam();
        OVR_LOG("finished loading ram");
    } else {
        OVR_LOG("could not load VB rom file");
    }

    for (int i = 0; i < 10; ++i) {
        if (!LoadStateImage(i)) {
            currentGame->saveStates[i].hasImage = false;

            // clear memory
            memset(currentGame->saveStates[i].saveImage, 0, sizeof(uint8_t) * VIDEO_WIDTH * 2 * VIDEO_HEIGHT);
        } else {
            currentGame->saveStates[i].hasImage = true;
        }

        currentGame->saveStates[i].hasState = StateExists(i);
    }

    UpdateStateImage(0);

    OVR_LOG("LOADED VRVB ROM");
}

void Emulator::UpdateEmptySlotLabel(MenuItem *item, uint *buttonState, uint *lastButtonState) {
    item->Visible = !currentGame->saveStates[ovrVirtualBoyGo::global.saveSlot].hasState;
}

void Emulator::UpdateNoImageSlotLabel(MenuItem *item, uint *buttonState, uint *lastButtonState) {
    item->Visible = currentGame->saveStates[ovrVirtualBoyGo::global.saveSlot].hasState && !currentGame->saveStates[ovrVirtualBoyGo::global.saveSlot].hasImage;
}

void Emulator::InitMainMenu(int posX, int posY, Menu &mainMenu) {
    using namespace std::placeholders;

    int offsetY = 30;

    std::shared_ptr<MenuLabel> labelEmptySlot, labelNoImage;
    std::shared_ptr<MenuImage> imageSlot, imageSlotBackground;

    // main menu
    imageSlotBackground = std::make_unique<MenuImage>(ovrVirtualBoyGo::global.textureWhiteId, MENU_WIDTH - VIDEO_WIDTH - 20 - 5,
                                                      HEADER_HEIGHT + offsetY - 5, VIDEO_WIDTH + 10,
                                                      VIDEO_HEIGHT + 10, ovrVirtualBoyGo::global.MenuBackgroundOverlayColor);

    labelEmptySlot = std::make_unique<MenuLabel>(&ovrVirtualBoyGo::global.fontSlot, "- Empty Slot -", MENU_WIDTH - VIDEO_WIDTH - 20,
                                                 HEADER_HEIGHT + offsetY, VIDEO_WIDTH, VIDEO_HEIGHT, ovrVector4f{1.0f, 1.0f, 1.0f, 1.0f});
    labelEmptySlot->UpdateFunction = std::bind(&Emulator::UpdateEmptySlotLabel, this, _1, _2, _3);

    labelNoImage = std::make_unique<MenuLabel>(&ovrVirtualBoyGo::global.fontSlot, "- -", MENU_WIDTH - VIDEO_WIDTH - 20, HEADER_HEIGHT + offsetY, VIDEO_WIDTH,
                                               VIDEO_HEIGHT, ovrVector4f{1.0f, 1.0f, 1.0f, 1.0f});
    labelNoImage->UpdateFunction = std::bind(&Emulator::UpdateNoImageSlotLabel, this, _1, _2, _3);

    // image slot
    imageSlot = std::make_unique<MenuImage>(stateImageId, MENU_WIDTH - VIDEO_WIDTH - 20, HEADER_HEIGHT + offsetY, VIDEO_WIDTH, VIDEO_HEIGHT,
                                            ovrVector4f{1.0f, 1.0f, 1.0f, 1.0f});

    mainMenu.MenuItems.push_back(imageSlotBackground);
    mainMenu.MenuItems.push_back(labelEmptySlot);
    mainMenu.MenuItems.push_back(labelNoImage);
    mainMenu.MenuItems.push_back(imageSlot);
}

void Emulator::ChangeOffset(MenuButton *item, float dir) {
    threedeeIPD += dir;

    if (threedeeIPD < minIPD)
        threedeeIPD = minIPD;
    else if (threedeeIPD > maxIPD)
        threedeeIPD = maxIPD;

    item->Text = "IPD offset: " + ToString(threedeeIPD);
}

void Emulator::ChangePalette(MenuButton *item, float dir) {
    selectedPredefColor += dir;
    if (selectedPredefColor < 0)
        selectedPredefColor = predefColorCount - 1;
    else if (selectedPredefColor >= predefColorCount)
        selectedPredefColor = 0;

    color[0] = predefColors[selectedPredefColor].x;
    color[1] = predefColors[selectedPredefColor].y;
    color[2] = predefColors[selectedPredefColor].z;

    ChangeColor(rButton.get(), 0, 0);
    ChangeColor(gButton.get(), 1, 0);
    ChangeColor(bButton.get(), 2, 0);

    item->Text = "Color Palette: " + ToString(selectedPredefColor);

    // update screen
    if (currentScreenData)
        UpdateScreen(currentScreenData);
    // update save slot color
    UpdateStateImage(ovrVirtualBoyGo::global.saveSlot);
}

void Emulator::SetThreeDeeMode(MenuItem *item, bool newMode) {
    useThreeDeeMode = newMode;
    ((MenuButton *) item)->IconId = useThreeDeeMode ? ovrVirtualBoyGo::global.threedeeIconId : ovrVirtualBoyGo::global.twodeeIconId;
    ((MenuButton *) item)->Text = useThreeDeeMode ? "3D Screen" : "2D Screen";
}

void Emulator::SetCurvedMove(MenuItem *item, bool newMode) {
    useCubeMap = newMode;
    ((MenuButton *) item)->Text = useCubeMap ? "Flat Screen" : "Curved Screen";
}

void Emulator::OnClickCurveScreen(MenuItem *item) { SetCurvedMove(item, !useCubeMap); }

void Emulator::OnClickScreenMode(MenuItem *item) { SetThreeDeeMode(item, !useThreeDeeMode); }

void Emulator::OnClickPrefabColorLeft(MenuItem *item) { ChangePalette((MenuButton *) item, -1); }

void Emulator::OnClickPrefabColorRight(MenuItem *item) { ChangePalette((MenuButton *) item, 1); }

void Emulator::OnClickOffsetLeft(MenuItem *item) { ChangeOffset((MenuButton *) item, -IPD_STEP_SIZE); }

void Emulator::OnClickOffsetRight(MenuItem *item) { ChangeOffset((MenuButton *) item, IPD_STEP_SIZE); }

void Emulator::OnClickResetOffset(MenuItem *item) {
    threedeeIPD = 0;
    ChangeOffset((MenuButton *) item, 0);
}

void Emulator::OnClickRom(Rom *rom) {
    OVR_LOG("LOAD ROM");
    LoadGame(rom);
    OnRomLoaded();
}

void Emulator::SaveEmulatorSettings(std::ofstream *saveFile) {
    saveFile->write(reinterpret_cast<const char *>(&romList->CurrentSelection), sizeof(int));
    saveFile->write(reinterpret_cast<const char *>(&color[0]), sizeof(float));
    saveFile->write(reinterpret_cast<const char *>(&color[1]), sizeof(float));
    saveFile->write(reinterpret_cast<const char *>(&color[2]), sizeof(float));
    saveFile->write(reinterpret_cast<const char *>(&selectedPredefColor), sizeof(int));
    saveFile->write(reinterpret_cast<const char *>(&threedeeIPD), sizeof(float));
    saveFile->write(reinterpret_cast<const char *>(&useThreeDeeMode), sizeof(bool));

    // save button mapping
    for (int i = 0; i < buttonCount; ++i) {
        int device0 = buttonMapping[i].Buttons[0].IsSet ? buttonMapping[i].Buttons[0].InputDevice : -1;
        int device1 = buttonMapping[i].Buttons[1].IsSet ? buttonMapping[i].Buttons[1].InputDevice : -1;

        saveFile->write(reinterpret_cast<const char *>(&device0), sizeof(int));
        saveFile->write(reinterpret_cast<const char *>(&buttonMapping[i].Buttons[0].ButtonIndex), sizeof(int));
        saveFile->write(reinterpret_cast<const char *>(&device1), sizeof(int));
        saveFile->write(reinterpret_cast<const char *>(&buttonMapping[i].Buttons[1].ButtonIndex), sizeof(int));
    }
}

void Emulator::LoadEmulatorSettings(std::ifstream *readFile) {
    readFile->read((char *) &romSelection, sizeof(int));
    readFile->read((char *) &color[0], sizeof(float));
    readFile->read((char *) &color[1], sizeof(float));
    readFile->read((char *) &color[2], sizeof(float));
    readFile->read((char *) &selectedPredefColor, sizeof(int));
    readFile->read((char *) &threedeeIPD, sizeof(float));
    readFile->read((char *) &useThreeDeeMode, sizeof(bool));

    // load button mapping
    for (int i = 0; i < buttonCount; ++i) {
        readFile->read((char *) &buttonMapping[i].Buttons[0].InputDevice, sizeof(int));
        readFile->read((char *) &buttonMapping[i].Buttons[0].ButtonIndex, sizeof(int));
        readFile->read((char *) &buttonMapping[i].Buttons[1].InputDevice, sizeof(int));
        readFile->read((char *) &buttonMapping[i].Buttons[1].ButtonIndex, sizeof(int));

        if (buttonMapping[i].Buttons[0].InputDevice < 0) {
            buttonMapping[i].Buttons[0].IsSet = false;
            buttonMapping[i].Buttons[0].InputDevice = 0;
        }
        if (buttonMapping[i].Buttons[1].InputDevice < 0) {
            buttonMapping[i].Buttons[1].IsSet = false;
            buttonMapping[i].Buttons[1].InputDevice = 0;
        }
    }
}

void Emulator::AddRom(const std::string &strFullPath, const std::string &strFilename) {
    size_t lastIndex = strFilename.find_last_of(".");
    std::string listName = strFilename.substr(0, lastIndex);
    size_t lastIndexSave = (strFullPath).find_last_of(".");
    std::string listNameSave = strFullPath.substr(0, lastIndexSave);

    Rom newRom;
    newRom.RomName = listName;
    newRom.FullPath = strFullPath;
    newRom.FullPathNorm = listNameSave;
    newRom.SavePath = listNameSave + ".srm";

    romFileList.push_back(newRom);

    OVR_LOG("add rom: %s %s %s", newRom.RomName.c_str(), newRom.FullPath.c_str(),
            newRom.SavePath.c_str());
}

// sort the roms by name
bool Emulator::SortByRomName(const Rom &first, const Rom &second) {
    return first.RomName < second.RomName;
}

void Emulator::SortRomList() {
    OVR_LOG("sort list");
    std::sort(romFileList.begin(), romFileList.end(), SortByRomName);
    OVR_LOG("finished sorting list");
}

void Emulator::ResetGame() {
    VRVB::Reset();
}

void Emulator::SaveRam() {
    if (CurrentRom != nullptr && VRVB::save_ram_size() > 0) {
        OVR_LOG("save ram %i", (int) VRVB::save_ram_size());
        std::ofstream outfile(CurrentRom->SavePath, std::ios::trunc | std::ios::binary);
        outfile.write((const char *) VRVB::save_ram(), VRVB::save_ram_size());
        outfile.close();
        OVR_LOG("finished writing ram file");
    }
}

void Emulator::LoadRam() {
    std::ifstream file(CurrentRom->SavePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        long romBufferSize = file.tellg();
        char *memblock = new char[romBufferSize];
        file.seekg(0, std::ios::beg);
        file.read(memblock, romBufferSize);
        file.close();
        OVR_LOG("loaded ram %ld", romBufferSize);

        OVR_LOG("ram size %i", (int) VRVB::save_ram_size());

        if (romBufferSize != (int) VRVB::save_ram_size()) {
            OVR_LOG("ERROR loaded ram size is wrong");
        } else {
            memcpy(VRVB::save_ram(), memblock, VRVB::save_ram_size());
            OVR_LOG("finished loading ram");
        }

        delete[] memblock;
    } else {
        OVR_LOG("could not load ram file: %s", CurrentRom->SavePath.c_str());
    }
}

void Emulator::SaveState(int slot) {
    // get the size of the savestate
    size_t size = VRVB::retro_serialize_size();

    if (size > 0) {
        std::string savePath = stateFolderPath + CurrentRom->RomName + ".state";
        if (ovrVirtualBoyGo::global.saveSlot > 0) savePath += ToString(ovrVirtualBoyGo::global.saveSlot);

        OVR_LOG("save slot");
        void *data = new uint8_t[size];
        VRVB::retro_serialize(data, size);

        OVR_LOG("save slot to %s", savePath.c_str());
        std::ofstream outfile(savePath, std::ios::trunc | std::ios::binary);
        outfile.write((const char *) data, size);
        outfile.close();
        OVR_LOG("finished writing slot to file");
    }

    OVR_LOG("copy image");
    memcpy(currentGame->saveStates[ovrVirtualBoyGo::global.saveSlot].saveImage, screenData,
           sizeof(uint8_t) * VIDEO_WIDTH * VIDEO_HEIGHT);
    OVR_LOG("update image");
    UpdateStateImage(ovrVirtualBoyGo::global.saveSlot);
    // save image for the slot
    SaveStateImage(ovrVirtualBoyGo::global.saveSlot);
    currentGame->saveStates[ovrVirtualBoyGo::global.saveSlot].hasImage = true;
    currentGame->saveStates[ovrVirtualBoyGo::global.saveSlot].hasState = true;
}

void Emulator::LoadState(int slot) {
    std::string savePath = stateFolderPath + CurrentRom->RomName + ".state";
    if (slot > 0) savePath += ToString(slot);

    std::ifstream file(savePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        long size = file.tellg();
        char *data = new char[size];

        file.seekg(0, std::ios::beg);
        file.read(data, size);
        file.close();
        OVR_LOG("loaded slot has size: %ld", size);

        VRVB::retro_unserialize(data, size);

        delete[] data;
    } else {
        OVR_LOG("could not load ram file: %s", CurrentRom->SavePath.c_str());
    }
}

void Emulator::ResetButtonMapping() {
    for (int i = 0; i < buttonCount; ++i) {
        buttonMapping[i].Buttons[0].InputDevice = ButtonMapper::DeviceGamepad;
        buttonMapping[i].Buttons[0].IsSet = true;
        buttonMapping[i].Buttons[1].IsSet = true;
    }

    // gamepad mapping
    buttonMapping[0].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_A;
    buttonMapping[1].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_B;
    buttonMapping[2].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_RShoulder;
    buttonMapping[3].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_LShoulder;
    buttonMapping[4].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_RightStickUp;
    buttonMapping[5].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_RightStickRight;
    buttonMapping[6].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_Right;
    buttonMapping[7].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_Left;
    buttonMapping[8].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_Down;
    buttonMapping[9].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_Up;
    buttonMapping[10].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_Enter;
    buttonMapping[11].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_Back;
    buttonMapping[12].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_RightStickLeft;
    buttonMapping[13].Buttons[0].ButtonIndex = ButtonMapper::EmuButton_RightStickDown;

    // touch controller mapping
    buttonMapping[0].Buttons[1].InputDevice = ButtonMapper::DeviceRightTouch;
    buttonMapping[0].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_A;
    buttonMapping[1].Buttons[1].InputDevice = ButtonMapper::DeviceRightTouch;
    buttonMapping[1].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_B;
    buttonMapping[2].Buttons[1].InputDevice = ButtonMapper::DeviceRightTouch;
    buttonMapping[2].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Trigger;
    buttonMapping[3].Buttons[1].InputDevice = ButtonMapper::DeviceLeftTouch;
    buttonMapping[3].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Trigger;
    buttonMapping[4].Buttons[1].InputDevice = ButtonMapper::DeviceRightTouch;
    buttonMapping[4].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Up;
    buttonMapping[5].Buttons[1].InputDevice = ButtonMapper::DeviceRightTouch;
    buttonMapping[5].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Right;
    buttonMapping[6].Buttons[1].InputDevice = ButtonMapper::DeviceLeftTouch;
    buttonMapping[6].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Right;
    buttonMapping[7].Buttons[1].InputDevice = ButtonMapper::DeviceLeftTouch;
    buttonMapping[7].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Left;
    buttonMapping[8].Buttons[1].InputDevice = ButtonMapper::DeviceLeftTouch;
    buttonMapping[8].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Down;
    buttonMapping[9].Buttons[1].InputDevice = ButtonMapper::DeviceLeftTouch;
    buttonMapping[9].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Up;
    buttonMapping[10].Buttons[1].InputDevice = ButtonMapper::DeviceLeftTouch;
    buttonMapping[10].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_X;
    buttonMapping[11].Buttons[1].InputDevice = ButtonMapper::DeviceLeftTouch;
    buttonMapping[11].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Y;
    buttonMapping[12].Buttons[1].InputDevice = ButtonMapper::DeviceRightTouch;
    buttonMapping[12].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Left;
    buttonMapping[13].Buttons[1].InputDevice = ButtonMapper::DeviceRightTouch;
    buttonMapping[13].Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Down;
}

void Emulator::Update(const OVRFW::ovrApplFrameIn &in, uint *buttonState, uint *lastButtonState) {
    // methode will only get called "emulationSpeed" times a second
    frameCounter += in.DeltaSeconds;
    if (frameCounter < 1 / emulationSpeed) {
        return;
    }
    frameCounter -= 1 / emulationSpeed;

    // TODO
    VRVB::input_buf[0] = 0;

    for (int i = 0; i < buttonCount; ++i)
        for (int x = 0; x < 2; ++x)
            VRVB::input_buf[0] |= (buttonMapping[i].Buttons[x].IsSet && (buttonState[buttonMapping[i].Buttons[x].InputDevice] &
                                                                         ButtonMapper::ButtonMapping[buttonMapping[i].Buttons[x].ButtonIndex])) ? (1 << i) : 0;

    VRVB::Run();
}

// Aspect is width / height
Matrix4f BoundsScreenMatrix(const Bounds3f &bounds, const float movieAspect) {
    const Vector3f size = bounds.b[1] - bounds.b[0];
    const Vector3f center = bounds.b[0] + size * 0.5f;

    const float screenHeight = size.y;
    const float screenWidth = OVR::OVRMath_Max(size.x, size.z);

    float widthScale;
    float heightScale;

    float aspect = (movieAspect == 0.0f) ? 1.0f : movieAspect;

    if (screenWidth / screenHeight >
        aspect) {    // screen is wider than movie, clamp size to height
        heightScale = screenHeight * 0.5f;
        widthScale = heightScale * aspect;
    } else {    // screen is taller than movie, clamp size to width
        widthScale = screenWidth * 0.5f;
        heightScale = widthScale / aspect;
    }

    const float rotateAngle = (size.x > size.z) ? 0.0f : MATH_FLOAT_PI * 0.5f;

    return Matrix4f::Translation(center) *
           Matrix4f::RotationY(rotateAngle) *
           Matrix4f::Scaling(widthScale, heightScale, 1.0f);
}

void Emulator::UpdateScreen(const void *data) {
    screenData = (uint8_t *) data;
    uint8_t *dataArray = (uint8_t *) data;

    for (int y = 0; y < VIDEO_HEIGHT; ++y) {
        for (int x = 0; x < VIDEO_WIDTH; ++x) {
            uint8_t das = dataArray[x + y * VIDEO_WIDTH];
            pixelData[x + y * VIDEO_WIDTH] =
                    0xFF000000 | ((int) (das * color[2]) << 16) | ((int) (das * color[1]) << 8) | (int) (das * color[0]);
        }
    }

    for (int y = 0; y < VIDEO_HEIGHT; ++y) {
        for (int x = 0; x < VIDEO_WIDTH; ++x) {
            uint8_t das = dataArray[x + (y + VIDEO_HEIGHT + 12) * VIDEO_WIDTH];
            pixelData[x + (y + VIDEO_HEIGHT + screenborder * 2) * VIDEO_WIDTH] =
                    0xFF000000 | ((int) (das * color[2]) << 16) | ((int) (das * color[1]) << 8) | (int) (das * color[0]);
        }
    }

    // make the space between the two images transparent
    memset(&pixelData[VIDEO_WIDTH * VIDEO_HEIGHT], 0x00000000, screenborder * 1 * VIDEO_WIDTH * 4);
    memset(&pixelData[VIDEO_WIDTH * VIDEO_HEIGHT + VIDEO_WIDTH], 0x00000000, screenborder * 1 * VIDEO_WIDTH * 4);

    glBindTexture(GL_TEXTURE_2D, screenTextureId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CylinderWidth, TextureHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    // render image to the screen texture
    glBindFramebuffer(GL_FRAMEBUFFER, screenFramebuffer[0]);
    glViewport(0, 0, VIDEO_WIDTH * 2 + screenborder * 2, TextureHeight * 2 + screenborder * 4);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // @HACK: make DrawTexture better
    // 640, 576 is used because it is the full size of the projection set before
    // TODO use 6px border
    drawHelper->DrawTexture(screenTextureId,
                            640 * ((float) screenborder * 2 / (VIDEO_WIDTH * 2 + screenborder * 4)),
                            576 * ((float) screenborder * 2 / (TextureHeight * 2 + screenborder * 4)),
                            640 * ((float) (VIDEO_WIDTH * 2) / (VIDEO_WIDTH * 2 + screenborder * 4)),
                            576 * ((float) (TextureHeight * 2) / (TextureHeight * 2 + screenborder * 4)),
                            {1.0f, 1.0f, 1.0f, 1.0f}, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // TODO whut
    ScreenTexture[0] = GlTexture(screenTextureCylinderId, GL_TEXTURE_2D, VIDEO_WIDTH * 2 + screenborder * 4, TextureHeight * 2 + screenborder * 4);
    ScreenTexture[1] = GlTexture(screenTextureCylinderId, GL_TEXTURE_2D, VIDEO_WIDTH * 2 + screenborder * 4, TextureHeight * 2 + screenborder * 4);
}

void Emulator::DrawScreenLayer(ApplInterface &appl, const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out, const ovrTracking2 &tracking) {
    ovrLayerCylinder2 layer = layerBuilder->BuildGameCylinderLayer3D(
            CylinderSwapChain, CylinderWidth, CylinderHeight, &tracking, ovrVirtualBoyGo::global.followHead,
            !ovrVirtualBoyGo::global.menuOpen && useThreeDeeMode, threedeeIPD, in.IPD);

    layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
    layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

    appl.AddLayerCylinder2(layer);
}
