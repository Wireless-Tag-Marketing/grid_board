#pragma once
#include "app_config.hpp"
#include "lvgl.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <map>
#include <memory>

namespace board {
// Build a bounded glyph cache for one displayed message. No FreeType work or
// allocation happens in LVGL's draw callbacks, and cache pixels live in PSRAM.
class BoardFont {
public:
    ~BoardFont();
    bool initialize(int size, const Layout &layout, std::string &error);
    const lv_font_t *font() const { return &font_; }
    size_t missing_glyphs() const { return missing_; }
private:
    struct Glyph {
        lv_font_glyph_dsc_t descriptor{};
        lv_draw_buf_t buffer{};
        uint8_t *pixels = nullptr;
    };
    bool load(uint32_t cp, std::string &error);
    static bool descriptor(const lv_font_t *, lv_font_glyph_dsc_t *, uint32_t, uint32_t);
    static const void *bitmap(lv_font_glyph_dsc_t *, uint32_t, lv_draw_buf_t *);
    FT_Library library_ = nullptr;
    FT_Face cjk_ = nullptr;
    FT_Face emoji_ = nullptr;
    lv_font_t font_{};
    std::map<uint32_t, Glyph> glyphs_;
    size_t missing_ = 0;
};
} // namespace board
