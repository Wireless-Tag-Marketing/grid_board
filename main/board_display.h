#pragma once
#include "esp_err.h"
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
void board_backlight_hold_off(void);
esp_err_t board_display_start(lv_display_t **display);
esp_err_t board_display_brightness(int percent);
#ifdef __cplusplus
}
#endif
