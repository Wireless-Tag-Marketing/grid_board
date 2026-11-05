#pragma once
#include "board_renderer.hpp"

namespace board {
// UI callbacks and all public methods execute under the LVGL lock. The writer
// must reject an obsolete revision, and return success only after persistence.
class DeviceSettings {
public:
    using Read = State (*)(void *);
    using Write = bool (*)(void *, State, uint32_t, std::string &);
    ~DeviceSettings();
    bool initialize(BoardRenderer &renderer, Read read, Write write, void *context, std::string &error);
    void open();
    bool is_open() const;
private:
    enum class Action { Back = 1, Current, Defaults, Save, Reset, Factory, Cancel, Discard, Confirm };
    enum class Field { Font, Fall, Scroll, Brightness, Grid };
    static void action_event(lv_event_t *event);
    static void field_event(lv_event_t *event);
    void action(Action action);
    void load(bool defaults);
    void refresh_values();
    void close();
    void dismiss_dialog();
    void confirm(Action action);
    bool save();
    bool apply(State next);
    bool dirty() const;
    void message(const char *text, bool error = false);
    lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width);
    lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int width, Action action);
    std::unique_ptr<BoardFont> font_;
    BoardRenderer *renderer_ = nullptr;
    Read read_ = nullptr;
    Write write_ = nullptr;
    void *context_ = nullptr;
    lv_obj_t *root_ = nullptr, *dialog_ = nullptr, *feedback_ = nullptr;
    lv_obj_t *current_tab_ = nullptr, *defaults_tab_ = nullptr;
    lv_obj_t *sliders_[4]{}, *values_[4]{}, *grid_ = nullptr;
    State snapshot_;
    DisplayConfig draft_;
    bool defaults_ = false;
    Action pending_ = Action::Back;
};
} // namespace board
