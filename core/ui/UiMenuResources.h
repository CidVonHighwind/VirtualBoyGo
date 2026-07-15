#pragma once

#include "UiFontManager.h" // UiFontHandle
#include "UiIconSet.h"

class Emulator;
class AppMenu;
struct AppSettings;

enum class ButtonMappingProfile
{
    Vr,
    Desktop
};

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
    // Not const - SettingsPage/MoveScreenPage/the button-map pages all write
    // through this directly (single shared instance, see AppSettings' own
    // doc comment) - no per-page copies to keep in sync.
    AppSettings *settings = nullptr;
    ButtonMappingProfile buttonMappingProfile = ButtonMappingProfile::Vr;
};
