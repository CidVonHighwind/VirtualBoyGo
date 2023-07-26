#include "Menu.h"
#include <sstream>
#include <dirent.h>
#include <fstream>
#include <OVR_LogUtils.h>
#include <VrApi_SystemUtils.h>
#include <VirtualBoyGo/Src/main.h>

#include "drawHelper.h"
#include "layerBuilder.h"
#include "emulator.h"
#include "Audio/OpenSLWrap.h"
#include "FontMaster.h"
#include "TextureLoader.h"
#include "MenuHelper.h"
#include "Global.h"
#include "ButtonMapping.h"

#define OPEN_MENU_SPEED 0.15f
#define MENU_TRANSITION_SPEED 0.15f
#define MAPPING_OVERLAY_SPEED 0.15f

#define MAX_SAVE_SLOTS 10
#define RotateSpeed  0.5
#define MoveSpeed  0.03125f
#define ZoomSpeed 0.0625f
#define MIN_DISTANCE 0.5f
#define MAX_DISTANCE 10.0f

using namespace OVRFW;

const int batteryColorCount = 5;
const ovrVector4f BatteryColors[] = {{0.745F, 0.114F, 0.176F, 1.0F},
                                     {0.92F,  0.361F, 0.176F, 1.0F},
                                     {0.976F, 0.69F,  0.255F, 1.0F},
                                     {0.545F, 0.769F, 0.247F, 1.0F},
                                     {0.545F, 0.769F, 0.247F, 1.0F},
                                     {0.0F,   0.78F,  0.078F, 1.0F},
                                     {0.0F,   0.78F,  0.078F, 1.0F}};

const std::string strMove[] = {"Follow Head: Yes", "Follow Head: No"};

