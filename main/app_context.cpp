#include "app_context.hpp"
#include "board_display.h"
#include "esp_log.h"

namespace board {
CommitResult AppContext::commit(State next, uint32_t expected_revision, std::string &error)
{
    if (state.revision != expected_revision) {
        error = "配置已被网页修改，请返回重新打开设置";
        return CommitResult::Conflict;
    }
    Layout layout;
    if (!make_layout(next.current, layout, error) || !make_layout(next.defaults, layout, error))
        return CommitResult::Invalid;
    if (!persistence_ready) { error = "存储未就绪，无法保存"; return CommitResult::Unavailable; }
    const bool changed = !(next.current == state.current);
    if (!changed && next.defaults == state.defaults && !store.recovered_defaults()) return CommitResult::Ok;
    std::unique_ptr<PreparedDisplay> prepared;
    if (changed) {
        if (!display_ready) { error = "屏幕未就绪，无法保存"; return CommitResult::Unavailable; }
        prepared = renderer.prepare(next.current, error);
        if (!prepared) return CommitResult::Failed;
    }
    next.revision = state.revision + 1;
    const esp_err_t result = store.save(next);
    if (result != ESP_OK) {
        ESP_LOGE("app_context", "NVS save failed: %s", esp_err_to_name(result));
        error = "写入存储失败，当前显示未改变";
        return CommitResult::Failed;
    }
    if (changed) {
        renderer.show(std::move(prepared), next.current, true);
        ESP_ERROR_CHECK_WITHOUT_ABORT(board_display_brightness(next.current.brightness));
    }
    state = std::move(next);
    return CommitResult::Ok;
}
} // namespace board
