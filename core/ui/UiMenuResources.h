#pragma once

#include "UiFontManager.h" // UiFontHandle
#include "UiIconSet.h"

class Emulator;
class AppMenu;

// Shared read-only resources every menu page needs, loaded once by
// AppMenu::Initialize and handed to each page's Init() as a single bundle -
// keeps MenuPage::Init from growing a new parameter every time a page needs
// another shared font/atlas.
struct UiMenuResources
{
    UiFontHandle menuFont;
    UiFontHandle smallFont;
    const UiIconSet *icons = nullptr;
    // Not const - RomSelectPage's press callback calls LoadRom on it, and
    // MainPage/RomSelectPage call Hide() on the AppMenu below to resume
    // gameplay.
    Emulator *emulator = nullptr;
    AppMenu *appMenu = nullptr;
};
