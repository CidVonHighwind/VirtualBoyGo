#include "Emulator.h"

#include <VrApi/Include/VrApi_Types.h>
#include <VrAppFramework/Src/Framebuffer.h>
#include <sys/stat.h>
#include <vrvb.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <VrAppFramework/Include/OVR_Input.h>
#include <VrApi/Include/VrApi_Input.h>

#include "Audio/OpenSLWrap.h"
#include "DrawHelper.h"
#include "FontMaster.h"
#include "LayerBuilder.h"
#include "MenuHelper.h"
#include "Global.h"

#include "OvrApp.h"

template<typename T>
std::string to_string(T value) {
    std::ostringstream os;
    os << value;
    return os.str();
}

template<>
void MenuList<Emulator::Rom>::DrawTexture(float offsetX, float offsetY, float transparency) {
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
    DrawHelper::DrawTexture(textureWhiteId, PosX + offsetX + 2, PosY + 2, scrollbarWidth - 4,
                            scrollbarHeight - 4, MenuBackgroundOverlayColor, transparency);
    // slider
    DrawHelper::DrawTexture(textureWhiteId, PosX + offsetX, PosY + recPosY, scrollbarWidth,
                            recHeight,
                            sliderColor, transparency);

    // draw the cartridge icons
    for (uint i = (uint) menuListFState; i < menuListFState + maxListItems; i++) {
        if (i < ItemList->size()) {
            // fading in or out
            float fadeTransparency = 1;
            if (i - menuListFState < 0) {
                fadeTransparency = 1 - (menuListFState - i);
            } else if (i - menuListFState >= maxListItems - 1 &&
                       menuListFState != (int) menuListFState) {
                fadeTransparency = menuListFState - (int) menuListFState;
            }

            DrawHelper::DrawTexture(textureVbIconId,
                                    PosX + offsetX + scrollbarWidth + 15 - 3
                                    + (((uint) CurrentSelection == i) ? 5 : 0),
                                    listStartY + listItemSize / 2 - 12
                                    + listItemSize * (i - menuListFState) + offsetY, 24, 24,
                                    {1.0f, 1.0f, 1.0f, 1.0f}, transparency * fadeTransparency);
        }
    }
}

template<>
void MenuList<Emulator::Rom>::DrawText(float offsetX, float offsetY, float transparency) {
    // draw rom list
    for (uint i = (uint) menuListFState; i < menuListFState + maxListItems; i++) {
        if (i < ItemList->size()) {
            // fading in or out
            float fadeTransparency = 1;
            if (i - menuListFState < 0) {
                fadeTransparency = 1 - (menuListFState - i);
            } else if (i - menuListFState >= maxListItems - 1 &&
                       menuListFState != (int) menuListFState) {
                fadeTransparency = menuListFState - (int) menuListFState;
            }

            FontManager::RenderText(
                    *Font,
                    ItemList->at(i).RomName,
                    PosX + offsetX + scrollbarWidth + 44 + (((uint) CurrentSelection == i) ? 5 : 0),
                    listStartY + itemOffsetY + listItemSize * (i - menuListFState) + offsetY,
                    1.0f,
                    ((uint) CurrentSelection == i) ? textSelectionColor : textColor,
                    transparency * fadeTransparency);
        } else
            break;
    }
}

const std::string STR_HEADER = "VirtualBoyGo";
const std::string STR_VERSION = "ver.1.3";
const float DisplayRefreshRate = 72.0f;
const int SAVE_FILE_VERSION = 25;

ovrVector4f headerTextColor = {0.9f, 0.1f, 0.1f, 1.0f};
ovrVector4f textSelectionColor = {0.9f, 0.1f, 0.1f, 1.0f};
ovrVector4f textColor = {0.8f, 0.8f, 0.8f, 1.0f};
ovrVector4f sliderColor = {0.8f, 0.8f, 0.8f, 0.8f};
ovrVector4f MenuBackgroundColor = {0.2f, 0.2f, 0.2f, 0.95f};
ovrVector4f MenuBackgroundOverlayHeader = {0.5f, 0.5f, 0.5f, 0.15f};
ovrVector4f MenuBackgroundOverlayColorLight = {0.5f, 0.5f, 0.5f, 0.15f};
ovrVector4f MenuBackgroundOverlayColor = {0.431f, 0.412f, 0.443f, 0.75f};
ovrVector4f textColorBattery = {0.25f, 0.25f, 0.25f, 1.0f};
ovrVector4f textColorVersion = {0.8f, 0.8f, 0.8f, 1.0f};
ovrVector4f BatteryBackgroundColor = {0.25f, 0.25f, 0.25f, 1.0f};
ovrVector4f MenuBottomColor = {0.25f, 0.25f, 0.25f, 1.0f};

const ovrJava *java;
jclass clsData;

