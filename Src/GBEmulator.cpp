#include "GBEmulator.h"
#include <sys/stat.h>
#include "Audio/OpenSLWrap.h"
#include "DrawHelper.h"
#include "FontMaster.h"
#include "Global.h"
#include "LayerBuilder.h"
#include "MenuHelper.h"

template<typename T>
std::string to_string(T value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

using namespace OVR;

template<>
void MenuList<GBEmulator::Rom>::DrawTexture(float offsetX, float offsetY, float transparency) {
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
  DrawHelper::DrawTexture(textureWhiteId, PosX + offsetX, PosY + recPosY, scrollbarWidth, recHeight,
                          sliderColor, transparency);

  // draw the cartridge icons
  for (uint i = (uint) menuListState; i < menuListState + maxListItems; i++) {
    if (i < ItemList->size()) {
      DrawHelper::DrawTexture(
          ItemList->at(i).isGbc ? textureGbcIconId : textureGbIconId,
          PosX + offsetX + scrollbarWidth + 15 + (((uint) CurrentSelection == i) ? 5 : 0),
          listStartY + listItemSize / 2 - 12 + listItemSize * (i - menuListState) + offsetY, 21, 24,
          {1.0f, 1.0f, 1.0f, 1.0f}, transparency);
    }
  }
}

template<>
void MenuList<GBEmulator::Rom>::DrawText(float offsetX, float offsetY, float transparency) {
  // draw rom list
  for (uint i = (uint) menuListState; i < menuListState + maxListItems; i++) {
    if (i < ItemList->size()) {
      FontManager::RenderText(
          *Font, ItemList->at(i).RomName,
          PosX + offsetX + scrollbarWidth + 44 + (((uint) CurrentSelection == i) ? 5 : 0),
          listStartY + itemOffsetY + listItemSize * (i - menuListState) + offsetY, 1.0f,
          ((uint) CurrentSelection == i) ? textSelectionColor : textColor, transparency);
    } else
      break;
  }
}

namespace GBEmulator {

const std::string romFolderPath = "/Roms/GB/";

const std::vector<std::string> supportedFileNames = {".gb", ".dmg", ".gbc", ".cgb", ".sgb"};

const int VIDEO_WIDTH = 160;
const int VIDEO_HEIGHT = 144;

std::string strForceDMG[2] = {"Force DMG: Yes", "Force DMG: No"};

GB_Color *gearboy_frame_buf;
GearboyCore *core;

GLuint screenTextureId, gbTextureID, stateImageId;
GLuint ScreenFramebuffer = 0;

int romSelection = 0;

s16 audio_buf[AUDIO_BUFFER_SIZE * 2];
int audio_sample_count;
int audio_sample_skip_count;

std::string stateFolderPath;

Rom *CurrentRom;
LoadedGame *currentGame;

const int CylinderWidth = VIDEO_WIDTH * 3;
const int CylinderHeight = VIDEO_HEIGHT * 3;

ovrTextureSwapChain *CylinderSwapChain;

std::vector<Rom> *romFileList = new std::vector<Rom>();

bool forceDMG = false;

int MapButtons[4] = {BUTTON_A, BUTTON_B, BUTTON_X, BUTTON_Y};
int button_mapping[2] = {0, 0};
int button_mapping_index[2] = {1, 0};

bool audioInit = false;
int selectedPalette = 15;

uint32_t *texData;

MenuList<Rom> *romList;
MenuImage *imagePalette[4];

MenuLabel *emptySlotLabel, *noImageSlotLabel;

const int paletteCount = 30;
// clang-format off
GB_Color palettes[paletteCount][4] = {
    {{0xFF, 0xFF, 0xFF, 0xFF}, {0xB6, 0xB6, 0xB6, 0xFF}, {0x67, 0x67, 0x67, 0xFF},
     {0x00, 0x00, 0x00, 0xFF}},
    {{0xE0, 0xDB, 0xCD, 0xFF}, {0xA8, 0x9F, 0x94, 0xFF}, {0x70, 0x6B, 0x66, 0xFF},
     {0x2B, 0x2B, 0x26, 0xFF}},
    {{0xDB, 0xF4, 0xB4, 0xFF}, {0xAB, 0xC3, 0x96, 0xFF}, {0x7B, 0x92, 0x78, 0xFF},
     {0x4C, 0x62, 0x5A, 0xFF}},
    {{0xC4, 0xCF, 0xA1, 0xFF}, {0x8B, 0x95, 0x6D, 0xFF}, {0x4D, 0x53, 0x3C, 0xFF},
     {0x1F, 0x1F, 0x1F, 0xFF}},
    {{0xE3, 0xEE, 0xC0, 0xFF}, {0xAE, 0xBA, 0x89, 0xFF}, {0x5E, 0x67, 0x45, 0xFF},
     {0x20, 0x20, 0x20, 0xFF}},
    {{0xAC, 0xB5, 0x6B, 0xFF}, {0x76, 0x84, 0x48, 0xFF}, {0x3F, 0x50, 0x3F, 0xFF},
     {0x24, 0x31, 0x37, 0xFF}},
    {{0x7E, 0x84, 0x16, 0xFF}, {0x57, 0x7B, 0x46, 0xFF}, {0x38, 0x5D, 0x49, 0xFF},
     {0x2E, 0x46, 0x3D, 0xFF}},
    {{0xA5, 0xEB, 0xD4, 0xFF}, {0x7C, 0xB8, 0x62, 0xFF}, {0x5D, 0x76, 0x27, 0xFF},
     {0x39, 0x39, 0x1D, 0xFF}},
    {{0xEF, 0xFA, 0xF5, 0xFF}, {0x70, 0xC2, 0x86, 0xFF}, {0x57, 0x69, 0x2F, 0xFF},
     {0x20, 0x19, 0x0B, 0xFF}},
    {{0xE0, 0xF8, 0xD0, 0xFF}, {0x88, 0xC0, 0x70, 0xFF}, {0x34, 0x68, 0x56, 0xFF},
     {0x08, 0x18, 0x20, 0xFF}},
    {{0xC4, 0xF0, 0xC2, 0xFF}, {0x5A, 0xB9, 0xA8, 0xFF}, {0x1E, 0x60, 0x6E, 0xFF},
     {0x2D, 0x1B, 0x00, 0xFF}},
    {{0xFF, 0xFF, 0xB5, 0xFF}, {0x7B, 0xC6, 0x7B, 0xFF}, {0x6B, 0x8C, 0x42, 0xFF},
     {0x5A, 0x39, 0x21, 0xFF}},
    {{0xE2, 0xF3, 0xE4, 0xFF}, {0x94, 0xE3, 0x44, 0xFF}, {0x46, 0x87, 0x8F, 0xFF},
     {0x33, 0x2C, 0x50, 0xFF}},
    {{0xEB, 0xDD, 0x77, 0xFF}, {0xA1, 0xBC, 0x00, 0xFF}, {0x0D, 0x88, 0x33, 0xFF},
     {0x00, 0x43, 0x33, 0xFF}},
    {{0x03, 0x96, 0x87, 0xFF}, {0x03, 0x6B, 0x4D, 0xFF}, {0x03, 0x55, 0x2B, 0xFF},
     {0x03, 0x44, 0x14, 0xFF}},
    {{0xA1, 0xEF, 0x8C, 0xFF}, {0x3F, 0xAC, 0x95, 0xFF}, {0x44, 0x61, 0x76, 0xFF},
     {0x2C, 0x21, 0x37, 0xFF}},
    {{0xAA, 0xE0, 0xE0, 0xFF}, {0x7C, 0xB8, 0xB0, 0xFF}, {0x5B, 0x82, 0x72, 0xFF},
     {0x17, 0x34, 0x39, 0xFF}},
    {{0x8B, 0xE5, 0xFF, 0xFF}, {0x60, 0x8F, 0xCF, 0xFF}, {0x75, 0x50, 0xE8, 0xFF},
     {0x62, 0x2E, 0x4C, 0xFF}},
    {{0xCE, 0xCE, 0xCE, 0xFF}, {0x6F, 0x9E, 0xDF, 0xFF}, {0x42, 0x67, 0x8E, 0xFF},
     {0x10, 0x25, 0x33, 0xFF}},
    {{0xC8, 0xE8, 0xF8, 0xFF}, {0x48, 0x90, 0xD8, 0xFF}, {0x20, 0x34, 0xA8, 0xFF},
     {0x50, 0x18, 0x30, 0xFF}},
    {{0xF7, 0xBE, 0xF7, 0xFF}, {0xE7, 0x86, 0x86, 0xFF}, {0x77, 0x33, 0xE7, 0xFF},
     {0x2C, 0x2C, 0x96, 0xFF}},
    {{0xA9, 0x68, 0x68, 0xFF}, {0xED, 0xB4, 0xA1, 0xFF}, {0x76, 0x44, 0x62, 0xFF},
     {0x2C, 0x21, 0x37, 0xFF}},
    {{0xCE, 0xF7, 0xF7, 0xFF}, {0xF7, 0x8E, 0x50, 0xFF}, {0x9E, 0x00, 0x00, 0xFF},
     {0x1E, 0x00, 0x00, 0xFF}},
    {{0xF7, 0xE7, 0xC6, 0xFF}, {0xD6, 0x8E, 0x49, 0xFF}, {0xA6, 0x37, 0x25, 0xFF},
     {0x33, 0x1E, 0x50, 0xFF}},
    {{0xFF, 0xE4, 0xC2, 0xFF}, {0xDC, 0xA4, 0x56, 0xFF}, {0xA9, 0x60, 0x4C, 0xFF},
     {0x42, 0x29, 0x36, 0xFF}},
    {{0xFF, 0xF6, 0xD3, 0xFF}, {0xF9, 0xA8, 0x75, 0xFF}, {0xEB, 0x6B, 0x6F, 0xFF},
     {0x7C, 0x3F, 0x58, 0xFF}},
    {{0xFF, 0xF5, 0xDD, 0xFF}, {0xF4, 0xB2, 0x6B, 0xFF}, {0xB7, 0x65, 0x91, 0xFF},
     {0x65, 0x29, 0x6C, 0xFF}},
    {{0xFF, 0xEF, 0xFF, 0xFF}, {0xF7, 0xB5, 0x8C, 0xFF}, {0x84, 0x73, 0x9C, 0xFF},
     {0x18, 0x10, 0x10, 0xFF}},
    {{0xEF, 0xF7, 0xB6, 0xFF}, {0xDF, 0xA6, 0x77, 0xFF}, {0x11, 0xC6, 0x00, 0xFF},
     {0x00, 0x00, 0x00, 0xFF}},
    {{0xAE, 0xDF, 0x1E, 0xFF}, {0xB6, 0x25, 0x58, 0xFF}, {0x04, 0x7E, 0x60, 0xFF},
     {0x2C, 0x17, 0x00, 0xFF}},
};
// clang-format on

GB_Color *current_palette = palettes[selectedPalette];

void InitScreen();
void InitStateImage();
void SetPalette(GB_Color *newPalette);
void LoadRam();
bool LoadStateImage(int slot);

void Init(std::string stateFolder) {
  stateFolderPath = stateFolder;

  gearboy_frame_buf = new GB_Color[VIDEO_WIDTH * VIDEO_HEIGHT];
  texData = (uint32_t *) malloc(CylinderWidth * CylinderHeight * sizeof(uint32_t));

  InitScreen();
  InitStateImage();

  LOG("init emulator");
  core = new GearboyCore();
  core->Init();

  currentGame = new LoadedGame();
  for (int i = 0; i < 10; ++i) {
    currentGame->saveStates[i].saveImage = new GB_Color[VIDEO_WIDTH * VIDEO_HEIGHT]();
  }

  for (int i = 0; i < 2; ++i) {
    button_mapping[i] = MapButtons[button_mapping_index[i]];
  }

  SetPalette(palettes[selectedPalette]);
}

void Delete() { SafeDelete(core); }

void UpdatePalettes() {
  for (int i = 0; i < 4; ++i)
    imagePalette[i]->Color = {
        palettes[selectedPalette][i].red / 255.0f, palettes[selectedPalette][i].green / 255.0f,
        palettes[selectedPalette][i].blue / 255.0f, palettes[selectedPalette][i].alpha / 255.0f};
}

void ChangePalette(int dir) {
  selectedPalette += dir;

  if (selectedPalette < 0) {
    selectedPalette = paletteCount - 1;
  } else if (selectedPalette >= paletteCount) {
    selectedPalette = 0;
  }

  SetPalette(palettes[selectedPalette]);
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

  // check if it is a gbc rom
  newRom.isGbc = (strFilename.find(".gbc") != std::string::npos ||
      strFilename.find(".cgb") != std::string::npos);

  romFileList->push_back(newRom);

  LOG("found rom: %s %s %s", newRom.RomName.c_str(), newRom.FullPath.c_str(),
      newRom.SavePath.c_str());
}

// sort the roms by type (gb or gbc) and then name
bool SortByRomName(const Rom &first, const Rom &second) {
  return (first.isGbc == second.isGbc) ? first.RomName < second.RomName : first.isGbc;
}

void InitScreen() {
  // emu screen layer
  CylinderSwapChain = vrapi_CreateTextureSwapChain(VRAPI_TEXTURE_TYPE_2D, VRAPI_TEXTURE_FORMAT_8888,
                                                   CylinderWidth, CylinderHeight, 1, false);

  screenTextureId = vrapi_GetTextureSwapChainHandle(CylinderSwapChain, 0);
  glBindTexture(GL_TEXTURE_2D, screenTextureId);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, CylinderWidth, CylinderHeight, GL_RGBA, GL_UNSIGNED_BYTE,
                  NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  GLfloat borderColor[] = {1.0f, 0.0f, 0.0f, 1.0f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

  glBindTexture(GL_TEXTURE_2D, 0);

  // emulator output texture
  glGenTextures(1, &gbTextureID);
  glBindTexture(GL_TEXTURE_2D, gbTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VIDEO_WIDTH, VIDEO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

  glBindTexture(GL_TEXTURE_2D, 0);

  // create the framebuffer for the screen texture
  glGenFramebuffers(1, &ScreenFramebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, ScreenFramebuffer);
  // Set "renderedTexture" as our colour attachement #0
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTextureId, 0);
  // Set the list of draw buffers.
  GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
  glDrawBuffers(1, DrawBuffers);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void InitStateImage() {
  glGenTextures(1, &stateImageId);
  glBindTexture(GL_TEXTURE_2D, stateImageId);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VIDEO_WIDTH, VIDEO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void UpdateStateImage(int saveSlot) {
  glBindTexture(GL_TEXTURE_2D, stateImageId);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE,
                  currentGame->saveStates[saveSlot].saveImage);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void UpdateScreen() {
  // TODO: look into making this faster
  // update the emulator texture
  glBindTexture(GL_TEXTURE_2D, gbTextureID);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE,
                  gearboy_frame_buf);
  glBindTexture(GL_TEXTURE_2D, 0);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
  glBlendEquation(GL_FUNC_ADD);

  // render image to the screen texture
  glBindFramebuffer(GL_FRAMEBUFFER, ScreenFramebuffer);

  glViewport(0, 0, CylinderWidth, CylinderHeight);
  glClearColor(1.0f, 0.5f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // @HACK: make DrawTexture better
  // 640, 576 is used because it is the full size of the projection set before
  DrawHelper::DrawTexture(gbTextureID, 0, 0, 640, 576, {1.0f, 1.0f, 1.0f, 1.0f}, 1);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ResetGame() { core->ResetROMPreservingRAM(forceDMG); }

void SaveEmulatorSettings(std::ofstream *saveFile) {
  saveFile->write(reinterpret_cast<const char *>(&romList->CurrentSelection), sizeof(int));
  saveFile->write(reinterpret_cast<const char *>(&selectedPalette), sizeof(int));
  saveFile->write(reinterpret_cast<const char *>(&forceDMG), sizeof(bool));

  // save button mapping
  for (int i = 0; i < 2; ++i) {
    saveFile->write(reinterpret_cast<const char *>(&button_mapping_index[i]), sizeof(int));
  }
}

void LoadEmulatorSettings(std::ifstream *loadFile) {
  loadFile->read((char *) &romSelection, sizeof(int));
  loadFile->read((char *) &selectedPalette, sizeof(int));
  loadFile->read((char *) &forceDMG, sizeof(bool));

  // load button mapping
  for (int i = 0; i < 2; ++i) {
    loadFile->read((char *) &button_mapping_index[i], sizeof(int));
  }
}

void UpdateButtonMapping() {
  for (int i = 0; i < 2; ++i) {
    button_mapping[i] = MapButtons[button_mapping_index[i]];
  }
}

void ChangeButtonMapping(int buttonIndex, int dir) {
  button_mapping_index[buttonIndex] += dir;

  if (button_mapping_index[buttonIndex] < 0) button_mapping_index[buttonIndex] = 3;
  if (button_mapping_index[buttonIndex] > 3) button_mapping_index[buttonIndex] = 0;

  button_mapping[buttonIndex] = MapButtons[button_mapping_index[buttonIndex]];
}

void LoadRom(std::string path) {
  std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
  if (file.is_open()) {
    long romBufferSize = file.tellg();
    u8 *memblock = new u8[romBufferSize];
    file.seekg(0, std::ios::beg);
    file.read((char *) memblock, romBufferSize);
    file.close();
    LOG("loaded file %ld", romBufferSize);

    LOG("try loading rom in core");
    bool loadedCore = core->LoadROMFromBuffer(memblock, (int) romBufferSize, forceDMG);
    LOG("loaded rom: %i", loadedCore);

    delete[] memblock;
  } else {
    LOG("could not load rom file: %s", path.c_str());
  }
}

bool StateExists(int slot) {
  std::string savePath = stateFolderPath + CurrentRom->RomName + ".state";
  if (slot > 0) savePath += to_string(slot);
  struct stat buffer;
  return (stat(savePath.c_str(), &buffer) == 0);
}

void LoadGame(Rom *rom) {
  LOG("save ram");
  SaveRam();
  LOG("load ram");

  CurrentRom = rom;

  LoadRom(rom->FullPath);

  LOG("try loading ram");
  LoadRam();
  LOG("end loading ram");

  for (int i = 0; i < 10; ++i) {
    if (!LoadStateImage(i)) {
      currentGame->saveStates[i].hasImage = false;

      // clear memory
      memset(currentGame->saveStates[i].saveImage, 0,
             sizeof(GB_Color) * VIDEO_WIDTH * VIDEO_HEIGHT);
    } else {
      currentGame->saveStates[i].hasImage = true;
    }

    currentGame->saveStates[i].hasState = StateExists(i);
  }

  UpdateStateImage(0);
}

void SetPalette(GB_Color *newPalette) {
  current_palette = newPalette;
  core->SetDMGPalette(current_palette[0], current_palette[1], current_palette[2],
                      current_palette[3]);

  // only update if it is in gb mode
  if (!core->IsCGB()) {
    core->RenderDMGFrame(gearboy_frame_buf);
    UpdateScreen();
  }
}

void SaveRam() {
  if (CurrentRom != nullptr && core->GetMemory()->GetCurrentRule()->GetRamSize() > 0) {
    LOG("save ram %s", CurrentRom->SavePath.c_str());
    std::ofstream outfile(CurrentRom->SavePath, std::ios::trunc | std::ios::binary);
    outfile.write((const char *) core->GetMemory()->GetCurrentRule()->GetRamBanks(),
                  core->GetMemory()->GetCurrentRule()->GetRamSize());
    outfile.close();

    LOG("finished writing ram file");
  }
}

void LoadRam() {
  std::ifstream file(CurrentRom->SavePath, std::ios::in | std::ios::binary | std::ios::ate);
  if (file.is_open()) {
    long romBufferSize = file.tellg();
    u8 *memblock = new u8[romBufferSize];
    file.seekg(0, std::ios::beg);
    file.read((char *) memblock, romBufferSize);
    file.close();
    LOG("loaded ram %ld", romBufferSize);

    LOG("ram size %u", (unsigned int) core->GetMemory()->GetCurrentRule()->GetRamSize());

    if (romBufferSize != (unsigned int) core->GetMemory()->GetCurrentRule()->GetRamSize()) {
      LOG("ERROR loaded ram size is wrong");
    } else {
      memcpy(core->GetMemory()->GetCurrentRule()->GetRamBanks(), memblock,
             core->GetMemory()->GetCurrentRule()->GetRamSize());
      LOG("finished loading ram");
    }

    delete[] memblock;
  } else {
    LOG("could not load ram file: %s", CurrentRom->SavePath.c_str());
  }
}

void SaveStateImage(int slot) {
  std::string savePath = stateFolderPath + CurrentRom->RomName + ".stateimg";
  if (slot > 0) savePath += to_string(slot);

  LOG("save image of slot to %s", savePath.c_str());
  std::ofstream outfile(savePath, std::ios::trunc | std::ios::binary);
  outfile.write((const char *) currentGame->saveStates[slot].saveImage,
                sizeof(GB_Color) * VIDEO_WIDTH * VIDEO_HEIGHT);
  outfile.close();
  LOG("finished writing save image to file");
}

bool LoadStateImage(int slot) {
  std::string savePath = stateFolderPath + CurrentRom->RomName + ".stateimg";
  if (slot > 0) savePath += to_string(slot);

  std::ifstream file(savePath, std::ios::in | std::ios::binary | std::ios::ate);
  if (file.is_open()) {
    u8 *memblock = new u8[sizeof(GB_Color) * VIDEO_WIDTH * VIDEO_HEIGHT];
    file.seekg(0, std::ios::beg);
    file.read((char *) memblock, sizeof(GB_Color) * VIDEO_WIDTH * VIDEO_HEIGHT);
    file.close();

    memcpy(currentGame->saveStates[slot].saveImage, memblock,
           sizeof(GB_Color) * VIDEO_WIDTH * VIDEO_HEIGHT);

    delete[] memblock;

    LOG("loaded image file: %s", savePath.c_str());

    return true;
  }

  LOG("could not load image file: %s", savePath.c_str());
  return false;
}

void SaveState(int saveSlot) {
  std::string savePath = stateFolderPath + CurrentRom->RomName + ".state";
  if (saveSlot > 0) savePath += to_string(saveSlot);

  // get the size of the savestate
  size_t size = 0;
  core->SaveState(NULL, size);

  if (size > 0) {
    u8 *memblock = new u8[size];

    if (core->SaveState(memblock, size)) {
      LOG("save slot to %s", savePath.c_str());
      std::ofstream outfile(savePath, std::ios::trunc | std::ios::binary);
      outfile.write((const char *) memblock, size);
      outfile.close();
      LOG("finished writing slot to file");
    }
  }

  LOG("copy image");
  memcpy(currentGame->saveStates[saveSlot].saveImage, gearboy_frame_buf,
         sizeof(GB_Color) * VIDEO_WIDTH * VIDEO_HEIGHT);
  LOG("update image");
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
    long romBufferSize = file.tellg();
    u8 *memblock = new u8[romBufferSize];
    file.seekg(0, std::ios::beg);
    file.read((char *) memblock, romBufferSize);
    file.close();
    LOG("loaded slot has size: %ld", romBufferSize);

    core->LoadState(memblock, romBufferSize);

    delete[] memblock;
  } else {
    LOG("could not load ram file: %s", CurrentRom->SavePath.c_str());
  }
}

void AudioFrame(s16 *audio, unsigned sampleCount) {
  if (!audioInit) {
    audioInit = true;
    StartPlaying();
  }

  SetBuffer((unsigned short *) audio, sampleCount);
}

void UpdateCoreInput(const ovrFrameInput &vrFrame, unsigned int lastButtonState) {
  if (vrFrame.Input.buttonState & BUTTON_LSTICK_UP || vrFrame.Input.buttonState & BUTTON_DPAD_UP)
    core->KeyPressed(Up_Key);
  else
    core->KeyReleased(Up_Key);

  if (vrFrame.Input.buttonState & BUTTON_LSTICK_DOWN ||
      vrFrame.Input.buttonState & BUTTON_DPAD_DOWN)
    core->KeyPressed(Down_Key);
  else
    core->KeyReleased(Down_Key);

  if (vrFrame.Input.buttonState & BUTTON_LSTICK_LEFT ||
      vrFrame.Input.buttonState & BUTTON_DPAD_LEFT)
    core->KeyPressed(Left_Key);
  else
    core->KeyReleased(Left_Key);

  if (vrFrame.Input.buttonState & BUTTON_LSTICK_RIGHT ||
      vrFrame.Input.buttonState & BUTTON_DPAD_RIGHT)
    core->KeyPressed(Right_Key);
  else
    core->KeyReleased(Right_Key);

  if (vrFrame.Input.buttonState & button_mapping[0] && !(lastButtonState & button_mapping[0]))
    core->KeyPressed(A_Key);
  else if (!(vrFrame.Input.buttonState & button_mapping[0]) && lastButtonState & button_mapping[0])
    core->KeyReleased(A_Key);

  if (vrFrame.Input.buttonState & button_mapping[1] && !(lastButtonState & button_mapping[1]))
    core->KeyPressed(B_Key);
  else if (!(vrFrame.Input.buttonState & button_mapping[1]) && lastButtonState & button_mapping[1])
    core->KeyReleased(B_Key);

  if (vrFrame.Input.buttonState & BUTTON_START && !(lastButtonState & BUTTON_START))
    core->KeyPressed(Start_Key);
  else if (!(vrFrame.Input.buttonState & BUTTON_START) && lastButtonState & BUTTON_START)
    core->KeyReleased(Start_Key);

  if (vrFrame.Input.buttonState & BUTTON_SELECT && !(lastButtonState & BUTTON_SELECT))
    core->KeyPressed(Select_Key);
  else if (!(vrFrame.Input.buttonState & BUTTON_SELECT) && lastButtonState & BUTTON_SELECT)
    core->KeyReleased(Select_Key);
}

void MergeAudioBuffer() {
  int newLength = audio_sample_skip_count / 2 + audio_sample_count / 2;
  if (audio_sample_skip_count < audio_sample_count)
    newLength = audio_sample_skip_count;
  else
    newLength = audio_sample_count;

  LOG("audio size: %i, %i, %i", audio_sample_count, audio_sample_skip_count, newLength);

  /*
  for (int i = 0; i < audio_sample_count / 2; ++i) {
      audio_buf[i * 2] = audio_buf[i *
                                   4];// (s16) (((int)audio_buf[i * 4] +
  (int)audio_buf[i * 4 + 2]) / 2); audio_buf[i * 2 + 1] = audio_buf[i * 4 +
                                       1];//(s16) (((int)audio_buf[i * 4 + 1] +
  (int)audio_buf[i * 4 + 3]) / 2);
  }
  for (int i = 0; i < audio_sample_skip_count / 2; ++i) {
      audio_buf[audio_sample_count / 2 + i * 2] = audio_buf_skip[i *
                                                                 4];// (s16)
  (((int)audio_buf_skip[i * 4] + (int)audio_buf_skip[i * 4 + 2]) / 2);
      audio_buf[audio_sample_count / 2 + i * 2 + 1] = audio_buf_skip[i * 4 +
                                                                     1];//(s16)
  (((int)audio_buf_skip[i * 4 + 1] + (int)audio_buf_skip[i * 4 + 3]) / 2);
  }
  */

  int blockStart = 0;
  int blockSize = 160;
  for (int i = 0; i < newLength; ++i) {
    blockStart = (i / blockSize) * (blockSize * 2) + i % blockSize;
    audio_buf[i] = audio_buf[blockStart];
  }

  audio_sample_count = newLength;
}

void Update(const ovrFrameInput &vrFrame, unsigned int lastButtonState) {
  UpdateCoreInput(vrFrame, lastButtonState);

  // update the emulator
  core->RunToVBlank(gearboy_frame_buf, audio_buf, &audio_sample_count);

  if (vrFrame.Input.buttonState & BUTTON_LEFT_TRIGGER) {
    /*
    // append the audio buffer
    audio_buf_skip = &audio_buf[audio_sample_count];
    core->RunToVBlank(gearboy_frame_buf, audio_buf_skip,
    &audio_sample_skip_count); MergeAudioBuffer();
    */
    core->RunToVBlank(gearboy_frame_buf, audio_buf, &audio_sample_count);
  }
  if (vrFrame.Input.buttonState & BUTTON_RIGHT_TRIGGER) {
    core->RunToVBlank(gearboy_frame_buf, audio_buf, &audio_sample_count);
    core->RunToVBlank(gearboy_frame_buf, audio_buf, &audio_sample_count);
  }

  if (audio_sample_count > 0) {
    // LOG("sample count: %i", audio_sample_count);
    AudioFrame(audio_buf, (unsigned int) audio_sample_count);
    // audio_batch_cb(audio_buf, audio_sample_count / 2);
  }

  audio_sample_count = 0;

  // update draw texture
  UpdateScreen();
}

void DrawScreenLayer(ovrFrameResult &res, const ovrFrameInput &vrFrame) {
  // virtual screen layer
  res.Layers[res.LayerCount].Cylinder = LayerBuilder::BuildGameCylinderLayer(
      CylinderSwapChain, CylinderWidth, CylinderHeight, &vrFrame.Tracking, followHead);
  res.Layers[res.LayerCount].Cylinder.Header.Flags |=
      VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;
  res.LayerCount++;
}

void ChangePaletteMenu(MenuItem *item, int dir) {
  ChangePalette(dir);
  UpdatePalettes();
  ((MenuButton *) item)->Text = "Palette: " + to_string(selectedPalette + 1);
}

void OnClickChangePaletteLeft(MenuItem *item) { ChangePaletteMenu(item, -1); }

void OnClickChangePaletteRight(MenuItem *item) { ChangePaletteMenu(item, 1); }

void SetForceDMG(MenuItem *item, bool newForceDMG) {
  forceDMG = newForceDMG;
  ((MenuButton *) item)->Text = strForceDMG[forceDMG ? 0 : 1];
}

void OnClickEmulatedModel(MenuItem *item) { SetForceDMG(item, !forceDMG); }

void OnClickRom(Rom *rom) {
  LOG("LOAD ROM");
  LoadGame(rom);
  ResetMenuState();
}

void UpdateEmptySlotLabel(MenuItem *item, uint &buttonState, uint &lastButtonState) {
  item->Visible = !currentGame->saveStates[saveSlot].hasState;
}

void UpdateNoImageSlotLabel(MenuItem *item, uint &buttonState, uint &lastButtonState) {
  item->Visible =
      currentGame->saveStates[saveSlot].hasState && !currentGame->saveStates[saveSlot].hasImage;
}

void InitRomSelectionMenu(int posX, int posY, Menu &romSelectionMenu) {
  // rom list
  romList = new MenuList<Rom>(&fontList, OnClickRom, romFileList, 10, HEADER_HEIGHT + 10,
                              MENU_WIDTH - 20, (MENU_HEIGHT - HEADER_HEIGHT - BOTTOM_HEIGHT - 20));
  romList->CurrentSelection = romSelection;
  romSelectionMenu.MenuItems.push_back(romList);
}

void InitSettingsMenu(int &posX, int &posY, Menu &settingsMenu) {
  // state screenshot background
  settingsMenu.MenuItems.push_back(new MenuImage(textureWhiteId, MENU_WIDTH - 320 - 20 - 5,
                                                 HEADER_HEIGHT + 20 - 5, 320 + 10, 288 + 10,
                                                 MenuBackgroundOverlayColor));
  settingsMenu.MenuItems.push_back(new MenuImage(screenTextureId, MENU_WIDTH - 320 - 20,
                                                 HEADER_HEIGHT + 20, 320, 288,
                                                 {1.0f, 1.0f, 1.0f, 1.0f}));

  MenuContainer *menuContainer = new MenuContainer();
  MenuButton *paletteButton = new MenuButton(&fontMenu, texturePaletteIconId, "", posX,
                                             posY += menuItemSize, OnClickChangePaletteRight,
                                             OnClickChangePaletteLeft, OnClickChangePaletteRight);
  menuContainer->MenuItems.push_back(paletteButton);
  for (int i = 0; i < 4; ++i) {
    imagePalette[i] =
        new MenuImage(textureWhiteId, posX + (18) * i + 157, posY + fontMenu.FontSize / 2 - 9, 18,
                      18, {0.0f, 0.0f, 0.0f, 0.0f});
    menuContainer->MenuItems.push_back(imagePalette[i]);
  }

  MenuButton *dmgButton =
      new MenuButton(&fontMenu, textureDMGIconId, "", posX, posY += menuItemSize,
                     OnClickEmulatedModel, OnClickEmulatedModel, OnClickEmulatedModel);

  // set text
  ChangePaletteMenu(paletteButton, 0);
  SetForceDMG(dmgButton, forceDMG);

  settingsMenu.MenuItems.push_back(menuContainer);
  settingsMenu.MenuItems.push_back(dmgButton);

  UpdatePalettes();
}

void InitMainMenu(int posX, int posY, Menu &mainMenu) {
  // main menu
  mainMenu.MenuItems.push_back(new MenuImage(textureWhiteId, MENU_WIDTH - 320 - 20 - 5,
                                             HEADER_HEIGHT + 20 - 5, 320 + 10, 288 + 10,
                                             MenuBackgroundOverlayColor));
  emptySlotLabel = new MenuLabel(&fontSlot, "- Empty Slot -", MENU_WIDTH - 320 - 20,
                                 HEADER_HEIGHT + 20, 320, 288, {1.0f, 1.0f, 1.0f, 1.0f});
  emptySlotLabel->UpdateFunction = UpdateEmptySlotLabel;

  noImageSlotLabel = new MenuLabel(&fontSlot, "- -", MENU_WIDTH - 320 - 20, HEADER_HEIGHT + 20, 320,
                                   288, {1.0f, 1.0f, 1.0f, 1.0f});
  noImageSlotLabel->UpdateFunction = UpdateNoImageSlotLabel;

  mainMenu.MenuItems.push_back(emptySlotLabel);
  mainMenu.MenuItems.push_back(noImageSlotLabel);
  // image slot
  mainMenu.MenuItems.push_back(new MenuImage(stateImageId, MENU_WIDTH - 320 - 20,
                                             HEADER_HEIGHT + 20, 320, 288,
                                             {1.0f, 1.0f, 1.0f, 1.0f}));
}

}