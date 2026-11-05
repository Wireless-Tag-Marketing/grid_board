#pragma once
#include "app_config.hpp"
#include "cJSON.h"

namespace board {
// Patches are strict: unknown/duplicate fields and coerced types are rejected.
bool parse_display_patch(const cJSON *json, DisplayConfig &config, std::string &error);
cJSON *display_json(const DisplayConfig &config);
std::string encode_state(const State &state);
bool decode_state(const std::string &data, State &state, std::string &error);
bool parse_json_object(const std::string &data, cJSON *&json, std::string &error);
} // namespace board
