#pragma once
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
#define GRID_BOARD_IP "192.168.4.1"
esp_err_t wifi_ap_start(void);
#ifdef __cplusplus
}
#endif
