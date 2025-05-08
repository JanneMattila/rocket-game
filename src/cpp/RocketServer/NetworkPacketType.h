#pragma once

#include <cstdint>

enum class NetworkPacketType : int8_t {
    CONNECTION_REQUEST = 1,
    CONNECTION_DENIED = 2,
    CHALLENGE = 3,
    CHALLENGE_RESPONSE = 4,
	CONNECTION_ACCEPTED = 5,
	DISCONNECT = 6,
	
    GAME_STATE = 10
};
