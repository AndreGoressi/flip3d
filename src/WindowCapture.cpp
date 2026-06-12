#include "WindowCapture.h"
#include <d2d1_2.h>

// ---------------------------------------------------------------------------
// Private InteropCompositor factory interface (IID from ADeltaX blog).
// ---------------------------------------------------------------------------
MIDL_INTERFACE("22118adf-23f1-4801-bcfa-66cbf48cc51b")
IInteropCompositorFactoryPartner : IInspectable
{
    virtual HRESULT __stdcall CreateInteropCompositor(
        IUnknown *renderingDevice, IUnknown *callback,
        REFIID iid, void **instance) = 0;
    virtual HRESULT __stdcall CheckEnabled(
        bool *enableInteropCompositor, bool *enableExposeVisual) = 0;
};

// ---------------------------------------------------------------------------
// Statics
// ---------------------------------------------------------------------------
DwmThumbApi WindowCapture::s_api;
ComPtr<IDCompositionDesktopDevice> WindowCapture::s_dcompDevice;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
template <typename T>
static ComPtr<T> GetFactory(const wchar_t *className)
{
    ComPtr<T> factory;
    HSTRING hstr = nullptr;
    if (SUCCEEDED(WindowsCreateString(className, static_cast<UINT32>(wcslen(className)), &hstr)))
    {
        RoGetActivationFactory(hstr, IID_PPV_ARGS(&factory));
        WindowsDeleteString(hstr);
    }
    return factory;
}

struct __declspec(uuid("A9B3D012-3DF2-4EE3-B8D1-8695F457D3C1")) IDirect3DDxgiInterfaceAccess : IUnknown
{
    virtual HRESULT __stdcall GetInterface(REFIID riid, void **ppv) = 0;
};

template <typename T, typename U>
static ComPtr<T> TryUpgrade(U &source)
{
    ComPtr<T> result;
    source.As(&result);
    return result;
}

static ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>
GetD3DDeviceWrapper(IDXGIDevice *dxgiDevice)
{
    ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> result;
    CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice, &result);
    return result;
}

// ---------------------------------------------------------------------------
// Platform support check
// ---------------------------------------------------------------------------
bool WindowCapture::IsDwmThumbnailPlatformSupported()
{
    return s_api.Load();
}

// ---------------------------------------------------------------------------
// Move / copy
// ---------------------------------------------------------------------------
WindowCapture::WindowCapture(WindowCapture &&other) noexcept
    : m_device(std::move(other.m_device))
    , m_context(std::move(other.m_context))
    , m_captureTexture(std::move(other.m_captureTexture))
    , m_srv(std::move(other.m_srv))
    , m_captureItem(std::move(other.m_captureItem))
    , m_framePool(std::move(other.m_framePool))
    , m_session(std::move(other.m_session))
    , m_thumbVisual(std::move(other.m_thumbVisual))
    , m_hThumbnail(other.m_hThumbnail)
{
    other.m_hThumbnail = nullptr;
}

