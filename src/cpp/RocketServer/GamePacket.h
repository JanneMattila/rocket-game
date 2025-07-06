#pragma once
#include "NetworkPacket.h"
#include "Keyboard.h"
#include "PlayerState.h"

class GamePacket :
    public NetworkPacket
{
private:

public:
    void SerializePlayerState(const PlayerState& playerState);
    std::vector<PlayerState> DeserializePlayerStates();
    inline PlayerState DeserializePlayerState();
};

