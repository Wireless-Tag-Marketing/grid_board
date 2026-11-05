cmake_minimum_required(VERSION 3.22)
set(IDF_TARGET esp32p4 CACHE STRING "Hardware target")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(grid_board_web VERSION 1.2.2)
# esp_lvgl_port 2.8 names an LVGL enum removed in LVGL 9.1. This board
# exclusively uses unswapped RGB565; keep the compatibility fix target-local.
if(TARGET lvgl_port_lib)
    target_compile_definitions(lvgl_port_lib PRIVATE LV_COLOR_FORMAT_RGB565_SWAPPED=LV_COLOR_FORMAT_RGB565)
endif()
if(NOT IDF_VERSION_MAJOR EQUAL 6 OR NOT IDF_VERSION_MINOR EQUAL 1)
    message(FATAL_ERROR "Grid Board Web requires ESP-IDF 6.1.x")
endif()
