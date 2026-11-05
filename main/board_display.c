/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 * Display initialization adapted from the reference WT99P461-S1 hardware project.
 */
#include "board_display.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_jd9165.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "hal/mipi_dsi_host_ll.h"

static const char *TAG = "board_display";
static esp_ldo_channel_handle_t s_dphy_power;

void board_backlight_hold_off(void)
{
    gpio_reset_pin(GPIO_NUM_20);
    gpio_set_direction(GPIO_NUM_20, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_20, 0);
}

esp_err_t board_display_brightness(int percent)
{
    if (percent < 0 || percent > 100) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1,
                                    1023 * percent / 100), TAG, "backlight duty");
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

esp_err_t board_display_start(lv_display_t **display)
{
    ESP_RETURN_ON_FALSE(display, ESP_ERR_INVALID_ARG, TAG, "display is NULL");
    *display = NULL;
    lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_cfg.task_stack = 32768;
    lvgl_cfg.task_max_sleep_ms = 20;
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL init");

    // GT911's active-high reset on GPIO23 is shared with the panel. Preserve
    // the original reset ordering: initialize/reset touch before sending DSI commands.
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM_1, .sda_io_num = GPIO_NUM_48,
        .scl_io_num = GPIO_NUM_47, .clk_source = I2C_CLK_SRC_DEFAULT,
    };
    i2c_master_bus_handle_t i2c;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_cfg, &i2c), TAG, "touch I2C");
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz = 400000;
    esp_lcd_panel_io_handle_t touch_io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c, &io_cfg, &touch_io), TAG, "touch IO");
    esp_lcd_touch_io_gt911_config_t address = {.dev_addr = 0x5d};
    esp_lcd_touch_config_t touch_cfg = {
        .x_max = 1024, .y_max = 600,
        .rst_gpio_num = GPIO_NUM_23, .int_gpio_num = GPIO_NUM_21,
        .levels = {.reset = 1, .interrupt = 0}, .driver_data = &address,
    };
    esp_lcd_touch_handle_t touch;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(touch_io, &touch_cfg, &touch), TAG, "GT911");

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_1, .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t backlight_cfg = {
        .gpio_num = GPIO_NUM_20, .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1, .timer_sel = LEDC_TIMER_1, .duty = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "LEDC timer");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&backlight_cfg), TAG, "LEDC channel");
    esp_ldo_channel_config_t power_cfg = {.chan_id = 3, .voltage_mv = 2500};
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&power_cfg, &s_dphy_power), TAG, "DSI LDO3");
    gpio_config_t scan_cfg = {
        .pin_bit_mask = (1ULL << GPIO_NUM_26) | (1ULL << GPIO_NUM_27),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&scan_cfg), TAG, "scan GPIO");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_26, 1), TAG, "UPDN");
    ESP_RETURN_ON_ERROR(gpio_set_level(GPIO_NUM_27, 1), TAG, "SHLR");

    esp_lcd_dsi_bus_config_t bus_cfg = JD9165BA_PANEL_BUS_DSI_2CH_CONFIG();
    esp_lcd_dsi_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &bus), TAG, "DSI bus");
    esp_lcd_dbi_io_config_t dbi_cfg = JD9165BA_PANEL_IO_DBI_CONFIG();
    esp_lcd_panel_io_handle_t panel_io;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(bus, &dbi_cfg, &panel_io), TAG, "DSI DBI");
    mipi_dsi_host_ll_enable_cmd_ack(MIPI_DSI_LL_GET_HOST(0), false);
    esp_lcd_dpi_panel_config_t dpi_cfg = JD9165BA_1024_600_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_FMT_RGB565);
    dpi_cfg.num_fbs = 2;
    jd9165_vendor_config_t vendor_cfg = {.mipi_config = {.dsi_bus = bus, .dpi_config = &dpi_cfg}};
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = GPIO_NUM_NC, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16, .flags.reset_active_high = 1, .vendor_config = &vendor_cfg,
    };
    esp_lcd_panel_handle_t panel;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_jd9165(panel_io, &panel_cfg, &panel), TAG, "JD9165BA");
    mipi_dsi_host_ll_dpi_enable_frame_ack(MIPI_DSI_LL_GET_HOST(0), false);
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_enable_dma2d(panel), TAG, "DMA2D");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "panel init");

    lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = panel_io, .panel_handle = panel,
        .buffer_size = 1024 * 600, .double_buffer = true,
        .hres = 1024, .vres = 600, .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {.buff_spiram = true, .direct_mode = true},
    };
    lvgl_port_display_dsi_cfg_t dsi_cfg = {.flags.avoid_tearing = true};
    *display = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    ESP_RETURN_ON_FALSE(*display, ESP_FAIL, TAG, "LVGL DSI display");
    lvgl_port_touch_cfg_t input_cfg = {.disp = *display, .handle = touch};
    ESP_RETURN_ON_FALSE(lvgl_port_add_touch(&input_cfg), ESP_FAIL, TAG, "LVGL touch");
    return ESP_OK;
}
