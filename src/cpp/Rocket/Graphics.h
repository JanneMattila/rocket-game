#pragma once
// DirectX Header Files
#include <d2d1.h>
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

    // Handle to the Direct2D render target
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;

    // Initialize Direct2D Factory
    ID2D1Factory* m_pD2DFactory = nullptr;

    // Initialize WIC Factory
    IWICImagingFactory* m_pIWICFactory = nullptr;

    // Create a DirectWrite factory and text format object
    IDWriteFactory* m_pDWriteFactory = nullptr;
    IDWriteTextFormat* m_pTextFormat = nullptr;

    ID2D1SolidColorBrush* m_pWhiteBrush = nullptr;
    ID2D1SolidColorBrush* m_pGrayBrush = nullptr;
    ID2D1SolidColorBrush* m_pBlueBrush = nullptr;

    ID2D1Bitmap* m_pCarBitmap = nullptr;
    ID2D1Bitmap* m_pExplosionBitmap = nullptr;
    ID2D1Bitmap* m_pTileBitmap = nullptr;

public:
    Graphics();
    ~Graphics();
    HRESULT InitializeDevice(HWND hWnd, HINSTANCE hInstance);
    void CleanupDevice();
    HRESULT LoadPng(UINT resourceID, ID2D1Bitmap** ppBitmap);
    void Render(double deltaTime);
};

