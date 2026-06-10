#include "ShellOverlayContext.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

ShellOverlayContext::ShellOverlayContext()
    : m_instance(nullptr), m_hwnd(nullptr), m_shellHookMsg(0),
      m_screenW(0), m_screenH(0)
{
}

ShellOverlayContext::~ShellOverlayContext()
{
    Cleanup();
}

bool ShellOverlayContext::Initialize(HINSTANCE instance)
{
    m_instance = instance;
    m_screenW  = GetSystemMetrics(SM_CXSCREEN);
    m_screenH  = GetSystemMetrics(SM_CYSCREEN);

    WNDCLASSEXW wc   = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc   = ShellOverlayContext::OverlayWndProc;
    wc.hInstance     = instance;
    wc.lpszClassName = L"ShellOverlayClass";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc)) return false;

    // KEIN WS_EX_LAYERED! DComp und Layered Windows sind inkompatibel.
    // DWM ignoriert DComp-Visuals auf Layered Windows komplett.
    // Stattdessen: leere WindowRegion macht das Fenster fuer Win32 unsichtbar,
    // DComp rendert aber trotzdem darueber (arbeitet unterhalb der Region-Pruefung).
    m_hwnd = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        nullptr,
        WS_POPUP | WS_VISIBLE,
        0, 0, m_screenW, m_screenH,
        nullptr, nullptr, instance, this
    );
    if (!m_hwnd) return false;

    // Leere Region: Fenster zeichnet sich selbst nie, kein weisser Blitz.
    // DComp-Visuals leben im DWM-Compositor und werden davon nicht beeinflusst.
    HRGN emptyRgn = CreateRectRgn(0, 0, 0, 0);
    SetWindowRgn(m_hwnd, emptyRgn, FALSE);
    // Kein DeleteObject(emptyRgn) - SetWindowRgn uebernimmt Ownership!

    RegisterShellHookWindow(m_hwnd);
    m_shellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");

    return CreateD3DAndComposition();
}

bool ShellOverlayContext::CreateD3DAndComposition()
{
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION,
        &m_d3dDevice, &featureLevel, &m_d3dContext
    );
    if (FAILED(hr)) return false;

    ComPtr<IDXGIDevice>   dxgiDevice;
    ComPtr<IDXGIAdapter>  dxgiAdapter;
    ComPtr<IDXGIFactory2> dxgiFactory;
    m_d3dDevice.As(&dxgiDevice);
    dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width            = (UINT)m_screenW;
    desc.Height           = (UINT)m_screenH;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = 2;
    desc.Scaling          = DXGI_SCALING_STRETCH;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode        = DXGI_ALPHA_MODE_PREMULTIPLIED;
    desc.SampleDesc.Count = 1;

    hr = dxgiFactory->CreateSwapChainForComposition(
        m_d3dDevice.Get(), &desc, nullptr, &m_swapChain
    );
    if (FAILED(hr)) return false;

    hr = DCompositionCreateDevice(
        dxgiDevice.Get(), __uuidof(IDCompositionDevice), (void**)&m_dcompDevice
    );
    if (FAILED(hr)) return false;

    hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    if (FAILED(hr)) return false;

    hr = m_dcompDevice->CreateVisual(&m_rootVisual);
    if (FAILED(hr)) return false;

    m_rootVisual->SetCompositeMode(DCOMPOSITION_COMPOSITE_MODE_SOURCE_OVER);
    m_rootVisual->SetContent(m_swapChain.Get());
    m_dcompTarget->SetRoot(m_rootVisual.Get());

    // Acrylic Effect Graph
    ComPtr<IDCompositionDevice3> dcompDevice3;
    if (SUCCEEDED(m_dcompDevice.As(&dcompDevice3)))
    {
        ComPtr<IDCompositionGaussianBlurEffect> blurEffect;
        if (SUCCEEDED(dcompDevice3->CreateGaussianBlurEffect(&blurEffect)))
        {
            blurEffect->SetStandardDeviation(20.0f);
            blurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
            // UINT(1) = Backdrop-Source = Desktop hinter dem Visual
            blurEffect->SetInput(0, nullptr, static_cast<UINT>(1));
            m_rootVisual->SetEffect(blurEffect.Get());
        }
    }

    RenderAcrylicWash();
    m_dcompDevice->Commit();

    return true;
}

void ShellOverlayContext::RenderAcrylicWash()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    ComPtr<ID3D11RenderTargetView> rtv;
    m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);

    // Premultiplied Alpha: Ziel = RGB(15,15,25) @ 45% Alpha
    // Premult-Werte = RGB/255 * Alpha
    float a = 0.45f;
    float clearColor[4] = {
        (15.0f / 255.0f) * a,   // R = 0.0265
        (15.0f / 255.0f) * a,   // G = 0.0265
        (25.0f / 255.0f) * a,   // B = 0.0441
        a
    };
    m_d3dContext->ClearRenderTargetView(rtv.Get(), clearColor);
    m_swapChain->Present(1, 0);
}

LRESULT CALLBACK ShellOverlayContext::OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ShellOverlayContext* ctx = reinterpret_cast<ShellOverlayContext*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA)
    );

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    if (ctx) {
        if (msg == ctx->m_shellHookMsg && wp == HSHELL_WINDOWACTIVATED) {
            PostQuitMessage(0);
            return 0;
        }
        if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        if (msg == WM_ERASEBKGND) return 1;
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShellOverlayContext::RunMessageLoop()
{
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void ShellOverlayContext::Cleanup()
{
    if (m_hwnd) {
        DeregisterShellHookWindow(m_hwnd);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}