namespace OVR {

#if defined( OVR_OS_ANDROID )
    extern "C" {

    jlong
    Java_com_nintendont_virtualboygo_MainActivity_nativeSetAppInterface(JNIEnv *jni, jclass clazz,
                                                                        jobject activity,
                                                                        jstring fromPackageName,
                                                                        jstring commandString,
                                                                        jstring uriString) {

        jmethodID messageMe = jni->GetMethodID(clazz, "getInternalStorageDir", "()Ljava/lang/String;");
        jobject result = (jstring) jni->CallObjectMethod(activity, messageMe);
        const char *storageDir = jni->GetStringUTFChars((jstring) result, NULL);

        messageMe = jni->GetMethodID(clazz, "getExternalFilesDir", "()Ljava/lang/String;");
        result = (jstring) jni->CallObjectMethod(activity, messageMe);
        const char *appDir = jni->GetStringUTFChars((jstring) result, NULL);

        appStoragePath = storageDir;

        saveFilePath = appDir;
        saveFilePath.append("/settings.config");

        OVR_LOG("got string from java: appdir %s", appDir);
        OVR_LOG("got string from java: storageDir %s", storageDir);

        OVR_LOG("nativeSetAppInterface");
        return (new OvrApp())->SetActivity(jni, clazz, activity, fromPackageName, commandString, uriString);
    }

    } // extern "C"
#endif
}

namespace Emulator {

    GLuint screenTextureId, stateImageId;
    GLuint screenTextureCylinderId;
    ovrTextureSwapChain *CylinderSwapChain;

    GlProgram Program;

    static const char *movieUiVertexShaderSrc =
            "uniform TextureMatrices\n"
            "{\n"
            "highp mat4 Texm[NUM_VIEWS];\n"
            "};\n"
            "attribute vec4 Position;\n"
            "attribute vec2 TexCoord;\n"
            "uniform lowp vec4 UniformColor;\n"
            "varying  highp vec2 oTexCoord;\n"
            "varying  lowp vec4 oColor;\n"
            "void main()\n"
            "{\n"
            "   gl_Position = TransformVertex( Position );\n"
            "   oTexCoord = vec2( Texm[ VIEW_ID ] * vec4(TexCoord,1,1) );\n"
            "   oColor = UniformColor;\n"
            "}\n";

    const char *movieUiFragmentShaderSrc =
            "uniform sampler2D Texture0;\n"
            "uniform sampler2D Texture1;\n"    // fade / clamp texture
            "uniform lowp vec4 ColorBias;\n"
            "varying highp vec2 oTexCoord;\n"
            "varying lowp vec4	oColor;\n"
            "void main()\n"
            "{\n"
            "	lowp vec4 movieColor = texture2D( Texture0, oTexCoord ) * texture2D( Texture1, oTexCoord );\n"
            "	gl_FragColor = ColorBias + oColor * movieColor;\n"
            "}\n";

    const void *currentScreenData = nullptr;

    ovrSurfaceDef ScreenSurfaceDef;
    Bounds3f SceneScreenBounds;

    Vector4f ScreenColor[2];        // { UniformColor, ScaleBias }
    GlTexture ScreenTexture[2];    // { MovieTexture, Fade Texture }
    Matrix4f ScreenTexMatrix[2];
    GlBuffer ScreenTexMatrices;

    double startTime;

    const float COLOR_STEP_SIZE = 0.05f;
    const float IPD_STEP_SIZE = 0.001953125f;

// 384
// 768
    const int VIDEO_WIDTH = 384;
    const int VIDEO_HEIGHT = 224;

    const int CylinderWidth = VIDEO_WIDTH;
    const int CylinderHeight = VIDEO_HEIGHT;

    std::string strColor[]{"R: ", "G: ", "B: "};
    float color[]{1.0f, 1.0f, 1.0f};
    float threedeeIPD = 0;
    float minIPD = -0.1953125f;
    float maxIPD = 0.1953125f;

