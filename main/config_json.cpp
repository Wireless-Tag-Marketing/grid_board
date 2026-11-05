#include "config_json.hpp"
#include <cmath>
#include <cstring>
#include <set>

namespace board {
bool parse_json_object(const std::string &data, cJSON *&json, std::string &error)
{
    json = nullptr;
    // cJSON stores strings as NUL-terminated C strings; reject encoded NULs
    // instead of silently accepting a truncated display message or field name.
    if (data.find('\0') != std::string::npos || data.find("\\u0000") != std::string::npos) {
        error = "JSON 不能包含 NUL 字符"; return false;
    }
    const char *end = nullptr;
    json = cJSON_ParseWithLengthOpts(data.c_str(), data.size() + 1, &end, true);
    if (!json || !cJSON_IsObject(json)) {
        cJSON_Delete(json); json = nullptr;
        error = "请求必须是有效的 JSON 对象"; return false;
    }
    return true;
}

bool parse_display_patch(const cJSON *json, DisplayConfig &config, std::string &error)
{
    if (!cJSON_IsObject(json)) { error = "配置必须是 JSON 对象"; return false; }
    DisplayConfig next = config;
    std::set<std::string> seen;
    const cJSON *item;
    cJSON_ArrayForEach(item, json) {
        std::string key = item->string ? item->string : "";
        if (!seen.insert(key).second) { error = "存在重复字段：" + key; return false; }
        if (key == "text") {
            if (!cJSON_IsString(item)) { error = "text 必须是字符串"; return false; }
            next.text = normalize_newlines(item->valuestring);
        } else if (key == "show_grid") {
            if (!cJSON_IsBool(item)) { error = "show_grid 必须是布尔值"; return false; }
            next.show_grid = cJSON_IsTrue(item);
        } else if (key == "font_size" || key == "fall_ms" || key == "brightness" || key == "scroll_speed") {
            if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
                std::floor(item->valuedouble) != item->valuedouble ||
                item->valuedouble < 0 || item->valuedouble > 10000) {
                error = key + " 必须是范围内的整数"; return false;
            }
            int value = static_cast<int>(item->valuedouble);
            if (key == "font_size") next.font_size = value;
            else if (key == "fall_ms") next.fall_ms = value;
            else if (key == "brightness") next.brightness = value;
            else next.scroll_speed = value;
        } else { error = "未知字段：" + key; return false; }
    }
    Layout layout;
    if (!make_layout(next, layout, error)) return false;
    config = std::move(next);
    return true;
}

cJSON *display_json(const DisplayConfig &c)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) return nullptr;
    if (!cJSON_AddStringToObject(json, "text", c.text.c_str()) ||
        !cJSON_AddNumberToObject(json, "font_size", c.font_size) ||
        !cJSON_AddNumberToObject(json, "fall_ms", c.fall_ms) ||
        !cJSON_AddBoolToObject(json, "show_grid", c.show_grid) ||
        !cJSON_AddNumberToObject(json, "brightness", c.brightness) ||
        !cJSON_AddNumberToObject(json, "scroll_speed", c.scroll_speed)) {
        cJSON_Delete(json);
        return nullptr;
    }
    return json;
}

std::string encode_state(const State &state)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) return {};
    if (!cJSON_AddNumberToObject(json, "version", 2) ||
        !cJSON_AddNumberToObject(json, "revision", state.revision)) {
        cJSON_Delete(json); return {};
    }
    cJSON *current = display_json(state.current);
    if (!current || !cJSON_AddItemToObject(json, "current", current)) {
        cJSON_Delete(current); cJSON_Delete(json); return {};
    }
    cJSON *defaults = display_json(state.defaults);
    if (!defaults || !cJSON_AddItemToObject(json, "defaults", defaults)) {
        cJSON_Delete(defaults); cJSON_Delete(json); return {};
    }
    char *data = cJSON_PrintUnformatted(json);
    std::string result = data ? data : "";
    cJSON_free(data);
    cJSON_Delete(json);
    return result;
}

static bool complete_display_json(const cJSON *json, bool legacy)
{
    if (!cJSON_IsObject(json) || cJSON_GetArraySize(json) != (legacy ? 5 : 6)) return false;
    for (const char *name : {"text", "font_size", "fall_ms", "show_grid", "brightness"}) {
        if (!cJSON_GetObjectItemCaseSensitive(json, name)) return false;
    }
    return legacy || cJSON_GetObjectItemCaseSensitive(json, "scroll_speed");
}

bool decode_state(const std::string &data, State &state, std::string &error)
{
    cJSON *json = nullptr;
    if (!parse_json_object(data, json, error)) return false;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
    const cJSON *revision = cJSON_GetObjectItemCaseSensitive(json, "revision");
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(json, "current");
    const cJSON *defaults = cJSON_GetObjectItemCaseSensitive(json, "defaults");
    State next;
    const bool legacy = cJSON_IsNumber(version) && version->valuedouble == 1;
    bool valid = cJSON_IsNumber(version) && (legacy || version->valuedouble == 2) &&
                 cJSON_IsNumber(revision) && revision->valuedouble >= 0 &&
                 revision->valuedouble <= UINT32_MAX && std::isfinite(revision->valuedouble) &&
                 std::floor(revision->valuedouble) == revision->valuedouble &&
                 complete_display_json(current, legacy) && complete_display_json(defaults, legacy) &&
                 parse_display_patch(current, next.current, error) &&
                 parse_display_patch(defaults, next.defaults, error);
    if (valid) {
        next.revision = static_cast<uint32_t>(revision->valuedouble);
        state = std::move(next);
    } else if (error.empty()) error = "保存的数据版本或结构无效";
    cJSON_Delete(json);
    return valid;
}
} // namespace board
