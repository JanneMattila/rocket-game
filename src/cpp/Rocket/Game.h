#pragma once
#include "Graphics.h"
#include "Keyboard.h"
#include "Scene.h"
#include "Client.h"

class Game
{
private:
    std::shared_ptr<Logger> m_logger;
    volatile std::sig_atomic_t& m_running;
    std::jthread m_networkingThread{};
    std::stop_token m_stopToken{};

    Graphics m_graphics;
    Scene m_scene;
    std::unique_ptr<Client> m_client;

public:
    Game(std::shared_ptr<Logger> logger, volatile std::sig_atomic_t& running);
    ~Game();

    int InitializeNetwork(std::string server, int port);
#ifdef _WIN32
    HRESULT InitializeGraphics(HWND hWnd, HINSTANCE hInstance);
#endif
    void Update(double deltaTime, const Keyboard& keyboard);
    void Render(double fps);
    void NetworkLoop(std::stop_token stopToken);
};
