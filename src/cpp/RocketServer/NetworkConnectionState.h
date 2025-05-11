#pragma once
#include <cstdint>

enum class NetworkConnectionState : int8_t {
    DISCONNECTED = 1,
    CONNECTING = 2,
    CONNECTED = 3
};
