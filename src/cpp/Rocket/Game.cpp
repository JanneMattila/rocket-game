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
}

void Game::Render(double fps)
{
    m_graphics.Render(fps);
    m_graphics.Present(); // VSYNC support - this will block until next refresh
}
