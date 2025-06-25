#include <chrono>
#include "framework.h"
#include "main.h"
#include "Graphics.h"

constexpr auto MAX_LOADSTRING = 100;

// Global Variables:
HINSTANCE hInst;                                // current instance
HWND g_hwnd;                                      // Handle to the window
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
bool g_bRunning = true;

// Global or class member variables
UINT32 g_frameCount = 0;
double g_lastTime = 0.0;
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
        // Display an error message box
        MessageBox(NULL, L"Failed to initialize the application.", L"Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ROCKET));

    MSG msg = {};

    // Use high_resolution_clock for timing
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto timeSpan = currentTime.time_since_epoch();
    double seconds = std::chrono::duration_cast<std::chrono::duration<double>>(timeSpan).count();

    while (g_bRunning)
    {
        // Update frame count every frame
        g_frameCount++;

        // Calculate delta time
        auto newTime = std::chrono::high_resolution_clock::now();
        auto newTimeSpan = newTime.time_since_epoch();
        double newSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(newTimeSpan).count();
        double runningTime = newSeconds - g_lastTime;
        double deltaTime = newSeconds - seconds;
        seconds = newSeconds;

        // Update FPS every second
        if (runningTime >= 1.0)
        {
            g_fps = g_frameCount / runningTime;
            g_frameCount = 0;
            g_lastTime = newSeconds;
        }

        // Process messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        g_graphics.Render(deltaTime);
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
