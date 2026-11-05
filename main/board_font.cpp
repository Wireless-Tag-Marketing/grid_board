#include "board_font.hpp"
#include "esp_heap_caps.h"
#include <algorithm>
#include <cstring>
#include FT_MODULE_H

extern const uint8_t cjk_start[] asm("_binary_NotoSansSC_Regular_otf_start");
extern const uint8_t cjk_end[] asm("_binary_NotoSansSC_Regular_otf_end");
extern const uint8_t emoji_start[] asm("_binary_NotoEmoji_Regular_ttf_start");
extern const uint8_t emoji_end[] asm("_binary_NotoEmoji_Regular_ttf_end");

namespace board {
namespace {
// CJK face metadata and FreeType scratch buffers must not consume the small
// internal heap needed by Wi-Fi and task stacks. All callbacks are thread-safe
// heap-cap APIs; the allocator outlives every independently owned FT library.
FT_MemoryRec_ font_memory = {
    nullptr,
    [](FT_Memory, long size) -> void * {
        return size > 0 ? heap_caps_malloc(static_cast<size_t>(size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : nullptr;
    },
    [](FT_Memory, void *block) { heap_caps_free(block); },
    [](FT_Memory, long, long size, void *block) -> void * {
        if (size <= 0) { heap_caps_free(block); return nullptr; }
        return heap_caps_realloc(block, static_cast<size_t>(size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    },
};
}

BoardFont::~BoardFont()
{
    for (auto &entry : glyphs_) heap_caps_free(entry.second.pixels);
    if (emoji_) FT_Done_Face(emoji_);
    if (cjk_) FT_Done_Face(cjk_);
    if (library_) FT_Done_Library(library_);
}

bool BoardFont::initialize(int size, const Layout &layout, std::string &error)
{
    if (FT_New_Library(&font_memory, &library_)) {
        error = "中文字体初始化失败"; return false;
    }
    FT_Add_Default_Modules(library_);
    FT_Set_Default_Properties(library_);
    if (FT_New_Memory_Face(library_, cjk_start, cjk_end - cjk_start, 0, &cjk_) ||
        FT_New_Memory_Face(library_, emoji_start, emoji_end - emoji_start, 0, &emoji_) ||
        FT_Set_Pixel_Sizes(cjk_, 0, size) || FT_Set_Pixel_Sizes(emoji_, 0, size)) {
        error = "中文字体初始化失败"; return false;
    }
    font_.get_glyph_dsc = descriptor;
    font_.get_glyph_bitmap = bitmap;
    // Noto Sans SC ascender + descender is 1.448 em. Use the cell's font area
    // as a common line height so emoji and Chinese are centered consistently.
    font_.line_height = layout.cell_height - 16;
    font_.base_line = (-(cjk_->size->metrics.descender) + 63) / 64;
    font_.dsc = this;
    font_.kerning = LV_FONT_KERNING_NONE;
    if (!load('?', error)) return false;
    for (const auto &cell : layout.cells) if (!load(cell.codepoint, error)) return false;
    return true;
}

bool BoardFont::load(uint32_t cp, std::string &error)
{
    if (glyphs_.count(cp)) return true;
    FT_Face face = cjk_;
    FT_UInt index = FT_Get_Char_Index(face, cp);
    if (!index) { face = emoji_; index = FT_Get_Char_Index(face, cp); }
    if (!index) {
        // The missing glyph uses the cached '?' without owning its bitmap.
        ++missing_;
        glyphs_.emplace(cp, Glyph{});
        return true;
    }
    if (FT_Load_Glyph(face, index, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL)) {
        error = "字形渲染失败"; return false;
    }
    FT_GlyphSlot slot = face->glyph;
    const FT_Bitmap &bitmap = slot->bitmap;
    if (bitmap.width && bitmap.rows && bitmap.pixel_mode != FT_PIXEL_MODE_GRAY) {
        error = "字体不支持灰度渲染"; return false;
    }
    Glyph glyph;
    auto &d = glyph.descriptor;
    d.adv_w = static_cast<uint16_t>((slot->advance.x + 32) / 64);
    d.box_w = bitmap.width;
    d.box_h = bitmap.rows;
    d.ofs_x = slot->bitmap_left;
    d.ofs_y = slot->bitmap_top - static_cast<int>(bitmap.rows);
    d.format = LV_FONT_GLYPH_FORMAT_A8;
    d.glyph_index = cp;
    if (bitmap.width && bitmap.rows) {
        const uint32_t stride = lv_draw_buf_width_to_stride(bitmap.width, LV_COLOR_FORMAT_A8);
        const uint32_t bytes = stride * bitmap.rows;
        glyph.pixels = static_cast<uint8_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!glyph.pixels) { error = "字形缓存内存不足"; return false; }
        std::memset(glyph.pixels, 0, bytes);
        for (unsigned y = 0; y < bitmap.rows; ++y) {
            const uint8_t *src = bitmap.buffer + (bitmap.pitch >= 0 ? y : bitmap.rows - 1 - y) *
                                 std::abs(bitmap.pitch);
            std::memcpy(glyph.pixels + y * stride, src, bitmap.width);
        }
        lv_draw_buf_init(&glyph.buffer, bitmap.width, bitmap.rows, LV_COLOR_FORMAT_A8,
                         stride, glyph.pixels, bytes);
    }
    glyphs_.emplace(cp, glyph);
    return true;
}

bool BoardFont::descriptor(const lv_font_t *font, lv_font_glyph_dsc_t *out, uint32_t cp, uint32_t)
{
    auto *self = static_cast<const BoardFont *>(font->dsc);
    auto item = self->glyphs_.find(cp);
    if (item == self->glyphs_.end() || item->second.descriptor.format == 0) item = self->glyphs_.find('?');
    if (item == self->glyphs_.end()) return false;
    *out = item->second.descriptor;
    out->resolved_font = font;
    return true;
}

const void *BoardFont::bitmap(lv_font_glyph_dsc_t *descriptor, uint32_t, lv_draw_buf_t *)
{
    auto *self = static_cast<const BoardFont *>(descriptor->resolved_font->dsc);
    auto item = self->glyphs_.find(descriptor->glyph_index);
    return item == self->glyphs_.end() ? nullptr : &item->second.buffer;
}
} // namespace board
