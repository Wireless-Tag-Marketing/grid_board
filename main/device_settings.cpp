#include "device_settings.hpp"
#include <cstdio>

namespace board {
namespace {
// Cache the complete menu alphabet once, independently of the displayed text.
constexpr char kMenuText[] =
    "设备设置返回看板当前显示默认配置文字请在网页修改调整后保存生效"
    "字体大小下落时长上下滚动速度屏幕亮度显示网格栅栏开启关闭直接显示"
    "保存并返回恢复已设默认恢复出厂配置松手停秒后续播"
    "有未保存的修改是否保存后返回放弃并返回继续编辑取消确认"
    "切换会放弃未保存修改是否继续应用已保存的默认内容与样式"
    "清除当前内容和自定义默认并恢复出厂配置"
    "配置已被网页修改请返回重新打开设置存储未就绪屏幕未就绪"
    "写入存储失败当前显示未改变屏幕对象内存不足滚动视口网格文字"
    "中文字体初始化失败字形渲染失败缓存不支持灰度渲染"
    "配置无效无法保存请重试已停止滚动"
    "保存默认配置不改变当前显示保存后立即显示断电后恢复"
    "0123456789 px ms / s % . : – ，。？：；（）+";
constexpr uint32_t kBackground = 0x101317, kPanel = 0x252a30, kAccent = 0x2364ca;
}

DeviceSettings::~DeviceSettings()
{
    if (renderer_) renderer_->on_settings(nullptr, nullptr);
    if (root_) lv_obj_delete(root_);
}

lv_obj_t *DeviceSettings::label(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    auto *object = lv_label_create(parent);
    lv_obj_set_style_text_font(object, font_->font(), 0);
    lv_obj_set_style_text_color(object, lv_color_hex(0xf3f4f6), 0);
    lv_label_set_text(object, text);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    return object;
}

lv_obj_t *DeviceSettings::button(lv_obj_t *parent, const char *text, int x, int y, int width, Action action)
{
    auto *object = lv_button_create(parent);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, 54);
    lv_obj_set_style_bg_color(object, lv_color_hex(kPanel), 0);
    lv_obj_set_style_radius(object, 8, 0);
    lv_obj_set_user_data(object, reinterpret_cast<void *>(static_cast<intptr_t>(action)));
    lv_obj_add_event_cb(object, action_event, LV_EVENT_CLICKED, this);
    auto *caption = label(object, text, 0, 0, width - 12);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(caption);
    return object;
}

bool DeviceSettings::initialize(BoardRenderer &renderer, Read read, Write write, void *context, std::string &error)
{
    if (root_ || !read || !write) { error = "配置无效"; return false; }
    font_ = std::make_unique<BoardFont>();
    Layout alphabet;
    alphabet.cell_height = 52;
    std::vector<uint32_t> codepoints;
    decode_utf8(kMenuText, codepoints);
    for (auto cp : codepoints) alphabet.cells.push_back({cp, 0, 0});
    if (!font_->initialize(24, alphabet, error)) return false;
    renderer_ = &renderer;
    read_ = read;
    write_ = write;
    context_ = context;
    root_ = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, kScreenWidth, kScreenHeight);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    button(root_, "返回看板", 20, 12, 172, Action::Back);
    label(root_, "设备设置", 420, 16, 200);
    current_tab_ = button(root_, "当前显示", 204, 78, 292, Action::Current);
    defaults_tab_ = button(root_, "默认配置", 516, 78, 292, Action::Defaults);
    auto *hint = label(root_, "文字请在网页修改：192.168.4.1", 28, 136, 968);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    const char *names[] = {"字体大小", "下落时长", "上下滚动速度", "屏幕亮度"};
    const int minimum[] = {kMinFontSize, 0, kMinScrollSpeed, 1};
    const int maximum[] = {kMaxFontSize, kMaxFallMs, kMaxScrollSpeed, 100};
    for (int index = 0; index < 4; ++index) {
        const int y = 184 + index * 52;
        label(root_, names[index], 32, y, 220);
        sliders_[index] = lv_slider_create(root_);
        lv_obj_set_pos(sliders_[index], 284, y + 13);
        lv_obj_set_size(sliders_[index], 440, 18);
        lv_obj_set_ext_click_area(sliders_[index], 16);
        lv_slider_set_range(sliders_[index], minimum[index], maximum[index]);
        lv_obj_set_user_data(sliders_[index], reinterpret_cast<void *>(static_cast<intptr_t>(index)));
        lv_obj_add_event_cb(sliders_[index], field_event, LV_EVENT_VALUE_CHANGED, this);
        values_[index] = label(root_, "", 770, y, 222);
    }
    label(root_, "显示网格栅栏", 32, 394, 220);
    grid_ = lv_switch_create(root_);
    lv_obj_set_pos(grid_, 282, 394);
    lv_obj_set_size(grid_, 80, 38);
    lv_obj_set_user_data(grid_, reinterpret_cast<void *>(static_cast<intptr_t>(Field::Grid)));
    lv_obj_add_event_cb(grid_, field_event, LV_EVENT_VALUE_CHANGED, this);
    label(root_, "调整后保存生效", 420, 392, 540);
    feedback_ = label(root_, "", 32, 444, 960);
    lv_label_set_long_mode(feedback_, LV_LABEL_LONG_DOT);
    auto *save_button = button(root_, "保存并返回", 32, 492, 280, Action::Save);
    lv_obj_set_style_bg_color(save_button, lv_color_hex(kAccent), 0);
    button(root_, "恢复已设默认", 336, 492, 280, Action::Reset);
    auto *factory = button(root_, "恢复出厂配置", 640, 492, 352, Action::Factory);
    lv_obj_set_style_bg_color(factory, lv_color_hex(0x702c35), 0);
    hint = label(root_, "返回看板3秒后续播", 32, 554, 960);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    renderer.on_settings([](void *owner) { static_cast<DeviceSettings *>(owner)->open(); }, this);
    return true;
}

