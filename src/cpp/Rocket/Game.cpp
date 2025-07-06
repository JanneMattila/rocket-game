#include "Game.h"

Game::Game()
{
}

Game::~Game()
{
}

HRESULT Game::Initialize(HWND hWnd, HINSTANCE hInstance)
{
    HRESULT hr = m_graphics.InitializeDevice(hWnd, hInstance);
    if (FAILED(hr)) return hr;

    hr = m_graphics.LoadResources();
    if (FAILED(hr)) return hr;

    return S_OK;
}

void Game::Update(double deltaTime, const Keyboard& keyboard)
{
    // Other update logic here
    m_scene.deltaTime = deltaTime;
}

void Game::Render(double fps, uint64_t roundTripTimeMs)
{
    m_scene.fps = fps;
    m_scene.roundTripTimeMs = roundTripTimeMs;

    m_graphics.Render(m_scene);
    m_graphics.Present(); // VSYNC support - this will block until next refresh
}
