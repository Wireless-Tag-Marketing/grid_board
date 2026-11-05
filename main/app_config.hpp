#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace board {
constexpr int kScreenWidth = 1024;
constexpr int kScreenHeight = 600;
constexpr int kMargin = 16;
constexpr size_t kMaxTextBytes = 2048;
constexpr int kMinFontSize = 24;
constexpr int kMaxFontSize = 128;
constexpr int kMaxFallMs = 3000;
constexpr int kMinScrollSpeed = 5;
constexpr int kMaxScrollSpeed = 240;
constexpr uint32_t kScrollPauseMs = 1500;

struct DisplayConfig {
    std::string text = "欢迎使用\n中文网格看板\nhttps://wireless-tag.com";
    int font_size = 64;
    int fall_ms = 600; // 0: no animation; larger values: slower fall.
    bool show_grid = true;
    int brightness = 80;
    int scroll_speed = 40; // Pixels per second, in both directions.
    bool operator==(const DisplayConfig &other) const;
};

struct State {
    DisplayConfig current;
    DisplayConfig defaults;
    uint32_t revision = 0;
};

// Refresh an unchanged built-in welcome without replacing custom messages/styles.
bool upgrade_factory_welcome(State &state);

struct Cell {
    uint32_t codepoint;
    int row;
    int col;
};

struct Layout {
    int columns = 0;
    int rows = 0;
    int content_rows = 1;
    int cell_width = 0;
    int cell_height = 0;
    std::vector<Cell> cells;
};

std::string normalize_newlines(const std::string &text);
// Strict Unicode decoding: rejects overlong UTF-8, surrogates and invalid scalars.
bool decode_utf8(const std::string &text, std::vector<uint32_t> &out);
std::string encode_utf8(uint32_t codepoint);
// Public validation/layout boundary shared by persistence, HTTP and the display.
bool make_layout(const DisplayConfig &config, Layout &layout, std::string &error);
int scroll_distance_px(const Layout &layout);
uint32_t scroll_duration_ms(const DisplayConfig &config, const Layout &layout);
} // namespace board
