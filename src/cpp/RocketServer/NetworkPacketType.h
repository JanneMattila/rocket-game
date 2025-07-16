#pragma once

#include <cstdint>

enum class NetworkPacketType : int8_t {
    UNKNOWN = 0,
    CONNECTION_REQUEST = 1,
    CONNECTION_DENIED = 2,
    CHALLENGE = 3,
    CHALLENGE_RESPONSE = 4,
	CONNECTION_ACCEPTED = 5,
	
    GAME_STATE = 10,
    INPUT_FRAME = 11,

	DISCONNECT = 20,

	PAUSE = 30,
	RESUME = 31,

    CLOCK = 40,
    CLOCK_RESPONSE = 41
};