WindowCapture &WindowCapture::operator=(WindowCapture &&other) noexcept
{
    if (this != &other)
    {
        Release();
        m_device         = std::move(other.m_device);
        m_context        = std::move(other.m_context);
        m_captureTexture = std::move(other.m_captureTexture);
        m_srv            = std::move(other.m_srv);
        m_captureItem    = std::move(other.m_captureItem);
        m_framePool      = std::move(other.m_framePool);
        m_session        = std::move(other.m_session);
        m_thumbVisual    = std::move(other.m_thumbVisual);
        m_hThumbnail     = other.m_hThumbnail;
        other.m_hThumbnail = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------
void WindowCapture::Release()
{
    m_session.Reset();
    m_framePool.Reset();
    m_captureItem.Reset();
    m_thumbVisual.Reset();
    m_srv.Reset();
    m_captureTexture.Reset();
    if (m_context)
    {
        m_context->Flush();
        m_context.Reset();
    }
    m_device.Reset();
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------
HRESULT WindowCapture::Initialize(HWND hwndCapture, HWND hwndDest, ID3D11Device *device)
{
    if (!hwndCapture || !hwndDest || !device)
        return E_INVALIDARG;

    m_device = device;
    device->GetImmediateContext(&m_context);

    // Only one path: DWM thumbnail → InteropCompositor → WGC(CreateFromVisual)
    return InitViaThumbnail(hwndCapture, hwndDest);
}

HRESULT WindowCapture::InitViaThumbnail(HWND hwndCapture, HWND hwndDestination)
{
    // --- Lazy global init: InteropCompositor ---
    if (!s_dcompDevice)
    {
        if (!s_api.Load())
            return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

        ComPtr<ID2D1Factory2> d2dFactory;
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
            IID_PPV_ARGS(&d2dFactory));
        if (FAILED(hr))
            return hr;

        ComPtr<IDXGIDevice> dxgiDevice;
        m_device.As(&dxgiDevice);
        if (!dxgiDevice)
            return E_FAIL;

        ComPtr<ID2D1Device> d2dDevice;
        hr = d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
        if (FAILED(hr))
            return hr;

        ComPtr<IInteropCompositorFactoryPartner> interopFactory;
        {
            HSTRING hstr = nullptr;
            hr = WindowsCreateString(L"Windows.UI.Composition.Compositor", 33, &hstr);
            if (FAILED(hr))
                return hr;
            hr = RoGetActivationFactory(hstr, IID_PPV_ARGS(&interopFactory));
            WindowsDeleteString(hstr);
            if (FAILED(hr))
                return hr;
        }

        ComPtr<IUnknown> compositor;
        hr = interopFactory->CreateInteropCompositor(
            d2dDevice.Get(), nullptr, IID_PPV_ARGS(&compositor));
        if (FAILED(hr))
            return hr;

        hr = compositor.As(&s_dcompDevice);
        if (FAILED(hr))
            return hr;
    }

    // --- Query source size ---
    /*SIZE srcSize = {};
    if (FAILED(s_api.QueryWindowThumbnailSourceSize(hwndCapture, FALSE, &srcSize))
        || srcSize.cx <= 0 || srcSize.cy <= 0)
    {
        return E_FAIL;
    }*/

    SIZE srcSize = {};
    if (FAILED(s_api.QueryWindowThumbnailSourceSize(hwndCapture, FALSE, &srcSize))
        || srcSize.cx <= 0 || srcSize.cy <= 0)
    {

        if (FAILED(s_api.QueryWindowThumbnailSourceSize(hwndCapture, TRUE, &srcSize))
            || srcSize.cx <= 0 || srcSize.cy <= 0)
        {
            RECT wr = {};
            if (!GetWindowRect(hwndCapture, &wr))
                return E_FAIL;
            srcSize.cx = wr.right - wr.left;
            srcSize.cy = wr.bottom - wr.top;
            if (srcSize.cx <= 0 || srcSize.cy <= 0)
                return E_FAIL;
        }
    }

    // --- Create DWM thumbnail visual ---
    DWM_THUMBNAIL_PROPERTIES thumbProps = {};
    thumbProps.dwFlags        = DWM_TNP_VISIBLE
                                | DWM_TNP_RECTDESTINATION 
                                | DWM_TNP_ENABLE3D 
                                | DWM_TNP_DISABLEFORCECVI
                                | DWM_TNP_SOURCECLIENTAREAONLY
                                | 0x00000010;
    thumbProps.fVisible       = TRUE;
    thumbProps.rcDestination  = { 0, 0, srcSize.cx, srcSize.cy };

    HTHUMBNAIL hThumb = nullptr;
    HRESULT hr = s_api.CreateSharedThumbnailVisual(
        hwndDestination, hwndCapture,
        TT_DEFAULT, &thumbProps,
        s_dcompDevice.Get(),
        (void**)m_thumbVisual.GetAddressOf(),
        &hThumb);   
    //
    if (FAILED(hr) || !m_thumbVisual)
        return hr;
    m_hThumbnail = hThumb;

    // --- Wrap in container (needed for IVisual QI) ---
    ComPtr<IDCompositionVisual2> container;
    hr = s_dcompDevice->CreateVisual(&container);
    if (FAILED(hr))
        return hr;

    container->AddVisual(m_thumbVisual.Get(), TRUE, nullptr);

    ComPtr<ABI::Windows::UI::Composition::IVisual> compositionVisual;
    hr = container.As(&compositionVisual);
    if (FAILED(hr))
        return hr;

    // --- Create WGC capture item from the Visual ---
    auto itemStatics = GetFactory<ABI::Windows::Graphics::Capture::IGraphicsCaptureItemStatics>(
        L"Windows.Graphics.Capture.GraphicsCaptureItem");
    if (!itemStatics)
        return E_FAIL;

    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> captureItem;
    hr = itemStatics->CreateFromVisual(compositionVisual.Get(), &captureItem);
    if (FAILED(hr))
        return hr;

    // --- Start WGC session ---
    return StartWGCSession(captureItem.Get(),
        static_cast<UINT>(srcSize.cx), static_cast<UINT>(srcSize.cy));
}

// ---------------------------------------------------------------------------
// WGC session + texture setup (shared)
// ---------------------------------------------------------------------------
HRESULT WindowCapture::StartWGCSession(
    ABI::Windows::Graphics::Capture::IGraphicsCaptureItem *captureItem,
    UINT width, 
    UINT height
)
{
    using namespace ABI::Windows::Graphics::Capture;
    using namespace ABI::Windows::Graphics::DirectX;

    m_captureItem = captureItem;

    ComPtr<IDXGIDevice> dxgiDevice;
    m_device.As(&dxgiDevice);
    if (!dxgiDevice)
        return E_FAIL;

    auto d3dDevice = GetD3DDeviceWrapper(dxgiDevice.Get());
    if (!d3dDevice)
        return E_FAIL;

    ABI::Windows::Graphics::SizeInt32 itemSize = {};
    itemSize.Width  = static_cast<int>(width);
    itemSize.Height = static_cast<int>(height);

    auto poolStatics = GetFactory<IDirect3D11CaptureFramePoolStatics>(
        L"Windows.Graphics.Capture.Direct3D11CaptureFramePool");
    if (!poolStatics)
        return E_FAIL;

    HRESULT hr = poolStatics->Create(
        d3dDevice.Get(),
        DirectXPixelFormat::DirectXPixelFormat_B8G8R8A8UIntNormalized, // B8G8R8A8UIntNormalized
        2,
        itemSize,
        &m_framePool);
    if (FAILED(hr))
        return hr;

    hr = CreateTextureAndSRV(width, height);
    if (FAILED(hr))
        return hr;

    hr = m_framePool->CreateCaptureSession(m_captureItem.Get(), &m_session);
    if (FAILED(hr))
        return hr;

    hr = m_session->StartCapture();
    if (FAILED(hr))
        return hr;

    // Configure session for lower GPU usage.
    // Minimum 33ms between capture updates (~30 fps per window).
    if (auto s5 = TryUpgrade<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession5>(m_session))
    {
        ABI::Windows::Foundation::TimeSpan interval = {};
        interval.Duration = 330000; // 33ms in 100-ns units
        s5->put_MinUpdateInterval(interval);
    }

    // Report-and-render dirty region mode (efficient partial updates).
    if (auto s4 = TryUpgrade<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession4>(m_session))
    {
        s4->put_DirtyRegionMode(
            ABI::Windows::Graphics::Capture::GraphicsCaptureDirtyRegionMode_ReportAndRender);
    }

    return S_OK;
}

// ---------------------------------------------------------------------------
// Texture and SRV creation
// ---------------------------------------------------------------------------
HRESULT WindowCapture::CreateTextureAndSRV(UINT width, UINT height)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width     = width;
    desc.Height    = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage     = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_captureTexture);
    if (FAILED(hr))
        return hr;

    // Zero-fill so there's no garbage before the first captured frame
    {
        std::vector<uint8_t> zeros(width * height * 4, 0);
        m_context->UpdateSubresource(m_captureTexture.Get(), 0, nullptr,
            zeros.data(), width * 4, 0);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format              = desc.Format;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    return m_device->CreateShaderResourceView(
        m_captureTexture.Get(), &srvDesc, &m_srv);
}

// ---------------------------------------------------------------------------
// Per-frame capture polling
// ---------------------------------------------------------------------------
void WindowCapture::PollFrame()
{
    if (!m_framePool || !m_srv || !m_context)
        return;

    using namespace ABI::Windows::Graphics::Capture;

    ComPtr<IDirect3D11CaptureFrame> frame;
    HRESULT hr = m_framePool->TryGetNextFrame(&frame);
    if (FAILED(hr) || !frame)
        return;

    ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface> surface;
    hr = frame->get_Surface(&surface);
    if (FAILED(hr) || !surface)
        return;

    ComPtr<IDirect3DDxgiInterfaceAccess> dxgiAccess;
    hr = surface.As(&dxgiAccess);
    if (FAILED(hr))
        return;

    ComPtr<ID3D11Texture2D> capturedTexture;
    hr = dxgiAccess->GetInterface(IID_PPV_ARGS(&capturedTexture));
    if (FAILED(hr) || !capturedTexture)
        return;

    // GPU-side copy to our managed texture
    m_context->CopyResource(m_captureTexture.Get(), capturedTexture.Get());
}
