#include "Graphics.h"
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")

Graphics::Graphics()
{
}

Graphics::~Graphics()
{
    // Release all Direct2D resources
    CleanupDevice();

    // Uninitialize COM library
    CoUninitialize();
}

HRESULT Graphics::InitializeDevice(HWND hWnd, HINSTANCE hInstance)
{
    if (!hWnd || !hInstance) return E_POINTER;

    m_hInstance = hInstance;
    m_hWnd = hWnd;

    // Create a Direct2D factory
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return hr;

    // Get the client rectangle of the window
    RECT rc{};
    GetClientRect(hWnd, &rc); // hWnd is the handle to your application window

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    hr = m_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hWnd, size),
        &m_pRenderTarget);
    if (FAILED(hr)) return hr;

    hr = m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White),
        &m_pWhiteBrush);
    if (FAILED(hr)) return hr;

    hr = m_pRenderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Gray),
        &m_pGrayBrush);
    if (FAILED(hr)) return hr;

    hr = m_pRenderTarget->CreateSolidColorBrush(
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

    // Load a PNG image from the resources
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<void**>(&m_pIWICFactory));
    if (FAILED(hr)) return hr;

    return hr;
}

void Graphics::CleanupDevice()
{
    // Release all Direct2D resources
    SAFE_RELEASE(m_pRenderTarget);
    SAFE_RELEASE(m_pD2DFactory);
    SAFE_RELEASE(m_pIWICFactory);
    SAFE_RELEASE(m_pTextFormat);
    SAFE_RELEASE(m_pWhiteBrush);
    SAFE_RELEASE(m_pGrayBrush);
    SAFE_RELEASE(m_pBlueBrush);
    SAFE_RELEASE(m_pCarBitmap);
    SAFE_RELEASE(m_pExplosionBitmap);
    SAFE_RELEASE(m_pTileBitmap);
}

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
    hr = m_pRenderTarget->CreateBitmapFromWicBitmap(pConverter, nullptr, ppBitmap);
    if (FAILED(hr)) return hr;
    if (*ppBitmap == nullptr) return E_FAIL;

    // Clean up.
    SAFE_RELEASE(pConverter);
    SAFE_RELEASE(pFrame);
    SAFE_RELEASE(pDecoder);
    SAFE_RELEASE(pStream);
    return hr;
}

void Graphics::Render(double deltaTime)
{
    if (m_pRenderTarget == nullptr) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    std::wstring text = L"Statistics:\n";

    m_pRenderTarget->DrawText(
        text.c_str(),
        (int)text.length(),
        m_pTextFormat,
        D2D1::RectF(10, 10, 400, 200), // Position and size of the text
        m_pWhiteBrush);

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        CleanupDevice();

        // Device lost, recreate device resources
        InitializeDevice(m_hWnd, m_hInstance);
    }
}
