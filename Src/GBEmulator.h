#ifndef GB_EMULATOR_H
#define GB_EMULATOR_H

#include "App.h"
#include <gearboy.h>
#include "MenuHelper.h"

using namespace OVR;

namespace GBEmulator {

struct Rom {
  bool isGbc;
  std::string RomName;
  std::string FullPath;
  std::string FullPathNorm;
  std::string SavePath;
};

struct SaveState {
  bool hasImage;
  bool hasState;
  GB_Color *saveImage;
};

struct LoadedGame {
  SaveState saveStates[10];
};

extern int button_mapping_index[];

extern const std::string romFolderPath;

extern const std::vector<std::string> supportedFileNames;

void Init(std::string stateFolder);

void InitSettingsMenu(int &posX, int &posY, Menu &settingsMenu);

void InitRomSelectionMenu(int posX, int posY, Menu &romSelectionMenu);

void InitMainMenu(int posX, int posY, Menu &mainMenu);

void SaveEmulatorSettings(std::ofstream *outfile);

void LoadEmulatorSettings(std::ifstream *file);

void AddRom(std::string strFullPath, std::string strFilename);

void ResetGame();

void SaveState(int slot);

void LoadState(int slot);

void UpdateStateImage(int saveSlot);

void ChangeButtonMapping(int buttonIndex, int dir);

void UpdateButtonMapping();

void SaveRam();

void Update(const ovrFrameInput &vrFrame, unsigned int lastButtonState);

void DrawScreenLayer(ovrFrameResult &res, const ovrFrameInput &vrFrame);

/*
void UpdatePalettes();

void ChangePalette(MenuItem *item, int dir);

void Delete();

bool SortByRomName(const Rom &first, const Rom &second);

void LoadRom(std::string path);

void LoadGame(Rom *rom);

void OnClickRom(Rom *rom);

void LoadRam();

void SaveStateImage(int slot);

bool LoadStateImage(int slot);

void ChangePalette(int dir);

void MergeAudioBuffer();

void InitScreen();

void InitStateImage();

bool StateExists(int slot);

void UpdateCoreInput(const ovrFrameInput &vrFrame, unsigned int lastButtonState);

void SetPalette(GB_Color *newPalette);

void AudioFrame(s16 *audio, unsigned sampleCount);

void UpdateScreen();

void OnClickChangePaletteLeft(MenuItem *item);

void OnClickChangePaletteRight(MenuItem *item);

void SetForceDMG(MenuItem *item, bool newForceDMG);

void OnClickEmulatedModel(MenuItem *item);

void UpdateEmptySlotLabel(MenuItem *item, int &buttonState, int &lastButtonState);

void UpdateNoImageSlotLabel(MenuItem *item, int &buttonState, int &lastButtonState);
*/

};

#endif