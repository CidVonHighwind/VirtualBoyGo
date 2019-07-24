#ifndef VB_EMULATOR_H
#define VB_EMULATOR_H

#include <string>
#include <vector>
#include <VrSamples/FrontendGo/ButtonMapping.h>
#include "App.h"
#include "MenuHelper.h"

using namespace OVR;

namespace Emulator {

    struct Rom {
        std::string RomName;
        std::string FullPath;
        std::string FullPathNorm;
        std::string SavePath;
    };

    struct SaveState {
        bool hasImage;
        bool hasState;
        uint8_t *saveImage;
    };

    struct LoadedGame {
        SaveState saveStates[10];
    };

    const static int buttonCount = 14;
    extern GLuint *button_icons[];
    extern MappedButtons buttonMapping[];
    extern int buttonOrder[14];

    extern const std::string romFolderPath;
    extern const std::vector<std::string> supportedFileNames;

    void Init(std::string stateFolder);

    void InitSettingsMenu(int &posX, int &posY, Menu &settingsMenu);

    void InitRomSelectionMenu(int posX, int posY, Menu &romSelectionMenu);

    void InitMainMenu(int posX, int posY, Menu &mainMenu);

    void SaveEmulatorSettings(std::ofstream *outfile);

    void LoadEmulatorSettings(std::ifstream *file);

    void AddRom(std::string strFullPath, std::string strFilename);

    void SortRomList();

    void ResetGame();

    void SaveState(int slot);

    void LoadState(int slot);

    void UpdateStateImage(int saveSlot);

    void ChangeButtonMapping(int buttonIndex, int dir);

    void UpdateButtonMapping();

    void ResetButtonMapping();

    void SaveRam();

    void Update(const ovrFrameInput &vrFrame, uint* buttonStates, uint* lastButtonStates);

    void DrawScreenLayer(ovrFrameResult &res, const ovrFrameInput &vrFrame);

}  // namespace Emulator

#endif