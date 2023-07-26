#pragma once

#include "Emulator.h"
#include "Appl.h"
#include "LayerBuilder.h"

//==============================================================
// ovrInputDeviceBase
// Abstract base class for generically tracking controllers of different types.
class ovrInputDeviceBase {
public:
    ovrInputDeviceBase() {
    }

    virtual    ~ovrInputDeviceBase() {
    }

    virtual const ovrInputCapabilityHeader *GetCaps() const = 0;

    virtual ovrControllerType GetType() const = 0;

    virtual ovrDeviceID GetDeviceID() const = 0;

    virtual const char *GetName() const = 0;
};

//==============================================================
// ovrInputDevice_TrackedRemote
class ovrInputDevice_TrackedRemote : public ovrInputDeviceBase {
public:
    ovrInputDevice_TrackedRemote(const ovrInputTrackedRemoteCapabilities &caps, const uint8_t lastRecenterCount)
            : ovrInputDeviceBase(), MinTrackpad(FLT_MAX), MaxTrackpad(-FLT_MAX), Caps(caps), LastRecenterCount(lastRecenterCount) {
        IsActiveInputDevice = false;
    }

    virtual ~ovrInputDevice_TrackedRemote() {
    }

    static ovrInputDeviceBase *Create(OVRFW::ovrAppl &app, const ovrInputTrackedRemoteCapabilities &capsHeader);

    virtual const ovrInputCapabilityHeader *GetCaps() const OVR_OVERRIDE { return &Caps.Header; }

    virtual ovrControllerType GetType() const OVR_OVERRIDE { return Caps.Header.Type; }

    virtual ovrDeviceID GetDeviceID() const OVR_OVERRIDE { return Caps.Header.DeviceID; }

    virtual const char *GetName() const OVR_OVERRIDE { return "TrackedRemote"; }

    bool IsLeftHand() const {
        return (Caps.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0;
    }

    const ovrTracking &GetTracking() const { return Tracking; }

    void SetTracking(const ovrTracking &tracking) { Tracking = tracking; }

    uint8_t GetLastRecenterCount() const { return LastRecenterCount; }

    void SetLastRecenterCount(const uint8_t c) { LastRecenterCount = c; }

    const ovrInputTrackedRemoteCapabilities &GetTrackedRemoteCaps() const { return Caps; }

    OVR::Vector2f MinTrackpad;
    OVR::Vector2f MaxTrackpad;
    bool IsActiveInputDevice;

private:
    ovrInputTrackedRemoteCapabilities Caps;
    uint8_t LastRecenterCount;
    ovrTracking Tracking;
};

//==============================================================
// ovrInputDevice_Gamepad
class ovrInputDevice_Gamepad : public ovrInputDeviceBase {
public:
    ovrInputDevice_Gamepad(const ovrInputGamepadCapabilities &caps) : ovrInputDeviceBase(), Caps(caps) {}

    virtual ~ovrInputDevice_Gamepad() {}

    static ovrInputDeviceBase *Create(const ovrInputGamepadCapabilities &gamepadCapabilities);

    virtual const ovrInputCapabilityHeader *GetCaps() const OVR_OVERRIDE { return &Caps.Header; }

    virtual ovrControllerType GetType() const OVR_OVERRIDE { return Caps.Header.Type; }

    virtual ovrDeviceID GetDeviceID() const OVR_OVERRIDE { return Caps.Header.DeviceID; }

    virtual const char *GetName() const OVR_OVERRIDE { return "Gamepad"; }

    const ovrInputGamepadCapabilities &GetGamepadCaps() const { return Caps; }

private:
    ovrInputGamepadCapabilities Caps;
};

class MenuGo {
private:
    Emulator *emulator;
    LayerBuilder *layerBuilder;

    DrawHelper *drawHelper;
    FontManager *fontManager;

    const ovrJava *java;
    jclass *clsData;

    jmethodID getVal;

    ovrTextureSwapChain *MenuSwapChain;
    GLuint MenuFrameBuffer = 0;

    // saved variables
    bool showExitDialog = false;
    bool resetView = false;

    float transitionPercentage = 1.0F;

    uint buttonStatesReal[3];
    uint buttonStates[3];
    uint lastButtonStates[3];

    ButtonMapper::MappedButtons buttonMappingMenu;
    ButtonMapper::MappedButton *remapButton;

    // battery display
    std::string battery_string;
    int batteryLevel;
    int batter_string_width;

    // time display
    std::string time_string;
    int time_string_width;

    bool UpdateMapping = false;
    bool UpdateMappingUseTimer = false;

    float UpdateMappingTimer;
    float MappingOverlayPercentage;

    bool isTransitioning;
    int transitionMoveDir = 1;
    float transitionState = 1;

    bool loadedRom = false;

    float deltaSeconds;

    GLuint textureIdMenu;

    std::function<void()> updateMappingText;

    Menu *currentMenu;
    Menu *nextMenu, *currentBottomBar;

    Menu romSelectionMenu, mainMenu, settingsMenu, buttonMenuMapMenu, buttonEmulatorMapMenu, bottomBar, buttonMappingOverlay, moveMenu;

    MenuButton *mappedButton;
    std::shared_ptr<MenuButton> backHelp, menuHelp, selectHelp, yawButton, pitchButton, rollButton, scaleButton, distanceButton, slotButton;

