#pragma once

#include <string>
#include <vector>
#include <FrontendGo/LayerBuilder.h>
#include <FrontendGo/Global.h>
#include <FrontendGo/Audio/OpenSLWrap.h>
#include "MenuHelper.h"
#include "ButtonMapping.h"
#include "Global.h"

using namespace OVR;

class Emulator {
public:
    struct Rom {
        std::string RomName;
        std::string FullPath;
        std::string FullPathNorm;
        std::string SavePath;
    };

    static bool SortByRomName(const Rom &first, const Rom &second);

    struct SaveState {
        bool hasImage;
        bool hasState;
        uint8_t *saveImage;
    };

    struct LoadedGame {
        SaveState saveStates[10];
    };

    LayerBuilder *layerBuilder;
    DrawHelper *drawHelper;

    OpenSLWrapper *openSlWrap;

    std::function<void()> OnRomLoaded;

    //extern const int HEADER_HEIGHT, BOTTOM_HEIGHT, MENU_WIDTH, MENU_HEIGHT;

    const std::string romFolderPath = "/Roms/VB/";
    const std::string stateFilePath = "/Roms/VB/States/";
    const std::vector<std::string> supportedFileNames = {".vb", ".vboy", ".bin"};

    const static int buttonCount = 14;
    ButtonMapper::MappedButtons buttonMapping[buttonCount];
    int buttonOrder[14] = {0, 1, 3, 2, 11, 10, 7, 6, 9, 8, 12, 5, 4, 13};

    // menu size
    const int MENU_WIDTH = 640;
    const int MENU_HEIGHT = 576;

    const int HEADER_HEIGHT = 75;
    const int BOTTOM_HEIGHT = 30;

    // 384, 768
    const int VIDEO_WIDTH = 384;
    const int VIDEO_HEIGHT = 224;

    const std::string STR_HEADER = "VirtualBoyGo";
    const std::string STR_VERSION = "ver.1.4";

    // maybe this will be supported on future headsets?
    const float DisplayRefreshRate = 50.27f;

    const int SAVE_FILE_VERSION = 27;

    GLuint *button_icons[buttonCount];

    void Free();

    void Init(std::string stateFolder, LayerBuilder *_layerBuilder, DrawHelper *_drawHelper, OpenSLWrapper *openSLWrap);

    void ResetGame();

    void SaveState(int slot);

    void LoadState(int slot);

    void UpdateStateImage(int saveSlot);

    void ResetButtonMapping();

    void AddRom(const std::string &strFullPath, const std::string &strFilename);

    void SortRomList();

    void Update(const OVRFW::ovrApplFrameIn &in, uint *buttonStates, uint *lastButtonStates);

    void DrawScreenLayer(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out);

    void LoadEmulatorSettings(std::ifstream *file);

    void InitSettingsMenu(int &posX, int &posY, Menu &settingsMenu);

    void UpdateScreen(const void *data);

    void InitMainMenu(int posX, int posY, Menu &mainMenu);

    void SaveEmulatorSettings(std::ofstream *outfile);

    void SaveRam();

    void VB_Audio_CB(int16_t *SoundBuf, int32_t SoundBufSize);

    void VB_VIDEO_CB(const void *data, unsigned int width, unsigned int height);

    void InitStateImage();

    void OnClickRom(Rom *rom);

    void InitRomSelectionMenu(int posX, int posY, Menu &romSelectionMenu);

private:

    ovrVector3f predefColors[11] = {{1.0f,  0.0f,  0.0f},
                                    {0.9f,  0.3f,  0.1f},
                                    {1.0f,  0.85f, 0.1f},
                                    {0.25f, 1.0f,  0.1f},
                                    {0.0f,  1.0f,  0.45f},
                                    {0.0f,  1.0f,  0.85f},
                                    {0.0f,  0.85f, 1.0f},
                                    {0.15f, 1.0f,  1.0f},
                                    {0.75f, 0.65f, 1.0f},
                                    {1.0f,  1.0f,  1.0f},
                                    {1.0f,  0.3f,  0.2f}};

    const std::string strColor[3]{"R: ", "G: ", "B: "};
    float color[3]{1.0f, 1.0f, 1.0f};

    std::vector<Rom> romFileList;

    GLuint screenTextureId, stateImageId;
    GLuint screenTextureCylinderId;
    ovrTextureSwapChain *CylinderSwapChain;

    const void *currentScreenData = nullptr;

    ovrSurfaceDef ScreenSurfaceDef;
    Bounds3f SceneScreenBounds;

    Vector4f ScreenColor[2];        // { UniformColor, ScaleBias }
    GlTexture ScreenTexture[2];    // { MovieTexture, Fade Texture }
    Matrix4f ScreenTexMatrix[2];
    GlBuffer ScreenTexMatrices;

    const float COLOR_STEP_SIZE = 0.05f;
    const float IPD_STEP_SIZE = 0.00390625f;

    const int CylinderWidth = VIDEO_WIDTH;
    const int CylinderHeight = VIDEO_HEIGHT;

    float threedeeIPD = 0;
    float minIPD = -0.125f;
    float maxIPD = 0.125f;

    int selectedPredefColor;
    const int predefColorCount = 11;

    LoadedGame *currentGame;

    std::string stateFolderPath;

    bool audioInit;

    float emulationSpeed = 50.27;
    float frameCounter = 0;

    uint8_t *screenData;

    int screenPosY;

    int screenborder = 1;
    int TextureHeight = VIDEO_HEIGHT * 2 + 1 * 2;//12;

    int32_t *pixelData = new int32_t[VIDEO_WIDTH * TextureHeight];

    int32_t *stateImageData = new int32_t[VIDEO_WIDTH * VIDEO_HEIGHT];

    bool useCubeMap = false;
    bool useThreeDeeMode = true;

    Rom *CurrentRom = nullptr;
    GLuint screenFramebuffer[2];
    int romSelection = 0;

    std::shared_ptr<MenuList<Rom>> romList;
    std::shared_ptr<MenuButton> screenModeButton, offsetButton, paletteButton;
    std::shared_ptr<MenuButton> rButton, gButton, bButton;

    void OnClickRLeft(MenuItem *item);

    void OnClickRRight(MenuItem *item);

    void OnClickGLeft(MenuItem *item);

    void OnClickGRight(MenuItem *item);

    void OnClickBRight(MenuItem *item);

    void OnClickBLeft(MenuItem *item);

    void ChangeColor(MenuButton *item, int colorIndex, float dir);

    void ChangeOffset(MenuButton *item, float dir);

    void ChangePalette(MenuButton *item, float dir);

    void SetThreeDeeMode(MenuItem *item, bool newMode);

    void SetCurvedMove(MenuItem *item, bool newMode);

    void OnClickCurveScreen(MenuItem *item);

    void OnClickScreenMode(MenuItem *item);

    void OnClickPrefabColorLeft(MenuItem *item);

    void OnClickPrefabColorRight(MenuItem *item);

    void OnClickOffsetLeft(MenuItem *item);

    void OnClickOffsetRight(MenuItem *item);

    void OnClickResetOffset(MenuItem *item);

    void LoadRam();

    void LoadGame(Rom *rom);

    bool StateExists(int slot);

    void SaveStateImage(int slot);

    bool LoadStateImage(int slot);

    void AudioFrame(unsigned short *audio, int32_t sampleCount);

    void UpdateNoImageSlotLabel(MenuItem *item, uint *buttonState, uint *lastButtonState);

    void UpdateEmptySlotLabel(MenuItem *item, uint *buttonState, uint *lastButtonState);
};