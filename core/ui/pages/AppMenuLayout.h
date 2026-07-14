#pragma once

#include <openxr/openxr.h> // XrColor4f

// Layout constants shared across all menu pages. Mirror AppMenu's static
// members so page .cpp files don't need to include the full AppMenu header.
//
// kMenuWidth/kMenuHeight (and every other constant below) are logical
// units, not physical pixels. AppMenu renders into an offscreen texture
// sized kMenuWidth*kMenuScale x kMenuHeight*kMenuScale (see
// AppMenu::Initialize) - full physical resolution, so text/shapes stay
// crisp - but tells UiRenderer to map that logical 320x240 coordinate
// space across the whole physical viewport (BeginOffscreenFrame's
// logicalWidth/logicalHeight; see its doc comment in UiRenderer.h for the
// mechanism). The composite step then draws that texture 1:1 (AppMenu::Draw).
// At the default 320x240 logical size and 2x scale, that's the same
// 640x480 apparent size the menu always had, but a platform can now pick a
// different kMenuScale (e.g. render sharper into a higher-res VR panel)
// without touching any layout math below - only kMenuScale changes.
//
// Most position/size constants here are float rather than int: halving the
// old 640x480-scale layout doesn't always land on a whole logical-space
// pixel (75/2 = 37.5), and truncating that to 37 would drift the composited
// result off the original by a scaled pixel. Keeping the fraction and
// letting kMenuScale multiply it back out lands exactly on the same final
// pixel instead.
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
inline constexpr const char *kVersionString = "v2.0.0-dev";
