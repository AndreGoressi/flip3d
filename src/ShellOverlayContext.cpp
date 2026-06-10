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

    // ---------------------------------------------------------------------------
    // 1. Fensterklasse registrieren
    // ---------------------------------------------------------------------------
    WNDCLASSEXW wc    = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc    = ShellOverlayContext::OverlayWndProc;
    wc.hInstance      = instance;
    wc.lpszClassName  = L"ShellOverlayClass";
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc)) return false;

    // ---------------------------------------------------------------------------
    // 2. Das Ankerfenster anlegen
    //
    //    Warum diese Flags?
    //    WS_EX_LAYERED       -> DWM behandelt das Fenster als composited surface,
    //                           nur so kann DComp darueber rendern
    //    WS_EX_TRANSPARENT   -> Mausklicks fallen durch das Fenster hindurch
    //    WS_EX_TOPMOST       -> Liegt ueber allem ausser dem eigentlichen DComp-Visual
    //    WS_EX_TOOLWINDOW    -> Taucht nicht in der Taskleiste auf
    //    WS_EX_NOACTIVATE    -> Stiehlt keinen Fokus beim Erscheinen
    //
    //    Das Fenster bekommt VOLLE Bildschirmgroesse, damit DComp den
    //    CompleteTarget korrekt berechnen kann. Es ist trotzdem unsichtbar
    //    weil wir SetLayeredWindowAttributes mit Alpha=0 setzen, BEVOR
    //    WS_VISIBLE aktiv wird.  DComp rendert dann darueber.
    // ---------------------------------------------------------------------------
    m_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        nullptr,
        WS_POPUP,
        0, 0, m_screenW, m_screenH,
        nullptr, nullptr, instance, this
    );
    if (!m_hwnd) return false;

    // Fenster komplett durchsichtig machen (DComp-Visual laeuft darueber)
    SetLayeredWindowAttributes(m_hwnd, 0, 0, LWA_ALPHA);

    // Fenster sichtbar schalten (noetig damit DComp ein gueltiges Target bekommt)
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hwnd);

    // ---------------------------------------------------------------------------
    // 3. Shell-Hook fuer Fokus-Erkennung registrieren
    // ---------------------------------------------------------------------------
    RegisterShellHookWindow(m_hwnd);
    m_shellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");

    // ---------------------------------------------------------------------------
    // 4. DirectX + DirectComposition + Acrylic-Effekt-Graph hochfahren
    // ---------------------------------------------------------------------------
    return CreateD3DAndComposition();
}

bool ShellOverlayContext::CreateD3DAndComposition()
{
    // --- D3D11 Device ---
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,   // Pflicht fuer DComp
        nullptr, 0, D3D11_SDK_VERSION,
        &m_d3dDevice, &featureLevel, &m_d3dContext
    );
    if (FAILED(hr)) return false;

    // --- DXGI Interfaces ---
    ComPtr<IDXGIDevice>   dxgiDevice;
    ComPtr<IDXGIAdapter>  dxgiAdapter;
    ComPtr<IDXGIFactory2> dxgiFactory;

    m_d3dDevice.As(&dxgiDevice);
    dxgiDevice->GetAdapter(&dxgiAdapter);
    dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);

    // --- Composition-Swapchain (fensterlos, lebt im DComp-Visual-Tree) ---
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width       = (UINT)m_screenW;
    desc.Height      = (UINT)m_screenH;
    desc.Format      = DXGI_FORMAT_B8G8R8A8_UNORM; // BGRA ist Standard fuer DComp
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling     = DXGI_SCALING_STRETCH;
    desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED; // Pflicht fuer Alpha-Blending mit DWM
    desc.SampleDesc.Count = 1;

    hr = dxgiFactory->CreateSwapChainForComposition(
        m_d3dDevice.Get(), &desc, nullptr, &m_swapChain
    );
    if (FAILED(hr)) return false;

    // --- DComp Device ---
    hr = DCompositionCreateDevice(
        dxgiDevice.Get(), __uuidof(IDCompositionDevice), (void**)&m_dcompDevice
    );
    if (FAILED(hr)) return false;

    // --- Target an das SICHTBARE, bildschirmgrosse Fenster haengen ---
    //     TRUE = topmost in der Visual-Tree Reihenfolge
    hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    if (FAILED(hr)) return false;

    // --- Root-Visual erstellen und Swapchain als Inhalt setzen ---
    hr = m_dcompDevice->CreateVisual(&m_rootVisual);
    if (FAILED(hr)) return false;

    m_rootVisual->SetContent(m_swapChain.Get());
    m_dcompTarget->SetRoot(m_rootVisual.Get());

    // -----------------------------------------------------------------------
    // ACRYLIC EFFECT GRAPH
    // IDCompositionDevice3 ist noetig fuer Blur-Effekte
    // -----------------------------------------------------------------------
    ComPtr<IDCompositionDevice3> dcompDevice3;
    if (SUCCEEDED(m_dcompDevice.As(&dcompDevice3)))
    {
        ComPtr<IDCompositionGaussianBlurEffect> blurEffect;
        if (SUCCEEDED(dcompDevice3->CreateGaussianBlurEffect(&blurEffect)))
        {
            blurEffect->SetStandardDeviation(30.0f);

            // Input-Index 0, nullptr als Source = Desktop-Hintergrund (Wert 1 = backdrop)
            blurEffect->SetInput(0, nullptr, static_cast<UINT>(1));

            m_rootVisual->SetEffect(blurEffect.Get());
        }
    }

    // Den Acrylic-Farb-Wash rendern (dunkles Anthrazit mit 40% Deckkraft)
    RenderAcrylicWash();

    // Alles an DWM uebergeben
    m_dcompDevice->Commit();

    return true;
}

void ShellOverlayContext::RenderAcrylicWash()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    ComPtr<ID3D11RenderTargetView> rtv;
    m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);

    // Premultiplied Alpha: RGB = 0.06 * 0.4 (alpha) = 0.024
    // Leicht blaustichiges Anthrazit, angelehnt an Win11 Task View
    float clearColor[4] = { 0.024f, 0.024f, 0.040f, 0.4f };
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
        // Shell meldet: ein anderes Fenster wurde aktiv -> wir verschwinden
        if (msg == ctx->m_shellHookMsg && wp == HSHELL_WINDOWACTIVATED) {
            OutputDebugStringW(L"[ShellOverlay] Fokusaenderung -> Exit.\n");
            PostQuitMessage(0);
            return 0;
        }

        // ESC als Notausstieg waehrend der Entwicklung
        if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
            PostQuitMessage(0);
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
