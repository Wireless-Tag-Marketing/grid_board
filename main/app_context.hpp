#pragma once
#include "config_store.hpp"
#include "device_settings.hpp"

namespace board {
enum class CommitResult { Ok, Invalid, Conflict, Unavailable, Failed };
struct AppContext {
    State state;
    ConfigStore store;
    BoardRenderer renderer;
    DeviceSettings settings; // Destroy the overlay before its renderer/fonts.
    bool display_ready = false;
    bool persistence_ready = false;
    // Shared by HTTP and touch. Caller holds the LVGL lock, including all state
    // reads: one lock order avoids HTTP/LVGL deadlocks and partial snapshots.
    CommitResult commit(State next, uint32_t expected_revision, std::string &error);
};
} // namespace board
