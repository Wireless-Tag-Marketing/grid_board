#pragma once
#include "app_config.hpp"
#include "esp_err.h"
#include "nvs.h"

namespace board {
class ConfigStore {
public:
    esp_err_t initialize(State &state);
    esp_err_t save(const State &state);
    bool available() const { return handle_ != 0; }
    bool recovered_defaults() const { return recovered_defaults_; }
private:
    nvs_handle_t handle_ = 0;
    bool recovered_defaults_ = false;
};
} // namespace board