bool DeviceSettings::is_open() const
{
    return root_ && !lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void DeviceSettings::open()
{
    if (!root_ || is_open()) return;
    snapshot_ = read_(context_);
    load(false);
    renderer_->pause_for_settings(true);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(root_);
}

void DeviceSettings::close()
{
    dismiss_dialog();
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    renderer_->pause_for_settings(false);
}

void DeviceSettings::load(bool defaults)
{
    defaults_ = defaults;
    draft_ = defaults ? snapshot_.defaults : snapshot_.current;
    const int values[] = {draft_.font_size, draft_.fall_ms, draft_.scroll_speed, draft_.brightness};
    for (int index = 0; index < 4; ++index) lv_slider_set_value(sliders_[index], values[index], LV_ANIM_OFF);
    if (draft_.show_grid) lv_obj_add_state(grid_, LV_STATE_CHECKED);
    else lv_obj_remove_state(grid_, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(current_tab_, lv_color_hex(defaults ? kPanel : kAccent), 0);
    lv_obj_set_style_bg_color(defaults_tab_, lv_color_hex(defaults ? kAccent : kPanel), 0);
    refresh_values();
    message(defaults ? "保存默认配置，不改变当前显示" : "保存后立即显示，断电后恢复");
}

void DeviceSettings::refresh_values()
{
    char value[48];
    std::snprintf(value, sizeof(value), "%d px", draft_.font_size);
    lv_label_set_text(values_[0], value);
    if (draft_.fall_ms == 0) lv_label_set_text(values_[1], "直接显示");
    else {
        std::snprintf(value, sizeof(value), "%d ms", draft_.fall_ms);
        lv_label_set_text(values_[1], value);
    }
    std::snprintf(value, sizeof(value), "%d px/s", draft_.scroll_speed);
    lv_label_set_text(values_[2], value);
    std::snprintf(value, sizeof(value), "%d %%", draft_.brightness);
    lv_label_set_text(values_[3], value);
}

void DeviceSettings::field_event(lv_event_t *event)
{
    auto &self = *static_cast<DeviceSettings *>(lv_event_get_user_data(event));
    auto *object = static_cast<lv_obj_t *>(lv_event_get_target(event));
    const auto field = static_cast<Field>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(object)));
    const int value = field == Field::Grid ? 0 : lv_slider_get_value(object);
    switch (field) {
        case Field::Font: self.draft_.font_size = value; break;
        case Field::Fall: self.draft_.fall_ms = (value + 25) / 50 * 50; break;
        case Field::Scroll: self.draft_.scroll_speed = value; break;
        case Field::Brightness: self.draft_.brightness = value; break;
        case Field::Grid: self.draft_.show_grid = lv_obj_has_state(object, LV_STATE_CHECKED); break;
    }
    self.refresh_values();
}