    int selectedPredefColor;
    const int predefColorCount = 11;
    ovrVector3f predefColors[] = {{1.0f,  0.0f,  0.0f},
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

/*
 *     {BUTTON_A, BUTTON_B, BUTTON_RIGHT_TRIGGER, BUTTON_LEFT_TRIGGER, BUTTON_RSTICK_UP,
     BUTTON_RSTICK_RIGHT, BUTTON_LSTICK_RIGHT, BUTTON_LSTICK_LEFT,
     BUTTON_LSTICK_DOWN, BUTTON_LSTICK_UP, BUTTON_START, BUTTON_SELECT,
     BUTTON_RSTICK_LEFT, BUTTON_RSTICK_DOWN};
 */

    GLuint *button_icons[buttonCount] =
            {&textureButtonAIconId, &textureButtonBIconId, &mappingTriggerRight, &mappingTriggerLeft, &mappingRightUpId, &mappingRightRightId,
             &mappingLeftRightId, &mappingLeftLeftId, &mappingLeftDownId, &mappingLeftUpId, &mappingStartId, &mappingSelectId,
             &mappingRightLeftId, &mappingRightDownId};

    MappedButtons buttonMapping[buttonCount];
    int buttonOrder[14] = {0, 1, 3, 2, 11, 10, 7, 6, 9, 8, 12, 5, 4, 13};

    LoadedGame *currentGame;

    const std::string romFolderPath = "/Roms/VB/";
    const std::string stateFilePath = "/Roms/VB/States/";

    std::string stateFolderPath;

    const std::vector<std::string> supportedFileNames = {".vb", ".vboy", ".bin"};

    MenuList<Rom> *romList;
    std::vector<Rom> *romFileList = new std::vector<Rom>();

    bool audioInit;

    float emulationSpeed = 50.27;
    float frameCounter = 1;

    uint8_t *screenData;

    int screenPosY;

    int screenborder = 1;
    int TextureHeight = VIDEO_HEIGHT * 2 + 1 * 2;//12;

    int32_t *pixelData = new int32_t[VIDEO_WIDTH * TextureHeight];

    int32_t *stateImageData = new int32_t[VIDEO_WIDTH * VIDEO_HEIGHT];

    bool useCubeMap = false;
    bool useThreeDeeMode = true;

    Rom *CurrentRom;
    GLuint screenFramebuffer[2];
    int romSelection = 0;

    MenuButton *rButton, *gButton, *bButton;

    void LoadRam();

    void InitStateImage() {
        glGenTextures(1, &stateImageId);
        glBindTexture(GL_TEXTURE_2D, stateImageId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VIDEO_WIDTH, VIDEO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void UpdateStateImage(int saveSlot) {
        glBindTexture(GL_TEXTURE_2D, stateImageId);

        uint8_t *dataArray = currentGame->saveStates[saveSlot].saveImage;
        for (int y = 0; y < VIDEO_HEIGHT; ++y) {
            for (int x = 0; x < VIDEO_WIDTH; ++x) {
                uint8_t das = dataArray[x + y * VIDEO_WIDTH];
                stateImageData[x + y * VIDEO_WIDTH] =
                        0xFF000000 | ((int) (das * color[2]) << 16) | ((int) (das * color[1]) << 8) | (int) (das * color[0]);
            }
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, GL_RGBA,
                        GL_UNSIGNED_BYTE,
                        stateImageData);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void UpdateScreen(const void *data) {
        screenData = (uint8_t *) data;
        uint8_t *dataArray = (uint8_t *) data;

        {
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
            DrawHelper::DrawTexture(screenTextureId,
                                    640 * ((float) screenborder * 2 / (VIDEO_WIDTH * 2 + screenborder * 4)),
                                    576 * ((float) screenborder * 2 / (TextureHeight * 2 + screenborder * 4)),
                                    640 * ((float) (VIDEO_WIDTH * 2) / (VIDEO_WIDTH * 2 + screenborder * 4)),
                                    576 * ((float) (TextureHeight * 2) / (TextureHeight * 2 + screenborder * 4)),
                                    {1.0f, 1.0f, 1.0f, 1.0f}, 1);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // TODO whut
        ScreenTexture[0] = GlTexture(screenTextureCylinderId, GL_TEXTURE_2D,
                                     VIDEO_WIDTH * 2 + screenborder * 4, TextureHeight * 2 + screenborder * 4);
        ScreenTexture[1] = GlTexture(screenTextureCylinderId, GL_TEXTURE_2D,
                                     VIDEO_WIDTH * 2 + screenborder * 4, TextureHeight * 2 + screenborder * 4);

    }

    void AudioFrame(unsigned short *audio, int32_t sampleCount) {
        if (!audioInit) {
            audioInit = true;
            StartPlaying();
        }

        SetBuffer(audio, (unsigned) (sampleCount * 2));
        // 52602
        // 877
        // OVR_LOG("VRVB audio size: %i", sampleCount);
    }

    void VB_Audio_CB(int16_t *SoundBuf, int32_t SoundBufSize) {
        AudioFrame((unsigned short *) SoundBuf, SoundBufSize);
    }

    void VB_VIDEO_CB(const void *data, unsigned width, unsigned height) {
        // OVR_LOG("VRVB width: %i, height: %i, %i", width, height, (((int8_t *) data)[5])); // 144 + 31 * 384
        // update the screen texture with the newly received image
        currentScreenData = data;
        UpdateScreen(data);
    }

    bool StateExists(int slot) {
        std::string savePath = stateFolderPath + CurrentRom->RomName + ".state";
        if (slot > 0) savePath += to_string(slot);
        struct stat buffer;
        return (stat(savePath.c_str(), &buffer) == 0);
    }

    void SaveStateImage(int slot) {
        std::string savePath = stateFolderPath + CurrentRom->RomName + ".stateimg";
        if (slot > 0) savePath += to_string(slot);

        OVR_LOG("save image of slot to %s", savePath.c_str());
        std::ofstream outfile(savePath, std::ios::trunc | std::ios::binary);
        outfile.write((const char *) currentGame->saveStates[slot].saveImage,
                      sizeof(uint8_t) * VIDEO_WIDTH * VIDEO_HEIGHT);
        outfile.close();
        OVR_LOG("finished writing save image to file");
    }

    bool LoadStateImage(int slot) {
        std::string savePath = stateFolderPath + CurrentRom->RomName + ".stateimg";
        if (slot > 0) savePath += to_string(slot);

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

    void LoadGame(Rom *rom) {
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
                memset(currentGame->saveStates[i].saveImage, 0,
                       sizeof(uint8_t) * VIDEO_WIDTH * 2 * VIDEO_HEIGHT);
            } else {
                currentGame->saveStates[i].hasImage = true;
            }

            currentGame->saveStates[i].hasState = StateExists(i);
        }

        UpdateStateImage(0);

        OVR_LOG("LOADED VRVB ROM");
    }

    void Init(std::string appFolderPath) {
        stateFolderPath = appFolderPath + stateFilePath;

        // set the button mapping
        UpdateButtonMapping();

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
        VRVB::Init();

        VRVB::audio_cb = VB_Audio_CB;
        VRVB::video_cb = VB_VIDEO_CB;

        InitStateImage();
        currentGame = new LoadedGame();
        for (int i = 0; i < 10; ++i) {
            currentGame->saveStates[i].saveImage = new uint8_t[VIDEO_WIDTH * 2 * VIDEO_HEIGHT]();
        }

        static ovrProgramParm MovieExternalUiUniformParms[] =
                {
                        {"TextureMatrices", ovrProgramParmType::BUFFER_UNIFORM},
                        {"UniformColor",    ovrProgramParmType::FLOAT_VECTOR4},
                        {"Texture0",        ovrProgramParmType::TEXTURE_SAMPLED},
                        {"Texture1",        ovrProgramParmType::TEXTURE_SAMPLED},
                        {"ColorBias",       ovrProgramParmType::FLOAT_VECTOR4},
                };
        GlProgram MovieExternalUiProgram = GlProgram::Build(movieUiVertexShaderSrc, movieUiFragmentShaderSrc, MovieExternalUiUniformParms,
                                                            sizeof(MovieExternalUiUniformParms) / sizeof(ovrProgramParm));

        ScreenTexMatrices.Create(GLBUFFER_TYPE_UNIFORM, sizeof(Matrix4f) * GlProgram::MAX_VIEWS, NULL);

        ScreenColor[0] = Vector4f(1.0f, 1.0f, 1.0f, 0.0f);
        ScreenColor[1] = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);

        // this leads to the app crashing on exit
        // ScreenSurfaceDef.surfaceName = "ScreenSurf";
        ScreenSurfaceDef.geo = BuildTesselatedQuad(1, 1);
        ScreenSurfaceDef.graphicsCommand.Program = MovieExternalUiProgram;
        ScreenSurfaceDef.graphicsCommand.UniformData[0].Data = &ScreenTexMatrices;
        ScreenSurfaceDef.graphicsCommand.UniformData[1].Data = &ScreenColor[0];
        ScreenSurfaceDef.graphicsCommand.UniformData[2].Data = &ScreenTexture[0];
        ScreenSurfaceDef.graphicsCommand.UniformData[3].Data = &ScreenTexture[1];
        ScreenSurfaceDef.graphicsCommand.UniformData[4].Data = &ScreenColor[1];

        Vector3f size(5.25f, 5.25f * (VIDEO_HEIGHT / (float) VIDEO_WIDTH), 0.0f);

        SceneScreenBounds = Bounds3f(size * -0.5f, size * 0.5f);
        SceneScreenBounds.Translate(Vector3f(0.0f, 1.66f, -5.61f));

        startTime = SystemClock::GetTimeInSeconds();
    }

    void UpdateEmptySlotLabel(MenuItem *item, uint *buttonState, uint *lastButtonState) {
        item->Visible = !currentGame->saveStates[saveSlot].hasState;
    }

    void UpdateNoImageSlotLabel(MenuItem *item, uint *buttonState, uint *lastButtonState) {
        item->Visible =
                currentGame->saveStates[saveSlot].hasState &&
                !currentGame->saveStates[saveSlot].hasImage;
    }

    void InitMainMenu(int posX, int posY, Menu &mainMenu) {
        int offsetY = 30;
        // main menu
        mainMenu.MenuItems.push_back(
                new MenuImage(textureWhiteId, MENU_WIDTH - VIDEO_WIDTH - 20 - 5,
                              HEADER_HEIGHT + offsetY - 5, VIDEO_WIDTH + 10,
                              VIDEO_HEIGHT + 10, MenuBackgroundOverlayColor));
        MenuLabel *emptySlotLabel =
                new MenuLabel(&fontSlot, "- Empty Slot -", MENU_WIDTH - VIDEO_WIDTH - 20,
                              HEADER_HEIGHT + offsetY,
                              VIDEO_WIDTH, VIDEO_HEIGHT, {1.0f, 1.0f, 1.0f, 1.0f});
        emptySlotLabel->UpdateFunction = UpdateEmptySlotLabel;

        MenuLabel *noImageSlotLabel =
                new MenuLabel(&fontSlot, "- -", MENU_WIDTH - VIDEO_WIDTH - 20,
                              HEADER_HEIGHT + offsetY,
                              VIDEO_WIDTH, VIDEO_HEIGHT, {1.0f, 1.0f, 1.0f, 1.0f});
        noImageSlotLabel->UpdateFunction = UpdateNoImageSlotLabel;

        mainMenu.MenuItems.push_back(emptySlotLabel);
        mainMenu.MenuItems.push_back(noImageSlotLabel);
        // image slot
        mainMenu.MenuItems.push_back(new MenuImage(stateImageId, MENU_WIDTH - VIDEO_WIDTH - 20,
                                                   HEADER_HEIGHT + offsetY, VIDEO_WIDTH,
                                                   VIDEO_HEIGHT,
                                                   {1.0f, 1.0f, 1.0f, 1.0f}));
    }

    void ChangeColor(MenuButton *item, int colorIndex, float dir) {
        color[colorIndex] += dir;

        if (color[colorIndex] < 0)
            color[colorIndex] = 0;
        else if (color[colorIndex] > 1)
            color[colorIndex] = 1;

        item->Text = strColor[colorIndex] + to_string(color[colorIndex]);

        // update screen
        if (currentScreenData)
            UpdateScreen(currentScreenData);
        // update save slot color
        UpdateStateImage(saveSlot);
    }

    void ChangeOffset(MenuButton *item, float dir) {
        threedeeIPD += dir;

        if (threedeeIPD < minIPD)
            threedeeIPD = minIPD;
        else if (threedeeIPD > maxIPD)
            threedeeIPD = maxIPD;

        item->Text = "IPD offset: " + to_string(threedeeIPD * 256);
    }

    void ChangePalette(MenuButton *item, float dir) {
        selectedPredefColor += dir;
        if (selectedPredefColor < 0)
            selectedPredefColor = predefColorCount - 1;
        else if (selectedPredefColor >= predefColorCount)
            selectedPredefColor = 0;

        color[0] = predefColors[selectedPredefColor].x;
        color[1] = predefColors[selectedPredefColor].y;
        color[2] = predefColors[selectedPredefColor].z;

        ChangeColor(rButton, 0, 0);
        ChangeColor(gButton, 1, 0);
        ChangeColor(bButton, 2, 0);

        item->Text = "Palette: " + to_string(selectedPredefColor);

        // update screen
        if (currentScreenData)
            UpdateScreen(currentScreenData);
        // update save slot color
        UpdateStateImage(saveSlot);
    }

    void SetThreeDeeMode(MenuItem *item, bool newMode) {
        useThreeDeeMode = newMode;
        ((MenuButton *) item)->IconId = useThreeDeeMode ? threedeeIconId : twodeeIconId;
        ((MenuButton *) item)->Text = useThreeDeeMode ? "3D Screen" : "2D Screen";
    }

    void SetCurvedMove(MenuItem *item, bool newMode) {
        useCubeMap = newMode;
        ((MenuButton *) item)->Text = useCubeMap ? "Flat Screen" : "Curved Screen";
    }

    void OnClickCurveScreen(MenuItem *item) { SetCurvedMove(item, !useCubeMap); }

    void OnClickScreenMode(MenuItem *item) { SetThreeDeeMode(item, !useThreeDeeMode); }

    void OnClickPrefabColorLeft(MenuItem *item) { ChangePalette((MenuButton *) item, -1); }

    void OnClickPrefabColorRight(MenuItem *item) { ChangePalette((MenuButton *) item, 1); }

    void OnClickRLeft(MenuItem *item) { ChangeColor((MenuButton *) item, 0, -COLOR_STEP_SIZE); }

    void OnClickRRight(MenuItem *item) { ChangeColor((MenuButton *) item, 0, COLOR_STEP_SIZE); }

    void OnClickGLeft(MenuItem *item) { ChangeColor((MenuButton *) item, 1, -COLOR_STEP_SIZE); }

    void OnClickGRight(MenuItem *item) { ChangeColor((MenuButton *) item, 1, COLOR_STEP_SIZE); }

    void OnClickBLeft(MenuItem *item) { ChangeColor((MenuButton *) item, 2, -COLOR_STEP_SIZE); }

    void OnClickBRight(MenuItem *item) { ChangeColor((MenuButton *) item, 2, COLOR_STEP_SIZE); }

    void OnClickOffsetLeft(MenuItem *item) { ChangeOffset((MenuButton *) item, -IPD_STEP_SIZE); }

    void OnClickOffsetRight(MenuItem *item) { ChangeOffset((MenuButton *) item, IPD_STEP_SIZE); }

    void OnClickResetOffset(MenuItem *item) {
        threedeeIPD = 0;
        ChangeOffset((MenuButton *) item, 0);
    }

    void InitSettingsMenu(int &posX, int &posY, Menu &settingsMenu) {
        //MenuButton *curveButton =
        //    new MenuButton(&fontMenu, texturePaletteIconId, "", posX, posY += menuItemSize,
        //                   OnClickCurveScreen, nullptr, nullptr);

        MenuButton *screenModeButton =
                new MenuButton(&fontMenu, threedeeIconId, "", posX, posY += menuItemSize, OnClickScreenMode, OnClickScreenMode, OnClickScreenMode);

        MenuButton *offsetButton =
                new MenuButton(&fontMenu, textureIpdIconId, "", posX, posY += menuItemSize, OnClickResetOffset, OnClickOffsetLeft,
                               OnClickOffsetRight);

        MenuButton *paletteButton = new MenuButton(&fontMenu, texturePaletteIconId, "", posX, posY += menuItemSize + 5, OnClickPrefabColorRight,
                                                   OnClickPrefabColorLeft, OnClickPrefabColorRight);

        rButton = new MenuButton(&fontMenu, texturePaletteIconId, "", posX, posY += menuItemSize, nullptr, OnClickRLeft, OnClickRRight);
        gButton = new MenuButton(&fontMenu, texturePaletteIconId, "", posX, posY += menuItemSize, nullptr, OnClickGLeft, OnClickGRight);
        bButton = new MenuButton(&fontMenu, texturePaletteIconId, "", posX, posY += menuItemSize, nullptr, OnClickBLeft, OnClickBRight);

        //settingsMenu.MenuItems.push_back(curveButton);
        settingsMenu.MenuItems.push_back(screenModeButton);
        settingsMenu.MenuItems.push_back(offsetButton);
        settingsMenu.MenuItems.push_back(paletteButton);
        settingsMenu.MenuItems.push_back(rButton);
        settingsMenu.MenuItems.push_back(gButton);
        settingsMenu.MenuItems.push_back(bButton);

        ChangeOffset(offsetButton, 0);
        SetThreeDeeMode(screenModeButton, useThreeDeeMode);
        ChangePalette(paletteButton, 0);
    }

    void OnClickRom(Rom *rom) {
        OVR_LOG("LOAD ROM");
        LoadGame(rom);
        ResetMenuState();
    }

    void InitRomSelectionMenu(int posX, int posY, Menu &romSelectionMenu) {
        // rom list
        romList = new MenuList<Rom>(&fontList, OnClickRom, romFileList, 10, HEADER_HEIGHT + 10,
                                    MENU_WIDTH - 20, (MENU_HEIGHT - HEADER_HEIGHT - BOTTOM_HEIGHT - 20));

        if (romSelection < 0 || romSelection >= romList->ItemList->size())
            romSelection = 0;

        romList->CurrentSelection = romSelection;
        romSelectionMenu.MenuItems.push_back(romList);
    }

    void SaveEmulatorSettings(std::ofstream *saveFile) {
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

    void LoadEmulatorSettings(std::ifstream *readFile) {

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

    void AddRom(std::string strFullPath, std::string strFilename) {
        size_t lastIndex = strFilename.find_last_of(".");
        std::string listName = strFilename.substr(0, lastIndex);
        size_t lastIndexSave = (strFullPath).find_last_of(".");
        std::string listNameSave = strFullPath.substr(0, lastIndexSave);

        Rom newRom;
        newRom.RomName = listName;
        newRom.FullPath = strFullPath;
        newRom.FullPathNorm = listNameSave;
        newRom.SavePath = listNameSave + ".srm";

        romFileList->push_back(newRom);

        OVR_LOG("found rom: %s %s %s", newRom.RomName.c_str(), newRom.FullPath.c_str(),
                newRom.SavePath.c_str());
    }

    // sort the roms by name
    bool SortByRomName(const Rom &first, const Rom &second) {
        return first.RomName < second.RomName;
    }

    void SortRomList() {
        OVR_LOG("sort list");
        std::sort(romFileList->begin(), romFileList->end(), SortByRomName);
        OVR_LOG("finished sorting list");
    }

    void ResetGame() {
        VRVB::Reset();
    }

    void SaveRam() {
        if (CurrentRom != nullptr && VRVB::save_ram_size() > 0) {
            OVR_LOG("save ram %i", (int) VRVB::save_ram_size());
            std::ofstream outfile(CurrentRom->SavePath, std::ios::trunc | std::ios::binary);
            outfile.write((const char *) VRVB::save_ram(), VRVB::save_ram_size());
            outfile.close();
            OVR_LOG("finished writing ram file");
        }
    }

    void LoadRam() {
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

    void SaveState(int slot) {
        // get the size of the savestate
        size_t size = VRVB::retro_serialize_size();

        if (size > 0) {
            std::string savePath = stateFolderPath + CurrentRom->RomName + ".state";
            if (saveSlot > 0) savePath += to_string(saveSlot);

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
        memcpy(currentGame->saveStates[saveSlot].saveImage, screenData,
               sizeof(uint8_t) * VIDEO_WIDTH * VIDEO_HEIGHT);
        OVR_LOG("update image");
        UpdateStateImage(saveSlot);
        // save image for the slot
        SaveStateImage(saveSlot);
        currentGame->saveStates[saveSlot].hasImage = true;
        currentGame->saveStates[saveSlot].hasState = true;
    }

    void LoadState(int slot) {
        std::string savePath = stateFolderPath + CurrentRom->RomName + ".state";
        if (slot > 0) savePath += to_string(slot);

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

    void ChangeButtonMapping(int buttonIndex, int dir) {}

    void UpdateButtonMapping() {
        for (int i = 0; i < buttonCount; ++i) {
            buttonMapping[i].Buttons[0].Button = ButtonMapping[buttonMapping[i].Buttons[0].ButtonIndex];
        }
    }

    void ResetButtonMapping() {
        for (int i = 0; i < buttonCount; ++i) {
            buttonMapping[i].Buttons[0].InputDevice = DeviceGamepad;
            buttonMapping[i].Buttons[0].IsSet = true;
            buttonMapping[i].Buttons[1].IsSet = true;
        }

//        {&textureButtonAIconId, &textureButtonBIconId, &mappingTriggerRight, &mappingTriggerLeft, &mappingRightUpId, &mappingRightRightId,
//                    &mappingLeftRightId, &mappingLeftLeftId, &mappingLeftDownId, &mappingLeftUpId, &mappingStartId, &mappingSelectId,
//                    &mappingRightLeftId, &mappingRightDownId};

        // gamepad mapping
        buttonMapping[0].Buttons[0].Button = EmuButton_A;
        buttonMapping[1].Buttons[0].Button = EmuButton_B;
        buttonMapping[2].Buttons[0].Button = EmuButton_RShoulder;
        buttonMapping[3].Buttons[0].Button = EmuButton_LShoulder;
        buttonMapping[4].Buttons[0].Button = EmuButton_RightStickUp;
        buttonMapping[5].Buttons[0].Button = EmuButton_RightStickRight;
        buttonMapping[6].Buttons[0].Button = EmuButton_Right;
        buttonMapping[7].Buttons[0].Button = EmuButton_Left;
        buttonMapping[8].Buttons[0].Button = EmuButton_Down;
        buttonMapping[9].Buttons[0].Button = EmuButton_Up;
        buttonMapping[10].Buttons[0].Button = EmuButton_Enter;
        buttonMapping[11].Buttons[0].Button = EmuButton_Back;
        buttonMapping[12].Buttons[0].Button = EmuButton_RightStickLeft;
        buttonMapping[13].Buttons[0].Button = EmuButton_RightStickDown;

        // touch controller mapping
        buttonMapping[0].Buttons[1].InputDevice = DeviceRightTouch;
        buttonMapping[0].Buttons[1].Button = EmuButton_A;
        buttonMapping[1].Buttons[1].InputDevice = DeviceRightTouch;
        buttonMapping[1].Buttons[1].Button = EmuButton_B;
        buttonMapping[2].Buttons[1].InputDevice = DeviceRightTouch;
        buttonMapping[2].Buttons[1].Button = EmuButton_Trigger;
        buttonMapping[3].Buttons[1].InputDevice = DeviceLeftTouch;
        buttonMapping[3].Buttons[1].Button = EmuButton_Trigger;
        buttonMapping[4].Buttons[1].InputDevice = DeviceRightTouch;
        buttonMapping[4].Buttons[1].Button = EmuButton_Up;
        buttonMapping[5].Buttons[1].InputDevice = DeviceRightTouch;
        buttonMapping[5].Buttons[1].Button = EmuButton_Right;
        buttonMapping[6].Buttons[1].InputDevice = DeviceLeftTouch;
        buttonMapping[6].Buttons[1].Button = EmuButton_Right;
        buttonMapping[7].Buttons[1].InputDevice = DeviceLeftTouch;
        buttonMapping[7].Buttons[1].Button = EmuButton_Left;
        buttonMapping[8].Buttons[1].InputDevice = DeviceLeftTouch;
        buttonMapping[8].Buttons[1].Button = EmuButton_Down;
        buttonMapping[9].Buttons[1].InputDevice = DeviceLeftTouch;
        buttonMapping[9].Buttons[1].Button = EmuButton_Up;
        buttonMapping[10].Buttons[1].InputDevice = DeviceLeftTouch;
        buttonMapping[10].Buttons[1].Button = EmuButton_X;
        buttonMapping[11].Buttons[1].InputDevice = DeviceLeftTouch;
        buttonMapping[11].Buttons[1].Button = EmuButton_Y;
        buttonMapping[12].Buttons[1].InputDevice = DeviceRightTouch;
        buttonMapping[12].Buttons[1].Button = EmuButton_Left;
        buttonMapping[13].Buttons[1].InputDevice = DeviceRightTouch;
        buttonMapping[13].Buttons[1].Button = EmuButton_Down;

        // TODO: make this nicer...
        for (int j = 0; j < buttonCount; ++j) {
            for (int i = 0; i < EmuButtonCount; ++i) {
                if (buttonMapping[j].Buttons[0].Button == ButtonMapping[i])
                    buttonMapping[j].Buttons[0].ButtonIndex = i;
                if (buttonMapping[j].Buttons[1].Button == ButtonMapping[i])
                    buttonMapping[j].Buttons[1].ButtonIndex = i;
            }
        }
    }

    void Update(const ovrFrameInput &vrFrame, uint *buttonState, uint *lastButtonState) {
        // methode will only get called "emulationSpeed" times a second
        frameCounter += vrFrame.DeltaSeconds;
        if (frameCounter < 1 / emulationSpeed) {
            return;
        }
        frameCounter -= 1 / emulationSpeed;

        // TODO
        VRVB::input_buf[0] = 0;

        for (int i = 0; i < buttonCount; ++i)
            VRVB::input_buf[0] |=
                    ((buttonMapping[i].Buttons[0].IsSet &&
                      (buttonState[buttonMapping[i].Buttons[0].InputDevice] & buttonMapping[i].Buttons[0].Button)) ||
                     (buttonMapping[i].Buttons[1].IsSet &&
                      (buttonState[buttonMapping[i].Buttons[1].InputDevice] & buttonMapping[i].Buttons[1].Button))) ? (1 << i) : 0;

        VRVB::Run();
    }

// Aspect is width / height
    Matrix4f BoundsScreenMatrix(const Bounds3f &bounds, const float movieAspect) {
        const Vector3f size = bounds.b[1] - bounds.b[0];
        const Vector3f center = bounds.b[0] + size * 0.5f;

        const float screenHeight = size.y;
        const float screenWidth = OVR::Alg::Max(size.x, size.z);

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

    Matrix4f ScreenMatrix() {
        return BoundsScreenMatrix(SceneScreenBounds,
                                  VIDEO_WIDTH
                                  /
                                  (float) VIDEO_HEIGHT); // : ( (float)CurrentMovieWidth / CurrentMovieHeight )
    }

    void DrawScreenLayer(ovrFrameResult &res, const ovrFrameInput &vrFrame) {

        /*
             res.Layers[res.LayerCount].Cube = LayerBuilder::BuildCubeLayer(
              CylinderSwapChainCubeLeft,
              !menuOpen ? CylinderSwapChainCubeRight : CylinderSwapChainCubeLeft, &vrFrame.Tracking,
              followHead);
          res.Layers[res.LayerCount].Cube.Header.Flags |=
              VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
          res.LayerCount++;
         */

        if (useCubeMap) {
            Matrix4f texMatrix[2];

            float imgHeight = 0.5f;// (VIDEO_HEIGHT * 2) / (float) (TextureHeight * 2);
            const Matrix4f stretchTop(
                    1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, imgHeight, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f);
            const Matrix4f stretchBottom(
                    1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, imgHeight, 1 - imgHeight, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f);

            texMatrix[0] = texMatrix[1] = Matrix4f::Identity();

            texMatrix[0] = stretchTop;
            texMatrix[1] = stretchBottom;

            ScreenTexMatrix[0] = texMatrix[0].Transposed();
            ScreenTexMatrix[1] = texMatrix[1].Transposed();

            ScreenTexMatrices.Update(2 * sizeof(Matrix4f), &ScreenTexMatrix[0]);

            ScreenSurfaceDef.graphicsCommand.GpuState.depthEnable = false;
            res.Surfaces.PushBack(ovrDrawSurface(ScreenMatrix(), &ScreenSurfaceDef));
        } else {
            // virtual screen layer
            res.Layers[res.LayerCount].Cylinder = LayerBuilder::BuildGameCylinderLayer3D(
                    CylinderSwapChain, CylinderWidth, CylinderHeight, &vrFrame.Tracking, followHead,
                    !menuOpen && useThreeDeeMode, threedeeIPD);
            res.Layers[res.LayerCount].Cylinder.Header.Flags |=
                    VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
            res.Layers[res.LayerCount].Cylinder.Header.Flags |=
                    VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
            res.LayerCount++;
        }
    }

}  // namespace VBEmulator