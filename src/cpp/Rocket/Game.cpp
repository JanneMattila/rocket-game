#include <winsock2.h>
#include <chrono>
#include "Game.h"

Game::Game(std::shared_ptr<Logger> logger, volatile std::sig_atomic_t& running)
    : m_logger(logger), m_running(running)
{
}

Game::~Game()
{
}

int Game::InitializeNetwork(std::string server, int port)
{
    m_client = std::make_unique<Client>(m_logger, std::make_unique<Network>(m_logger));
    int result = m_client->Initialize(server, port);
    if (result != 0) return E_FAIL;

    std::unique_ptr<Network> network = std::make_unique<Network>(m_logger);
    m_client = std::make_unique<Client>(m_logger, std::move(network));

    if (m_client->Initialize(server, port) != 0)
    {
        m_logger->Log(LogLevel::WARNING, "Failed to initialize network");
        return 1;
    }

    m_networkingThread = std::jthread(&Game::NetworkLoop, this, m_stopToken);

    return S_OK;
}

 void Game::NetworkLoop(std::stop_token stopToken)
{
    while (m_client->EstablishConnection() != 0 && !stopToken.stop_requested())
    {
        if (stopToken.stop_requested()) break;

        m_logger->Log(LogLevel::WARNING, "Failed to establish connection");
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    if (!stopToken.stop_requested())
    {
        m_logger->Log(LogLevel::INFO, "Connection established");

        if (m_client->ExecuteGame(m_running) != 0)
        {
            m_logger->Log(LogLevel::EXCEPTION, "Game stopped unexpectedly");
            return;
        }

        m_client->QuitGame();
    }
}

 
#ifdef _WIN32
HRESULT Game::InitializeGraphics(HWND hWnd, HINSTANCE hInstance)
{
    HRESULT hr = m_graphics.InitializeDevice(hWnd, hInstance);
    if (FAILED(hr)) return hr;

    hr = m_graphics.LoadResources();
    if (FAILED(hr)) return hr;

    return S_OK;
}
#endif

void Game::Update(double deltaTime, const Keyboard& keyboard)
{
    auto connectionState = m_client->GetConnectionState();
    auto roundTripTimeMs = m_client->GetRoundTripTimeMs();
    if (connectionState != NetworkConnectionState::CONNECTED)
    {
        m_scene.roundTripTimeMs = 0;
        m_scene.players.clear(); // Clear players if not connected
        return; // Exit early if not connected
    }
    
    m_scene.deltaTime = deltaTime;
    m_scene.roundTripTimeMs = roundTripTimeMs;
    /*
    std::optional<std::vector<PlayerState>> playerStates = m_client->ReceiveQueue.pop();

    if (playerStates.has_value())
    {
        m_scene.players = playerStates.value();
        //for (const PlayerState& player : playerStates.value())
        //{

        //}
    }

    if (!m_scene.players.empty())
    {
        PlayerState& player = m_scene.players[0];

        // Convert deltaTime to float for calculations
        float dt = static_cast<float>(deltaTime);

        // Rocket movement parameters
        float& rotation = player.rotation.floatValue;
        float& speed = player.speed.floatValue;
        float& posX = player.pos.x.floatValue;
        float& posY = player.pos.y.floatValue;
        float& velX = player.vel.x.floatValue;
        float& velY = player.vel.y.floatValue;

        // Set default speed if not set
        if (speed == 0.0f) speed = 4000.0f;

        // Handle rotation
        if (keyboard.left)
        {
            rotation -= 2.0f * dt;
        }
        if (keyboard.right)
        {
            rotation += 2.0f * dt;
        }

        // Handle acceleration
        if (keyboard.up)
        {
            velX += std::cos(rotation) * speed * dt;
            velY += std::sin(rotation) * speed * dt;
        }

        // Air resistance
        velX *= 1.0f - (0.5f * dt);
        velY *= 1.0f - (0.5f * dt);

        // Gravity
        velY += 50.0f * dt;

        // Update position
        posX += velX * dt;
        posY += velY * dt;

        // Clamp to screen bounds (assuming you have access to screen width/height)
        const float rocketWidth = 64.0f;  // Replace with actual sprite width if available
        const float rocketHeight = 64.0f; // Replace with actual sprite height if available
        const float screenWidth = 1280.0f; // Replace with your actual screen width
        const float screenHeight = 720.0f; // Replace with your actual screen height

        if (posX < rocketWidth / 2)
        {
            posX = rocketWidth / 2;
            velX = 0;
        }
        if (posX > screenWidth - rocketWidth / 2)
        {
            posX = screenWidth - rocketWidth / 2;
            velX = 0;
        }
        if (posY < rocketHeight / 2)
        {
            posY = rocketHeight / 2;
            velY = 0;
        }
        if (posY > screenHeight - rocketHeight / 2)
        {
            posY = screenHeight - rocketHeight / 2;
            velY = 0;
        }

        // Send game state to the server
        //m_client->SendQueue.push(player);
    }
    */
}

void Game::Render(double fps)
{
    m_scene.fps = fps;
    m_scene.roundTripTimeMs = m_scene.roundTripTimeMs;

    m_graphics.Render(m_scene);
    m_graphics.Present(); // VSYNC support - this will block until next refresh
}
