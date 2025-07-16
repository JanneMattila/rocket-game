#include <chrono>
#include "framework.h"
#include "main.h"
#include "Graphics.h"
#include "Game.h"
#include "Keyboard.h"
#include "Logger.h"
#include "Client.h"

constexpr auto MAX_LOADSTRING = 100;

// Global Variables:
HINSTANCE hInst;                                
HWND g_hwnd;                                    
WCHAR szTitle[MAX_LOADSTRING];                  
WCHAR szWindowClass[MAX_LOADSTRING];            
volatile std::sig_atomic_t g_running = 1;

// Key state variables
Keyboard g_keyboard{};
std::shared_ptr<Logger> g_logger;
std::unique_ptr<Game> g_game;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
HRESULT             InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

static std::string GetEnvVariable(const char* varName) {
    std::string result;

#ifdef _WIN32
    char* buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, varName) == 0 && buffer != nullptr) {
        result = buffer;
        free(buffer); // Free the allocated memory
    }
#else
    const char* value = std::getenv(varName);
    if (value != nullptr) {
        result = value;
    }
#endif

    return result;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_logger = std::make_shared<Logger>();
    g_logger->SetLogLevel(LogLevel::DEBUG);

    g_logger->Log(LogLevel::INFO, "Rocket starting");

    std::string server = "127.0.0.1";
    int udpPort = 3501;

    // Check for environment variables
    std::string envServer = GetEnvVariable("UDP_SERVER");
    std::string envPort = GetEnvVariable("UDP_PORT");
    if (!envPort.empty())
    {
        udpPort = std::atoi(envPort.data());
    }
    if (!envServer.empty())
    {
        server = envServer;
    }

    g_logger->Log(LogLevel::INFO, "UDP Server", { KVS(server), KV(udpPort) });

    g_game = std::make_unique<Game>(g_logger, g_running);

    if (g_game->InitializeNetwork(server, udpPort) != 0)
    {
        g_logger->Log(LogLevel::WARNING, "Failed to initialize network");
        return 1;
    }

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

    MSG msg{};

    // Initialize timing
    UINT32 frameCount = 0;
    std::chrono::high_resolution_clock::time_point g_lastFpsUpdate;
    std::chrono::high_resolution_clock::time_point g_lastFrameTime;
    double fps = 0.0;

    auto currentTime = std::chrono::high_resolution_clock::now();
    g_lastFrameTime = currentTime;
    g_lastFpsUpdate = currentTime;

    while (g_running)
    {
        auto newTime = std::chrono::high_resolution_clock::now();
        
        // Calculate delta time in seconds
        auto deltaTimeDuration = newTime - g_lastFrameTime;
        double deltaTime = std::chrono::duration_cast<std::chrono::duration<double>>(deltaTimeDuration).count();
        g_lastFrameTime = newTime;

        // Update frame count
        frameCount++;

        // Calculate FPS every second
        auto timeSinceLastFpsUpdate = newTime - g_lastFpsUpdate;
        double secondsSinceLastUpdate = std::chrono::duration_cast<std::chrono::duration<double>>(timeSinceLastFpsUpdate).count();
        
        if (secondsSinceLastUpdate >= 1.0)
        {
            fps = frameCount / secondsSinceLastUpdate;
            frameCount = 0;
            g_lastFpsUpdate = newTime;
        }

        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        g_game->Update(deltaTime, g_keyboard);
        g_game->Render(fps);
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

   //HRESULT hr = g_game->InitializeGraphics(hWnd, hInstance);
   //if (FAILED(hr)) return hr;

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
            case VK_ESCAPE:g_running = false; break;
            case VK_SPACE: g_keyboard.space = true; break;
            case VK_UP:    g_keyboard.up = true; break;
            case VK_DOWN:  g_keyboard.down = true; break;
            case VK_LEFT:  g_keyboard.left = true; break;
            case VK_RIGHT: g_keyboard.right = true; break;
        }
        break;

    case WM_KEYUP:
        switch (wParam)
        {
            case VK_SPACE: g_keyboard.space = false; break;
            case VK_UP:    g_keyboard.up = false; break;
            case VK_DOWN:  g_keyboard.down = false; break;
            case VK_LEFT:  g_keyboard.left = false; break;
            case VK_RIGHT: g_keyboard.right = false; break;
        }
        break;

    case WM_DESTROY:
        g_running = false; // This will cause the game loop to exit
    PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
