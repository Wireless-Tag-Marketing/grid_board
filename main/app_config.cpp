#include "app_config.hpp"
#include <algorithm>

namespace board {
bool upgrade_factory_welcome(State &state)
{
    constexpr char previous[] = "欢迎使用\n中文网格看板";
    const std::string welcome = DisplayConfig{}.text;
    bool changed = false;
    for (DisplayConfig *config : {&state.current, &state.defaults}) {
        if (config->text == previous) {
            config->text = welcome;
            changed = true;
        }
    }
    if (changed) ++state.revision;
    return changed;
}

bool DisplayConfig::operator==(const DisplayConfig &other) const
{
    return text == other.text && font_size == other.font_size && fall_ms == other.fall_ms &&
           show_grid == other.show_grid && brightness == other.brightness &&
           scroll_speed == other.scroll_speed;
}

std::string normalize_newlines(const std::string &text)
{
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            result += '\n';
        } else {
            result += text[i];
        }
    }
    return result;
}

bool decode_utf8(const std::string &text, std::vector<uint32_t> &out)
{
    out.clear();
    for (size_t i = 0; i < text.size();) {
        uint8_t lead = static_cast<uint8_t>(text[i++]);
        uint32_t cp;
        unsigned count;
        uint32_t minimum;
        if (lead < 0x80) { cp = lead; count = 0; minimum = 0; }
        else if (lead >= 0xc2 && lead <= 0xdf) { cp = lead & 0x1f; count = 1; minimum = 0x80; }
        else if (lead >= 0xe0 && lead <= 0xef) { cp = lead & 0x0f; count = 2; minimum = 0x800; }
        else if (lead >= 0xf0 && lead <= 0xf4) { cp = lead & 7; count = 3; minimum = 0x10000; }
        else return false;
        if (i + count > text.size()) return false;
        for (unsigned n = 0; n < count; ++n) {
            uint8_t c = static_cast<uint8_t>(text[i++]);
            if ((c & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (c & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
        out.push_back(cp);
    }
    return true;
}

std::string encode_utf8(uint32_t cp)
{
    std::string s;
    if (cp < 0x80) s += static_cast<char>(cp);
    else if (cp < 0x800) {
        s += static_cast<char>(0xc0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 63));
    } else if (cp < 0x10000) {
        s += static_cast<char>(0xe0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 63));
        s += static_cast<char>(0x80 | (cp & 63));
    } else {
        s += static_cast<char>(0xf0 | (cp >> 18));
        s += static_cast<char>(0x80 | ((cp >> 12) & 63));
        s += static_cast<char>(0x80 | ((cp >> 6) & 63));
        s += static_cast<char>(0x80 | (cp & 63));
    }
    return s;
}

bool make_layout(const DisplayConfig &c, Layout &layout, std::string &error)
{
    layout = {};
    error.clear();
    if (c.font_size < kMinFontSize || c.font_size > kMaxFontSize) {
        error = "字号必须是 24–128 之间的整数"; return false;
    }
    if (c.fall_ms < 0 || c.fall_ms > kMaxFallMs) {
        error = "下落时长必须是 0–3000 毫秒之间的整数"; return false;
    }
    if (c.brightness < 1 || c.brightness > 100) {
        error = "亮度必须是 1–100 之间的整数"; return false;
    }
    if (c.scroll_speed < kMinScrollSpeed || c.scroll_speed > kMaxScrollSpeed) {
        error = "滚动速度必须是 5–240 像素/秒之间的整数"; return false;
    }
    if (c.text.size() > kMaxTextBytes) {
        error = "文字不能超过 2048 字节"; return false;
    }
    std::vector<uint32_t> codepoints;
    if (!decode_utf8(c.text, codepoints)) {
        error = "文字必须是有效的 UTF-8"; return false;
    }
    layout.cell_width = c.font_size + 16;
    layout.cell_height = (c.font_size * 3 + 1) / 2 + 16;
    layout.columns = (kScreenWidth - 2 * kMargin) / layout.cell_width;
    layout.rows = (kScreenHeight - 2 * kMargin) / layout.cell_height;
    int row = 0, col = 0;
    for (uint32_t cp : codepoints) {
        if (cp == '\n') { ++row; col = 0; continue; }
        if (cp < 0x20 || (cp >= 0x7f && cp < 0xa0) || cp == 0x2028 || cp == 0x2029 ||
            cp == 0x200d || (cp >= 0x202a && cp <= 0x202e) ||
            (cp >= 0x2066 && cp <= 0x2069)) {
            error = "文字包含不支持的控制字符或组合表情连接符"; return false;
        }
        // Emoji presentation selectors do not occupy a separate display cell.
        if (cp == 0xfe0e || cp == 0xfe0f) continue;
        if (col == layout.columns) { ++row; col = 0; }
        layout.cells.push_back({cp, row, col++});
    }
    layout.content_rows = row + 1;
    return true;
}

int scroll_distance_px(const Layout &layout)
{
    return std::max(0, layout.content_rows - layout.rows) * layout.cell_height;
}

uint32_t scroll_duration_ms(const DisplayConfig &config, const Layout &layout)
{
    if (config.scroll_speed < kMinScrollSpeed) return 0;
    const uint64_t distance = static_cast<uint64_t>(scroll_distance_px(layout));
    return static_cast<uint32_t>((distance * 1000 + config.scroll_speed - 1) / config.scroll_speed);
}
} // namespace board