template<typename T>
static std::string ToString(T value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

void MenuGo::Free() {
    vrapi_DestroyTextureSwapChain(MenuSwapChain);
}

void MenuGo::Init(Emulator *_emulator, LayerBuilder *_layerBuilder, DrawHelper *_drawHelper, FontManager *_fontManager,
                  const ovrJava *_java, jclass *_clsData) {
    emulator = _emulator;
    layerBuilder = _layerBuilder;
    drawHelper = _drawHelper;
    fontManager = _fontManager;

    emulator->OnRomLoaded = std::bind(&MenuGo::ResetMenuState, this);

    java = _java;
    clsData = _clsData;
    getVal = java->Env->GetMethodID(*clsData, "GetBatteryLevel", "()I");
}

void MenuGo::SetUpMenu() {
    OVR_LOG_WITH_TAG("Menu", "Set up Menu");
    int bigGap = 10;
    int smallGap = 5;

    OVR_LOG_WITH_TAG("Menu", "got emulator");

    ovrVirtualBoyGo::global.menuItemSize = (ovrVirtualBoyGo::global.fontMenu.FontSize + 4);

    {
        OVR_LOG_WITH_TAG("Menu", "Set up rom selection menu");
        emulator->InitRomSelectionMenu(0, 0, romSelectionMenu);

        romSelectionMenu.CurrentSelection = 0;

        romSelectionMenu.BackPress = std::bind(&MenuGo::OnBackPressedRomList, this);
        romSelectionMenu.Init();
    }

    {
        OVR_LOG_WITH_TAG("Menu", "Set up bottom bar");
        // TODO: now that the menu button can be mapped to ever button not so sure what to do with this
//        menuHelp = new MenuButton(&fontBottom, buttonMappingMenu.Buttons[0].Button == EmuButton_X ? textureButtonXIconId : textureButtonYIconId,
//                                  "Close Menu", 7, MENU_HEIGHT - BOTTOM_HEIGHT, 0, BOTTOM_HEIGHT, nullptr, nullptr, nullptr);
//        menuHelp->Color = MenuBottomColor;
//        bottomBar.MenuItems.push_back(menuHelp);

        backHelp = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontBottom,
                                                ovrVirtualBoyGo::global.SwappSelectBackButton ? ovrVirtualBoyGo::global.textureButtonAIconId
                                                                                              : ovrVirtualBoyGo::global.textureButtonBIconId,
                                                "Back", emulator->MENU_WIDTH - 210, emulator->MENU_HEIGHT - emulator->BOTTOM_HEIGHT, 0, emulator->BOTTOM_HEIGHT,
                                                nullptr, nullptr, nullptr);
        backHelp->Color = ovrVirtualBoyGo::global.MenuBottomColor;
        bottomBar.MenuItems.push_back(backHelp);

        selectHelp = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontBottom,
                                                  ovrVirtualBoyGo::global.SwappSelectBackButton ? ovrVirtualBoyGo::global.textureButtonBIconId
                                                                                                : ovrVirtualBoyGo::global.textureButtonAIconId,
                                                  "Select", emulator->MENU_WIDTH - 110, emulator->MENU_HEIGHT - emulator->BOTTOM_HEIGHT, 0,
                                                  emulator->BOTTOM_HEIGHT, nullptr,
                                                  nullptr, nullptr);
        selectHelp->Color = ovrVirtualBoyGo::global.MenuBottomColor;
        bottomBar.MenuItems.push_back(selectHelp);

        currentBottomBar = &bottomBar;
    }

    int posX = 20;
    int posY = emulator->HEADER_HEIGHT + 20;

    using namespace std::placeholders;

    // -- main menu page --
    auto buttonResumeGame = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureResumeId,
                                                         "Resume Game",
                                                         posX, posY, std::bind(&MenuGo::OnClickResumGame, this, _1), nullptr, nullptr);
    mainMenu.MenuItems.push_back(buttonResumeGame);

    auto buttonResetGame = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureResetIconId,
                                                        "Reset Game", posX,
                                                        posY += ovrVirtualBoyGo::global.menuItemSize,
                                                        std::bind(&MenuGo::OnClickResetGame, this, _1), nullptr, nullptr);
    mainMenu.MenuItems.push_back(buttonResetGame);
    slotButton = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureSaveSlotIconId, "", posX,
                                              posY += ovrVirtualBoyGo::global.menuItemSize + 10,
                                              std::bind(&MenuGo::OnClickSaveSlotRight, this, _1),
                                              std::bind(&MenuGo::OnClickSaveSlotLeft, this, _1),
                                              std::bind(&MenuGo::OnClickSaveSlotRight, this, _1));
    slotButton->ScrollTimeH = 0.1;
    ChangeSaveSlot(slotButton.get(), 0);
    mainMenu.MenuItems.push_back(slotButton);

    OVR_LOG_WITH_TAG("Menu", "Set up Main Menu");
    emulator->InitMainMenu(posX, posY, mainMenu);


    OVR_LOG_WITH_TAG("Menu", "Set up Main Menu3");
    mainMenu.MenuItems.push_back(std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureSaveIconId, "Save",
                                                              posX, posY += ovrVirtualBoyGo::global.menuItemSize,
                                                              std::bind(&MenuGo::OnClickSaveGame, this, _1), nullptr, nullptr));
    mainMenu.MenuItems.push_back(std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureLoadIconId, "Load",
                                                              posX, posY += ovrVirtualBoyGo::global.menuItemSize,
                                                              std::bind(&MenuGo::OnClickLoadGame, this, _1), nullptr, nullptr));
    mainMenu.MenuItems.push_back(std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureLoadRomIconId, "Load Rom", posX,
                                                              posY += ovrVirtualBoyGo::global.menuItemSize + 10,
                                                              std::bind(&MenuGo::OnClickLoadRomGame, this, _1), nullptr, nullptr));
    mainMenu.MenuItems.push_back(std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureResetViewIconId,
                                                              "Reset View", posX, posY += ovrVirtualBoyGo::global.menuItemSize,
                                                              std::bind(&MenuGo::OnClickResetView, this, _1), nullptr, nullptr));
    mainMenu.MenuItems.push_back(std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureSettingsId,
                                                              "Settings", posX,
                                                              posY += ovrVirtualBoyGo::global.menuItemSize,
                                                              std::bind(&MenuGo::OnClickSettingsGame, this, _1), nullptr, nullptr));
    mainMenu.MenuItems.push_back(std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureExitIconId, "Exit",
                                                              posX, posY += ovrVirtualBoyGo::global.menuItemSize + 10,
                                                              std::bind(&MenuGo::OnClickExit, this, _1), nullptr, nullptr));

    mainMenu.Init();
    OVR_LOG_WITH_TAG("Menu", "Set up Main Menu2");
    mainMenu.BackPress = std::bind(&MenuGo::OnClickBackMainMenu, this);

    // -- settings page --
    OVR_LOG_WITH_TAG("Menu", "Set up Settings Menu");

    posY = emulator->HEADER_HEIGHT + 20;

    buttonSettingsMenuMapping = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureMappingIconId,
                                                             "Menu Button Mapping",
                                                             posX, posY,
                                                             std::bind(&MenuGo::OnClickMenuMappingScreen, this, _1), nullptr, nullptr);
    buttonSettingsEmulatorMapping = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureMappingIconId,
                                                                 "Emulator Button Mapping",
                                                                 posX, posY += ovrVirtualBoyGo::global.menuItemSize,
                                                                 std::bind(&MenuGo::OnClickEmulatorMappingScreen, this, _1), nullptr, nullptr);
    buttonSettingsMoveScreen = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureMoveIconId, "Move Screen", posX,
                                                            posY += ovrVirtualBoyGo::global.menuItemSize,
                                                            std::bind(&MenuGo::OnClickMoveScreen, this, _1), nullptr, nullptr);
    buttonSettingsFollowHead = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureFollowHeadIconId,
                                                            strMove[ovrVirtualBoyGo::global.followHead ? 0 : 1], posX,
                                                            posY += ovrVirtualBoyGo::global.menuItemSize + bigGap,
                                                            std::bind(&MenuGo::OnClickFollowMode, this, _1),
                                                            std::bind(&MenuGo::OnClickFollowMode, this, _1),
                                                            std::bind(&MenuGo::OnClickFollowMode, this, _1));

    settingsMenu.MenuItems.push_back(buttonSettingsMenuMapping);
    settingsMenu.MenuItems.push_back(buttonSettingsEmulatorMapping);
    settingsMenu.MenuItems.push_back(buttonSettingsMoveScreen);
    settingsMenu.MenuItems.push_back(buttonSettingsFollowHead);

    emulator->InitSettingsMenu(posX, posY, settingsMenu);

    settingsMenu.MenuItems.push_back(
            std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureBackIconId, "Save and Back", posX,
                                         posY += ovrVirtualBoyGo::global.menuItemSize + bigGap,
                                         std::bind(&MenuGo::OnClickBackAndSave, this, _1), nullptr, nullptr));

    // version number at the bottom
    settingsMenu.MenuItems.push_back(
            std::make_unique<MenuLabel>(&ovrVirtualBoyGo::global.fontVersion, emulator->STR_VERSION, emulator->MENU_WIDTH - 70,
                                        emulator->MENU_HEIGHT - emulator->BOTTOM_HEIGHT - 50 + 10, 70, 50, ovrVirtualBoyGo::global.textColorVersion));

    settingsMenu.BackPress = std::bind(&MenuGo::OnBackPressedSettings, this);
    settingsMenu.Init();

    // -- menu button mapping --
    posY = emulator->HEADER_HEIGHT + 20;

    auto swapButton = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureLoadRomIconId, "", posX, posY,
                                                   std::bind(&MenuGo::SwapButtonSelectBack, this, _1),
                                                   std::bind(&MenuGo::SwapButtonSelectBack, this, _1),
                                                   std::bind(&MenuGo::SwapButtonSelectBack, this, _1));
    swapButton->UpdateFunction = std::bind(&MenuGo::UpdateButtonMapping, this, _1, _2, _3);
    buttonMenuMapMenu.MenuItems.push_back(swapButton);

    posY += ovrVirtualBoyGo::global.menuItemSize;

    // buttons
    auto menuMappingButton = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureLoadRomIconId, "menu mapped to:",
                                                          posX,
                                                          posY,
                                                          nullptr, nullptr, nullptr);
    menuMappingButton->Selectable = false;
    buttonMenuMapMenu.MenuItems.push_back(menuMappingButton);

    // first button
    auto menuMappingButton0 = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, 0, "", 250, posY,
                                                           (emulator->MENU_WIDTH - 250 - 20) / 2, ovrVirtualBoyGo::global.menuItemSize,
                                                           std::bind(&MenuGo::OnClickChangeMenuButtonEnter, this, _1), nullptr,
                                                           std::bind(&MenuGo::OnClickMenuMappingButtonRight, this, _1));
    menuMappingButton0->Tag = 0;
    SetMappingText(menuMappingButton0.get(), &buttonMappingMenu.Buttons[0]);
    buttonMenuMapMenu.MenuItems.push_back(menuMappingButton0);

    // second button
    auto menuMappingButton1 = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, 0, "",
                                                           250 + (emulator->MENU_WIDTH - 250 - 20) / 2, posY,
                                                           (emulator->MENU_WIDTH - 250 - 20) / 2, ovrVirtualBoyGo::global.menuItemSize,
                                                           std::bind(&MenuGo::OnClickChangeMenuButtonEnter, this, _1),
                                                           std::bind(&MenuGo::OnClickMenuMappingButtonLeft, this, _1), nullptr);
    menuMappingButton1->Tag = 1;
    SetMappingText(menuMappingButton1.get(), &buttonMappingMenu.Buttons[1]);
    menuMappingButton1->OnSelectFunction = std::bind(&MenuGo::OnMenuMappingButtonSelect, this, _1, _2);
    buttonMenuMapMenu.MenuItems.push_back(menuMappingButton1);

    SwapButtonSelectBack(swapButton.get());
    SwapButtonSelectBack(swapButton.get());

    buttonMenuMapMenu.MenuItems.push_back(
            std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureBackIconId, "Back", posX,
                                         posY += ovrVirtualBoyGo::global.menuItemSize + bigGap,
                                         std::bind(&MenuGo::OnClickBackMove, this, _1), nullptr, nullptr));
    buttonMenuMapMenu.BackPress = std::bind(&MenuGo::OnBackPressedMove, this);
    buttonMenuMapMenu.Init();


    // -- emulator button mapping --
    posY = emulator->HEADER_HEIGHT + 10;

    for (int i = 0; i < emulator->buttonCount; ++i) {
        OVR_LOG_WITH_TAG("Menu", "Set up mapping for %i", i);

//            MenuLabel *newButtonLabel = new MenuLabel(&fontMenu, "A Button",
//                                                      posX, posY, 150, menuItemSize,
//                                                      {0.9f, 0.9f, 0.9f, 0.9f});

        // image of the button
        auto newButtonImage = std::make_shared<MenuImage>(*emulator->button_icons[emulator->buttonOrder[i]],
                                                          80 / 2 - ovrVirtualBoyGo::global.menuItemSize / 2, posY, ovrVirtualBoyGo::global.menuItemSize,
                                                          ovrVirtualBoyGo::global.menuItemSize, ovrVector4f{0.9F, 0.9F, 0.9F, 0.9F});
        buttonEmulatorMapMenu.MenuItems.push_back(newButtonImage);

        int mappingButtonWidth = (emulator->MENU_WIDTH - 80 - 20) / 2;

        auto newButtonLeft = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, 0, "abc", 80, posY, mappingButtonWidth,
                                                          ovrVirtualBoyGo::global.menuItemSize,
                                                          std::bind(&MenuGo::OnClickChangeButtonMappingEnter, this, _1), nullptr,
                                                          std::bind(&MenuGo::OnClickMappingButtonRight, this, _1));

        // left button
        // @hack: this is only done because the menu currently only really supports lists
        if (i != 0)
            newButtonLeft->OnSelectFunction = std::bind(&MenuGo::OnMappingButtonSelect, this, _1, _2);
        newButtonLeft->Tag = i;
        newButtonLeft->Tag2 = 0;
        UpdateButtonMappingText(newButtonLeft.get());
        if (i == 0)
            newButtonLeft->UpdateFunction = std::bind(&MenuGo::UpdateButtonMapping, this, _1, _2, _3);

        // right button
        auto newButtonRight = std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, 0, "abc", 80 + mappingButtonWidth, posY, mappingButtonWidth,
                                                           ovrVirtualBoyGo::global.menuItemSize,
                                                           std::bind(&MenuGo::OnClickChangeButtonMappingEnter, this, _1),
                                                           std::bind(&MenuGo::OnClickMappingButtonLeft, this, _1), nullptr);
        // @hack: this is only done because the menu currently only really supports lists
        newButtonRight->OnSelectFunction = std::bind(&MenuGo::OnMappingButtonSelect, this, _1, _2);
        newButtonRight->Tag = i;
        newButtonRight->Tag2 = 1;
        UpdateButtonMappingText(newButtonRight.get());

        buttonMapping.push_back(newButtonLeft);
        buttonMapping.push_back(newButtonRight);

        posY += ovrVirtualBoyGo::global.menuItemSize;
    }

    // select the first element
    buttonEmulatorMapMenu.CurrentSelection = (int) buttonEmulatorMapMenu.MenuItems.size();

    // button mapping page
    for (int i = 0; i < emulator->buttonCount * 2; ++i)
        buttonEmulatorMapMenu.MenuItems.push_back(buttonMapping.at(i));

    buttonEmulatorMapMenu.MenuItems.push_back(
            std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureResetViewIconId, "Reset Mapping", posX,
                                         posY += bigGap,
                                         std::bind(&MenuGo::OnClickResetEmulatorMapping, this, _1), nullptr, nullptr));
    buttonEmulatorMapMenu.MenuItems.push_back(
            std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureBackIconId, "Back", posX,
                                         posY += ovrVirtualBoyGo::global.menuItemSize - 3, std::bind(&MenuGo::OnClickBackMove, this, _1), nullptr, nullptr));
    buttonEmulatorMapMenu.BackPress = std::bind(&MenuGo::OnBackPressedMove, this);
    buttonEmulatorMapMenu.Init();


    // -- button mapping overlay --
    buttonMappingOverlay.MenuItems.push_back(
            std::make_shared<MenuImage>(ovrVirtualBoyGo::global.textureWhiteId, 0, 0, emulator->MENU_WIDTH, emulator->MENU_HEIGHT,
                                        ovrVector4f{0.0F, 0.0F, 0.0F, 0.8F}));
    int overlayWidth = 250;
    int overlayHeight = 80;
    int margin = 15;
    buttonMappingOverlay.MenuItems.push_back(
            std::make_shared<MenuImage>(ovrVirtualBoyGo::global.textureWhiteId, emulator->MENU_WIDTH / 2 - overlayWidth / 2,
                                        emulator->MENU_HEIGHT / 2 - overlayHeight / 2 - margin, overlayWidth,
                                        overlayHeight + margin * 2, ovrVector4f{0.05F, 0.05F, 0.05F, 0.3F}));

    mappingButtonLabel = std::make_unique<MenuLabel>(&ovrVirtualBoyGo::global.fontMenu, "A Button", emulator->MENU_WIDTH / 2 - overlayWidth / 2,
                                                     emulator->MENU_HEIGHT / 2 - overlayHeight / 2, overlayWidth, overlayHeight / 3,
                                                     ovrVector4f{0.9F, 0.9F, 0.9F, 0.9F});
    possibleMappingLabel = std::make_unique<MenuLabel>(&ovrVirtualBoyGo::global.fontMenu, "(A, B, X, Y)", emulator->MENU_WIDTH / 2 - overlayWidth / 2,
                                                       emulator->MENU_HEIGHT / 2 + overlayHeight / 2 - overlayHeight / 3, overlayWidth, overlayHeight / 3,
                                                       ovrVector4f{0.9F, 0.9F, 0.9F, 0.9F});
    pressButtonLabel = std::make_unique<MenuLabel>(&ovrVirtualBoyGo::global.fontMenu, "Press Button", emulator->MENU_WIDTH / 2 - overlayWidth / 2,
                                                   emulator->MENU_HEIGHT / 2 - overlayHeight / 2 + overlayHeight / 3,
                                                   overlayWidth, overlayHeight / 3, ovrVector4f{0.9F, 0.9F, 0.9F, 0.9F});

    buttonMappingOverlay.MenuItems.push_back(mappingButtonLabel);
    buttonMappingOverlay.MenuItems.push_back(pressButtonLabel);
    buttonMappingOverlay.MenuItems.push_back(possibleMappingLabel);

    // -- move menu page --
    posY = emulator->HEADER_HEIGHT + 20;
    yawButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.texuterLeftRightIconId, "", posX, posY,
                                             std::bind(&MenuGo::OnClickYaw, this, _1),
                                             std::bind(&MenuGo::OnClickMoveScreenYawLeft, this, _1),
                                             std::bind(&MenuGo::OnClickMoveScreenYawRight, this, _1));
    yawButton->ScrollTimeH = 0.025;
    pitchButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureUpDownIconId, "", posX,
                                               posY += ovrVirtualBoyGo::global.menuItemSize,
                                               std::bind(&MenuGo::OnClickPitch, this, _1),
                                               std::bind(&MenuGo::OnClickMoveScreenPitchLeft, this, _1),
                                               std::bind(&MenuGo::OnClickMoveScreenPitchRight, this, _1));
    pitchButton->ScrollTimeH = 0.025;
    rollButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureResetIconId, "", posX,
                                              posY += ovrVirtualBoyGo::global.menuItemSize,
                                              std::bind(&MenuGo::OnClickRoll, this, _1),
                                              std::bind(&MenuGo::OnClickMoveScreenRollLeft, this, _1),
                                              std::bind(&MenuGo::OnClickMoveScreenRollRight, this, _1));
    rollButton->ScrollTimeH = 0.025;
    distanceButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureDistanceIconId, "", posX,
                                                  posY += ovrVirtualBoyGo::global.menuItemSize,
                                                  std::bind(&MenuGo::OnClickDistance, this, _1),
                                                  std::bind(&MenuGo::OnClickMoveScreenDistanceLeft, this, _1),
                                                  std::bind(&MenuGo::OnClickMoveScreenDistanceRight, this, _1));
    distanceButton->ScrollTimeH = 0.025;
    scaleButton = std::make_unique<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureScaleIconId, "", posX,
                                               posY += ovrVirtualBoyGo::global.menuItemSize,
                                               std::bind(&MenuGo::OnClickScale, this, _1),
                                               std::bind(&MenuGo::OnClickMoveScreenScaleLeft, this, _1),
                                               std::bind(&MenuGo::OnClickMoveScreenScaleRight, this, _1));
    scaleButton->ScrollTimeH = 0.025;

    moveMenu.MenuItems.push_back(yawButton);
    moveMenu.MenuItems.push_back(pitchButton);
    moveMenu.MenuItems.push_back(rollButton);
    moveMenu.MenuItems.push_back(distanceButton);
    moveMenu.MenuItems.push_back(scaleButton);

    moveMenu.MenuItems.push_back(
            std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureResetViewIconId, "Reset View", posX,
                                         posY += ovrVirtualBoyGo::global.menuItemSize + smallGap,
                                         std::bind(&MenuGo::OnClickResetViewSettings, this, _1), nullptr, nullptr));
    moveMenu.MenuItems.push_back(std::make_shared<MenuButton>(&ovrVirtualBoyGo::global.fontMenu, ovrVirtualBoyGo::global.textureBackIconId, "Back", posX,
                                                              posY += ovrVirtualBoyGo::global.menuItemSize + bigGap,
                                                              std::bind(&MenuGo::OnClickBackMove, this, _1), nullptr, nullptr));

    moveMenu.BackPress = std::bind(&MenuGo::OnBackPressedMove, this);
    moveMenu.Init();

    currentMenu = &romSelectionMenu;

    // updates the visible values
    MoveYaw(yawButton.get(), 0);
    MovePitch(pitchButton.get(), 0);
    MoveRoll(rollButton.get(), 0);
    ChangeDistance(distanceButton.get(), 0);
    ChangeScale(scaleButton.get(), 0);
}

