#pragma once
#include <chrono>
#include "Graphics.h"
#include "Keyboard.h"
#include "Scene.h"

class Game
{
private:
    Graphics m_graphics;
    Scene m_scene;

public:
    Game();
    ~Game();

    HRESULT Initialize(HWND hWnd, HINSTANCE hInstance);
    void Update(double deltaTime, const Keyboard& keyboard);
    void Render(double fps, uint64_t roundTripTimeMs);
};

