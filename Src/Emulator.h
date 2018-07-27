#include "KingInclude.h"

class Emulator {
 public:
  struct Rom {};

  struct SaveState {};

  struct LoadedGame {};

  const std::vector<std::string> supportedFileNames;

  std::vector<Rom> *romFileList;

  LoadedGame *currentGame;

  GLuint screenTextureId, stateImageId;

  const int CylinderWidth, CylinderHeight;

  ovrTextureSwapChain *CylinderSwapChain;

  int button_mapping_index[];

  virtual void void Delete() = 0;

  virtual void AddRom(std::string strFullPath, std::string strFilename) = 0;

  virtual void Init(std::string stateFolder) = 0;

  virtual void UpdateStateImage(int saveSlot) = 0;

  virtual void LoadRom(std::string path) = 0;

  virtual void void ResetGame() = 0;

  virtual void void UpdateButtonMapping() = 0;

  virtual void void ChangeButtonMapping(int buttonIndex, int dir) = 0;

  virtual void void SaveSettings(std::ofstream *outfile) = 0;

  virtual void void LoadSettings(std::ifstream *file) = 0;

  virtual void void LoadGame(Rom *rom) = 0;

  virtual void void SaveRam() = 0;

  virtual void void LoadRam() = 0;

  virtual void void SaveStateImage(int slot) = 0;

  virtual void bool LoadStateImage(int slot) = 0;

  virtual void void SaveState(int saveSlot) = 0;

  virtual void void LoadState(int slot) = 0;

  virtual void void Update(const ovrFrameInput &vrFrame, unsigned int lastButtonState) = 0;

  virtual void bool SortByRomName(const Rom &first, const Rom &second) = 0;
};
