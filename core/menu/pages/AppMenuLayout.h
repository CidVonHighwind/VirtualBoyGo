#pragma once

#include "Version.h" // kGeneratedVersionString - build-generated, see cmake/GenerateVersion.cmake

#include <openxr/openxr.h> // XrColor4f

// Layout constants shared across all menu pages, mirroring AppMenu's static
// members so page .cpp files don't need the full AppMenu header.
//
// All values are logical units, not physical pixels. AppMenu renders into an
// offscreen texture sized kMenuWidth*kMenuScale x kMenuHeight*kMenuScale but
// maps this logical 320x240 space across the whole physical viewport (see
// UiRenderer::BeginOffscreenFrame), so a platform can raise kMenuScale for a
// sharper VR panel without touching any layout math below.
//
// Values are float, not int: halving the old 640x480-scale layout lands on
// sub-pixel positions (75/2 = 37.5); keeping the fraction and letting
// kMenuScale multiply it back out hits the exact same final pixel that
// truncating to int would drift off.
inline constexpr int kMenuWidth = 320;
inline constexpr int kMenuHeight = 240;
inline constexpr float kMenuScale = 2.0f;

inline constexpr float kHeaderHeight = 35.0f;
inline constexpr float kBottomHeight = 15.0f;
inline constexpr float kMenuContentPadding = 3.0f;
inline constexpr float kMenuContentX = 10.0f;
inline constexpr float kMenuContentY = kHeaderHeight + kMenuContentPadding;
inline constexpr int kMenuFontSize = 11;
inline constexpr float kMenuItemSize = kMenuFontSize + 3.0f; // 14px

// Height of a MenuList spacer row (see MenuList::AddSpacer), used to
// visually separate logical groups of entries within a list.
inline constexpr float kMenuSpacerSize = kMenuItemSize * 0.3f; // ~4px

// Height of the scrollable list region (fills between header and bottom bar)
inline constexpr float kListWidth = kMenuWidth - kMenuContentX * 2;
inline constexpr float kListHeight = kMenuHeight - kMenuContentY - kBottomHeight - kMenuContentPadding;

// Buffer-space slide distance for the page transition (see AppMenu::RenderContent).
inline constexpr float kTransitionSlideDistance = 37.5f;

inline constexpr XrColor4f kMenuTextColor = {0.8f, 0.8f, 0.8f, 1.0f};
inline constexpr XrColor4f kMenuSelectionColor = {0.9f, 0.1f, 0.1f, 1.0f};

// Header/bottom bar color and the body color between them.
inline constexpr XrColor4f kMenuOverlayColor = {0.35f, 0.35f, 0.35f, 0.98f};
inline constexpr XrColor4f kMenuBodyColor = {0.2f, 0.2f, 0.2f, 0.975f};

// Shared secondary/small-print font (battery %, version string) - smaller
// than kMenuFontSize since it's decorative/informational, not a menu row.
inline constexpr int kSmallFontSize = 8;
inline constexpr XrColor4f kMenuVersionColor = {0.6f, 0.6f, 0.6f, 1.0f};
inline constexpr const char *kVersionString = kGeneratedVersionString;
