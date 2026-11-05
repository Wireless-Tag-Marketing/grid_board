#include "board_renderer.hpp"
#include <algorithm>
#include <cstdlib>

namespace board {
BoardRenderer::~BoardRenderer()
{
    if (timer_) lv_timer_delete(timer_);
}

PreparedDisplay::~PreparedDisplay()
{
    lv_anim_delete(this, nullptr);
    if (root) lv_obj_delete(root); // LVGL also removes animations targeting deleted objects.
}

void PreparedDisplay::set_scroll_offset(int32_t pixels)
{
    pixels = std::clamp<int32_t>(pixels, 0, scroll_distance_px(layout));
    if (pixels == offset) return;
    const int row = pixels / layout.cell_height;
    const int fraction = pixels % layout.cell_height;
    const int total_rows = static_cast<int>(codepoints.size()) / layout.columns;
    for (int r = 0; r < pool_rows; ++r) {
        const int source_row = row + r;
        for (int c = 0; c < layout.columns; ++c) {
            const size_t index = r * layout.columns + c;
            lv_obj_t *cell = cells[index];
            if (source_row >= total_rows) {
                lv_obj_add_flag(cell, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_remove_flag(cell, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(cell, r * layout.cell_height - fraction + 2);
            if (row == first_row) continue;
            const uint32_t cp = codepoints[source_row * layout.columns + c];
            lv_obj_t *label = labels[index];
            if (cp == ' ' || cp == 0x3000) {
                lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_label_set_text(label, encode_utf8(cp).c_str());
                lv_obj_center(label);
                lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    first_row = row;
    offset = pixels;
}

std::unique_ptr<PreparedDisplay> BoardRenderer::prepare(const DisplayConfig &config, std::string &error)
{
    Layout layout;
    if (!make_layout(config, layout, error)) return nullptr;
    auto next = std::make_unique<PreparedDisplay>();
    next->font = std::make_unique<BoardFont>();
    if (!next->font->initialize(config.font_size, layout, error)) return nullptr;
    next->layout = layout;
    const int total_rows = std::max(layout.rows, layout.content_rows);
    next->pool_rows = std::min(total_rows, layout.rows + 1);
    next->codepoints.assign(static_cast<size_t>(total_rows) * layout.columns, ' ');
    for (const auto &position : layout.cells) {
        next->codepoints[position.row * layout.columns + position.col] = position.codepoint;
    }
    next->root = lv_obj_create(lv_screen_active());
    if (!next->root) { error = "屏幕对象内存不足"; return nullptr; }
    lv_obj_remove_style_all(next->root);
    lv_obj_set_size(next->root, kScreenWidth, kScreenHeight);
    lv_obj_remove_flag(next->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(next->root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(next->root, [](lv_event_t *event) {
        static_cast<BoardRenderer *>(lv_event_get_user_data(event))->touch(event);
    }, LV_EVENT_ALL, this);
    lv_obj_add_flag(next->root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(next->root, lv_color_hex(0x101317), 0);
    lv_obj_set_style_bg_opa(next->root, LV_OPA_COVER, 0);

    int origin_x = (kScreenWidth - layout.columns * layout.cell_width) / 2;
    int origin_y = (kScreenHeight - layout.rows * layout.cell_height) / 2;
    next->viewport = lv_obj_create(next->root);
    if (!next->viewport) { error = "滚动视口内存不足"; return nullptr; }
    lv_obj_remove_style_all(next->viewport);
    lv_obj_remove_flag(next->viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(next->viewport, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(next->viewport, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_pos(next->viewport, origin_x, origin_y);
    lv_obj_set_size(next->viewport, layout.columns * layout.cell_width, layout.rows * layout.cell_height);
    // Recycle only the visible rows plus one. Empty lines and long messages
    // must not turn into thousands of off-screen LVGL grid objects.
    for (int r = 0; r < next->pool_rows; ++r) {
        for (int c = 0; c < layout.columns; ++c) {
            lv_obj_t *cell = lv_obj_create(next->viewport);
            if (!cell) { error = "网格对象内存不足"; return nullptr; }
            lv_obj_remove_style_all(cell);
            lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(cell, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_pos(cell, c * layout.cell_width + 2, r * layout.cell_height + 2);
            lv_obj_set_size(cell, layout.cell_width - 4, layout.cell_height - 4);
            if (config.show_grid) {
                lv_obj_set_style_bg_color(cell, lv_color_hex(0x252a30), 0);
                lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(cell, 1, 0);
                lv_obj_set_style_border_color(cell, lv_color_hex(0x404750), 0);
                lv_obj_set_style_radius(cell, 4, 0);
                lv_obj_t *divider = lv_obj_create(cell);
                if (!divider) { error = "网格线内存不足"; return nullptr; }
                lv_obj_remove_style_all(divider);
                lv_obj_remove_flag(divider, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_size(divider, layout.cell_width - 4, 1);
                lv_obj_align(divider, LV_ALIGN_CENTER, 0, 0);
                lv_obj_set_style_bg_color(divider, lv_color_hex(0x101317), 0);
                lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
            }
            lv_obj_t *label = lv_label_create(cell);
            if (!label) { error = "文字对象内存不足"; return nullptr; }
            lv_obj_set_style_text_font(label, next->font->font(), 0);
            lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(label, lv_color_hex(0xf3f4f6), 0);
            lv_label_set_text(label, "");
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
            next->cells.push_back(cell);
            next->labels.push_back(label);
        }
    }
    next->set_scroll_offset(0);
    return next;
}

void BoardRenderer::show(std::unique_ptr<PreparedDisplay> next, const DisplayConfig &config, bool animate)
{
    active_.reset();
    active_ = std::move(next);
    pressed_ = dragged_ = tap_pending_ = false;
    direction_ = 1;
    speed_ = config.scroll_speed;
    subpixel_ = 0;
    lv_obj_remove_flag(active_->root, LV_OBJ_FLAG_HIDDEN);
    uint32_t scroll_delay = kScrollPauseMs;
    if (animate && !settings_paused_ && config.fall_ms > 0) for (size_t i = 0; i < active_->labels.size(); ++i) {
        lv_obj_t *label = active_->labels[i];
        if (lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_obj_set_style_translate_y(label, -active_->layout.cell_height, 0);
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, label);
        lv_anim_set_exec_cb(&animation, [](void *object, int32_t y) {
            lv_obj_set_style_translate_y(static_cast<lv_obj_t *>(object), y, 0);
        });
        lv_anim_set_values(&animation, -active_->layout.cell_height, 0);
        lv_anim_set_duration(&animation, config.fall_ms);
        uint32_t delay = (i % active_->layout.columns) * std::min(config.fall_ms / 8, 70) +
                         (i / active_->layout.columns) * std::min(config.fall_ms / 12, 40);
        lv_anim_set_delay(&animation, delay);
        scroll_delay = std::max(scroll_delay, delay + config.fall_ms + kScrollPauseMs);
        lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
        lv_anim_start(&animation);
    }
    pause(scroll_delay);
    if (!timer_) {
        timer_ = lv_timer_create([](lv_timer_t *timer) {
            static_cast<BoardRenderer *>(lv_timer_get_user_data(timer))->tick();
        }, 20, this);
    }
}

void BoardRenderer::on_settings(void (*callback)(void *), void *context)
{
    settings_callback_ = callback;
    settings_context_ = context;
}

void BoardRenderer::pause(uint32_t milliseconds)
{
    pause_started_ = last_tick_ = lv_tick_get();
    pause_ms_ = milliseconds;
    subpixel_ = 0;
}

void BoardRenderer::stop_fall()
{
    if (!active_) return;
    for (auto *label : active_->labels) {
        lv_anim_delete(label, nullptr);
        lv_obj_set_style_translate_y(label, 0, 0);
    }
}

void BoardRenderer::pause_for_settings(bool paused)
{
    settings_paused_ = paused;
    pressed_ = false;
    tap_pending_ = false;
    stop_fall();
    pause(kSettingsResumeMs);
}

void BoardRenderer::touch(lv_event_t *event)
{
    if (!active_ || settings_paused_) return;
    const auto code = lv_event_get_code(event);
    lv_indev_t *input = lv_indev_active();
    if (code == LV_EVENT_PRESSED && input) {
        pressed_ = true;
        dragged_ = false;
        pressed_at_ = lv_tick_get();
        if (tap_pending_ && pressed_at_ - tapped_at_ > kDoubleTapGapMs) tap_pending_ = false;
        lv_indev_get_point(input, &press_point_);
        press_offset_ = active_->offset;
        stop_fall();
    } else if (code == LV_EVENT_PRESSING && input && pressed_) {
        lv_point_t point;
        lv_indev_get_point(input, &point);
        if (std::abs(point.x - press_point_.x) > 12 || std::abs(point.y - press_point_.y) > 12) {
            dragged_ = true;
            tap_pending_ = false;
        }
        if (dragged_ && scroll_distance_px(active_->layout) > 0) {
            const int previous = active_->offset;
            active_->set_scroll_offset(press_offset_ + press_point_.y - point.y);
            if (previous != active_->offset) direction_ = active_->offset > previous ? 1 : -1;
        }
    } else if (code == LV_EVENT_RELEASED && pressed_) {
        lv_point_t point = press_point_;
        if (input) lv_indev_get_point(input, &point);
        const uint32_t now = lv_tick_get();
        const bool tap = !dragged_ && now - pressed_at_ <= kTapMaxMs &&
                         std::abs(point.x - press_point_.x) <= 12 && std::abs(point.y - press_point_.y) <= 12;
        const int dx = press_point_.x - tap_point_.x, dy = press_point_.y - tap_point_.y;
        const bool open = tap && tap_pending_ && pressed_at_ - tapped_at_ <= kDoubleTapGapMs &&
                          dx * dx + dy * dy <= kDoubleTapRadius * kDoubleTapRadius;
        pressed_ = false;
        pause(kTouchResumeMs);
        tap_pending_ = false;
        if (open && settings_callback_) {
            // Open on the second release: neither tap can click a menu button.
            settings_callback_(settings_context_);
        } else if (tap) {
            tap_pending_ = true;
            tapped_at_ = now;
            tap_point_ = press_point_;
        }
    } else if (code == LV_EVENT_PRESS_LOST) {
        pressed_ = tap_pending_ = false;
        pause(kTouchResumeMs);
    }
}

void BoardRenderer::tick()
{
    const uint32_t now = lv_tick_get();
    const uint32_t elapsed = now - last_tick_;
    last_tick_ = now;
    if (!active_ || settings_paused_) return;
    if (pressed_) return;
    const int distance = scroll_distance_px(active_->layout);
    if (distance == 0) return;
    if (pause_ms_) {
        if (now - pause_started_ < pause_ms_) return;
        pause_ms_ = 0;
        // Resume inward when the finger was released at either boundary.
        if (active_->offset == 0) direction_ = 1;
        if (active_->offset == distance) direction_ = -1;
    }
    const uint64_t scaled = static_cast<uint64_t>(elapsed) * speed_ + subpixel_;
    subpixel_ = scaled % 1000;
    const int step = static_cast<int>(scaled / 1000);
    active_->set_scroll_offset(active_->offset + direction_ * step);
    if ((direction_ > 0 && active_->offset == distance) || (direction_ < 0 && active_->offset == 0)) {
        direction_ = -direction_;
        pause(kScrollPauseMs);
    }
}

size_t BoardRenderer::missing_glyphs() const
{
    return active_ ? active_->font->missing_glyphs() : 0;
}
} // namespace board