void DeviceSettings::action_event(lv_event_t *event)
{
    auto &self = *static_cast<DeviceSettings *>(lv_event_get_user_data(event));
    auto *object = static_cast<lv_obj_t *>(lv_event_get_target(event));
    self.action(static_cast<Action>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(object))));
}

void DeviceSettings::message(const char *text, bool error)
{
    lv_label_set_text(feedback_, text);
    lv_obj_set_style_text_color(feedback_, lv_color_hex(error ? 0xffca5c : 0xaeb9c9), 0);
}

bool DeviceSettings::dirty() const
{
    return !(draft_ == (defaults_ ? snapshot_.defaults : snapshot_.current));
}

bool DeviceSettings::apply(State next)
{
    std::string error;
    if (!write_(context_, std::move(next), snapshot_.revision, error)) {
        message(error.c_str(), true);
        return false;
    }
    close();
    return true;
}

bool DeviceSettings::save()
{
    State next = snapshot_;
    (defaults_ ? next.defaults : next.current) = draft_;
    return apply(std::move(next));
}

void DeviceSettings::dismiss_dialog()
{
    if (dialog_) lv_obj_delete(dialog_);
    dialog_ = nullptr;
}

void DeviceSettings::confirm(Action action)
{
    pending_ = action;
    dialog_ = lv_obj_create(root_);
    lv_obj_remove_style_all(dialog_);
    lv_obj_set_size(dialog_, kScreenWidth, kScreenHeight);
    lv_obj_set_style_bg_color(dialog_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(dialog_, LV_OPA_70, 0);
    lv_obj_remove_flag(dialog_, LV_OBJ_FLAG_SCROLLABLE);
    auto *panel = lv_obj_create(dialog_);
    lv_obj_set_size(panel, 880, 300);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(kPanel), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    const char *question = "切换会放弃未保存修改，是否继续？";
    if (action == Action::Back) question = "有未保存的修改，是否保存后返回？";
    if (action == Action::Reset) question = "应用已保存的默认内容与样式？\n未保存的修改会放弃。";
    if (action == Action::Factory) question = "清除当前内容和自定义默认，\n并恢复出厂配置？";
    auto *caption = label(panel, question, 32, 32, 816);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
    if (action == Action::Back) {
        auto *save_button = button(panel, "保存并返回", 20, 202, 270, Action::Save);
        auto *discard = button(panel, "放弃并返回", 306, 202, 270, Action::Discard);
        auto *stay = button(panel, "继续编辑", 592, 202, 268, Action::Cancel);
        lv_obj_set_style_bg_color(save_button, lv_color_hex(kAccent), 0);
        lv_obj_set_style_bg_color(discard, lv_color_hex(0x702c35), 0);
        lv_obj_set_style_bg_color(stay, lv_color_hex(0x344252), 0);
    } else {
        auto *cancel = button(panel, "取消", 100, 202, 300, Action::Cancel);
        auto *accept = button(panel, "确认", 480, 202, 300, Action::Confirm);
        lv_obj_set_style_bg_color(cancel, lv_color_hex(0x344252), 0);
        lv_obj_set_style_bg_color(accept, lv_color_hex(kAccent), 0);
    }
}

void DeviceSettings::action(Action action)
{
    if (action == Action::Cancel) { dismiss_dialog(); return; }
    if (action == Action::Save) { dismiss_dialog(); save(); return; }
    if (action == Action::Discard) { close(); return; }
    if (action == Action::Confirm) {
        action = pending_;
        dismiss_dialog();
        if (action == Action::Current || action == Action::Defaults) load(action == Action::Defaults);
        else {
            State next = action == Action::Factory ? State{} : snapshot_;
            if (action == Action::Reset) next.current = snapshot_.defaults;
            apply(std::move(next));
        }
        return;
    }
    if (action == Action::Back) {
        if (dirty()) confirm(action);
        else close();
    } else if (action == Action::Current || action == Action::Defaults) {
        if ((action == Action::Defaults) == defaults_) return;
        if (dirty()) confirm(action);
        else load(action == Action::Defaults);
    } else if (action == Action::Reset || action == Action::Factory) confirm(action);
}
} // namespace board
