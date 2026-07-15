#pragma once

#include "ui/ButtonMapping.h"

#include <openxr/openxr.h> // XrColor4f

#include <cstdint>

// How the screen/menu quads react to head rotation - see
// OpenXrApp::ComputeScreenOrientation.
enum class FollowHeadMode : int32_t
{
    Off = 0,     // world-fixed, no head influence (m_appSpace's own recentering only)
    Smooth = 1,  // gradually chases the head orientation - ported from FrontendGo's
                 // always-on slerp(current, goal, FOLLOW_SPEED*dt) follow
    Instant = 2, // rigidly locked to the current head orientation every frame
};

// Persisted app-wide settings - the new-project equivalent of FrontendGo's
// settings.config (MenuGo::SaveSettings/LoadSettings + Emulator::
// SaveEmulatorSettings/InitSettingsMenu), collapsed into one flat struct
// instead of scattered across Menu/Emulator/LayerBuilder. One instance lives
// for the lifetime of the app (owned by whichever object already owns
// Emulator/AppMenu - OpenXrApp, or pc2d's main()) and is threaded to every
// menu page via UiMenuResources::settings so they all read/write the same
// data with no per-page copies to keep in sync.
struct AppSettings
{
    // Bumped whenever the on-disk layout changes - Load() refuses (leaves
    // defaults in place) on a mismatch rather than attempting any migration,
    // same as FrontendGo's own SAVE_FILE_VERSION check.
    static constexpr int kVersion = 10;

    // Move Screen / Follow Head - ported from FrontendGo's LayerBuilder
    // (screenYaw/screenPitch/screenRoll/radiusMenuScreen/screenSize) and
    // Global::followHead. Only OpenXrApp's quad-layer pose consumes these -
    // meaningless on the flat pc2d debug build.
    FollowHeadMode followHeadMode = FollowHeadMode::Off;
    float screenYaw = 0.0f;
    float screenPitch = 0.0f;
    float screenRoll = 0.0f;
    // meters - the emulator screen only (also doubles as the curved-screen
    // option's cylinder radius, see OpenXrApp::RenderScreenLayer). The
    // menu's own distance is fixed and unaffected - see kMenuDistanceMeters.
    float screenDistance = 2.2f;
    float screenScale = 1.0f;
    // Curves the screen into a cylindrical section (ported from FrontendGo's
    // VrApi ovrLayerCylinder2 screen) instead of a flat quad - see
    // OpenXrApp::RenderScreenLayer. Falls back to flat if the runtime
    // doesn't support XR_KHR_composition_layer_cylinder (e.g. SteamVR).
    bool curvedScreen = false;

    // 3D/2D mode + IPD + color tint - ported from Emulator::InitSettingsMenu
    // (VirtualBoyGoMaster/Src/Emulator.cpp) via the old useThreeDeeMode/
    // threedeeIPD/color[3]/selectedPredefColor fields.
    bool useThreeDeeMode = true;
    float ipdOffset = 0.0f;  // meters, range/step match FrontendGo's IPD_STEP_SIZE/min/maxIPD
    int selectedPalette = 0; // authentic Virtual Boy red
    float colorR = 1.0f, colorG = 0.0f, colorB = 0.0f;

    // VB gameplay button remapping - indexed directly by VBButtonBit
    // constants (see Emulator.h); some slots (1, 9) are unused gaps in that
    // bit layout and stay permanently unbound. Each VB button carries two
    // independent physical bindings (MappedButtons::Buttons[0]/[1]) - either
    // one pressed triggers the button (see TranslateToVBBitmask).
    ButtonMapper::MappedButtons vbButtons[16];

    // Writes the current struct to disk (see SettingsFilePath in Settings.cpp
    // for the path). Best-effort - failures are silently ignored, same as
    // Emulator's save-state I/O.
    void Save() const;

    // Reads the struct from disk, replacing every field - returns false
    // (this object left completely untouched, so whatever defaults the
    // caller already had stay in place) if the file doesn't exist or its
    // version doesn't match kVersion.
    bool Load();
};

// FrontendGo's 11 preset VB screen colors, ported verbatim from
// VirtualBoyGoMaster/Src/Emulator.h's predefColors[11] table. Index 0 is
// the authentic red Virtual Boy display and is the factory default.
extern const XrColor4f kPredefColors[11];
