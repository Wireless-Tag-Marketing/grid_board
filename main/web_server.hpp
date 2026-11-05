#pragma once
#include "app_context.hpp"
#include "esp_http_server.h"

namespace board {
// HTTP and device controls use the same locked, revision-checked transaction.
esp_err_t web_server_start(AppContext &context);
} // namespace board
