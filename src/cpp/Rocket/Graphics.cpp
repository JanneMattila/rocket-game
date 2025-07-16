#ifdef _WIN32
#include <string>
#include "resource.h" // Include your resource header for resource IDs
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")
#endif
#include "Graphics.h"

Graphics::Graphics(std::shared_ptr<Logger> logger)
    : m_logger(logger)
{
}

Graphics::~Graphics()
{
#ifdef _WIN32
    // Clean up all Direct2D and DirectX resources BEFORE uninitializing COM
    CleanupDevice();
    
    // Uninitialize COM library last
    CoUninitialize();
#endif
}

HRESULT Graphics::InitializeDevice(HWND hWnd, HINSTANCE hInstance)
{
#ifdef _WIN32
    if (!hWnd || !hInstance) return E_POINTER;

    m_hInstance = hInstance;
    m_hWnd = hWnd;

    HRESULT hr = S_OK;

    // Create D3D11 device and context
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    // In debug builds, enable DXGI debug layer
#ifdef _DEBUG
    //UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
#else
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#endif

    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags, // Use the debug flag in debug builds
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_pD3DDevice,
        nullptr,
        &m_pD3DContext
    );
    if (FAILED(hr)) return hr;

    // Get DXGI device
    IDXGIDevice* dxgiDevice = nullptr;
    hr = m_pD3DDevice->QueryInterface(&dxgiDevice);
    if (FAILED(hr)) return hr;

    // Get DXGI adapter
    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return hr;

    // Get DXGI factory
    IDXGIFactory2* dxgiFactory = nullptr;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr)) return hr;

    m_refreshRateHz = GetDisplayRefreshRateWinAPI();

    // Create swap chain with the detected refresh rate
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = RENDER_WIDTH;
    swapChainDesc.Height = RENDER_HEIGHT;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2; // Double buffering
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Flags = 0;

    hr = dxgiFactory->CreateSwapChainForHwnd(
        m_pD3DDevice,
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &m_pSwapChain
    );
    if (FAILED(hr)) return hr;

    // Create Direct2D factory
    D2D1_FACTORY_OPTIONS options = {};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &m_pD2DFactory);
    if (FAILED(hr)) return hr;

    // Create Direct2D device
    hr = m_pD2DFactory->CreateDevice(dxgiDevice, &m_pD2DDevice);
    if (FAILED(hr)) return hr;

    // Create Direct2D device context
    hr = m_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_pD2DContext);
    if (FAILED(hr)) return hr;

    // Get back buffer from swap chain
    IDXGISurface* backBuffer = nullptr;
    hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return hr;

    // Create Direct2D bitmap from back buffer
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    hr = m_pD2DContext->CreateBitmapFromDxgiSurface(backBuffer, &bitmapProperties, &m_pD2DTargetBitmap);
    if (FAILED(hr)) return hr;

    // Set the bitmap as the target
    m_pD2DContext->SetTarget(m_pD2DTargetBitmap);

    // Create brushes
    hr = m_pD2DContext->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White),
        &m_pWhiteBrush);
    if (FAILED(hr)) return hr;

    hr = m_pD2DContext->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Gray),
        &m_pGrayBrush);
    if (FAILED(hr)) return hr;

    hr = m_pD2DContext->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Blue),
        &m_pBlueBrush);
    if (FAILED(hr)) return hr;

    // Create a DirectWrite factory and text format object
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory));

    m_pDWriteFactory->CreateTextFormat(
        L"Arial",                // Font family name
        nullptr,                 // Font collection (NULL sets it to the system font collection)
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f,                   // Font size
        L"",                     // Locale
        &m_pTextFormat);

    // Set alignment
    m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // Initialize COM library with STA concurrency model.
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return hr;

    // Create a WIC factory
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<void**>(&m_pIWICFactory));
    if (FAILED(hr)) return hr;

    // Clean up temporary interfaces
    SAFE_RELEASE(backBuffer);
    SAFE_RELEASE(dxgiDevice);
    SAFE_RELEASE(dxgiAdapter);
    SAFE_RELEASE(dxgiFactory);

    return hr;
#else
    return 0;
#endif
}