void MenuGo::StartTransition(Menu *next, int dir) {
    if (isTransitioning) return;
    isTransitioning = true;
    transitionMoveDir = dir;
    nextMenu = next;
}

void MenuGo::OnClickResumGame(MenuItem *item) {
    OVR_LOG_WITH_TAG("Menu", "Pressed RESUME GAME");
    if (loadedRom) ovrVirtualBoyGo::global.menuOpen = false;
}

void MenuGo::OnClickResetGame(MenuItem *item) {
    OVR_LOG_WITH_TAG("Menu", "RESET GAME");
    if (loadedRom) {
        emulator->ResetGame();
        ovrVirtualBoyGo::global.menuOpen = false;
    }
}

void MenuGo::OnClickSaveGame(MenuItem *item) {
    OVR_LOG_WITH_TAG("Menu", "on click save game");
    if (loadedRom) {
        emulator->SaveState(ovrVirtualBoyGo::global.saveSlot);
        ovrVirtualBoyGo::global.menuOpen = false;
    }
}

void MenuGo::OnClickLoadGame(MenuItem *item) {
    if (loadedRom) {
        emulator->LoadState(ovrVirtualBoyGo::global.saveSlot);
        ovrVirtualBoyGo::global.menuOpen = false;
    }
}

void MenuGo::OnClickLoadRomGame(MenuItem *item) { StartTransition(&romSelectionMenu, -1); }

void MenuGo::OnClickSettingsGame(MenuItem *item) { StartTransition(&settingsMenu, 1); }

void MenuGo::OnClickResetView(MenuItem *item) { resetView = true; }

void MenuGo::OnClickBackAndSave(MenuItem *item) {
    StartTransition(&mainMenu, -1);
    MenuGo::SaveSettings();
}

void MenuGo::OnBackPressedRomList() { StartTransition(&mainMenu, 1); }

void MenuGo::OnBackPressedSettings() {
    StartTransition(&mainMenu, -1);
    MenuGo::SaveSettings();
}

