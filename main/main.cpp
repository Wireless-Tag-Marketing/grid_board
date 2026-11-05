#include "web_server.hpp"
#include "board_display.h"
#include "wifi_ap.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static board::AppContext s_app;
static const char *TAG = "grid_board";

extern "C" void app_main(void)
{
    board_backlight_hold_off();
    esp_err_t result = s_app.store.initialize(s_app.state);
    s_app.persistence_ready = result == ESP_OK && s_app.store.available();
    if (!s_app.persistence_ready) ESP_LOGE(TAG, "Settings storage unavailable: %s", esp_err_to_name(result));

    lv_display_t *display = nullptr;
    result = board_display_start(&display);
    if (result == ESP_OK && lvgl_port_lock(5000)) {
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101317), 0);
        lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
        std::string error;
        auto prepared = s_app.renderer.prepare(s_app.state.current, error);
        if (prepared) {
            s_app.renderer.show(std::move(prepared), s_app.state.current, false);
            lv_refr_now(display);
            s_app.display_ready = true;
            if (!s_app.settings.initialize(s_app.renderer,
                    [](void *owner) { return static_cast<board::AppContext *>(owner)->state; },
                    [](void *owner, board::State next, uint32_t revision, std::string &failure) {
                        return static_cast<board::AppContext *>(owner)->commit(std::move(next), revision, failure)
                               == board::CommitResult::Ok;
                    }, &s_app, error)) {
                ESP_LOGE(TAG, "Touch settings initialization failed");
            } else ESP_LOGI(TAG, "Touch ready: drag to browse; double tap for settings");
        } else ESP_LOGE(TAG, "Display preparation failed");
        lvgl_port_unlock();
        if (s_app.display_ready) {
            vTaskDelay(pdMS_TO_TICKS(100));
            ESP_ERROR_CHECK_WITHOUT_ABORT(board_display_brightness(s_app.state.current.brightness));
        }
    } else ESP_LOGE(TAG, "Display initialization failed: %s", esp_err_to_name(result));

    // Display restoration is independent of C61 connectivity: a missing or
    // incompatible radio must not block the saved message from appearing.
    result = wifi_ap_start();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "AP initialization failed: %s; check C61 ESP-Hosted firmware", esp_err_to_name(result));
        return;
    }
    result = board::web_server_start(s_app);
    if (result == ESP_OK) ESP_LOGI(TAG, "Web configuration ready at http://192.168.4.1");
    else ESP_LOGE(TAG, "HTTP initialization failed: %s", esp_err_to_name(result));
}
