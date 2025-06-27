#include "GamePacket.h"

void GamePacket::SerializePlayerState(const PlayerState& playerState)
{
    WriteInt8(playerState.playerID);
    WriteInt32(playerState.pos.x.intValue);
    WriteInt32(playerState.pos.y.intValue);
    WriteInt32(playerState.vel.x.intValue);
    WriteInt32(playerState.vel.y.intValue);
    WriteInt32(playerState.speed.intValue);
    WriteInt32(playerState.rotation.intValue);
    WriteInt8(playerState.keyboard.ToByte());
}

std::vector<PlayerState> GamePacket::DeserializePlayerStates()
{
    std::vector<PlayerState> playerStates;
    auto players = ReadInt8();
    for (int i = 0; i < players; ++i) {
        PlayerState playerState = DeserializePlayerState();
        playerStates.push_back(playerState);
    }
    return playerStates;
}

inline PlayerState GamePacket::DeserializePlayerState()
{
    PlayerState playerState{};
    playerState.playerID = ReadInt8();
    playerState.pos.x.intValue = ReadInt32();
    playerState.pos.y.intValue = ReadInt32();
    playerState.vel.x.intValue = ReadInt32();
    playerState.vel.y.intValue = ReadInt32();
    playerState.speed.intValue = ReadInt32();
    playerState.rotation.intValue = ReadInt32();
    playerState.keyboard.ReadFromByte(ReadInt8());
    return playerState;
}