void Graphics::CleanupDevice()
{
#ifdef _WIN32
    // Release Direct2D resources first (they depend on D3D)
    SAFE_RELEASE(m_pD2DTargetBitmap);
    SAFE_RELEASE(m_pD2DContext);
    SAFE_RELEASE(m_pD2DDevice);
    SAFE_RELEASE(m_pD2DFactory);
    
    // Release brushes and bitmaps
    SAFE_RELEASE(m_pWhiteBrush);
    SAFE_RELEASE(m_pGrayBrush);
    SAFE_RELEASE(m_pBlueBrush);
    SAFE_RELEASE(m_pShipBitmap);
    SAFE_RELEASE(m_pExplosionBitmap);
    SAFE_RELEASE(m_pTileBitmap);
    
    // Release DirectWrite resources
    SAFE_RELEASE(m_pTextFormat);
    SAFE_RELEASE(m_pDWriteFactory);
    
    // Release WIC factory
    SAFE_RELEASE(m_pIWICFactory);
    
    // Release DXGI swap chain before D3D device
    SAFE_RELEASE(m_pSwapChain);
    
    // Release D3D resources last
    SAFE_RELEASE(m_pD3DContext);
    SAFE_RELEASE(m_pD3DDevice);
#endif
}

void Graphics::Render(const Scene& scene)
{
#ifdef _WIN32

    if (m_pD2DContext == nullptr) return;

    m_pD2DContext->BeginDraw();
    m_pD2DContext->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    wchar_t fpsText[256];
    swprintf_s(fpsText, L"FPS: %.0lf, Latency: %llu ms", scene.fps, scene.roundTripTimeMs);

    // Draw the text
    m_pD2DContext->DrawText(
        fpsText,
        (int)wcslen(fpsText),
        m_pTextFormat,
        D2D1::RectF(10, 10, 400, 50),
        m_pWhiteBrush);

    std::wstring text = L"TBA";

    m_pD2DContext->DrawText(
        text.c_str(),
        (int)text.length(),
        m_pTextFormat,
        D2D1::RectF(10, 100, 400, 200),
        m_pWhiteBrush);

    // Draw the first player's ship if available
    if (m_pShipBitmap && !scene.players.empty())
    {
        const PlayerState& player = scene.players[0];
        float x = player.pos.x.floatValue;
        float y = player.pos.y.floatValue;
        float rotation = player.rotation.floatValue;

        // Get ship bitmap size
        float shipWidth = static_cast<float>(m_pShipBitmap->GetSize().width);
        float shipHeight = static_cast<float>(m_pShipBitmap->GetSize().height);

        // Center the ship on (x, y)
        D2D1_POINT_2F center = D2D1::Point2F(x, y);

        // Save the current transform
        D2D1_MATRIX_3X2_F oldTransform;
        m_pD2DContext->GetTransform(&oldTransform);

        // Set transform: translate to position, rotate, then translate back
        D2D1_MATRIX_3X2_F transform =
            D2D1::Matrix3x2F::Rotation(rotation * 180.0f / 3.14159265f, center) *
            D2D1::Matrix3x2F::Translation(0, 0);
        m_pD2DContext->SetTransform(transform * oldTransform);

        // Draw the ship centered at (x, y)
        m_pD2DContext->DrawBitmap(
            m_pShipBitmap,
            D2D1::RectF(
                x - shipWidth / 2.0f,
                y - shipHeight / 2.0f,
                x + shipWidth / 2.0f,
                y + shipHeight / 2.0f
            )
        );

        // Restore the transform
        m_pD2DContext->SetTransform(oldTransform);
    }

    HRESULT hr = m_pD2DContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        // Device lost, need to recreate everything
        RecreateDeviceResources();
    }
#endif
}

HRESULT Graphics::LoadResources()
{
#ifdef _WIN32
    HRESULT hr = S_OK;
    if (m_pD2DContext == nullptr || m_pIWICFactory == nullptr) return E_FAIL;

    // Load ship bitmap
    hr = LoadPng(IDB_PNG_SHIP1, &m_pShipBitmap);
    if (FAILED(hr)) return hr;

    return S_OK;
#else
    return 0;
#endif
}

