#include "ShellOverlayContext.h"

ShellOverlayContext::ShellOverlayContext() 
    : m_dummyHwnd(nullptr), m_shellHookMsg(0) 
{
}

ShellOverlayContext::~ShellOverlayContext() 
{ 
    Cleanup(); 
}

bool ShellOverlayContext::Initialize(HINSTANCE instance)
{
    // 1. Das unsichtbare Dummy-Fenster registrieren
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc   = ShellOverlayContext::DummyWndProc;
    wc.hInstance     = instance;
    wc.lpszClassName = L"ShellOverlayDummyClass";
    RegisterClassExW(&wc);

    // 0x0 Pixel, kein WS_VISIBLE. Es blockiert physisch gar nichts, fängt nur Hooks!
    m_dummyHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW, 
        wc.lpszClassName, nullptr, 
        WS_POPUP, 
        0, 0, 0, 0, 
        nullptr, nullptr, instance, this
    );

    if (!m_dummyHwnd) return false;

    // 2. Den System-Hook scharfschalten (Überwacht globale Fensterwechsel der Shell)
    RegisterShellHookWindow(m_dummyHwnd);
    m_shellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");

    // 3. DirectX, DirectComposition und das Acrylic-Effekt-Netzwerk hochfahren
    return CreateD3DAndComposition();
}

bool ShellOverlayContext::CreateD3DAndComposition()
{
    // Direct3D 11 Device mit BGRA-Support zwingend erforderlich für DirectComposition
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, 
                                   D3D11_SDK_VERSION, &m_d3dDevice, &featureLevel, &m_d3dContext);
    if (FAILED(hr)) return false;

    // DXGI Interfaces für die Brücke zur Swapchain holen
    ComPtr<IDXGIDevice> dxgiDevice;
    m_d3dDevice.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);
    ComPtr<IDXGIFactory2> dxgiFactory;
    dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);

    // Gesamte Monitor-Auflösung abgreifen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    // Die fensterlose (Composition-) Swapchain beschreiben
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = screenW;
    desc.Height = screenH;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH; // Zwingend für DComp
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // Modernes Flip-Modell
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; // Essentiell für saubere Transparenz-Blends
    desc.SampleDesc.Count = 1;

    hr = dxgiFactory->CreateSwapChainForComposition(m_d3dDevice.Get(), &desc, nullptr, &m_swapChain);
    if (FAILED(hr)) return false;

    // Das Haupt-DirectComposition-Device starten
    hr = DCompositionCreateDevice(dxgiDevice.Get(), __uuidof(IDCompositionDevice), (void**)&m_dcompDevice);
    if (FAILED(hr)) return false;

    // Das Visual-Target an unser unsichtbares Fenster ketten (TRUE = Oben drüber rendern)
    hr = m_dcompDevice->CreateTargetForHwnd(m_dummyHwnd, TRUE, &m_dcompTarget);
    if (FAILED(hr)) return false;

    // Das Root-Visual erstellen (Unsere fensterlose Zeichenleinwand)
    hr = m_dcompDevice->CreateVisual(&m_rootVisual);
    if (FAILED(hr)) return false;

    // Die Swapchain als Inhalt des Visuals festlegen
    m_rootVisual->SetContent(m_swapChain.Get());
    m_dcompTarget->SetRoot(m_rootVisual.Get());

    // ========================================================================
    // DER MAGISCHE ACRYLIC EFFECT GRAPH
    // ========================================================================
    ComPtr<IDCompositionDevice3> dcompDevice3;
    if (SUCCEEDED(m_dcompDevice.As(&dcompDevice3)))
    {
        ComPtr<IDCompositionGaussianBlurEffect> blurEffect;
        if (SUCCEEDED(dcompDevice3->CreateGaussianBlurEffect(&blurEffect)))
        {
            // 30.0f Deviation entspricht dem originalen Windows 11 Task View Blur!
            blurEffect->SetStandardDeviation(30.0f);
            
            // DCOMP_SOURCE_MODIFIER_BACKGROUND krallt sich die Pixel DIREKT hinter der Ebene
            blurEffect->SetInput(0, nullptr, DCOMP_SOURCE_MODIFIER_BACKGROUND);
            
            // Dem Visual den DWM-System-Blur aufzwingen
            m_rootVisual->SetEffect(blurEffect.Get());
        }
    }

    // Die Swapchain mit dem farbigen Acrylic-Wash tönen
    RenderAcrylicWash();

    // Alles an den Desktop-Manager (DWM) übermitteln
    m_dcompDevice->Commit();

    return true;
}

void ShellOverlayContext::RenderAcrylicWash()
{
    // Backbuffer holen
    ComPtr<ID3D11Texture2D> backBuffer;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    // Render Target erstellen
    ComPtr<ID3D11RenderTargetView> rtv;
    m_d3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);

    // Der originale "Acrylic Slate Wash": Tiefes Anthrazit mit 40% Deckkraft.
    // Wegen PREMULTIPLIED ALPHA müssen die RGB-Kanäle mit dem Alpha-Wert multipliziert sein!
    // 0.1f (RGB) * 0.4f (Alpha) = 0.04f
    float clearColor[4] = { 0.04f, 0.04f, 0.04f, 0.4f };
    
    m_d3dContext->ClearRenderTargetView(rtv.Get(), clearColor);

    // Bild an den DWM übergeben
    m_swapChain->Present(1, 0);
}

LRESULT CALLBACK ShellOverlayContext::DummyWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ShellOverlayContext* ctx = reinterpret_cast<ShellOverlayContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }

    if (ctx) {
        // Das Gesetzbuch schlägt zu: Sobald im Windows-System irgendwo hingeklickt wird
        // (Taskleiste, Startmenü, andere App) und ein Fenster aktiv wird...
        if (msg == ctx->m_shellHookMsg && wp == HSHELL_WINDOWACTIVATED) {
            OutputDebugStringW(L"[ShellOverlay] Fokusänderung bemerkt -> Lautloser Exit.\n");
            PostQuitMessage(0); // ...beenden wir die App verzögerungs- und flackerfrei!
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
    if (m_dummyHwnd) {
        DeregisterShellHookWindow(m_dummyHwnd);
        DestroyWindow(m_dummyHwnd);
        m_dummyHwnd = nullptr;
    }
}