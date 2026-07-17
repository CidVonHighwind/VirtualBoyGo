#pragma once

#include "input/ButtonMapping.h"

#include <openxr/openxr.h> // XrColor4f

#include <cstdint>

// How the screen/menu quads react to head rotation - see
// OpenXrApp::ComputeScreenOrientation.
enum class FollowHeadMode : int32_t
{
    Off = 0,     // world-fixed, no head influence
    Smooth = 1,  // gradually chases the head orientation
    Instant = 2, // rigidly locked to the current head orientation every frame
};

// Persisted app-wide settings, one instance for the app's lifetime, threaded
// to every menu page via UiMenuResources::settings.
struct AppSettings
{
    // Bumped whenever the on-disk layout changes - Load() refuses (leaves
    // defaults in place) on a mismatch rather than attempting migration.
    static constexpr int kVersion = 11;

    // Move Screen / Follow Head. Only OpenXrApp's quad-layer pose consumes
    // these - meaningless on the flat pc2d debug build.
    FollowHeadMode followHeadMode = FollowHeadMode::Off;
    float screenYaw = 0.0f;
    float screenPitch = 0.0f;
    float screenRoll = 0.0f;
    // meters - also doubles as the curved-screen cylinder radius (see
    // OpenXrApp::RenderScreenLayer). Menu distance is separate, see
    // kMenuDistanceMeters.
    float screenDistance = 2.2f;
    float screenScale = 1.0f;
    // Curves the screen into a cylindrical section instead of a flat quad -
    // falls back to flat if the runtime lacks XR_KHR_composition_layer_cylinder
    // (e.g. SteamVR).
    bool curvedScreen = false;

    bool useThreeDeeMode = true;
    float ipdOffset = 0.0f;  // meters
    int selectedPalette = 0; // authentic Virtual Boy red
    float colorR = 1.0f, colorG = 0.0f, colorB = 0.0f;
    // -1 = off (flat colorR/G/B tint above); 0-5 = index into kScreenPatterns,
    // a multi-hue gradient recolor instead. Mutually exclusive with the tint:
    // SettingsPage hides the R/G/B rows while this is >= 0.
    int selectedPattern = -1;

    // VB gameplay button remapping, indexed by VBButtonBit constants (see
    // Emulator.h); slots 1 and 9 are unused gaps and stay unbound. Each
    // button carries two independent physical bindings
    // (MappedButtons::Buttons[0]/[1]) - either triggers it.
    ButtonMapper::MappedButtons vbButtons[16];

    // Best-effort; failures are silently ignored.
    void Save() const;

    // Replaces every field; returns false (self left untouched) if the file
    // doesn't exist or its version doesn't match kVersion.
    bool Load();
};

// 11 preset VB screen colors. Index 0 is the authentic red Virtual Boy
// display and the factory default.
extern const XrColor4f kPredefColors[11];

// 6 named multi-hue gradients (5 stops each, darkest to brightest) - shared
// by screen_pattern.frag's rendering and the Color Palette row's preview
// swatches (SettingsPage::DrawColorPreview), so no color data is duplicated
// into the shader itself.
extern const XrColor4f kScreenPatterns[6][5];
