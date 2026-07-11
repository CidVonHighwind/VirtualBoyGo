#pragma once

#include <openxr/openxr.h> // XrColor4f

// Layout constants shared across all menu pages. Mirror AppMenu's static
// members so page .cpp files don't need to include the full AppMenu header.
inline constexpr int kMenuWidth = 640;
inline constexpr int kMenuHeight = 480;
inline constexpr int kHeaderHeight = 75;
inline constexpr int kBottomHeight = 30;
inline constexpr int kMenuContentPadding = 6;
inline constexpr int kMenuContentX = 20;
inline constexpr int kMenuContentY = kHeaderHeight + kMenuContentPadding;
inline constexpr int kMenuFontSize = 22;
inline constexpr int kMenuItemSize = kMenuFontSize + 6; // 28px

// Height of the scrollable list region (fills between header and bottom bar)
inline constexpr int kListWidth = kMenuWidth - kMenuContentX * 2;
inline constexpr int kListHeight = kMenuHeight - kMenuContentY - kBottomHeight - kMenuContentPadding;

inline constexpr XrColor4f kMenuTextColor = {0.8f, 0.8f, 0.8f, 1.0f};
inline constexpr XrColor4f kMenuSelectionColor = {0.9f, 0.1f, 0.1f, 1.0f};