#ifdef _WIN32
HRESULT Graphics::LoadPng(UINT resourceID, ID2D1Bitmap** ppBitmap)
{
    HRSRC imageResHandle = nullptr;
    HGLOBAL imageResDataHandle = nullptr;
    void* pImageFile = nullptr;
    DWORD imageFileSize = 0;

    *ppBitmap = nullptr;

    // Locate the resource.
    imageResHandle = FindResource(m_hInstance, MAKEINTRESOURCE(resourceID), L"PNG");
    if (!imageResHandle) return E_FAIL;

    // Load the resource.
    imageResDataHandle = LoadResource(m_hInstance, imageResHandle);
    if (!imageResDataHandle) return E_FAIL;

    // Lock it to get a pointer to the resource data.
    pImageFile = LockResource(imageResDataHandle);
    imageFileSize = SizeofResource(m_hInstance, imageResHandle);

    // Create a WIC stream to read the image.
    IWICStream* pStream = nullptr;
    HRESULT hr = m_pIWICFactory->CreateStream(&pStream);
    if (FAILED(hr)) return hr;

    hr = pStream->InitializeFromMemory(reinterpret_cast<BYTE*>(pImageFile), imageFileSize);
    if (FAILED(hr)) return hr;

    // Create a decoder for the stream.
    IWICBitmapDecoder* pDecoder = nullptr;
    hr = m_pIWICFactory->CreateDecoderFromStream(pStream, nullptr, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr)) return hr;

    // Read the first frame of the image.
    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) return hr;

    // Convert the image to a pixel format that Direct2D expects
    IWICFormatConverter* pConverter = nullptr;
    hr = m_pIWICFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) return hr;

    hr = pConverter->Initialize(
        pFrame,                          // Input bitmap to convert
        GUID_WICPixelFormat32bppPBGRA,   // Destination pixel format
        WICBitmapDitherTypeNone,         // Specified dither pattern
        nullptr,                         // Specify a particular palette
        0.f,                             // Alpha threshold
        WICBitmapPaletteTypeCustom       // Palette translation type
    );
    if (FAILED(hr)) return hr;

    // Convert the frame to a Direct2D bitmap.
    hr = m_pD2DContext->CreateBitmapFromWicBitmap(pConverter, nullptr, ppBitmap);
    if (FAILED(hr)) return hr;
    if (*ppBitmap == nullptr) return E_FAIL;

    // Clean up.
    SAFE_RELEASE(pConverter);
    SAFE_RELEASE(pFrame);
    SAFE_RELEASE(pDecoder);
    SAFE_RELEASE(pStream);
    return hr;
}

// Add this new method to handle device recreation properly
HRESULT Graphics::RecreateDeviceResources()
{
    CleanupDevice();
    
    HRESULT hr = InitializeDevice(m_hWnd, m_hInstance);
    if (FAILED(hr))
    {
        // Log error or handle gracefully
        return hr;
    }
    
    // Reload all resources after recreating the device
    hr = LoadResources();
    if (FAILED(hr))
    {
        // Log error or handle gracefully
        return hr;
    }
    
    return S_OK;
}

double Graphics::GetDisplayRefreshRateWinAPI()
{
    DEVMODE devMode = {};
    devMode.dmSize = sizeof(DEVMODE);

    // Try to get settings for the primary display
    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode))
    {
        if (devMode.dmDisplayFrequency > 0 && devMode.dmDisplayFrequency != 1)
        {
            // dmDisplayFrequency of 1 usually means "hardware default"
            return static_cast<double>(devMode.dmDisplayFrequency);
        }
    }

    // If that fails, try getting the default registry setting
    if (EnumDisplaySettings(NULL, ENUM_REGISTRY_SETTINGS, &devMode))
    {
        if (devMode.dmDisplayFrequency > 0 && devMode.dmDisplayFrequency != 1)
        {
            return static_cast<double>(devMode.dmDisplayFrequency);
        }
    }

    return 60.0; // Ultimate fallback
}
#endif

void Graphics::Present()
{
#ifdef _WIN32
    if (m_pSwapChain)
    {
        // Present with VSYNC (1 = wait for vertical sync, 0 = no wait)
        m_pSwapChain->Present(1, 0);
    }
#endif
}
