#include "config_store.hpp"
#include "config_json.hpp"
#include "nvs_flash.h"
#include "esp_log.h"

namespace board {
static const char *TAG = "config_store";
static constexpr char kPartition[] = "board_nvs";

esp_err_t ConfigStore::initialize(State &state)
{
    state = {};
    esp_err_t result = nvs_flash_init_partition(kPartition);
    // Do not erase unrelated partitions, old settings, or a newer schema.
    if (result != ESP_OK) return result;
    result = nvs_open_from_partition(kPartition, "grid_board", NVS_READWRITE, &handle_);
    if (result != ESP_OK) return result;
    size_t size = 0;
    result = nvs_get_blob(handle_, "state", nullptr, &size);
    if (result == ESP_ERR_NVS_NOT_FOUND) return save(state);
    if (result != ESP_OK) return result;
    if (size == 0 || size > 16384) {
        recovered_defaults_ = true;
        ESP_LOGW(TAG, "Invalid settings size; using defaults without overwriting saved data");
        return ESP_OK;
    }
    std::string data(size, '\0');
    result = nvs_get_blob(handle_, "state", data.data(), &size);
    if (result != ESP_OK) return result;
    std::string error;
    if (!decode_state(data, state, error)) {
        recovered_defaults_ = true;
        ESP_LOGW(TAG, "Invalid settings; using defaults without overwriting saved data");
    } else if (upgrade_factory_welcome(state)) {
        return save(state);
    }
    return ESP_OK;
}

esp_err_t ConfigStore::save(const State &state)
{
    if (!available()) return ESP_ERR_INVALID_STATE;
    std::string data = encode_state(state);
    if (data.empty()) return ESP_ERR_NO_MEM;
    esp_err_t result = nvs_set_blob(handle_, "state", data.data(), data.size());
    if (result == ESP_OK) result = nvs_commit(handle_);
    if (result == ESP_OK) recovered_defaults_ = false;
    return result;
}
} // namespace board
