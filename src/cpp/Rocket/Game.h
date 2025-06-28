#pragma once
#include <chrono>
#include "Graphics.h"
#include "Keyboard.h"

class Game
{
private:
    Graphics m_graphics;

public:
    Game();
    ~Game();
    HRESULT Initialize(HWND hWnd, HINSTANCE hInstance);
    void Update(double deltaTime, const Keyboard& keyboard);
    void Render(double fps);
    void Shutdown();
};

