#include <chrono>
#include "framework.h"
#include "main.h"
#include "Graphics.h"

constexpr auto MAX_LOADSTRING = 100;

// Global Variables:
HINSTANCE hInst;                                
HWND g_hwnd;                                    
WCHAR szTitle[MAX_LOADSTRING];                  
WCHAR szWindowClass[MAX_LOADSTRING];            
bool g_bRunning = true;

// Improved timing variables
UINT32 g_frameCount = 0;
std::chrono::high_resolution_clock::time_point g_lastFpsUpdate;
std::chrono::high_resolution_clock::time_point g_lastFrameTime;
double g_fps = 0.0;

// Key state variables
bool g_keyUpPressed = false;
bool g_keyDownPressed = false;
bool g_keyLeftPressed = false;
bool g_keyRightPressed = false;

Graphics g_graphics;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HRESULT             InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ROCKET, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    HRESULT hr = InitInstance(hInstance, nCmdShow);
    if (FAILED(hr))
    {
        MessageBox(NULL, L"Failed to initialize the application.", L"Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ROCKET));

    MSG msg = {};

    // Initialize timing
    auto currentTime = std::chrono::high_resolution_clock::now();
    g_lastFrameTime = currentTime;
    g_lastFpsUpdate = currentTime;

    while (g_bRunning)
    {
        auto newTime = std::chrono::high_resolution_clock::now();
        
        // Calculate delta time in seconds
        auto deltaTimeDuration = newTime - g_lastFrameTime;
        double deltaTime = std::chrono::duration_cast<std::chrono::duration<double>>(deltaTimeDuration).count();
        g_lastFrameTime = newTime;

        // Update frame count
        g_frameCount++;

        // Calculate FPS every second
        auto timeSinceLastFpsUpdate = newTime - g_lastFpsUpdate;
        double secondsSinceLastUpdate = std::chrono::duration_cast<std::chrono::duration<double>>(timeSinceLastFpsUpdate).count();
        
        if (secondsSinceLastUpdate >= 1.0)
        {
            g_fps = g_frameCount / secondsSinceLastUpdate;
            g_frameCount = 0;
            g_lastFpsUpdate = newTime;
        }

        // Process messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        g_graphics.Render(deltaTime, g_fps);
        g_graphics.Present(); // VSYNC support - this will block until next refresh
    }

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ROCKET));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}


HRESULT InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd) return E_FAIL;

   HRESULT hr = g_graphics.InitializeDevice(hWnd, hInst);
   if (FAILED(hr)) return hr;

   hr = g_graphics.LoadResources();
   if (FAILED(hr)) return hr;

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   g_hwnd = hWnd;
   return S_OK;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        switch (wParam)
        {
            case VK_ESCAPE:g_bRunning = false; break;
            case VK_UP:    g_keyUpPressed = true; break;
            case VK_DOWN:  g_keyDownPressed = true; break;
            case VK_LEFT:  g_keyLeftPressed = true; break;
            case VK_RIGHT: g_keyRightPressed = true; break;
        }
        break;

    case WM_KEYUP:
        switch (wParam)
        {
            case VK_UP:    g_keyUpPressed = false; break;
            case VK_DOWN:  g_keyDownPressed = false; break;
            case VK_LEFT:  g_keyLeftPressed = false; break;
            case VK_RIGHT: g_keyRightPressed = false; break;
        }
        break;
    break;
    case WM_DESTROY:
        g_bRunning = false; // This will cause the game loop to exit
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
