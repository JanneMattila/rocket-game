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

    while (m_client->SyncClock() != 0 && !stopToken.stop_requested())
    {
        if (stopToken.stop_requested()) break;

        m_logger->Log(LogLevel::WARNING, "Failed to sync clock");
        std::this_thread::sleep_for(std::chrono::seconds(1));
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
    m_graphics = std::make_unique<Graphics>(m_logger);
    HRESULT hr = m_graphics->InitializeDevice(hWnd, hInstance);
    if (FAILED(hr)) return hr;

    hr = m_graphics->LoadResources();
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
    
    // Send input to network thread
    PlayerState playerState;
    playerState.deltaTime = deltaTime;
    playerState.keyboard = keyboard;
    m_client->OutgoingState.push(playerState);
    
    // Get current game state for rendering
    while (true)
    {
        auto currentState = m_client->IncomingStates.pop();
        if (!currentState.has_value())
        {
            break; // No more states to process
        }

        m_scene.players = currentState.value();
    }
}

void Game::Render(double fps)
{
    m_scene.fps = fps;

    if (m_graphics)
    {
        m_graphics->Render(m_scene);
        m_graphics->Present();
    }
}