void MenuGo::OnClickBackMove(MenuItem *item) {
    StartTransition(&settingsMenu, -1);
}

void MenuGo::ChangeSaveSlot(MenuItem *item, int dir) {
    ovrVirtualBoyGo::global.saveSlot += dir;
    if (ovrVirtualBoyGo::global.saveSlot < 0)
        ovrVirtualBoyGo::global.saveSlot = MAX_SAVE_SLOTS - 1;
    if (ovrVirtualBoyGo::global.saveSlot >= MAX_SAVE_SLOTS)
        ovrVirtualBoyGo::global.saveSlot = 0;

    emulator->UpdateStateImage(ovrVirtualBoyGo::global.saveSlot);
    ((MenuButton *) item)->Text = "Save Slot: " + ToString(ovrVirtualBoyGo::global.saveSlot);
}

void MenuGo::OnClickSaveSlotLeft(MenuItem *item) {
    ChangeSaveSlot(item, -1);
    MenuGo::SaveSettings();
}

void MenuGo::OnClickSaveSlotRight(MenuItem *item) {
    ChangeSaveSlot(item, 1);
    SaveSettings();
}

void MenuGo::SwapButtonSelectBack(MenuItem *item) {
    ovrVirtualBoyGo::global.SwappSelectBackButton = !ovrVirtualBoyGo::global.SwappSelectBackButton;
    ((MenuButton *) item)->Text = "Swap Select and Back: ";
    ((MenuButton *) item)->Text.append((ovrVirtualBoyGo::global.SwappSelectBackButton ? "Yes" : "No"));

    selectHelp->IconId = ovrVirtualBoyGo::global.SwappSelectBackButton ? ovrVirtualBoyGo::global.textureButtonBIconId
                                                                       : ovrVirtualBoyGo::global.textureButtonAIconId;
    backHelp->IconId = ovrVirtualBoyGo::global.SwappSelectBackButton ? ovrVirtualBoyGo::global.textureButtonAIconId
                                                                     : ovrVirtualBoyGo::global.textureButtonBIconId;
}

uint MenuGo::GetPressedButton(uint &_buttonState, uint &_lastButtonState) {
    return _buttonState & ~_lastButtonState;
}

void MenuGo::SetMappingText(MenuButton *Button, ButtonMapper::MappedButton *Mapping) {
    Button->SetText(Mapping->IsSet ? ButtonMapper::MapButtonStr[Mapping->InputDevice * 32 + Mapping->ButtonIndex] : "-");
}

// mapping functions
void MenuGo::UpdateButtonMappingText(MenuItem *item) {
    OVR_LOG_WITH_TAG("Menu", "Update mapping text for %i", item->Tag);

    SetMappingText(((MenuButton *) item), &emulator->buttonMapping[emulator->buttonOrder[item->Tag]].Buttons[item->Tag2]);
}

void MenuGo::UpdateMenuMapping() {
    mappedButton->SetText(ButtonMapper::MapButtonStr[buttonMappingMenu.Buttons[mappedButton->Tag].InputDevice * 32 +
                                                     buttonMappingMenu.Buttons[mappedButton->Tag].ButtonIndex]);
}

void MenuGo::UpdateEmulatorMapping() {
    // emulator->UpdateButtonMapping();
    SetMappingText(mappedButton, remapButton);
}

void MenuGo::OnMenuMappingButtonSelect(MenuItem *item, int direction) {
    buttonMenuMapMenu.MoveSelection(direction, false);
}

void MenuGo::OnClickMenuMappingButtonLeft(MenuItem *item) {
    buttonMenuMapMenu.CurrentSelection--;
}

void MenuGo::OnClickMenuMappingButtonRight(MenuItem *item) {
    buttonMenuMapMenu.CurrentSelection++;
}

void MenuGo::OnMappingButtonSelect(MenuItem *item, int direction) {
    buttonEmulatorMapMenu.MoveSelection(direction, false);
}

void MenuGo::OnClickMappingButtonLeft(MenuItem *item) {
    buttonEmulatorMapMenu.CurrentSelection--;
}

void MenuGo::OnClickMappingButtonRight(MenuItem *item) {
    buttonEmulatorMapMenu.CurrentSelection++;
}

void MenuGo::OnClickChangeMenuButtonEnter(MenuItem *item) {
    UpdateMapping = true;
    UpdateMappingUseTimer = false;
    remapButton = &buttonMappingMenu.Buttons[item->Tag];
    mappedButton = (MenuButton *) item;
    updateMappingText = std::bind(&MenuGo::UpdateMenuMapping, this);

    mappingButtonLabel->SetText("Menu Button");
    possibleMappingLabel->SetText("(A, B, X, Y,...)");
}

void MenuGo::OnClickChangeButtonMappingEnter(MenuItem *item) {
    UpdateMapping = true;
    UpdateMappingUseTimer = true;
    UpdateMappingTimer = 4.0f;

    remapButton = &emulator->buttonMapping[emulator->buttonOrder[item->Tag]].Buttons[item->Tag2];
    mappedButton = buttonMapping.at(item->Tag * 2 + item->Tag2).get();
    updateMappingText = std::bind(&MenuGo::UpdateEmulatorMapping, this);

    mappingButtonLabel->SetText("Button");
    possibleMappingLabel->SetText("(A, B, X, Y,...)");
}

void MenuGo::UpdateButtonMapping(MenuItem *item, uint *_buttonStates, uint *_lastButtonStates) {
    if (!UpdateMapping)
        return;

    for (uint i = 0; i < 3; ++i) {
        // get the buttons that are newly pressed
        uint newButtonState = GetPressedButton(_buttonStates[i], _lastButtonStates[i]);

        if (newButtonState) {
            for (uint j = 0; j < ButtonMapper::EmuButtonCount; ++j) {
                if (newButtonState & ButtonMapper::ButtonMapping[j]) {
                    OVR_LOG_WITH_TAG("Menu", "mapped to %i from device %i", j, i);
                    UpdateMapping = false;
                    remapButton->InputDevice = i;
                    remapButton->ButtonIndex = j;
                    remapButton->IsSet = true;
                    updateMappingText();
                    break;
                }
            }

            break;
        }
    }

    // ignore button press
    for (int i = 0; i < 3; ++i) {
        _buttonStates[i] = 0;
        _lastButtonStates[i] = 0;
    }
}

void MenuGo::OnClickExit(MenuItem *item) {
    emulator->SaveRam();
    showExitDialog = true;
}

void MenuGo::OnClickBackMainMenu() {
    if (loadedRom) ovrVirtualBoyGo::global.menuOpen = false;
}

void MenuGo::OnClickFollowMode(MenuItem *item) {
    ovrVirtualBoyGo::global.followHead = !ovrVirtualBoyGo::global.followHead;
    ((MenuButton *) item)->Text = strMove[ovrVirtualBoyGo::global.followHead ? 0 : 1];
}

void MenuGo::OnClickMoveScreen(MenuItem *item) { StartTransition(&moveMenu, 1); }

void MenuGo::OnClickMenuMappingScreen(MenuItem *item) { StartTransition(&buttonMenuMapMenu, 1); }

void MenuGo::OnClickEmulatorMappingScreen(MenuItem *item) {
    StartTransition(&buttonEmulatorMapMenu, 1);
}

void MenuGo::OnBackPressedMove() {
    StartTransition(&settingsMenu, -1);
}

float ToDegree(float radian) { return (int) (180.0 / VRAPI_PI * radian * 10) / 10.0F; }

void MenuGo::MoveYaw(MenuItem *item, float dir) {
    layerBuilder->screenYaw += dir;
    ((MenuButton *) item)->Text = "Yaw: " + ToString(layerBuilder->screenYaw) + "°";
}

void MenuGo::MovePitch(MenuItem *item, float dir) {
    layerBuilder->screenPitch += dir;
    ((MenuButton *) item)->Text = "Pitch: " + ToString(layerBuilder->screenPitch) + "°";
}

void MenuGo::ChangeDistance(MenuItem *item, float dir) {
    layerBuilder->radiusMenuScreen -= dir;

    if (layerBuilder->radiusMenuScreen < MIN_DISTANCE)
        layerBuilder->radiusMenuScreen = MIN_DISTANCE;
    if (layerBuilder->radiusMenuScreen > MAX_DISTANCE)
        layerBuilder->radiusMenuScreen = MAX_DISTANCE;

    ((MenuButton *) item)->Text = "Distance: " + ToString(layerBuilder->radiusMenuScreen);
}

void MenuGo::ChangeScale(MenuItem *item, float dir) {
    layerBuilder->screenSize -= dir;

    if (layerBuilder->screenSize < 0.25F) layerBuilder->screenSize = 0.25F;
    if (layerBuilder->screenSize > 2.5F) layerBuilder->screenSize = 2.5F;

    ((MenuButton *) item)->Text = "Scale: " + ToString(layerBuilder->screenSize);
}

