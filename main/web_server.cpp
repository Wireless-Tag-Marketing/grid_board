#include "web_server.hpp"
#include "config_json.hpp"
#include "wifi_ap.h"
#include "esp_lvgl_port.h"
#include "sdkconfig.h"
#include <cstring>
#include <memory>

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
extern const char js_start[] asm("_binary_app_js_start");
extern const char js_end[] asm("_binary_app_js_end");
extern const char css_start[] asm("_binary_style_css_start");
extern const char css_end[] asm("_binary_style_css_end");

namespace board {
namespace {
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
class LvglLock {
public:
    LvglLock() : locked_(lvgl_port_lock(5000)) {}
    ~LvglLock() { release(); }
    bool acquired() const { return locked_; }
    void release() { if (locked_) { lvgl_port_unlock(); locked_ = false; } }
private:
    bool locked_;
};

void response_headers(httpd_req_t *request)
{
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(request, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(request, "Content-Security-Policy",
                       "default-src 'self'; script-src 'self'; style-src 'self'; "
                       "connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; "
                       "form-action 'self'; base-uri 'none'");
}

esp_err_t send_json(httpd_req_t *request, cJSON *json, const char *status = "200 OK")
{
    response_headers(request);
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    char *serialized = json ? cJSON_PrintUnformatted(json) : nullptr;
    if (!serialized) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    esp_err_t result = httpd_resp_send(request, serialized, HTTPD_RESP_USE_STRLEN);
    cJSON_free(serialized);
    return result;
}

esp_err_t send_error(httpd_req_t *request, const char *status, const std::string &error)
{
    Json json(cJSON_CreateObject(), cJSON_Delete);
    if (json) cJSON_AddStringToObject(json.get(), "error", error.c_str());
    return send_json(request, json.get(), status);
}

esp_err_t send_state(httpd_req_t *request)
{
    auto &app = *static_cast<AppContext *>(request->user_ctx);
    LvglLock lock;
    if (!lock.acquired()) return send_error(request, "503 Service Unavailable", "屏幕忙，请重试");
    const std::string state = encode_state(app.state);
    Json json(cJSON_Parse(state.c_str()), cJSON_Delete);
    if (!json) return send_error(request, "500 Internal Server Error", "响应内存不足");
    cJSON *device = cJSON_AddObjectToObject(json.get(), "device");
    if (!device) return send_error(request, "500 Internal Server Error", "响应内存不足");
    cJSON_AddStringToObject(device, "ssid", CONFIG_GRID_BOARD_AP_SSID);
    cJSON_AddStringToObject(device, "ip", GRID_BOARD_IP);
    cJSON_AddStringToObject(device, "firmware", "1.2.2");
    cJSON_AddBoolToObject(device, "display_ready", app.display_ready);
    cJSON_AddBoolToObject(device, "persistence_ready", app.persistence_ready);
    cJSON_AddBoolToObject(device, "recovered_defaults", app.store.recovered_defaults());
    cJSON_AddNumberToObject(device, "missing_glyphs", app.renderer.missing_glyphs());
    cJSON_AddNumberToObject(device, "width", kScreenWidth);
    cJSON_AddNumberToObject(device, "height", kScreenHeight);
    cJSON *limits = cJSON_AddObjectToObject(json.get(), "limits");
    if (!limits) return send_error(request, "500 Internal Server Error", "响应内存不足");
    cJSON_AddNumberToObject(limits, "font_min", kMinFontSize);
    cJSON_AddNumberToObject(limits, "font_max", kMaxFontSize);
    cJSON_AddNumberToObject(limits, "max_text_bytes", kMaxTextBytes);
    cJSON_AddNumberToObject(limits, "fall_ms_max", kMaxFallMs);
    cJSON_AddNumberToObject(limits, "scroll_speed_min", kMinScrollSpeed);
    cJSON_AddNumberToObject(limits, "scroll_speed_max", kMaxScrollSpeed);
    const std::string etag = "\"" + std::to_string(app.state.revision) + "\"";
    httpd_resp_set_hdr(request, "ETag", etag.c_str());
    lock.release();
    return send_json(request, json.get());
}

bool same_origin(httpd_req_t *request)
{
    char host[64] = {};
    if (httpd_req_get_hdr_value_str(request, "Host", host, sizeof(host)) != ESP_OK ||
        (std::strcmp(host, GRID_BOARD_IP) != 0 && std::strcmp(host, GRID_BOARD_IP ":80") != 0)) return false;
    size_t size = httpd_req_get_hdr_value_len(request, "Origin");
    if (size == 0) return true; // Explicit API clients do not send Origin.
    char origin[80] = {};
    if (httpd_req_get_hdr_value_str(request, "Origin", origin, sizeof(origin)) != ESP_OK) return false;
    return std::strcmp(origin, "http://" GRID_BOARD_IP) == 0 ||
           std::strcmp(origin, "http://" GRID_BOARD_IP ":80") == 0;
}

esp_err_t mutate(httpd_req_t *request)
{
    auto &app = *static_cast<AppContext *>(request->user_ctx);
    if (!same_origin(request)) {
        httpd_resp_set_hdr(request, "Connection", "close");
        return send_error(request, "403 Forbidden", "请通过设备固定 IP 访问配置页面");
    }
    if (request->content_len == 0 || request->content_len > 8192) {
        httpd_resp_set_hdr(request, "Connection", "close");
        return send_error(request, "413 Content Too Large", "请求大小必须是 1–8192 字节");
    }
    char type[96] = {};
    if (httpd_req_get_hdr_value_str(request, "Content-Type", type, sizeof(type)) != ESP_OK ||
        (std::strcmp(type, "application/json") != 0 &&
         std::strncmp(type, "application/json;", 17) != 0)) {
        httpd_resp_set_hdr(request, "Connection", "close");
        return send_error(request, "415 Unsupported Media Type", "请发送 application/json");
    }
    char if_match[40] = {};
    bool has_revision = httpd_req_get_hdr_value_len(request, "If-Match") != 0;
    if (has_revision) {
        if (httpd_req_get_hdr_value_str(request, "If-Match", if_match, sizeof(if_match)) != ESP_OK) {
            httpd_resp_set_hdr(request, "Connection", "close");
            return send_error(request, "409 Conflict", "配置版本无效，请重新读取后再保存");
        }
    }
    std::string body(request->content_len, '\0');
    size_t received = 0;
    while (received < body.size()) {
        int length = httpd_req_recv(request, body.data() + received, body.size() - received);
        if (length <= 0) {
            httpd_resp_set_hdr(request, "Connection", "close");
            return send_error(request, "408 Request Timeout", "请求接收不完整，请重新提交");
        }
        received += length;
    }
    cJSON *parsed = nullptr;
    std::string error;
    if (!parse_json_object(body, parsed, error)) return send_error(request, "400 Bad Request", error);
    Json patch(parsed, cJSON_Delete);
    CommitResult result;
    {
        LvglLock lock;
        if (!lock.acquired()) return send_error(request, "503 Service Unavailable", "屏幕忙，请重试");
        // Receive/parse the request before locking. Check its revision only now,
        // since the user may have saved from the screen during HTTP reception.
        auto update = [&]() -> CommitResult {
            if (has_revision && "\"" + std::to_string(app.state.revision) + "\"" != if_match) {
                error = "配置已被设备或另一页面修改，请重新读取后再保存";
                return CommitResult::Conflict;
            }
            State next = app.state;
            if (std::strcmp(request->uri, "/api/config") == 0) {
                if (!parse_display_patch(patch.get(), next.current, error)) return CommitResult::Invalid;
            } else if (std::strcmp(request->uri, "/api/defaults") == 0) {
                if (!parse_display_patch(patch.get(), next.defaults, error)) return CommitResult::Invalid;
            } else {
                if (cJSON_GetArraySize(patch.get()) != 0) {
                    error = "恢复操作只接受空 JSON 对象 {}";
                    return CommitResult::Invalid;
                }
                if (std::strcmp(request->uri, "/api/reset") == 0) next.current = next.defaults;
                else if (std::strcmp(request->uri, "/api/factory-reset") == 0) next = State{};
                else { error = "接口不存在"; return CommitResult::Invalid; }
            }
            return app.commit(std::move(next), app.state.revision, error);
        };
        result = update();
    }
    if (result != CommitResult::Ok) {
        const char *status = "500 Internal Server Error";
        if (result == CommitResult::Conflict) status = "409 Conflict";
        if (result == CommitResult::Invalid) status = "400 Bad Request";
        if (result == CommitResult::Unavailable) status = "503 Service Unavailable";
        return send_error(request, status, error);
    }
    return send_state(request);
}

esp_err_t static_file(httpd_req_t *request)
{
    const char *start = index_start, *end = index_end;
    const char *type = "text/html; charset=utf-8";
    if (std::strcmp(request->uri, "/app.js") == 0) {
        start = js_start; end = js_end; type = "application/javascript; charset=utf-8";
    } else if (std::strcmp(request->uri, "/style.css") == 0) {
        start = css_start; end = css_end; type = "text/css; charset=utf-8";
    }
    response_headers(request);
    httpd_resp_set_type(request, type);
    // EMBED_TXTFILES appends a terminating NUL, which is not part of the HTTP body.
    return httpd_resp_send(request, start, end - start - 1);
}
} // namespace

esp_err_t web_server_start(AppContext &context)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 32768; // FreeType smooth renderer requires a substantial stack.
    config.max_uri_handlers = 10;
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    httpd_handle_t server;
    esp_err_t result = httpd_start(&server, &config);
    if (result != ESP_OK) return result;
    struct Route { const char *uri; httpd_method_t method; esp_err_t (*handler)(httpd_req_t *); };
    const Route routes[] = {
        {"/", HTTP_GET, static_file}, {"/app.js", HTTP_GET, static_file},
        {"/style.css", HTTP_GET, static_file}, {"/api/config", HTTP_GET, send_state},
        {"/api/config", HTTP_POST, mutate}, {"/api/defaults", HTTP_POST, mutate},
        {"/api/reset", HTTP_POST, mutate}, {"/api/factory-reset", HTTP_POST, mutate},
    };
    for (const auto &route : routes) {
        httpd_uri_t uri = {};
        uri.uri = route.uri; uri.method = route.method;
        uri.handler = route.handler; uri.user_ctx = &context;
        result = httpd_register_uri_handler(server, &uri);
        if (result != ESP_OK) { httpd_stop(server); return result; }
    }
    return ESP_OK;
}
} // namespace board
