#pragma once
// DirectX Header Files
#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwrite.h>
#include <windows.h>
#include <shellscalingapi.h>
#include <wincodec.h> // For WIC

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) { if ((x)) { (x)->Release(); (x) = nullptr; } }
#endif

class Graphics
{
private:
    // Handle to the application instance
    HINSTANCE m_hInstance = nullptr;

    // Handle to the main window
    HWND m_hWnd = nullptr;

    // DXGI and D3D11 interfaces for VSYNC support
    ID3D11Device* m_pD3DDevice = nullptr;
    ID3D11DeviceContext* m_pD3DContext = nullptr;
    IDXGISwapChain1* m_pSwapChain = nullptr;
    ID2D1Device* m_pD2DDevice = nullptr;
    ID2D1DeviceContext* m_pD2DContext = nullptr;
    ID2D1Bitmap1* m_pD2DTargetBitmap = nullptr;

    // Initialize Direct2D Factory
    ID2D1Factory1* m_pD2DFactory = nullptr;

    // Initialize WIC Factory
    IWICImagingFactory* m_pIWICFactory = nullptr;

    // Create a DirectWrite factory and text format object
    IDWriteFactory* m_pDWriteFactory = nullptr;
    IDWriteTextFormat* m_pTextFormat = nullptr;

    ID2D1SolidColorBrush* m_pWhiteBrush = nullptr;
    ID2D1SolidColorBrush* m_pGrayBrush = nullptr;
    ID2D1SolidColorBrush* m_pBlueBrush = nullptr;

    ID2D1Bitmap* m_pShipBitmap = nullptr;
    ID2D1Bitmap* m_pExplosionBitmap = nullptr;
    ID2D1Bitmap* m_pTileBitmap = nullptr;

    // Fixed render size
    static constexpr UINT RENDER_WIDTH = 1024;
    static constexpr UINT RENDER_HEIGHT = 768;

public:
    Graphics();
    ~Graphics();
    HRESULT InitializeDevice(HWND hWnd, HINSTANCE hInstance);
    void CleanupDevice();
    HRESULT LoadPng(UINT resourceID, ID2D1Bitmap** ppBitmap);
    HRESULT LoadResources();
    void Render(double deltaTime, double fps);
    void Present(); // New method for presenting with VSYNC
};