void MenuGo::MoveRoll(MenuItem *item, float dir) {
    layerBuilder->screenRoll += dir;
    ((MenuButton *) item)->Text = "Roll: " + ToString(layerBuilder->screenRoll) + "°";
}

void MenuGo::OnClickResetEmulatorMapping(MenuItem *item) {
    emulator->ResetButtonMapping();

    for (int i = 0; i < emulator->buttonCount * 2; ++i) {
        UpdateButtonMappingText(buttonMapping.at(i).get());
    }
}

void MenuGo::OnClickResetViewSettings(MenuItem *item) {
    layerBuilder->ResetValues();

    // updates the visible values
    MoveYaw(yawButton.get(), 0);
    MovePitch(pitchButton.get(), 0);
    MoveRoll(rollButton.get(), 0);
    ChangeDistance(distanceButton.get(), 0);
    ChangeScale(scaleButton.get(), 0);
}

void MenuGo::OnClickYaw(MenuItem *item) {
    layerBuilder->screenYaw = 0;
    MoveYaw(yawButton.get(), 0);
}

void MenuGo::OnClickPitch(MenuItem *item) {
    layerBuilder->screenPitch = 0;
    MovePitch(pitchButton.get(), 0);
}

void MenuGo::OnClickRoll(MenuItem *item) {
    layerBuilder->screenRoll = 0;
    MoveRoll(rollButton.get(), 0);
}

void MenuGo::OnClickDistance(MenuItem *item) {
    layerBuilder->radiusMenuScreen = 5.5F;
    ChangeDistance(distanceButton.get(), 0);
}

void MenuGo::OnClickScale(MenuItem *item) {
    layerBuilder->screenSize = 1.0F;
    ChangeScale(scaleButton.get(), 0);
}

void MenuGo::OnClickMoveScreenYawLeft(MenuItem *item) { MoveYaw(item, RotateSpeed); }

void MenuGo::OnClickMoveScreenYawRight(MenuItem *item) { MoveYaw(item, -RotateSpeed); }

void MenuGo::OnClickMoveScreenPitchLeft(MenuItem *item) { MovePitch(item, -RotateSpeed); }

void MenuGo::OnClickMoveScreenPitchRight(MenuItem *item) { MovePitch(item, RotateSpeed); }

void MenuGo::OnClickMoveScreenRollLeft(MenuItem *item) { MoveRoll(item, -RotateSpeed); }

void MenuGo::OnClickMoveScreenRollRight(MenuItem *item) { MoveRoll(item, RotateSpeed); }

void MenuGo::OnClickMoveScreenDistanceLeft(MenuItem *item) { ChangeDistance(item, ZoomSpeed); }

void MenuGo::OnClickMoveScreenDistanceRight(MenuItem *item) {
    ChangeDistance(item, -ZoomSpeed);
}

void MenuGo::OnClickMoveScreenScaleLeft(MenuItem *item) { ChangeScale(item, MoveSpeed); }

void MenuGo::OnClickMoveScreenScaleRight(MenuItem *item) { ChangeScale(item, -MoveSpeed); }

void MenuGo::ResetMenuState() {
    SaveSettings();
    ovrVirtualBoyGo::global.saveSlot = 0;
    ChangeSaveSlot(slotButton.get(), 0);
    currentMenu = &mainMenu;
    ovrVirtualBoyGo::global.menuOpen = false;
    loadedRom = true;
}

int MenuGo::UpdateBatteryLevel() {
    jint bLevel = java->Env->CallIntMethod(java->ActivityObject, getVal);
    int returnValue = (int) bLevel;
    return returnValue;
}

void MenuGo::CreateScreen() {
    // menu layer
    MenuSwapChain = vrapi_CreateTextureSwapChain(VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888_sRGB, emulator->MENU_WIDTH, emulator->MENU_HEIGHT, 1, false);

    textureIdMenu = vrapi_GetTextureSwapChainHandle(MenuSwapChain, 0);
    glBindTexture(GL_TEXTURE_2D, textureIdMenu);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, emulator->MENU_WIDTH, emulator->MENU_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE,
                    nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    GLfloat borderColor[] = {0.0F, 0.0F, 0.0F, 0.0F};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindTexture(GL_TEXTURE_2D, 0);

    // create hte framebuffer for the menu texture
    glGenFramebuffers(1, &MenuFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, MenuFrameBuffer);

    // Set "renderedTexture" as our colour attachement #0
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (GLuint) textureIdMenu, 0);

    // Set the list of draw buffers.
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);  // "1" is the size of DrawBuffers

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    OVR_LOG_WITH_TAG("Menu", "finished creating screens");
}

void MenuGo::SaveSettings() {

    std::ofstream saveFile(ovrVirtualBoyGo::global.saveFilePath, std::ios::trunc | std::ios::binary);
    saveFile.write(reinterpret_cast<const char *>(&emulator->SAVE_FILE_VERSION), sizeof(int));

    emulator->SaveEmulatorSettings(&saveFile);
    saveFile.write(reinterpret_cast<const char *>(&ovrVirtualBoyGo::global.followHead), sizeof(bool));
    saveFile.write(reinterpret_cast<const char *>(&layerBuilder->screenPitch), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&layerBuilder->screenYaw), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&layerBuilder->screenRoll), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&layerBuilder->radiusMenuScreen), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&layerBuilder->screenSize), sizeof(float));
    saveFile.write(reinterpret_cast<const char *>(&buttonMappingMenu.Buttons[0].InputDevice), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&buttonMappingMenu.Buttons[0].ButtonIndex), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&buttonMappingMenu.Buttons[1].InputDevice), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&buttonMappingMenu.Buttons[1].ButtonIndex), sizeof(int));
    saveFile.write(reinterpret_cast<const char *>(&ovrVirtualBoyGo::global.SwappSelectBackButton), sizeof(bool));

    saveFile.close();

    OVR_LOG_WITH_TAG("Menu", "saved settings");
}

