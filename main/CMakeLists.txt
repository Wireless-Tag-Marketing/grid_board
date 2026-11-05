idf_component_register(
    SRCS "main.cpp" "app_config.cpp" "config_json.cpp" "config_store.cpp"
         "board_display.c" "board_font.cpp" "board_renderer.cpp" "device_settings.cpp" "app_context.cpp"
         "web_server.cpp" "wifi_ap.c" "lvgl_memory.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash espressif__cjson esp_http_server esp_wifi esp_event esp_netif esp_timer
             esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_lcd esp_hw_support
             esp_hal_lcd esp_lcd_jd9165 esp_lcd_touch_gt911 esp_lvgl_port lvgl
             espressif__esp_hosted espressif__freetype
    EMBED_FILES "fonts/NotoSansSC-Regular.otf" "fonts/NotoEmoji-Regular.ttf"
    EMBED_TXTFILES "web/index.html" "web/app.js" "web/style.css"
    WHOLE_ARCHIVE)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
