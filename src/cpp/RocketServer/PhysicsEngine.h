#pragma once
#include <vector>
#include <cstdint>
#include "PlayerState.h"

class PhysicsEngine
{
private:
    // Physics constants
    static constexpr float ACCELERATION = 100.0f;
    static constexpr float MAX_SPEED = 500.0f;
    static constexpr float ROTATION_SPEED = 3.14159f;
    static constexpr float FRICTION = 0.95f;
    
public:
    // Pure function - no side effects, thread-safe
    static PlayerState SimulatePlayer(const PlayerState& state, const Keyboard& input, float deltaTime);
    
    // Simulate multiple players for one frame
    static std::vector<PlayerState> SimulateFrame(const std::vector<PlayerState>& currentState, 
                                                  float deltaTime);
    
private:
    static void ApplyInput(PlayerState& player, const Keyboard& input, float deltaTime);
    static void ApplyPhysics(PlayerState& player, float deltaTime);
    static void ClampValues(PlayerState& player);
};