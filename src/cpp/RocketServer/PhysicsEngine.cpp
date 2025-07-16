#include <cmath>
#include <algorithm>
#include "PhysicsEngine.h"
#include "PlayerState.h"
#include "Keyboard.h"

PlayerState PhysicsEngine::SimulatePlayer(const PlayerState& state, const Keyboard& input, float deltaTime)
{
    PlayerState newState = state;
    
    // Apply input and physics
    ApplyInput(newState, input, deltaTime);
    ApplyPhysics(newState, deltaTime);
    ClampValues(newState);
    
    // Store the input that was applied
    newState.keyboard = input;
    
    return newState;
}

std::vector<PlayerState> PhysicsEngine::SimulateFrame(const std::vector<PlayerState>& currentState, 
                                                     float deltaTime)
{
    std::vector<PlayerState> newState;
    
    // Apply inputs to corresponding players
    for (auto& player : currentState)
    {
        auto updatePlayer = SimulatePlayer(player, player.keyboard, deltaTime);
        newState.push_back(updatePlayer);
    }
    
    return newState;
}

void PhysicsEngine::ApplyInput(PlayerState& player, const Keyboard& input, float deltaTime)
{
    float dt = static_cast<float>(deltaTime);
    
    // Rotation
    if (input.left)
    {
        player.rotation.floatValue -= ROTATION_SPEED * dt;
    }
    if (input.right)
    {
        player.rotation.floatValue += ROTATION_SPEED * dt;
    }
    
    // Normalize rotation to [0, 2?)
    while (player.rotation.floatValue < 0.0f)
    {
        player.rotation.floatValue += 2.0f * 3.14159f;
    }
    while (player.rotation.floatValue >= 2.0f * 3.14159f)
    {
        player.rotation.floatValue -= 2.0f * 3.14159f;
    }
    
    // Thrust
    if (input.up)
    {
        float thrustX = std::cos(player.rotation.floatValue) * ACCELERATION * dt;
        float thrustY = std::sin(player.rotation.floatValue) * ACCELERATION * dt;
        
        player.vel.x.floatValue += thrustX;
        player.vel.y.floatValue += thrustY;
    }
    
    // Optional: Reverse thrust
    if (input.down)
    {
        float thrustX = std::cos(player.rotation.floatValue) * ACCELERATION * dt * -0.5f;
        float thrustY = std::sin(player.rotation.floatValue) * ACCELERATION * dt * -0.5f;
        
        player.vel.x.floatValue += thrustX;
        player.vel.y.floatValue += thrustY;
    }
}

void PhysicsEngine::ApplyPhysics(PlayerState& player, float deltaTime)
{
    float dt = static_cast<float>(deltaTime);
    
    // Apply velocity to position
    player.pos.x.floatValue += player.vel.x.floatValue * dt;
    player.pos.y.floatValue += player.vel.y.floatValue * dt;
    
    // Apply friction
    player.vel.x.floatValue *= FRICTION;
    player.vel.y.floatValue *= FRICTION;
    
    // Calculate speed for display/network
    player.speed.floatValue = std::sqrt(
        player.vel.x.floatValue * player.vel.x.floatValue + 
        player.vel.y.floatValue * player.vel.y.floatValue
    );
    
    // Simple screen wrapping (adjust bounds as needed)
    const float WORLD_WIDTH = 1920.0f;
    const float WORLD_HEIGHT = 1080.0f;
    
    if (player.pos.x.floatValue < 0.0f)
    {
        player.pos.x.floatValue = WORLD_WIDTH;
    } else if (player.pos.x.floatValue > WORLD_WIDTH)
    {
        player.pos.x.floatValue = 0.0f;
    }
    
    if (player.pos.y.floatValue < 0.0f)
    {
        player.pos.y.floatValue = WORLD_HEIGHT;
    } 
    else if (player.pos.y.floatValue > WORLD_HEIGHT)
    {
        player.pos.y.floatValue = 0.0f;
    }
}

void PhysicsEngine::ClampValues(PlayerState& player)
{
    // Clamp velocity to maximum speed
    float currentSpeed = player.speed.floatValue;
    if (currentSpeed > MAX_SPEED)
    {
        float scale = MAX_SPEED / currentSpeed;
        player.vel.x.floatValue *= scale;
        player.vel.y.floatValue *= scale;
        player.speed.floatValue = MAX_SPEED;
    }
}