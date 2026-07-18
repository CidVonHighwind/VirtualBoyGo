#pragma once

#include <string>

// Decodes the next UTF-8 codepoint starting at text[i], advancing i past it.
// Text throughout this UI layer (menu labels, ROM file names, ...) is stored
// as UTF-8 std::string - this is the one place that turns it into codepoints
// for glyph lookup/baking (see UiFontManager::EnsureGlyphsForText). Invalid
// or truncated sequences decode as U+FFFD and advance by a single byte, so a
// malformed string can never get the caller stuck in an infinite loop.
inline char32_t UiDecodeUtf8(const std::string &text, size_t &i)
{
    const unsigned char c0 = static_cast<unsigned char>(text[i]);

    if (c0 < 0x80)
    {
        i += 1;
        return c0;
    }
    if ((c0 & 0xE0) == 0xC0 && i + 1 < text.size())
    {
        const char32_t cp = (static_cast<char32_t>(c0 & 0x1F) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3F);
        i += 2;
        return cp;
    }
    if ((c0 & 0xF0) == 0xE0 && i + 2 < text.size())
    {
        const char32_t cp = (static_cast<char32_t>(c0 & 0x0F) << 12) |
                            ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 6) |
                            (static_cast<unsigned char>(text[i + 2]) & 0x3F);
        i += 3;
        return cp;
    }
    if ((c0 & 0xF8) == 0xF0 && i + 3 < text.size())
    {
        const char32_t cp = (static_cast<char32_t>(c0 & 0x07) << 18) |
                            ((static_cast<unsigned char>(text[i + 1]) & 0x3F) << 12) |
                            ((static_cast<unsigned char>(text[i + 2]) & 0x3F) << 6) |
                            (static_cast<unsigned char>(text[i + 3]) & 0x3F);
        i += 4;
        return cp;
    }

    i += 1;
    return 0xFFFD; // U+FFFD REPLACEMENT CHARACTER
}