void MenuGo::LoadSettings() {
    // default menu buttons
    buttonMappingMenu.Buttons[0].IsSet = true;
    buttonMappingMenu.Buttons[0].InputDevice = ButtonMapper::DeviceGamepad;
    buttonMappingMenu.Buttons[0].ButtonIndex = ButtonMapper::EmuButton_Y;
    // menu button on the left touch controller
    buttonMappingMenu.Buttons[1].IsSet = true;
    buttonMappingMenu.Buttons[1].InputDevice = ButtonMapper::DeviceLeftTouch;
    buttonMappingMenu.Buttons[1].ButtonIndex = ButtonMapper::EmuButton_Enter;

    std::ifstream loadFile(ovrVirtualBoyGo::global.saveFilePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (loadFile.is_open()) {
        loadFile.seekg(0, std::ios::beg);

        int saveFileVersion = 0;
        loadFile.read((char *) &saveFileVersion, sizeof(int));

        // only load if the save versions are compatible
        if (saveFileVersion == emulator->SAVE_FILE_VERSION) {
            emulator->LoadEmulatorSettings(&loadFile);
            loadFile.read((char *) &ovrVirtualBoyGo::global.followHead, sizeof(bool));
            loadFile.read((char *) &layerBuilder->screenPitch, sizeof(float));
            loadFile.read((char *) &layerBuilder->screenYaw, sizeof(float));
            loadFile.read((char *) &layerBuilder->screenRoll, sizeof(float));
            loadFile.read((char *) &layerBuilder->radiusMenuScreen, sizeof(float));
            loadFile.read((char *) &layerBuilder->screenSize, sizeof(float));
            loadFile.read((char *) &buttonMappingMenu.Buttons[0].InputDevice, sizeof(int));
            loadFile.read((char *) &buttonMappingMenu.Buttons[0].ButtonIndex, sizeof(int));
            loadFile.read((char *) &buttonMappingMenu.Buttons[1].InputDevice, sizeof(int));
            loadFile.read((char *) &buttonMappingMenu.Buttons[1].ButtonIndex, sizeof(int));
            loadFile.read((char *) &ovrVirtualBoyGo::global.SwappSelectBackButton, sizeof(bool));
        }

        // TODO: reset all loaded settings
        if (loadFile.fail())
            OVR_LOG_WITH_TAG("Menu", "failed loading settings");
        else
            OVR_LOG_WITH_TAG("Menu", "settings loaded");

        loadFile.close();
    }
}

void MenuGo::ScanDirectory() {
    DIR *dir;
    struct dirent *ent;
    std::string strFullPath;

    if ((dir = opendir(ovrVirtualBoyGo::global.romFolderPath.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            strFullPath = "";
            strFullPath.append(ovrVirtualBoyGo::global.romFolderPath);
            strFullPath.append(ent->d_name);

            if (ent->d_type == 8) {
                std::string strFilename = ent->d_name;

                // check if the filetype is supported by the emulator
                bool supportedFile = false;
                for (const auto &supportedFileName : emulator->supportedFileNames) {
                    if (strFilename.find(supportedFileName) !=
                        std::string::npos) {
                        supportedFile = true;
                        break;
                    }
                }

                if (supportedFile) {
                    emulator->AddRom(strFullPath, strFilename);
                }
            }
        }
        closedir(dir);

        emulator->SortRomList();
    } else {
        OVR_LOG_WITH_TAG("Menu", "could not open folder");
    }

    OVR_LOG_WITH_TAG("Menu", "scanned directory");
}

void MenuGo::SetTimeString(std::string &timeString) {
    struct timespec res{};
    clock_gettime(CLOCK_REALTIME, &res);
    time_t t = res.tv_sec;  // just in case types aren't the same
    tm tmv{};
    localtime_r(&t, &tmv);  // populate tmv with local time info

    timeString.clear();
    if (tmv.tm_hour < 10)
        timeString.append("0");
    timeString.append(ToString(tmv.tm_hour));
    timeString.append(":");
    if (tmv.tm_min < 10)
        timeString.append("0");
    timeString.append(ToString(tmv.tm_min));

    time_string_width = FontManager::GetWidth(ovrVirtualBoyGo::global.fontTime, timeString);
}

void MenuGo::GetBattryString(std::string &batteryString) {
    batteryLevel = UpdateBatteryLevel();
    batteryString.clear();
    batteryString.append(ToString(batteryLevel));
    batteryString.append("%");

    batter_string_width = FontManager::GetWidth(ovrVirtualBoyGo::global.fontBattery, batteryString);
}

void MenuGo::Update(OVRFW::ovrAppl &app, const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out) {
    deltaSeconds = in.DeltaSeconds;

    for (int i = 0; i < 3; ++i)
        lastButtonStates[i] = buttonStatesReal[i];

    // UpdateInput(app);
    UpdateInputDevices(app, in);

    // update button mapping timer
    if (UpdateMapping && UpdateMappingUseTimer) {
        UpdateMappingTimer -= in.DeltaSeconds;
        mappingButtonLabel->SetText(ToString((int) UpdateMappingTimer));

        if ((int) UpdateMappingTimer <= 0) {
            UpdateMapping = false;
            remapButton->IsSet = false;
            updateMappingText();
        }
    }

    if (!ovrVirtualBoyGo::global.menuOpen) {
        if (transitionPercentage > 0)
            transitionPercentage -= deltaSeconds / OPEN_MENU_SPEED;
        if (transitionPercentage < 0)
            transitionPercentage = 0;

        emulator->Update(in, buttonStates, lastButtonStates);
    } else {
        if (transitionPercentage < 1)
            transitionPercentage += deltaSeconds / OPEN_MENU_SPEED;
        if (transitionPercentage > 1)
            transitionPercentage = 1;

        UpdateCurrentMenu(in.DeltaSeconds);
    }

    // open/close menu
    if (loadedRom &&
        ((buttonStates[buttonMappingMenu.Buttons[0].InputDevice] & ButtonMapper::ButtonMapping[buttonMappingMenu.Buttons[0].ButtonIndex] &&
          !(lastButtonStates[buttonMappingMenu.Buttons[0].InputDevice] & ButtonMapper::ButtonMapping[buttonMappingMenu.Buttons[0].ButtonIndex])) ||
         (buttonStates[buttonMappingMenu.Buttons[1].InputDevice] & ButtonMapper::ButtonMapping[buttonMappingMenu.Buttons[1].ButtonIndex] &&
          !(lastButtonStates[buttonMappingMenu.Buttons[1].InputDevice] & ButtonMapper::ButtonMapping[buttonMappingMenu.Buttons[1].ButtonIndex])))) {
        ovrVirtualBoyGo::global.menuOpen = !ovrVirtualBoyGo::global.menuOpen;
    }

    layerBuilder->UpdateDirection(in);

    emulator->DrawScreenLayer(in, out);

    if (transitionPercentage > 0) {
        // menu layer
        if (ovrVirtualBoyGo::global.menuOpen)
            DrawGUI();

        float transitionP = sinf((transitionPercentage) * MATH_FLOAT_PIOVER2);

        out.Layers[out.NumLayers].Cylinder = layerBuilder->BuildSettingsCylinderLayer(
                MenuSwapChain, emulator->MENU_WIDTH, emulator->MENU_HEIGHT, &in.Tracking, ovrVirtualBoyGo::global.followHead, transitionP);

        out.Layers[out.NumLayers].Cylinder.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
        out.Layers[out.NumLayers].Cylinder.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

        out.Layers[out.NumLayers].Cylinder.Header.ColorScale = {transitionP, transitionP, transitionP, transitionP};
        out.Layers[out.NumLayers].Cylinder.Header.SrcBlend = VRAPI_FRAME_LAYER_BLEND_SRC_ALPHA;
        out.Layers[out.NumLayers].Cylinder.Header.DstBlend = VRAPI_FRAME_LAYER_BLEND_ONE_MINUS_SRC_ALPHA;

        out.NumLayers++;
    }

    if (resetView) {
        layerBuilder->ResetForwardDirection(out);
        resetView = false;
    }

    if (showExitDialog) {
        OVR_LOG("Open menu");
        showExitDialog = false;
        vrapi_ShowSystemUI(java, VRAPI_SYS_UI_CONFIRM_QUIT_MENU);
    }
}

void MenuGo::UpdateCurrentMenu(float deltaSeconds) {
    if (isTransitioning) {
        transitionState -= deltaSeconds / MENU_TRANSITION_SPEED;
        if (transitionState < 0) {
            transitionState = 1;
            isTransitioning = false;
            currentMenu = nextMenu;
        }
    } else {
        currentMenu->Update(buttonStates, lastButtonStates, deltaSeconds);
    }
}

void MenuGo::DrawGUI() {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    glBindFramebuffer(GL_FRAMEBUFFER, MenuFrameBuffer);
    // Render on the whole framebuffer, complete from the lower left corner to the
    // upper right
    glViewport(0, 0, emulator->MENU_WIDTH, emulator->MENU_HEIGHT);

    glClearColor(ovrVirtualBoyGo::global.MenuBackgroundColor.x, ovrVirtualBoyGo::global.MenuBackgroundColor.y, ovrVirtualBoyGo::global.MenuBackgroundColor.z,
                 ovrVirtualBoyGo::global.MenuBackgroundColor.w);
    glClear(GL_COLOR_BUFFER_BIT);

    // draw the backgroud image
    //drawHelper->DrawTexture(textureBackgroundId, 0, 0, menuWidth, menuHeight,
    //                        {0.7f,0.7f,0.7f,1.0f}, 0.985f);

    // header
    drawHelper->DrawTexture(ovrVirtualBoyGo::global.textureWhiteId, 0, 0, emulator->MENU_WIDTH, emulator->HEADER_HEIGHT,
                            ovrVirtualBoyGo::global.MenuBackgroundOverlayHeader, 1);
    drawHelper->DrawTexture(ovrVirtualBoyGo::global.textureWhiteId, 0, emulator->MENU_HEIGHT - emulator->BOTTOM_HEIGHT, emulator->MENU_WIDTH,
                            emulator->BOTTOM_HEIGHT,
                            ovrVirtualBoyGo::global.MenuBackgroundOverlayColorLight, 1);

    // icon
    //drawHelper->DrawTexture(textureHeaderIconId, 0, 0, 75, 75, headerTextColor, 1);

    fontManager->Begin();
    fontManager->RenderText(ovrVirtualBoyGo::global.fontHeader, emulator->STR_HEADER, 15,
                            emulator->HEADER_HEIGHT / 2 - ovrVirtualBoyGo::global.fontHeader.PHeight / 2 - ovrVirtualBoyGo::global.fontHeader.PStart, 1.0F,
                            ovrVirtualBoyGo::global.headerTextColor, 1);

    // update the battery string
    int batteryWidth = 10;
    int maxHeight = ovrVirtualBoyGo::global.fontBattery.PHeight + 1;
    int distX = 15;
    int distY = 2;
    int batteryPosY = emulator->HEADER_HEIGHT / 2 + distY + 3;

    // update the time string
    SetTimeString(time_string);
    fontManager->RenderText(ovrVirtualBoyGo::global.fontTime, time_string, emulator->MENU_WIDTH - time_string_width - distX,
                            emulator->HEADER_HEIGHT / 2 - distY - ovrVirtualBoyGo::global.fontBattery.FontSize + ovrVirtualBoyGo::global.fontBattery.PStart,
                            1.0F,
                            ovrVirtualBoyGo::global.textColorBattery, 1);

    GetBattryString(battery_string);
    fontManager->RenderText(ovrVirtualBoyGo::global.fontBattery, battery_string, emulator->MENU_WIDTH - batter_string_width - batteryWidth - 7 - distX,
                            emulator->HEADER_HEIGHT / 2 + distY + 3, 1.0F, ovrVirtualBoyGo::global.textColorBattery, 1);

    fontManager->End();

    // draw battery
    drawHelper->DrawTexture(ovrVirtualBoyGo::global.textureWhiteId, emulator->MENU_WIDTH - batteryWidth - distX - 2 - 2, batteryPosY,
                            batteryWidth + 4, maxHeight + 4, ovrVirtualBoyGo::global.BatteryBackgroundColor, 1);

    // calculate the battery color
    float colorState = ((batteryLevel * 10) % (1000 / (batteryColorCount))) / (float) (1000 / batteryColorCount);
    int currentColor = (int) (batteryLevel / (100.0F / batteryColorCount));
    ovrVector4f batteryColor = ovrVector4f{
            BatteryColors[currentColor].x * (1 - colorState) + BatteryColors[currentColor + 1].x * colorState,
            BatteryColors[currentColor].y * (1 - colorState) + BatteryColors[currentColor + 1].y * colorState,
            BatteryColors[currentColor].z * (1 - colorState) + BatteryColors[currentColor + 1].z * colorState,
            BatteryColors[currentColor].w * (1 - colorState) + BatteryColors[currentColor + 1].w * colorState
    };

    int height = (int) (batteryLevel / 100.0F * maxHeight);

    drawHelper->DrawTexture(ovrVirtualBoyGo::global.textureWhiteId, emulator->MENU_WIDTH - batteryWidth - distX - 2, batteryPosY + 2 + maxHeight - height,
                            batteryWidth, height, batteryColor, 1);

    DrawMenu();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MenuGo::DrawMenu() {
    // the
    float trProgressOut = ((transitionState - 0.35F) / 0.65F);
    float progressOut = sinf(trProgressOut * MATH_FLOAT_PIOVER2);
    if (trProgressOut < 0)
        trProgressOut = 0;

    float trProgressIn = (((1 - transitionState) - 0.35F) / 0.65F);
    float progressIn = sinf(trProgressIn * MATH_FLOAT_PIOVER2);
    if (transitionState < 0)
        trProgressIn = 0;

    int dist = 75;

    // blend the button mapping in/out
    if (UpdateMapping) {
        MappingOverlayPercentage += deltaSeconds / MAPPING_OVERLAY_SPEED;
        if (MappingOverlayPercentage > 1)
            MappingOverlayPercentage = 1;
    } else {
        MappingOverlayPercentage -= deltaSeconds / MAPPING_OVERLAY_SPEED;
        if (MappingOverlayPercentage < 0)
            MappingOverlayPercentage = 0;
    }

    // draw the current menu
    currentMenu->Draw(*drawHelper, *fontManager, -transitionMoveDir, 0, (1 - progressOut), dist, trProgressOut);

    // draw the next menu fading in
    if (isTransitioning)
        nextMenu->Draw(*drawHelper, *fontManager, transitionMoveDir, 0, (1 - progressIn), dist, trProgressIn);

    // draw the bottom bar
    currentBottomBar->Draw(*drawHelper, *fontManager, 0, 0, 0, 0, 1);

    if (MappingOverlayPercentage > 0) {
        buttonMappingOverlay.Draw(*drawHelper, *fontManager, 0, -1, (1 - sinf(MappingOverlayPercentage * MATH_FLOAT_PIOVER2)), dist, MappingOverlayPercentage);
    }

    //    // DEBUG: render the texture of the font with a black background
//    drawHelper->DrawTexture(textureWhiteId, 0, 200, fontMenu.FontSize * 30, fontMenu.FontSize * 8, {0.0f, 0.0f, 0.0f, 1.0f}, 1.0f);
//
//    fontManager->Begin();
//    fontManager->RenderFontTexture(fontMenu, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
//    fontManager->Close();
}

void MenuGo::UpdateInputDevices(OVRFW::ovrAppl &app, const OVRFW::ovrApplFrameIn &in) {
    using namespace ButtonMapper;

    for (int i = 0; i < 3; ++i) {
        buttonStatesReal[i] = 0;
        buttonStates[i] = 0;
    }

    EnumerateInputDevices(app);

    // for each device, query its current tracking state and input state
    // it's possible for a device to be removed during this loop, so we go through it backwards
    for (int i = (int) InputDevices.size() - 1; i >= 0; --i) {
        ovrInputDeviceBase *device = InputDevices[i];
        if (device == nullptr) {
            OVR_ASSERT(false);    // this should never happen!
            continue;
        }

        ovrDeviceID deviceID = device->GetDeviceID();
        if (deviceID == ovrDeviceIdType_Invalid) {
            OVR_ASSERT(deviceID != ovrDeviceIdType_Invalid);
            continue;
        } else if (device->GetType() == ovrControllerType_Gamepad) {

            if (deviceID != ovrDeviceIdType_Invalid) {
                ovrInputStateGamepad gamepadInputState;
                gamepadInputState.Header.ControllerType = ovrControllerType_Gamepad;
                ovrResult result = vrapi_GetCurrentInputState(app.GetSessionObject(), deviceID, &gamepadInputState.Header);

                if (result == ovrSuccess &&
                    gamepadInputState.Header.TimeInSeconds >= LastGamepadUpdateTimeInSeconds) {
                    LastGamepadUpdateTimeInSeconds = gamepadInputState.Header.TimeInSeconds;

                    // not so sure if this is such a good idea
                    // if they change the order of the buttons this will break
                    buttonStatesReal[0] = gamepadInputState.Buttons;

                    buttonStatesReal[0] |= (gamepadInputState.LeftJoystick.x < -0.5f) ? ButtonMapping[EmuButton_LeftStickLeft] : 0;
                    buttonStatesReal[0] |= (gamepadInputState.LeftJoystick.x > 0.5f) ? ButtonMapping[EmuButton_LeftStickRight] : 0;
                    buttonStatesReal[0] |= (gamepadInputState.LeftJoystick.y < -0.5f) ? ButtonMapping[EmuButton_LeftStickUp] : 0;
                    buttonStatesReal[0] |= (gamepadInputState.LeftJoystick.y > 0.5f) ? ButtonMapping[EmuButton_LeftStickDown] : 0;

                    buttonStatesReal[0] |= (gamepadInputState.RightJoystick.x < -0.5f) ? ButtonMapping[EmuButton_RightStickLeft] : 0;
                    buttonStatesReal[0] |= (gamepadInputState.RightJoystick.x > 0.5f) ? ButtonMapping[EmuButton_RightStickRight] : 0;
                    buttonStatesReal[0] |= (gamepadInputState.RightJoystick.y < -0.5f) ? ButtonMapping[EmuButton_RightStickUp] : 0;
                    buttonStatesReal[0] |= (gamepadInputState.RightJoystick.y > 0.5f) ? ButtonMapping[EmuButton_RightStickDown] : 0;

                    buttonStatesReal[0] |= (gamepadInputState.LeftTrigger > 0.25f) ? ButtonMapping[EmuButton_L2] : 0;
                    buttonStatesReal[0] |= (gamepadInputState.RightTrigger > 0.25f) ? ButtonMapping[EmuButton_R2] : 0;
                } else if (result == ovrSuccess) {
                    OVR_LOG("Gamepad Update Time");
                }
            }
        } else if (device->GetType() == ovrControllerType_TrackedRemote) {
            if (deviceID != ovrDeviceIdType_Invalid) {
                ovrInputDevice_TrackedRemote &trDevice = *dynamic_cast< ovrInputDevice_TrackedRemote *>( device );

                ovrTracking remoteTracking;
                ovrResult result = vrapi_GetInputTrackingState(app.GetSessionObject(), deviceID, in.PredictedDisplayTime, &remoteTracking);
                if (result != ovrSuccess) {
                    OnDeviceDisconnected(deviceID);
                    continue;
                }

                trDevice.SetTracking(remoteTracking);

                PopulateRemoteControllerInfo(app, trDevice);
            }
        } else {
            OVR_LOG_WITH_TAG("MLBUState", "Unexpected Device Type %d on %d", device->GetType(), i);
        }

        buttonStates[0] = buttonStatesReal[0];
        buttonStates[1] = buttonStatesReal[1];
        buttonStates[2] = buttonStatesReal[2];
    }
}

ovrResult MenuGo::PopulateRemoteControllerInfo(OVRFW::ovrAppl &app, ovrInputDevice_TrackedRemote &trDevice) {
    using namespace ButtonMapper;

    ovrDeviceID deviceID = trDevice.GetDeviceID();

    ovrInputStateTrackedRemote remoteInputState;
    remoteInputState.Header.ControllerType = trDevice.GetType();

    ovrResult result;
    result = vrapi_GetCurrentInputState(app.GetSessionObject(), deviceID, &remoteInputState.Header);

    if (result != ovrSuccess) {
        OVR_LOG_WITH_TAG("MLBUState", "ERROR %i getting remote input state!", result);
        OnDeviceDisconnected(deviceID);
        return result;
    }

    const auto *inputTrackedRemoteCapabilities = reinterpret_cast<const ovrInputGamepadCapabilities *>( trDevice.GetCaps());

    if (inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_ModelOculusTouch) {
        if (inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_LeftHand) {

            buttonStatesReal[1] = remoteInputState.Buttons;

            // the directions seem to be mirrored on the touch controllers compared to the gamepad
            buttonStatesReal[1] |= (remoteInputState.Joystick.x < -0.5f) ? ButtonMapping[EmuButton_Left] : 0;
            buttonStatesReal[1] |= (remoteInputState.Joystick.x > 0.5f) ? ButtonMapping[EmuButton_Right] : 0;
            buttonStatesReal[1] |= (remoteInputState.Joystick.y < -0.5f) ? ButtonMapping[EmuButton_Down] : 0;
            buttonStatesReal[1] |= (remoteInputState.Joystick.y > 0.5f) ? ButtonMapping[EmuButton_Up] : 0;

            buttonStatesReal[1] |= (remoteInputState.IndexTrigger > 0.25f) ? ButtonMapping[EmuButton_Trigger] : 0;
            buttonStatesReal[1] |= (remoteInputState.GripTrigger > 0.25f) ? ButtonMapping[EmuButton_GripTrigger] : 0;

        } else if (inputTrackedRemoteCapabilities->ControllerCapabilities & ovrControllerCaps_RightHand) {

            buttonStatesReal[2] = remoteInputState.Buttons;

            buttonStatesReal[2] |= (remoteInputState.Joystick.x < -0.5f) ? ButtonMapping[EmuButton_Left] : 0;
            buttonStatesReal[2] |= (remoteInputState.Joystick.x > 0.5f) ? ButtonMapping[EmuButton_Right] : 0;
            buttonStatesReal[2] |= (remoteInputState.Joystick.y < -0.5f) ? ButtonMapping[EmuButton_Down] : 0;
            buttonStatesReal[2] |= (remoteInputState.Joystick.y > 0.5f) ? ButtonMapping[EmuButton_Up] : 0;

            buttonStatesReal[2] |= (remoteInputState.IndexTrigger > 0.25f) ? ButtonMapping[EmuButton_Trigger] : 0;
            buttonStatesReal[2] |= (remoteInputState.GripTrigger > 0.25f) ? ButtonMapping[EmuButton_GripTrigger] : 0;
        }
    }

    if (remoteInputState.RecenterCount != trDevice.GetLastRecenterCount()) {
        OVR_LOG_WITH_TAG("MLBUState", "**RECENTERED** (%i != %i )", (int) remoteInputState.RecenterCount, (int) trDevice.GetLastRecenterCount());
        trDevice.SetLastRecenterCount(remoteInputState.RecenterCount);
    }

    return result;
}

//---------------------------------------------------------------------------------------------------
// Input device management
//---------------------------------------------------------------------------------------------------

//==============================
// ovrVrController::FindInputDevice
int MenuGo::FindInputDevice(const ovrDeviceID deviceID) {
    for (int i = 0; i < (int) InputDevices.size(); ++i) {
        if (InputDevices[i]->GetDeviceID() == deviceID) {
            return i;
        }
    }
    return -1;
}

//==============================
// ovrVrController::RemoveDevice
void MenuGo::RemoveDevice(const ovrDeviceID deviceID) {
    int index = FindInputDevice(deviceID);
    if (index < 0) {
        return;
    }
    ovrInputDeviceBase *device = InputDevices[index];
    delete device;
    InputDevices[index] = InputDevices.back();
    InputDevices[InputDevices.size() - 1] = nullptr;
    InputDevices.pop_back();
}

//==============================
// ovrVrController::IsDeviceTracked
bool MenuGo::IsDeviceTracked(const ovrDeviceID deviceID) {
    return FindInputDevice(deviceID) >= 0;
}

//==============================
// ovrVrController::OnDeviceConnected
void MenuGo::OnDeviceConnected(OVRFW::ovrAppl &app, const ovrInputCapabilityHeader &capsHeader) {
    ovrInputDeviceBase *device = nullptr;
    ovrResult result = ovrError_NotInitialized;

    switch (capsHeader.Type) {
        case ovrControllerType_Gamepad: {
            OVR_LOG_WITH_TAG("MLBUConnect", "Gamepad connected, ID = %u", capsHeader.DeviceID);

            ovrInputGamepadCapabilities gamepadCapabilities;
            gamepadCapabilities.Header = capsHeader;
            result = vrapi_GetInputDeviceCapabilities(app.GetSessionObject(), &gamepadCapabilities.Header);

            if (result == ovrSuccess)
                device = ovrInputDevice_Gamepad::Create(gamepadCapabilities);

            break;
        }

        case ovrControllerType_TrackedRemote: {
            OVR_LOG_WITH_TAG("MLBUConnect", "Controller connected, ID = %u", capsHeader.DeviceID);

            ovrInputTrackedRemoteCapabilities remoteCapabilities;
            remoteCapabilities.Header = capsHeader;

            result = vrapi_GetInputDeviceCapabilities(app.GetSessionObject(), &remoteCapabilities.Header);

            if (result == ovrSuccess) {
                device = ovrInputDevice_TrackedRemote::Create(app, remoteCapabilities);
            }
            break;
        }

        default:
            OVR_LOG("Unknown device connected!");
            OVR_ASSERT(false);
            return;
    }

    if (result != ovrSuccess) {
        OVR_LOG_WITH_TAG("MLBUConnect", "vrapi_GetInputDeviceCapabilities: Error %i", result);
    }

    if (device != nullptr) {
        OVR_LOG_WITH_TAG("MLBUConnect", "Added device '%s', id = %u", device->GetName(), capsHeader.DeviceID);
        InputDevices.push_back(device);
    } else {
        OVR_LOG_WITH_TAG("MLBUConnect", "Device creation failed for id = %u", capsHeader.DeviceID);
    }
}

//==============================
// ovrVrController::EnumerateInputDevices
void MenuGo::EnumerateInputDevices(OVRFW::ovrAppl &app) {
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader curCaps;

        if (vrapi_EnumerateInputDevices(app.GetSessionObject(), deviceIndex, &curCaps) < 0) {
            break;    // no more devices
        }

        if (!IsDeviceTracked(curCaps.DeviceID)) {
            OVR_LOG_WITH_TAG("Input", "     tracked");
            OnDeviceConnected(app, curCaps);
        }
    }
}

//==============================
// ovrVrController::OnDeviceDisconnected
void MenuGo::OnDeviceDisconnected(const ovrDeviceID deviceID) {
    OVR_LOG_WITH_TAG("MLBUConnect", "Controller disconnected, ID = %i", deviceID);
    RemoveDevice(deviceID);
}

//==============================
// ovrInputDevice_Gamepad::Create
ovrInputDeviceBase *ovrInputDevice_Gamepad::Create(const ovrInputGamepadCapabilities &gamepadCapabilities) {
    OVR_LOG_WITH_TAG("MLBUConnect", "Gamepad");

    auto *device = new ovrInputDevice_Gamepad(gamepadCapabilities);
    return device;
}

//==============================
// ovrInputDevice_TrackedRemote::Create
ovrInputDeviceBase *ovrInputDevice_TrackedRemote::Create(OVRFW::ovrAppl &app, const ovrInputTrackedRemoteCapabilities &remoteCapabilities) {
    OVR_LOG_WITH_TAG("MLBUConnect", "ovrInputDevice_TrackedRemote::Create");

    ovrInputStateTrackedRemote remoteInputState;
    remoteInputState.Header.ControllerType = remoteCapabilities.Header.Type;
    ovrResult result = vrapi_GetCurrentInputState(app.GetSessionObject(), remoteCapabilities.Header.DeviceID, &remoteInputState.Header);

    if (result == ovrSuccess) {
        auto *device = new ovrInputDevice_TrackedRemote(remoteCapabilities, remoteInputState.RecenterCount);
        return device;
    } else {
        OVR_LOG_WITH_TAG("MLBUConnect", "vrapi_GetCurrentInputState: Error %i", result);
    }

    return nullptr;
}
