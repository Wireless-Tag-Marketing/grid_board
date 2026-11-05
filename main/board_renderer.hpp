#pragma once
#include "board_font.hpp"

namespace board {
struct PreparedDisplay {
    lv_obj_t *root = nullptr;
    lv_obj_t *viewport = nullptr;
    std::unique_ptr<BoardFont> font;
    Layout layout;
    std::vector<lv_obj_t *> cells;
    std::vector<lv_obj_t *> labels;
    std::vector<uint32_t> codepoints;
    int pool_rows = 0;
    int offset = -1;
    int first_row = -1;
    void set_scroll_offset(int32_t pixels);
    // Destroy objects before their font; caller must hold the LVGL lock.
    ~PreparedDisplay();
};

class BoardRenderer {
public:
    ~BoardRenderer();
    // All methods run while holding lvgl_port_lock. Build the entire next view
    // before persisting it; a failed preparation leaves the current view intact.
    std::unique_ptr<PreparedDisplay> prepare(const DisplayConfig &, std::string &error);
    void show(std::unique_ptr<PreparedDisplay> next, const DisplayConfig &config, bool animate);
    size_t missing_glyphs() const;
    void on_settings(void (*callback)(void *), void *context);
    void pause_for_settings(bool paused);
private:
    void tick();
    void touch(lv_event_t *event);
    void stop_fall();
    void pause(uint32_t milliseconds);
    std::unique_ptr<PreparedDisplay> active_;
    lv_timer_t *timer_ = nullptr;
    void (*settings_callback_)(void *) = nullptr;
    void *settings_context_ = nullptr;
    uint32_t last_tick_ = 0, pause_started_ = 0, pause_ms_ = 0, pressed_at_ = 0, tapped_at_ = 0;
    uint32_t subpixel_ = 0;
    int speed_ = 40, direction_ = 1, press_offset_ = 0;
    lv_point_t press_point_{};
    lv_point_t tap_point_{};
    bool pressed_ = false, dragged_ = false, settings_paused_ = false, tap_pending_ = false;
};
constexpr uint32_t kTouchResumeMs = 5000;
constexpr uint32_t kSettingsResumeMs = 3000;
constexpr uint32_t kTapMaxMs = 250;
constexpr uint32_t kDoubleTapGapMs = 350;
constexpr int kDoubleTapRadius = 40;
} // namespace board