    // settings page
    std::shared_ptr<MenuButton> buttonSettingsMenuMapping, buttonSettingsEmulatorMapping, buttonSettingsMoveScreen, buttonSettingsFollowHead;

    std::shared_ptr<MenuLabel>  mappingButtonLabel, possibleMappingLabel, pressButtonLabel;

    std::vector<std::shared_ptr<MenuButton>> buttonMapping;

    std::vector<ovrInputDeviceBase *> InputDevices;
    double LastGamepadUpdateTimeInSeconds;

public:
    void Free();

    void Init(Emulator *_emulator, LayerBuilder *_layerBuilder, DrawHelper *_drawHelper, FontManager *_fontManager, const ovrJava *java, jclass *clsData);

    void SetUpMenu();

    void SaveSettings();

    void LoadSettings();

    void ScanDirectory();

    void CreateScreen();

    void DrawMenu();

    void Update(OVRFW::ovrAppl &app, const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out);

    // ui functions
    void OnClickSaveSlotLeft(MenuItem *item);

    void OnClickSaveSlotRight(MenuItem *item);

    void StartTransition(Menu *next, int dir);

    void OnClickResumGame(MenuItem *item);

    void OnClickResetGame(MenuItem *item);

    void OnClickSaveGame(MenuItem *item);

    void OnClickLoadGame(MenuItem *item);

    void OnClickLoadRomGame(MenuItem *item);

    void OnClickSettingsGame(MenuItem *item);

    void OnClickResetView(MenuItem *item);

    void OnClickBackAndSave(MenuItem *item);

    void OnBackPressedRomList();

    void OnBackPressedSettings();

    void OnClickBackMove(MenuItem *item);

    void ChangeSaveSlot(MenuItem *item, int dir);

    uint GetPressedButton(uint &_buttonState, uint &_lastButtonState);

    void SetMappingText(MenuButton *Button, ButtonMapper::MappedButton *Mapping);

    void UpdateButtonMappingText(MenuItem *item);

    void ResetMenuState();

    void OnClickMoveScreenScaleRight(MenuItem *item);

    void OnClickMoveScreenScaleLeft(MenuItem *item);

    void OnClickMoveScreenDistanceRight(MenuItem *item);

    void OnClickMoveScreenDistanceLeft(MenuItem *item);

    void OnClickMoveScreenRollRight(MenuItem *item);

    void OnClickMoveScreenRollLeft(MenuItem *item);

    void OnClickMoveScreenPitchRight(MenuItem *item);

    void OnClickMoveScreenPitchLeft(MenuItem *item);

    void OnClickMoveScreenYawRight(MenuItem *item);

    void UpdateMenuMapping();

    void UpdateEmulatorMapping();

    void OnMenuMappingButtonSelect(MenuItem *item, int direction);

    void OnClickMenuMappingButtonLeft(MenuItem *item);

    void OnClickMenuMappingButtonRight(MenuItem *item);

    void OnMappingButtonSelect(MenuItem *item, int direction);

    void OnClickMappingButtonLeft(MenuItem *item);

    void OnClickMappingButtonRight(MenuItem *item);

    void OnClickChangeMenuButtonEnter(MenuItem *item);

    void OnClickChangeButtonMappingEnter(MenuItem *item);

    void UpdateButtonMapping(MenuItem *item, uint *_buttonStates, uint *_lastButtonStates);

    void OnClickExit(MenuItem *item);

    void OnClickBackMainMenu();

    void OnClickFollowMode(MenuItem *item);

    void OnClickMoveScreen(MenuItem *item);

    void OnClickMenuMappingScreen(MenuItem *item);

    void OnClickEmulatorMappingScreen(MenuItem *item);

    void OnBackPressedMove();

    void MoveYaw(MenuItem *item, float dir);

    void MovePitch(MenuItem *item, float dir);

    void ChangeDistance(MenuItem *item, float dir);

    void ChangeScale(MenuItem *item, float dir);

    void MoveRoll(MenuItem *item, float dir);

    void OnClickResetEmulatorMapping(MenuItem *item);

    void OnClickResetViewSettings(MenuItem *item);

    void OnClickYaw(MenuItem *item);

    void OnClickPitch(MenuItem *item);

    void OnClickRoll(MenuItem *item);

    void OnClickDistance(MenuItem *item);

    void OnClickScale(MenuItem *item);

    void OnClickMoveScreenYawLeft(MenuItem *item);

    void SwapButtonSelectBack(MenuItem *item);

    void UpdateInputDevices(ovrAppl &app, const ovrApplFrameIn &in);

    ovrResult PopulateRemoteControllerInfo(ovrAppl &app, ovrInputDevice_TrackedRemote &trDevice);

    void OnDeviceDisconnected(const ovrDeviceID deviceID);

    void EnumerateInputDevices(ovrAppl &app);

    void OnDeviceConnected(ovrAppl &app, const ovrInputCapabilityHeader &capsHeader);

    bool IsDeviceTracked(const ovrDeviceID deviceID);

    void RemoveDevice(const ovrDeviceID deviceID);

    int FindInputDevice(const ovrDeviceID deviceID);

    void UpdateCurrentMenu(float deltaSeconds);

    void DrawGUI();

    void GetBattryString(std::string &batteryString);

    void SetTimeString(std::string &timeString);

    int UpdateBatteryLevel();

};