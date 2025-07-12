#pragma once

#ifdef _WIN32
// DirectX Header Files
#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwrite.h>
#include <shellscalingapi.h>
#include <wincodec.h> // For WIC
#endif

#include "Scene.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) { if ((x)) { (x)->Release(); (x) = nullptr; } }
#endif

class Graphics
{
#ifdef _WIN32
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
#endif

    // Fixed render size
    static constexpr UINT RENDER_WIDTH = 1920;
    static constexpr UINT RENDER_HEIGHT = 1080;

    double m_refreshRateHz = 60.0;

public:
    Graphics();
    ~Graphics();
    HRESULT InitializeDevice(HWND hWnd, HINSTANCE hInstance);
    HRESULT LoadResources();
    void Render(const Scene& scene);
    void Present(); // Use VSYNC

#ifdef _WIN32
    void CleanupDevice();
    HRESULT RecreateDeviceResources();
    HRESULT LoadPng(UINT resourceID, ID2D1Bitmap** ppBitmap);

    double GetRefreshRate() const { return m_refreshRateHz; }
    double GetDisplayRefreshRateWinAPI();
#endif
};
