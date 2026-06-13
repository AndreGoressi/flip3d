#include "Flip3DRenderer.h"
#include "Shaders.h"
#include "Capture.h"

namespace
{
    void ActivateWindowLikeThreadMessage403(HWND selectedHwnd)
    {
        if (selectedHwnd == nullptr || !IsWindow(selectedHwnd)) return;

        HWND activationTarget = selectedHwnd;

        if (!IsWindowEnabled(activationTarget))
        {
            if (HWND ancestor = GetAncestor(activationTarget, GA_ROOTOWNER))
            {
                if (HWND lastActivePopup = GetLastActivePopup(ancestor))
                    activationTarget = lastActivePopup;
            }
        }

        if (activationTarget != nullptr && IsWindow(activationTarget))
            SwitchToThisWindow(activationTarget, TRUE);
    }

    bool DispatchImmediateSelectedWindowActivation(HWND selectedHwnd, bool wasMinimized, bool wasShellDesktop)
    {
        if (selectedHwnd == nullptr || !IsWindow(selectedHwnd)) return false;
        if (wasMinimized) return false;

        if (wasShellDesktop || selectedHwnd == GetShellWindow())
        {
            if (HWND shellTray = FindWindowW(L"Shell_TrayWnd", nullptr))
                PostMessageW(shellTray, 0x579u, 1u, 0);
            return true;
        }

        return false;
    }

    void CompleteDeferredSelectedWindowActivation(HWND selectedHwnd, bool activationAlreadyDispatched)
    {
        if (activationAlreadyDispatched) return;
        ActivateWindowLikeThreadMessage403(selectedHwnd);
    }
} // namespace


// ============================================================================
// Initialisation
// ============================================================================
bool Flip3DRenderer::Initialize(HINSTANCE instance)
{
    RoInitialize(RO_INIT_MULTITHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    m_instance = instance;
    LoadFlip3DPreferences();
    BuildCardModels();

    if (!Render3Dstack()) return false;
    m_fRTLMirror = (GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;

    if (FAILED(InitializeD3D())) return false;
    CreateWindowCaptures();

    m_enterTimeline.Restart(0.0f, 1.0f, gEnterExitDurationSec, InterpolationMode::Cubic);
    m_state = ViewState::Enter;
    m_originalFrontHWND = m_cards.empty() ? nullptr : m_cards.front().hwnd;
    m_previousFrameTime = std::chrono::steady_clock::now();
    return true;
}

int Flip3DRenderer::Run()
{
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (msg.message == WM_QUIT) break;
        //
        if (!m_minimized)
        {
            if (m_frameLatencyWaitableObject != nullptr)
            {
                WaitForSingleObjectEx(m_frameLatencyWaitableObject, 1000, TRUE);
            }

            const auto now = std::chrono::steady_clock::now();
            const float deltaSeconds = std::chrono::duration<float>(now - m_previousFrameTime).count();
            m_previousFrameTime = now;
            Update(std::min(deltaSeconds, 0.05f));
            Render(); 
        }
        else
        {
            WaitMessage();
            m_previousFrameTime = std::chrono::steady_clock::now();
        }
    }
    return static_cast<int>(msg.wParam);
}

// ============================================================================
// Window procedure
// ============================================================================
LRESULT CALLBACK Flip3DRenderer::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        auto *self = static_cast<Flip3DRenderer *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    }
    auto *self = reinterpret_cast<Flip3DRenderer *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->HandleMessage(message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}


// ============================================================================
// Card model building
// ============================================================================
void Flip3DRenderer::BuildCardModels()
{
    m_cards.clear();
    const auto windowLayouts = CapturePrimaryMonitorWindowRects(kMaxProxyCards, m_hwnd);
    if (windowLayouts.empty()) return;

    for (const auto &layout : windowLayouts)
    {
        const bool isMinimized = layout.isMinimized;
        const RECT &targetRect = layout.targetRect;
        const RECT &originalRect = isMinimized ? targetRect : layout.originalRect;
        const RECT &wa = layout.monitorWork;
        const float monW = static_cast<float>(std::max(1L, wa.right - wa.left));
        const float monH = static_cast<float>(std::max(1L, wa.bottom - wa.top));

        const float targetWidth = static_cast<float>(std::max(1L, targetRect.right - targetRect.left));
        const float targetHeight = static_cast<float>(std::max(1L, targetRect.bottom - targetRect.top));
        const float originalWidth = static_cast<float>(std::max(1L, originalRect.right - originalRect.left));
        const float originalHeight = static_cast<float>(std::max(1L, originalRect.bottom - originalRect.top));

        float normW = targetWidth, normH = targetHeight;
        if (normW > monW || normH > monH)
        {
            const float scale = std::min(monW / normW, monH / normH);
            normW *= scale; normH *= scale;
        }

        CardModel card;
        card.aspectRatio = targetWidth / std::max(targetHeight, 1.0f);
        card.hwnd = layout.hwnd;
        card.targetWorldSize = { normW / monW, -(normH / monH) };
        card.originalWorldPosition = {
            (static_cast<float>(originalRect.left - wa.left) / monW) - 0.5f,
            0.5f - (static_cast<float>(originalRect.top - wa.top) / monH),
        };
        card.originalWorldSize = { originalWidth / monW, -(originalHeight / monH) };
        card.sourceOccupancy = std::max(std::abs(card.targetWorldSize.x), std::abs(card.targetWorldSize.y));
        card.isMinimized = layout.isMinimized;
        card.hasOriginalRect = true;
        m_cards.push_back(std::move(card));
    }
}

// ============================================================================
// Create per-window captures
// ============================================================================
void Flip3DRenderer::CreateWindowCaptures()
{
    if (!m_device) return;
    ComPtr<IDXGIDevice> dxgiDevice;
    m_device.As(&dxgiDevice);
    if (!dxgiDevice) return;

    for (auto &card : m_cards)
    {
        if (!card.hwnd) continue;
        auto cap = std::make_unique<WindowCapture>();
        HRESULT hr = cap->Initialize(card.hwnd, m_hwnd, m_device.Get());
        if (SUCCEEDED(hr))
            card.captureSRV = cap->GetSRV();
        card.capture = std::move(cap);
    }
}

enum ACCENT_STATE {
    ACCENT_DISABLED                   = 0,
    ACCENT_ENABLE_GRADIENT            = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND          = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND   = 4,
    ACCENT_ENABLE_HOSTBACKDROP        = 5,
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD        AccentFlags;
    DWORD        GradientColor;   // Format 0xAABBGGRR
    DWORD        AnimationId;
};

enum WINDOWCOMPOSITIONATTRIB { WCA_ACCENT_POLICY = 19 };

struct WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID                   pvData;
    SIZE_T                  cbData;
};

typedef BOOL (WINAPI* SetWindowCompositionAttribute_t)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

bool Flip3DRenderer::ApplyAcrylic(HWND hwnd)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return false;

    auto SetWCA = reinterpret_cast<SetWindowCompositionAttribute_t>(
        GetProcAddress(user32, "SetWindowCompositionAttribute"));
    if (!SetWCA) return false;

    ACCENT_POLICY accent = {};
    accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    accent.AccentFlags = 0;
    accent.GradientColor = 0x73190F0F; 

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &accent;
    data.cbData = sizeof(accent);

    return SetWCA(hwnd, &data) != FALSE;
}

// ============================================================================
// Window creation
// ============================================================================
bool Flip3DRenderer::Render3Dstack()
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize        = sizeof(windowClass);
    windowClass.hInstance     = m_instance;
    windowClass.lpfnWndProc   = &Flip3DRenderer::WndProc;
    windowClass.lpszClassName = kRenderClassName;
    windowClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.style         = CS_HREDRAW | CS_VREDRAW;
    windowClass.hbrBackground = nullptr; 

    if (!GetClassInfoExW(m_instance, kRenderClassName, &windowClass)) {
        if (!RegisterClassExW(&windowClass)) return false;
    }

    RECT wc{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wc, 0);
    const int w_x       = wc.left;
    const int w_y       = wc.top;
    const int w_screenW = wc.right  - wc.left;
    const int w_screenH = wc.bottom - wc.top;

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW, 
        kRenderClassName, 
        kTitle,
        WS_POPUP | WS_VISIBLE, 
        w_x, 
        w_y, 
        w_screenW, 
        w_screenH, 
        nullptr, 
        nullptr, 
        m_instance, 
        this
    );
    
    if (m_hwnd)
    {
        ApplyAcrylic(m_hwnd);
    }
    return m_hwnd != nullptr;
}

// ============================================================================
// D3D initialisation
// ============================================================================
HRESULT Flip3DRenderer::InitializeD3D()
{
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    static constexpr D3D_FEATURE_LEVEL requestedLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
    };
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
        requestedLevels, static_cast<UINT>(std::size(requestedLevels)), D3D11_SDK_VERSION,
        &m_device, nullptr, &m_context);
#if defined(_DEBUG)
    if (FAILED(hr))
    {
        creationFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
            requestedLevels, static_cast<UINT>(std::size(requestedLevels)), D3D11_SDK_VERSION,
            &m_device, nullptr, &m_context);
    }
#endif
    if (FAILED(hr))
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            requestedLevels, static_cast<UINT>(std::size(requestedLevels)), D3D11_SDK_VERSION,
            &m_device, nullptr, &m_context);
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) return hr;

    hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&m_dcompDevice));
    if (FAILED(hr)) return hr;
    hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    if (FAILED(hr)) return hr;
    hr = m_dcompDevice->CreateVisual(&m_dcompVisual);
    if (FAILED(hr)) return hr;
    hr = m_dcompTarget->SetRoot(m_dcompVisual.Get());
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) return hr;
    ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;

    RECT clientRect = {};
    GetClientRect(m_hwnd, &clientRect);
    m_width = std::max<UINT>(1, clientRect.right - clientRect.left);
    m_height = std::max<UINT>(1, clientRect.bottom - clientRect.top);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    
    hr = factory->CreateSwapChainForComposition(m_device.Get(), &swapChainDesc, nullptr, &m_swapChain);
    if (FAILED(hr)) return hr;

    ComPtr<IDXGISwapChain2> swapChain2;
    if (SUCCEEDED(m_swapChain.As(&swapChain2)))
    {
        swapChain2->SetMaximumFrameLatency(1); 
        m_frameLatencyWaitableObject = swapChain2->GetFrameLatencyWaitableObject();
    }

    hr = m_dcompVisual->SetContent(m_swapChain.Get());
    if (FAILED(hr)) return hr;
    hr = m_dcompDevice->Commit();
    if (FAILED(hr)) return hr;
    hr = CreateDeviceResources();
    if (FAILED(hr)) return hr;
    return CreateWindowSizeResources(false);
}

HRESULT Flip3DRenderer::CreateDeviceResources()
{
    HRESULT hr = S_OK;
    ComPtr<ID3DBlob> backgroundVS, backgroundPS, cardVS, cardPS;
    hr = CompileShader(kBackgroundVertexShader, "main", "vs_5_0", backgroundVS);
    if (FAILED(hr)) return hr;
    hr = CompileShader(kBackgroundPixelShader, "main", "ps_5_0", backgroundPS);
    if (FAILED(hr)) return hr;
    hr = CompileShader(kCardVertexShader, "main", "vs_5_0", cardVS);
    if (FAILED(hr)) return hr;
    hr = CompileShader(kCardPixelShader, "main", "ps_5_0", cardPS);
    if (FAILED(hr)) return hr;

    hr = m_device->CreateVertexShader(backgroundVS->GetBufferPointer(), backgroundVS->GetBufferSize(), nullptr, &m_backgroundVertexShader);
    if (FAILED(hr)) return hr;
    hr = m_device->CreatePixelShader(backgroundPS->GetBufferPointer(), backgroundPS->GetBufferSize(), nullptr, &m_backgroundPixelShader);
    if (FAILED(hr)) return hr;
    hr = m_device->CreateVertexShader(cardVS->GetBufferPointer(), cardVS->GetBufferSize(), nullptr, &m_cardVertexShader);
    if (FAILED(hr)) return hr;
    hr = m_device->CreatePixelShader(cardPS->GetBufferPointer(), cardPS->GetBufferSize(), nullptr, &m_cardPixelShader);
    if (FAILED(hr)) return hr;

    static constexpr D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = m_device->CreateInputLayout(inputLayoutDesc, static_cast<UINT>(std::size(inputLayoutDesc)),
        cardVS->GetBufferPointer(), cardVS->GetBufferSize(), &m_inputLayout);
    if (FAILED(hr)) return hr;

    static constexpr Vertex quadVertices[] = {
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, {{0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    };
    static constexpr std::uint16_t quadIndices[] = {0, 1, 2, 0, 2, 3};

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(quadVertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = quadVertices;
    hr = m_device->CreateBuffer(&vbDesc, &vbData, &m_vertexBuffer);
    if (FAILED(hr)) return hr;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(quadIndices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = quadIndices;
    hr = m_device->CreateBuffer(&ibDesc, &ibData, &m_indexBuffer);
    if (FAILED(hr)) return hr;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(FrameConstants);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_frameConstantsBuffer);
    if (FAILED(hr)) return hr;

    cbDesc.ByteWidth = sizeof(ObjectConstants);
    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_objectConstantsBuffer);
    if (FAILED(hr)) return hr;

    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.DepthClipEnable = TRUE;
    rsDesc.MultisampleEnable = TRUE;
    hr = m_device->CreateRasterizerState(&rsDesc, &m_rasterizerState);
    if (FAILED(hr)) return hr;

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    hr = m_device->CreateDepthStencilState(&dsDesc, &m_depthStencilState);
    if (FAILED(hr)) return hr;

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = m_device->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr)) return hr;

    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = m_device->CreateSamplerState(&sampDesc, &m_cardSampler);
    return hr;
}

HRESULT Flip3DRenderer::CreateWindowSizeResources(bool resizeBuffers)
{
    if (!m_swapChain) return E_FAIL;
    m_msaaRTV.Reset(); m_renderTargetView.Reset(); m_msaaRenderTarget.Reset();
    m_depthStencilTexture.Reset(); m_depthStencilView.Reset();
    m_context->OMSetRenderTargets(0, nullptr, nullptr);

    static constexpr UINT kSampleCount = 2;
    if (resizeBuffers)
    {
        HRESULT hr = m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) return hr;
    }

    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return hr;
    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
    if (FAILED(hr)) return hr;

    D3D11_TEXTURE2D_DESC msaaDesc = {};
    msaaDesc.Width = m_width; msaaDesc.Height = m_height;
    msaaDesc.MipLevels = 1; msaaDesc.ArraySize = 1;
    msaaDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    msaaDesc.SampleDesc.Count = kSampleCount;
    msaaDesc.Usage = D3D11_USAGE_DEFAULT;
    msaaDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
    hr = m_device->CreateTexture2D(&msaaDesc, nullptr, &m_msaaRenderTarget);
    if (FAILED(hr)) return hr;

    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
    hr = m_device->CreateRenderTargetView(m_msaaRenderTarget.Get(), &rtvDesc, &m_msaaRTV);
    if (FAILED(hr)) return hr;

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = m_width; depthDesc.Height = m_height;
    depthDesc.MipLevels = 1; depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = kSampleCount;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthStencilTexture);
    if (FAILED(hr)) return hr;
    hr = m_device->CreateDepthStencilView(m_depthStencilTexture.Get(), nullptr, &m_depthStencilView);
    if (FAILED(hr)) return hr;

    m_viewport = {0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f};
    return S_OK;
}

// ============================================================================
// Per-frame update
// ============================================================================
void Flip3DRenderer::Update(float deltaSeconds)
{
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) deltaSeconds *= 0.05f;
    m_totalTime += deltaSeconds;

    if (m_enterTimeline.active) m_enterTimeline.Update(deltaSeconds);
    if (m_rotateTimeline.active)
    {
        m_rotateTimeline.Update(deltaSeconds);
        OnGlobalTimeUpdated();
    }

    if (m_state == ViewState::Enter && !m_enterTimeline.active)
        m_state = ViewState::Interactive;

    if (!m_rotateTimeline.active)
    {
        m_showOutgoingDuringRotation = false;
        TickRepeatedRotate();
    }

    if (m_state == ViewState::Exit && !m_enterTimeline.active)
    {
        if (m_selectedWindowWasMinimized && m_selectedHWND && IsWindow(m_selectedHWND))
        {            
            SetLayeredWindowAttributes(m_selectedHWND, 0, 255, LWA_ALPHA);
            //SendMessage(m_selectedHWND, WM_SETREDRAW, FALSE, 0);
            //SetWindowLongPtrW(m_selectedHWND, GWL_EXSTYLE, GetWindowLongPtrW(m_selectedHWND, GWL_EXSTYLE) & ~WS_EX_LAYERED);
            
            SetForegroundWindow(m_selectedHWND);
            SetActiveWindow(m_selectedHWND);

            m_selectedWindowActivationDispatched = false; 
        }

        if (m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
        CompleteDeferredSelectedWindowActivation(m_selectedHWND, m_selectedWindowActivationDispatched);
        return;
    }

    if (m_state == ViewState::ExitRepeatedRotate
        && !m_enterTimeline.active && !m_rotateTimeline.active && m_rotationTargetIndex == -1)
    {
        if (m_selectedWindowWasMinimized && m_selectedHWND && IsWindow(m_selectedHWND))
        {
            SetLayeredWindowAttributes(m_selectedHWND, 0, 255, LWA_ALPHA);
            //SendMessage(m_selectedHWND, WM_SETREDRAW, FALSE, 0);
            //SetWindowLongPtrW(m_selectedHWND, GWL_EXSTYLE, GetWindowLongPtrW(m_selectedHWND, GWL_EXSTYLE) & ~WS_EX_LAYERED);

            //SendMessage(m_selectedHWND, WM_SETREDRAW, TRUE, 0);
            //RedrawWindow(m_selectedHWND, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
            
            SetForegroundWindow(m_selectedHWND);
            SetActiveWindow(m_selectedHWND);
            
            m_selectedWindowActivationDispatched = false;
        }

        if (m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
        CompleteDeferredSelectedWindowActivation(m_selectedHWND, m_selectedWindowActivationDispatched);
        return;
    }

    ContinueMouseWheelIfNeeded();
    ContinueKeyboardRepeatIfNeeded();
}

void Flip3DRenderer::OnGlobalTimeUpdated()
{
    if (m_showOutgoingDuringRotation && m_rotateTimeline.active
        && m_rotateTimeline.RawValue() > 0.5f)
        m_showOutgoingDuringRotation = false;
}

void Flip3DRenderer::TickRepeatedRotate()
{
    if (m_rotateTimeline.active || m_cards.empty()) return;

    if (m_state == ViewState::ExitRepeatedRotate)
    {
        if (!m_cards.empty() && m_cards.front().hwnd != m_selectedHWND)
        {
            // Old mapping: rate < 0 → passes -WHEEL_DELTA → rotationStep=+1 → front→back.
            constexpr bool backward = false;
            RotateListPhysically(backward);
            StartRotationStep(backward, RotationDurationForRotateList());
        }
        else
        {
            m_rotationTargetIndex = -1;
            m_rRepeatedRotateRate = 0.0f;
        }
        return;
    }

    if (m_state != ViewState::InteractiveRepeatedRotate) return;

    HWND frontHwnd = m_cards.empty() ? nullptr : m_cards.front().hwnd;
    HWND targetHwnd = nullptr;
    if (m_rotationTargetIndex >= 0)
    {
        size_t pos = 0;
        for (auto &card : m_cards)
        {
            if (static_cast<int>(pos) == m_rotationTargetIndex) { targetHwnd = card.hwnd; break; }
            ++pos;
        }
    }

    if (frontHwnd != nullptr && frontHwnd == targetHwnd)
    {
        m_rotationTargetIndex = -1;
        m_rRepeatedRotateRate = 0.0f;
        m_state = ViewState::Interactive;
        return;
    }

    // Old mapping: rate > 0 → +WHEEL_DELTA → back→front;  rate < 0 → -WHEEL_DELTA → front→back
    const bool backward = (m_rRepeatedRotateRate > 0.0f);
    RotateListPhysically(backward);
    StartRotationStep(backward, RotationDurationForRotateList());
}

// ============================================================================
// View state / rotation helpers
// ============================================================================
int Flip3DRenderer::FrontCardIndex() const
{
    if (m_cards.empty()) return -1;
    return 0;
}

int Flip3DRenderer::ResolveOriginalFrontIndex() const
{
    if (m_originalFrontHWND == nullptr) return FrontCardIndex();
    size_t pos = 0;
    for (auto &card : m_cards)
    {
        if (card.hwnd == m_originalFrontHWND) return static_cast<int>(pos);
        ++pos;
    }
    return FrontCardIndex();
}

int Flip3DRenderer::DistanceBetween(size_t sourcePos, size_t targetPos, bool forward) const
{
    const size_t count = m_cards.size();
    if (count <= 1) return 0;
    size_t dist = 0;
    size_t cur = sourcePos;
    while (cur != targetPos)
    {
        // Old semantics: forward=true → decrement → backward through list
        if (forward)
            cur = (cur + count - 1) % count;
        else
            cur = (cur + 1) % count;
        ++dist;
    }
    return static_cast<int>(dist);
}

void Flip3DRenderer::RotateListPhysically(bool backward)
{
    if (m_cards.size() <= 1) return;
    if (backward)
    {
        auto last = std::prev(m_cards.end());
        m_cards.splice(m_cards.begin(), m_cards, last);
    }
    else
    {
        m_cards.splice(m_cards.end(), m_cards, m_cards.begin());
    }
}

void Flip3DRenderer::StartRotationStep(bool backward, float duration)
{
    m_rotateBackward = backward;
    m_showOutgoingDuringRotation = true;
    m_rotateTimeline.Restart(0.0f, 1.0f, duration, InterpolationMode::Linear);
}

bool Flip3DRenderer::IsInteractiveKeyboardState() const
{
    return m_state == ViewState::Enter || m_state == ViewState::Interactive
        || m_state == ViewState::InteractiveRepeatedRotate;
}

bool Flip3DRenderer::IsSelectionKeyboardState() const
{
    return m_state == ViewState::Interactive || m_state == ViewState::InteractiveRepeatedRotate;
}

bool Flip3DRenderer::IsSelectionInputState() const
{
    return IsSelectionKeyboardState();
}

float Flip3DRenderer::RotationDurationForRotateList() const
{
    if (m_state == ViewState::InteractiveRepeatedRotate || m_state == ViewState::ExitRepeatedRotate)
        return std::max(std::abs(m_rRepeatedRotateRate), 0.005f);
    return gRotateListDurationSec;
}

bool Flip3DRenderer::ShouldReverseHorizontalWheel() const
{
    return m_hwnd != nullptr && (GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;
}

void Flip3DRenderer::ReplayEnterAnimation()
{
    BuildCardModels();
    CreateWindowCaptures();
    m_bufferedRotateDelta = 0;
    m_cMouseWheelLeftOver = 0;
    m_rotationTargetIndex = -1;
    m_rRepeatedRotateRate = 0.0f;
    m_vkLastRepeatingKey = 0;
    m_mousePressedCardIndex = -1;
    m_pendingWinKeyAction = false;
    m_showOutgoingDuringRotation = false;
    m_selectedHWND = nullptr;
    m_selectedWindowWasMinimized = false;
    m_selectedWindowWasShellDesktop = false;
    m_selectedWindowActivationDispatched = false;
    m_originalFrontHWND = m_cards.empty() ? nullptr : m_cards.front().hwnd;

    const float current = EnterProgress();
    m_state = ViewState::Enter;
    m_enterTimeline.Restart(current, 1.0f, std::max(0.1f, gEnterExitDurationSec * (1.0f - current)),
        InterpolationMode::Cubic);
}

void Flip3DRenderer::BeginExitView()
{
    if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate) return;
    if (m_minimized)
    {
        CompleteDeferredSelectedWindowActivation(m_selectedHWND, m_selectedWindowActivationDispatched);
        DestroyWindow(m_hwnd);
        return;
    }

    m_state = ViewState::Exit;
    m_bufferedRotateDelta = 0;
    m_cMouseWheelLeftOver = 0;
    m_rotationTargetIndex = -1;
    m_rRepeatedRotateRate = 0.0f;
    m_vkLastRepeatingKey = 0;
    m_mousePressedCardIndex = -1;
    m_pendingWinKeyAction = false;
    m_showOutgoingDuringRotation = false;
    m_enterTimeline.Restart(1.0f, 0.0f, gEnterExitDurationSec, InterpolationMode::Cubic);
}

void Flip3DRenderer::BeginExitAnimation() { BeginExitView(); }

// ============================================================================
// Selection
// ============================================================================
void Flip3DRenderer::SelectThumbnail(HWND targetHwnd)
{
    if (m_cards.empty() || !targetHwnd) return;

    // Find the target card
    CardModel *selectedCard = nullptr;
    for (auto &card : m_cards)
    {
        if (card.hwnd == targetHwnd) { selectedCard = &card; break; }
    }
    if (!selectedCard) return;

    // Refresh minimised window rect
    if (selectedCard->isMinimized && selectedCard->hwnd)
    {
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(monitorInfo);
        const HMONITOR monitorWnd = MonitorFromWindow(selectedCard->hwnd, MONITOR_DEFAULTTONEAREST);
        const HMONITOR monitor = monitorWnd ? monitorWnd : MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
        if (monitor && GetMonitorInfoW(monitor, &monitorInfo))
        {
            const float monW = static_cast<float>(std::max(1L, monitorInfo.rcWork.right - monitorInfo.rcWork.left));
            const float monH = static_cast<float>(std::max(1L, monitorInfo.rcWork.bottom - monitorInfo.rcWork.top));

            RECT restored = {};
            WINDOWPLACEMENT wp = {};
            wp.length = sizeof(wp);
            if (GetWindowPlacement(selectedCard->hwnd, &wp) && !IsRectEmpty(&wp.rcNormalPosition))
            {
                restored = wp.rcNormalPosition;
                OffsetRect(&restored, monitorInfo.rcWork.left, monitorInfo.rcWork.top);
                if (wp.flags & WPF_RESTORETOMAXIMIZED) restored = monitorInfo.rcWork;
            }

            RECT test = {};
            if ((IsRectEmpty(&restored) || !IntersectRect(&test, &restored, &monitorInfo.rcWork))
                && selectedCard->hasOriginalRect)
            {
                RECT ghost = {};
                ghost.left = static_cast<LONG>((selectedCard->originalWorldPosition.x + 0.5f) * monW + monitorInfo.rcWork.left);
                ghost.top = static_cast<LONG>((0.5f - selectedCard->originalWorldPosition.y) * monH + monitorInfo.rcWork.top);
                ghost.right = ghost.left + static_cast<LONG>(std::abs(selectedCard->originalWorldSize.x) * monW);
                ghost.bottom = ghost.top + static_cast<LONG>(std::abs(selectedCard->originalWorldSize.y) * monH);
                restored = ghost;
            }

            RECT clipped = {};
            if (IntersectRect(&clipped, &restored, &monitorInfo.rcWork))
            {
                const float rw = static_cast<float>(std::max(1L, clipped.right - clipped.left));
                const float rh = static_cast<float>(std::max(1L, clipped.bottom - clipped.top));
                float nw = rw, nh = rh;
                if (nw > monW || nh > monH) { const float s = std::min(monW / nw, monH / nh); nw *= s; nh *= s; }
                selectedCard->targetWorldSize = { nw / monW, -(nh / monH) };
                selectedCard->originalWorldPosition = {
                    (static_cast<float>(clipped.left - monitorInfo.rcWork.left) / monW) - 0.5f,
                    0.5f - (static_cast<float>(clipped.top - monitorInfo.rcWork.top) / monH) };
                selectedCard->originalWorldSize = { rw / monW, -(rh / monH) };
                selectedCard->sourceOccupancy = std::max(std::abs(selectedCard->targetWorldSize.x), std::abs(selectedCard->targetWorldSize.y));
                selectedCard->aspectRatio = rw / std::max(rh, 1.0f);
            }
        }
    }

    BeginExitView();
    if (m_state != ViewState::Exit) return;

    m_selectedHWND = selectedCard->hwnd;
    m_selectedWindowWasMinimized = selectedCard->isMinimized;
    m_selectedWindowWasShellDesktop = selectedCard->hwnd == GetShellWindow();

    if (m_selectedWindowWasMinimized && m_selectedHWND)
    {
        ShowWindow(m_selectedHWND, SW_SHOWNOACTIVATE);
        
        PostMessage(m_selectedHWND, WM_ACTIVATE, WA_ACTIVE, 0);
        PostMessage(m_selectedHWND, WM_ACTIVATE, WA_INACTIVE, 0);
    
        SetForegroundWindow(m_selectedHWND);
        m_selectedWindowActivationDispatched = true;
    }
    else
    {
        m_selectedWindowActivationDispatched = DispatchImmediateSelectedWindowActivation(
            m_selectedHWND, m_selectedWindowWasMinimized, m_selectedWindowWasShellDesktop);
    }

    size_t targetPos = 0;
    for (auto &card : m_cards) { if (card.hwnd == targetHwnd) break; ++targetPos; }
    HWND frontHwnd = m_cards.front().hwnd;

    if (frontHwnd != targetHwnd)
    {
        const int dist = DistanceBetween(0, targetPos, true);
        if (dist > 0)
        {
            m_rotationTargetIndex = -1; 
            m_rRepeatedRotateRate = 0.0f;

            m_state = ViewState::Exit; 
            
            TickRepeatedRotate(); 
        }
    }
}

void Flip3DRenderer::SelectFrontThumbnail()
{
    if (!m_cards.empty()) SelectThumbnail(m_cards.front().hwnd);
}

// ============================================================================
// Rotation input
// ============================================================================
void Flip3DRenderer::RotateListByMouseWheelAmount(int mouseWheelAmount)
{
    m_cMouseWheelLeftOver += mouseWheelAmount;
    if (m_rotateTimeline.active || m_cards.size() <= 1 || m_state == ViewState::Exit) return;

    const int wheelSteps = m_cMouseWheelLeftOver / WHEEL_DELTA;
    const int wheelDirection = (wheelSteps > 0) ? 1 : ((wheelSteps < 0) ? -1 : 0);
    if (wheelDirection == 0) return;

    m_cMouseWheelLeftOver -= wheelDirection * WHEEL_DELTA;
    // Old mapping: rotationStep = -wheelDirection → positive wheel → backward
    const bool backward = (wheelDirection > 0);
    RotateListPhysically(backward);
    StartRotationStep(backward, RotationDurationForRotateList());
}

void Flip3DRenderer::ContinueMouseWheelIfNeeded()
{
    if (m_state == ViewState::InteractiveRepeatedRotate || m_state == ViewState::ExitRepeatedRotate) return;
    RotateListByMouseWheelAmount(0);
}

void Flip3DRenderer::ContinueKeyboardRepeatIfNeeded()
{
    if (m_rotateTimeline.active) return;
    if (m_bufferedRotateDelta != 0 && m_cMouseWheelLeftOver == 0)
    {
        const int d = m_bufferedRotateDelta; m_bufferedRotateDelta = 0;
        RotateBy(d);
        return;
    }
    if (m_vkLastRepeatingKey == 0 || m_cMouseWheelLeftOver != 0) return;
    ProcessKeyboardInput(true, m_vkLastRepeatingKey, false);
}

void Flip3DRenderer::RotateBy(int delta)
{
    if (delta == 0 || m_cards.empty() || m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate) return;
    if (m_state == ViewState::InteractiveRepeatedRotate)
    {
        m_state = ViewState::Interactive;
        m_rotationTargetIndex = -1;
        m_rRepeatedRotateRate = 0.0f;
    }
    const int rawWheel = (delta > 0) ? -WHEEL_DELTA : WHEEL_DELTA;
    RotateListByMouseWheelAmount(rawWheel);
}

void Flip3DRenderer::RotateTo(int /*targetIndex*/) { BeginRotateToThumbnail(nullptr); }

void Flip3DRenderer::BeginRotateToThumbnail(HWND targetHwnd)
{
    const size_t count = m_cards.size();
    if (count <= 1 || m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate) return;

    size_t targetPos = 0;
    for (auto &card : m_cards) { if (card.hwnd == targetHwnd) break; ++targetPos; }

    if (targetPos == 0)
    {
        m_rotationTargetIndex = -1;
        m_rRepeatedRotateRate = 0.0f;
        m_state = ViewState::Interactive;
        return;
    }

    const int fwd = DistanceBetween(0, targetPos, true);   // true→decrement→backward distance
    const int bwd = DistanceBetween(0, targetPos, false);  // false→increment→forward distance
    int dist = fwd;
    int dir = 1;
    if (bwd <= fwd) { dist = bwd; dir = -1; }

    m_bufferedRotateDelta = 0;
    m_rotationTargetIndex = static_cast<int>(targetPos);
    m_rRepeatedRotateRate = static_cast<float>(dir)
        * std::min(gRotateListDurationSec, gRotateToHomeMaxDurationSec / static_cast<float>(std::max(dist, 1)));
    m_state = ViewState::InteractiveRepeatedRotate;
    TickRepeatedRotate();
}

// ============================================================================
// Hit testing
// ============================================================================
bool Flip3DRenderer::IntersectRayTriangle(
    const XMFLOAT3 &origin, const XMFLOAT3 &dir,
    const XMFLOAT3 &v0, const XMFLOAT3 &v1, const XMFLOAT3 &v2,
    float &t, float &u) const
{
    const float EPSILON = 0.0000001f;
    XMVECTOR e0 = XMLoadFloat3(&v1) - XMLoadFloat3(&v0);
    XMVECTOR e1 = XMLoadFloat3(&v2) - XMLoadFloat3(&v0);
    XMVECTOR pvec = XMVector3Cross(XMLoadFloat3(&dir), e1);
    float det = XMVectorGetX(XMVector3Dot(e0, pvec));
    if (det > -EPSILON && det < EPSILON) return false;
    float invDet = 1.0f / det;
    XMVECTOR tvec = XMLoadFloat3(&origin) - XMLoadFloat3(&v0);
    u = XMVectorGetX(XMVector3Dot(tvec, pvec)) * invDet;
    XMVECTOR qvec = XMVector3Cross(tvec, e0);
    float v = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&dir), qvec)) * invDet;
    t = XMVectorGetX(XMVector3Dot(e1, qvec)) * invDet;
    return u >= 0.0f && v >= 0.0f && (u + v) <= 1.0f && t > EPSILON;
}

int Flip3DRenderer::HitTest3DScene(LONG x, LONG y) const
{
    if (!IsSelectionInputState() || m_cards.empty()) return -1;
    if (m_monitorWidth <= 0 || m_monitorHeight <= 0) return -1;

    const float monitorW = static_cast<float>(m_monitorWidth);
    const float monitorH = static_cast<float>(m_monitorHeight);
    float ndcX = static_cast<float>(x);
    float ndcY = static_cast<float>(y);
    if (m_fRTLMirror) ndcX = monitorW - ndcX;
    ndcX = ndcX / monitorW - 0.5f;
    ndcY = -(ndcY / monitorH - 0.5f);

    const float enterProgress = EnterProgress();
    const float nearPlaneExtent = gNearPlaneEdgeSize;

    XMFLOAT3 rayOrigin = {0.0f, 0.0f, 0.0f};
    XMVECTOR nearPoint = XMVectorSet(ndcX * nearPlaneExtent, ndcY * nearPlaneExtent, -1.0f, 0.0f);
    XMVECTOR originWS = XMVector3TransformCoord(XMLoadFloat3(&rayOrigin), m_matHitTestInverse);
    XMVECTOR nearWS = XMVector3TransformCoord(nearPoint, m_matHitTestInverse);
    XMVECTOR rayDir = XMVector3Normalize(nearWS - originWS);
    XMFLOAT3 origin = {}, dir = {};
    XMStoreFloat3(&origin, originWS);
    XMStoreFloat3(&dir, rayDir);

    const DrawBuildContext context = CreateDrawBuildContext();
    if (context.countInt <= 0) return -1;

    const std::vector<VisibleCardStructure> structure = BuildVisibleCardStructure(context);
    for (auto it = structure.rbegin(); it != structure.rend(); ++it)
    {
        const auto &entry = *it;
        size_t pos = 0;
        const CardModel *cardPtr = nullptr;
        for (auto &card : m_cards) { if (pos == entry.cardPosition) { cardPtr = &card; break; } ++pos; }
        if (!cardPtr) continue;

        const CardAnimationState animState = ResolveCardAnimationState(entry, context);
        const CardWorldState worldState = GetWorldFromParametric(context, *cardPtr, entry.cardPosition, animState, enterProgress);
        const XMMATRIX world = XMLoadFloat4x4(&worldState.world);

        XMFLOAT3 c;
        c = {0.0f, 0.0f, 0.0f}; XMVECTOR p0 = XMVector3TransformCoord(XMLoadFloat3(&c), world);
        c = {1.0f, 0.0f, 0.0f}; XMVECTOR p1 = XMVector3TransformCoord(XMLoadFloat3(&c), world);
        c = {0.0f, 1.0f, 0.0f}; XMVECTOR p2 = XMVector3TransformCoord(XMLoadFloat3(&c), world);
        c = {1.0f, 1.0f, 0.0f}; XMVECTOR p3 = XMVector3TransformCoord(XMLoadFloat3(&c), world);

        XMFLOAT3 v[4];
        XMStoreFloat3(&v[0], p0); XMStoreFloat3(&v[1], p1);
        XMStoreFloat3(&v[2], p2); XMStoreFloat3(&v[3], p3);

        float t0 = 0, u0 = 0;
        if (IntersectRayTriangle(origin, dir, v[0], v[1], v[2], t0, u0)) return static_cast<int>(entry.cardPosition);
        float t1 = 0, u1 = 0;
        if (IntersectRayTriangle(origin, dir, v[0], v[1], v[3], t1, u1)) return static_cast<int>(entry.cardPosition);
    }
    return -1;
}

bool Flip3DRenderer::SelectThumbnailAtPoint(LONG x, LONG y)
{
    const int pos = HitTest3DScene(x, y);
    if (pos < 0) return false;
    size_t i = 0;
    for (auto &card : m_cards) { if (i == static_cast<size_t>(pos)) { SelectThumbnail(card.hwnd); return true; } ++i; }
    return false;
}

// ============================================================================
// Mouse / keyboard input
// ============================================================================
bool Flip3DRenderer::ProcessMouseWheelInput(int mouseWheelAmount, bool horizontalWheel)
{
    if (!IsInteractiveKeyboardState()) return false;
    int effective = mouseWheelAmount;
    if (horizontalWheel && ShouldReverseHorizontalWheel()) effective = -effective;
    if (std::abs((effective + m_cMouseWheelLeftOver) / WHEEL_DELTA) >= 5) return true;
    RotateListByMouseWheelAmount(effective);
    return true;
}

bool Flip3DRenderer::ProcessMouseButtonInput(LONG x, LONG y, bool pressed)
{
    if (!IsSelectionInputState()) return false;
    if (pressed)
    {
        m_mousePressedCardIndex = HitTest3DScene(x, y);
        return true;
    }
    const int pressedIdx = m_mousePressedCardIndex;
    m_mousePressedCardIndex = -1;
    const int releasedIdx = HitTest3DScene(x, y);
    if (pressedIdx >= 0 && pressedIdx == releasedIdx)
    {
        size_t i = 0;
        for (auto &card : m_cards) { if (i == static_cast<size_t>(pressedIdx)) { SelectThumbnail(card.hwnd); break; } ++i; }
    }
    else
    {
        BeginExitView();
    }
    return true;
}

bool Flip3DRenderer::ProcessMouseInput(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_MOUSEWHEEL:  return ProcessMouseWheelInput(GET_WHEEL_DELTA_WPARAM(wParam), false);
    case WM_MOUSEHWHEEL: return ProcessMouseWheelInput(GET_WHEEL_DELTA_WPARAM(wParam), true);
    case WM_LBUTTONDOWN:
        return ProcessMouseButtonInput(static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
            static_cast<LONG>(static_cast<short>(HIWORD(lParam))), true);
    case WM_LBUTTONUP:
        return ProcessMouseButtonInput(static_cast<LONG>(static_cast<short>(LOWORD(lParam))),
            static_cast<LONG>(static_cast<short>(HIWORD(lParam))), false);
    }
    return false;
}

bool Flip3DRenderer::ProcessKeyboardInput(bool isKeyDown, UINT vkCode, bool isRepeat)
{
    if (isKeyDown)
    {
        const bool isWinKey = vkCode == VK_LWIN || vkCode == VK_RWIN;
        if (!isWinKey) m_pendingWinKeyAction = false;
        const bool shouldExit = vkCode == VK_ESCAPE;
        bool allowAction = IsInteractiveKeyboardState();
        bool handled = shouldExit;

        if (isRepeat)
        {
            m_vkLastRepeatingKey = vkCode;
            if (m_rotateTimeline.active || m_cMouseWheelLeftOver != 0) allowAction = false;
        }

        if (allowAction)
        {
            switch (vkCode)
            {
            case VK_TAB:
                RotateBy(((GetKeyState(VK_SHIFT) & 0xFF80) == 0) ? 1 : -1); handled = true; break;
            case VK_UP:    RotateBy(1); handled = true; break;
            case VK_DOWN:  RotateBy(-1); handled = true; break;
            case VK_LEFT:  RotateBy(m_fRTLMirror ? 1 : -1); handled = true; break;
            case VK_RIGHT: RotateBy(m_fRTLMirror ? -1 : 1); handled = true; break;
            case VK_HOME:
                if (IsSelectionKeyboardState()) BeginRotateToThumbnail(m_originalFrontHWND);
                handled = true; break;
            case VK_RETURN:
                if (IsSelectionKeyboardState()) SelectFrontThumbnail();
                handled = true; break;
            case VK_LWIN: case VK_RWIN:
                m_pendingWinKeyAction = true; handled = true; break;
            }
        }
        if (shouldExit) { BeginExitView(); handled = true; }
        return handled;
    }

    if (m_vkLastRepeatingKey == vkCode) m_vkLastRepeatingKey = 0;
    if (vkCode == VK_LWIN || vkCode == VK_RWIN)
    {
        if (!m_pendingWinKeyAction) return true;
        const bool anyWinDown = ((GetKeyState(VK_LWIN) & 0xFF80) != 0) || ((GetKeyState(VK_RWIN) & 0xFF80) != 0);
        m_pendingWinKeyAction = false;
        if (anyWinDown) BeginExitView(); else SelectFrontThumbnail();
        return true;
    }
    return false;
}

// ============================================================================
// Drawing / Rendering pipeline
// ============================================================================
float Flip3DRenderer::EnterProgress() const
{
    return std::clamp(m_enterTimeline.Value(), 0.0f, 1.0f);
}

XMMATRIX Flip3DRenderer::BuildViewMatrix(float enterProgress) const
{
    const XMVECTOR eyeV = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    const XMVECTOR atV  = XMVectorZero();
    const XMVECTOR upV  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const float pitch = gCameraFinalRotate.x * enterProgress;
    const float yaw   = gCameraFinalRotate.y * enterProgress;
    const float roll  = gCameraFinalRotate.z * enterProgress;

    XMMATRIX view = XMMatrixTranslation(0.0f, 0.0f, 1.0f);
    view = view * XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
    view = view * XMMatrixTranslation(0.0f, 0.0f, -1.0f);
    view = view * XMMatrixTranslation(
        -(gCameraFinalTranslate.x * enterProgress),
        -(gCameraFinalTranslate.y * enterProgress),
        -(gCameraFinalTranslate.z * enterProgress));
    view = view * XMMatrixLookAtRH(eyeV, atV, upV);
    return view;
}

XMMATRIX Flip3DRenderer::BuildProjectionMatrix(float enterProgress) const
{
    const float nearPlaneExtent = (enterProgress * gNearPlaneEdgeSize) + (1.0f - enterProgress);
    return XMMatrixPerspectiveRH(nearPlaneExtent, nearPlaneExtent, kNearPlaneDistance, 50.0f);
}

DrawBuildContext Flip3DRenderer::CreateDrawBuildContext() const
{
    DrawBuildContext context = {};
    context.countInt = static_cast<int>(m_cards.size());
    context.count = static_cast<float>(context.countInt);
    if (context.countInt <= 0) return context;

    context.front = 0.0f;
    context.visibleCount = std::min<int>(gMaxVisibleCards, context.countInt);
    const int pathWindowCount = std::min(std::max(context.countInt, 5), gMaxVisibleCards);
    context.maxWindows = static_cast<float>(pathWindowCount);
    context.monitorAspectRatio = static_cast<float>(m_width) / static_cast<float>(std::max<UINT>(m_height, 1));
    context.hasHiddenQualifiedWindows = static_cast<int>(m_cards.size()) > context.visibleCount;
    context.isRotating = m_rotateTimeline.active;
    context.rotationSteps = context.isRotating ? (m_rotateBackward ? -1 : 1) : 0;
    context.rotationProgress = context.isRotating ? m_rotateTimeline.RawValue() : 1.0f;
    context.hiddenWindowFlipOutProgress = std::clamp(context.rotationProgress * 2.0f, 0.0f, 1.0f);
    context.hiddenWindowFlipInProgress = std::clamp((context.rotationProgress - 0.5f) * 2.0f, 0.0f, 1.0f);

    context.startFrontIndex = 0;
    context.endFrontIndex = 0;
    context.useOriginalHiddenWindowRotation = (context.rotationSteps != 0) && context.hasHiddenQualifiedWindows;

    if (context.rotationSteps != 0)
    {
        if (context.hasHiddenQualifiedWindows)
        {
            if (m_rotateBackward)
            {
                context.rotatingOutCardIndex = context.visibleCount;
                context.rotatingInCardIndex = 0;
                context.windowBeingFlippedCardIndex = context.rotatingInCardIndex;
            }
            else
            {
                context.rotatingOutCardIndex = context.countInt - 1;
                context.rotatingInCardIndex = context.visibleCount - 1;
                context.windowBeingFlippedCardIndex = context.rotatingOutCardIndex;
            }
        }
        else
        {
            const int cycleCardIndex = m_rotateBackward ? 0 : (context.countInt - 1);
            context.rotatingInCardIndex = cycleCardIndex;
            context.rotatingOutCardIndex = cycleCardIndex;
            context.windowBeingFlippedCardIndex = cycleCardIndex;
        }
    }

    return context;
}

float Flip3DRenderer::SteadyOpacityForRelative(const DrawBuildContext &context, float relative) const
{
    if (!context.hasHiddenQualifiedWindows) return 1.0f;
    if (relative < 0.0f) return 1.0f;
    if (relative >= static_cast<float>(context.visibleCount)) return 0.5f;
    return (relative >= static_cast<float>(context.visibleCount - 1)) ? 0.5f : 1.0f;
}

bool Flip3DRenderer::IsVisibleInView(
    size_t position, bool isVisibleInFinalView,
    size_t rotatingInPos, size_t rotatingOutPos) const
{
    if (isVisibleInFinalView)
    {
        if (position == rotatingInPos)
        {
            if (!m_showOutgoingDuringRotation) return true;
        }
        else if (position != rotatingOutPos)
        {
            return true;
        }
    }
    else
    {
        if (position == rotatingInPos)
        {
            if (!m_showOutgoingDuringRotation) return true;
        }
    }
    if (position == rotatingOutPos && m_showOutgoingDuringRotation) return true;
    return false;
}

std::vector<VisibleCardStructure> Flip3DRenderer::BuildVisibleCardStructure(const DrawBuildContext &context) const
{
    std::vector<VisibleCardStructure> result;
    result.reserve(m_cards.size());

    VisibleCardStructure specialOutgoingEntry = {};
    bool hasSpecialOutgoingEntry = false;
    const bool reorderOutgoingLikeOriginal = m_showOutgoingDuringRotation
        && context.windowBeingFlippedCardIndex >= 0
        && context.windowBeingFlippedCardIndex == context.rotatingOutCardIndex;

    const bool rotating = (context.rotationSteps != 0);
    const int n = context.countInt;
    const int visCount = context.visibleCount;
    size_t position = 0;
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it, ++position)
    {
        const int endSlot = static_cast<int>(position);
        int startSlot = endSlot;
        if (rotating)
        {
            if (context.rotationSteps < 0)
                startSlot = (endSlot + n - 1) % n;
            else
                startSlot = (endSlot + 1) % n;
        }
        const bool isVis = endSlot < visCount;
        const bool wasVis = startSlot < visCount;

        if (!rotating)
        {
            if (!isVis) continue;
            VisibleCardStructure entry = {};
            entry.cardPosition = position;
            entry.startSlot = startSlot;
            entry.endSlot = endSlot;
            result.push_back(entry);
            continue;
        }
        const bool isOut = wasVis && !isVis;
        const bool isBndIn = context.useOriginalHiddenWindowRotation && !wasVis && isVis;
        const bool isIn = !context.useOriginalHiddenWindowRotation && !wasVis && isVis;
        const bool isCyc = !context.hasHiddenQualifiedWindows
            && ((context.rotationSteps < 0 && position == 0)
                || (context.rotationSteps > 0 && position == static_cast<size_t>(n - 1)));
        if (!IsVisibleInView(position, isVis,
                static_cast<size_t>(context.rotatingInCardIndex),
                static_cast<size_t>(context.rotatingOutCardIndex)))
            continue;

        VisibleCardStructure entry = {};
        entry.cardPosition = position;
        entry.startSlot = startSlot;
        entry.endSlot = endSlot;
        entry.isCycleCard = isCyc;
        entry.isBoundaryIncomingCard = isBndIn;
        entry.isIncomingCard = isIn;
        entry.isOutgoingCard = isOut;
        if (reorderOutgoingLikeOriginal
            && position == static_cast<size_t>(context.windowBeingFlippedCardIndex))
        {
            specialOutgoingEntry = entry;
            hasSpecialOutgoingEntry = true;
            continue;
        }

        result.push_back(entry);
    }

    if (result.empty() && !hasSpecialOutgoingEntry) return result;
    std::reverse(result.begin(), result.end());

    if (hasSpecialOutgoingEntry)
    {
        // uDWM UpdateStructure gives the first-half special card explicit ordering.
        if (context.rotationSteps > 0)
            result.push_back(specialOutgoingEntry);
        else
            result.insert(result.begin(), specialOutgoingEntry);
    }

    return result;
}

CardAnimationState Flip3DRenderer::CreateBaseCardAnimationState(
    size_t position, const DrawBuildContext & /*context*/) const
{
    CardAnimationState state = {};
    state.logicalRelative = static_cast<float>(position);
    state.pathRelative = state.logicalRelative;
    state.startRelativeForOpacity = state.logicalRelative;
    state.endRelativeForOpacity = state.logicalRelative;
    return state;
}

CardAnimationState Flip3DRenderer::ResolveOutgoingCardAnimationState(
    const DrawBuildContext &context, CardAnimationState state) const
{
    const float startRelative = (context.rotationSteps > 0) ? 0.0f : static_cast<float>(context.visibleCount - 1);
    const float endRelative = (context.rotationSteps > 0) ? -1.0f : static_cast<float>(context.visibleCount);
    state.logicalRelative = Lerp(startRelative, endRelative, context.rotationProgress);
    state.pathRelative = state.logicalRelative;

    if (context.useOriginalHiddenWindowRotation)
    {
        state.useDirectSteadyOpacity = true;
        state.directSteadyOpacity = (context.rotationSteps > 0) ? 1.0f : 0.5f;
        state.startRelativeForOpacity = state.logicalRelative;
        state.endRelativeForOpacity = state.logicalRelative;
        return state;
    }
    state.startRelativeForOpacity = startRelative;
    state.endRelativeForOpacity = endRelative;
    return state;
}

CardAnimationState Flip3DRenderer::ResolveIncomingCardAnimationState(
    const VisibleCardStructure &entry, const DrawBuildContext &context,
    CardAnimationState state) const
{
    const float startRelative = (context.rotationSteps > 0) ? static_cast<float>(context.visibleCount) : -1.0f;
    const float endRelative = static_cast<float>(entry.endSlot);
    state.logicalRelative = Lerp(startRelative, endRelative, context.rotationProgress);
    state.pathRelative = state.logicalRelative;

    if (context.useOriginalHiddenWindowRotation)
    {
        state.useDirectSteadyOpacity = true;
        state.directSteadyOpacity = SteadyOpacityForRelative(context, endRelative);
        state.startRelativeForOpacity = state.logicalRelative;
        state.endRelativeForOpacity = state.logicalRelative;
        return state;
    }
    state.startRelativeForOpacity = startRelative;
    state.endRelativeForOpacity = endRelative;
    return state;
}

CardAnimationState Flip3DRenderer::ResolveSteadyCardAnimationState(
    const VisibleCardStructure &entry, const DrawBuildContext &context,
    CardAnimationState state) const
{
    float startRelative = static_cast<float>(entry.startSlot);
    const float endRelative = static_cast<float>(entry.endSlot);

    if (context.useOriginalHiddenWindowRotation)
    {
        startRelative = endRelative + ((context.rotationSteps > 0) ? 1.0f : -1.0f);
        state.logicalRelative = Lerp(startRelative, endRelative, context.rotationProgress);
        state.pathRelative = state.logicalRelative;
        state.startRelativeForOpacity = startRelative;
        state.endRelativeForOpacity = endRelative;
        state.useDirectSteadyOpacity = true;

        if (context.rotationSteps > 0)
        {
            if (entry.endSlot >= (context.visibleCount - 1))
                state.directSteadyOpacity = 0.5f;
            else if (entry.endSlot == (context.visibleCount - 2))
                state.directSteadyOpacity = Lerp(0.5f, 1.0f, context.rotationProgress);
            else
                state.directSteadyOpacity = 1.0f;
        }
        else if (entry.endSlot == (context.visibleCount - 1))
            state.directSteadyOpacity = Lerp(1.0f, 0.5f, context.rotationProgress);
        else
            state.directSteadyOpacity = 1.0f;
        return state;
    }

    if (context.rotationSteps < 0 && entry.startSlot > entry.endSlot)
        startRelative -= context.count;

    state.logicalRelative = Lerp(startRelative, endRelative, context.rotationProgress);
    state.pathRelative = state.logicalRelative;
    state.startRelativeForOpacity = startRelative;
    state.endRelativeForOpacity = endRelative;
    return state;
}

CardAnimationState Flip3DRenderer::ResolveCycleCardAnimationState(
    const DrawBuildContext &context, CardAnimationState state) const
{
    if (context.rotationSteps > 0)
    {
        if (context.rotationProgress < 0.5f)
        {
            const float phase = context.rotationProgress * 2.0f;
            state.logicalRelative = Lerp(0.0f, -1.0f, phase);
        }
        else
        {
            const float phase = (context.rotationProgress - 0.5f) * 2.0f;
            state.logicalRelative = Lerp(static_cast<float>(context.visibleCount),
                static_cast<float>(context.visibleCount - 1), phase);
        }
    }
    else
    {
        if (context.rotationProgress < 0.5f)
        {
            const float phase = context.rotationProgress * 2.0f;
            state.logicalRelative = Lerp(static_cast<float>(context.visibleCount - 1),
                static_cast<float>(context.visibleCount), phase);
        }
        else
        {
            const float phase = (context.rotationProgress - 0.5f) * 2.0f;
            state.logicalRelative = Lerp(-1.0f, 0.0f, phase);
        }
    }

    state.pathRelative = state.logicalRelative;
    state.startRelativeForOpacity = state.logicalRelative;
    state.endRelativeForOpacity = state.logicalRelative;
    state.useDirectSteadyOpacity = true;
    state.directSteadyOpacity = 1.0f;
    return state;
}

CardAnimationState Flip3DRenderer::ResolveCardAnimationState(
    const VisibleCardStructure &entry, const DrawBuildContext &context) const
{
    CardAnimationState state = CreateBaseCardAnimationState(entry.cardPosition, context);
    if (context.rotationSteps == 0) return state;

    if (entry.isCycleCard)
        state = ResolveCycleCardAnimationState(context, state);
    else if (entry.isOutgoingCard)
        state = ResolveOutgoingCardAnimationState(context, state);
    else if (entry.isBoundaryIncomingCard || entry.isIncomingCard)
        state = ResolveIncomingCardAnimationState(entry, context, state);
    else
        state = ResolveSteadyCardAnimationState(entry, context, state);

    // uDWM: zIndex lerps by ±1: v12 = (zIndex + adj)*(1-p) + zIndex*p.
    // For the hidden-window outgoing card, the regular outgoing logical path
    // already matches the live direction; applying a count-sized override here
    // incorrectly throws the front fade-out card onto the tail path.
    if (entry.isCycleCard) return state;
    const float p = context.rotationProgress;
    const float fpos = static_cast<float>(entry.cardPosition);
    const bool bw = (context.rotationSteps < 0);
    if (!entry.isOutgoingCard || !context.useOriginalHiddenWindowRotation)
        state.pathRelative = bw ? Lerp(fpos - 1.0f, fpos, p) : Lerp(fpos + 1.0f, fpos, p);
    return state;
}

CardWorldState Flip3DRenderer::GetWorldFromParametric(
    const DrawBuildContext &context, const CardModel &card, size_t position,
    const CardAnimationState &animationState, float enterProgress) const
{
    const float zIndex = -animationState.pathRelative;
    // uDWM passes t directly — negative for extreme zIndex, extrapolates past P0
    float t = 1.0f - ((zIndex - 0.5f) / -context.maxWindows);
    XMFLOAT3 curvePosition = EvaluateBezier(t);
    curvePosition.x = (curvePosition.x * kProxyPathScale) + kProxySceneOffset.x;
    curvePosition.y = (curvePosition.y * kProxyPathScale) + kProxySceneOffset.y;

    const XMFLOAT2 originalSize = card.hasOriginalRect
        ? card.originalWorldSize
        : BuildWorldSizeFromOccupancy(card.sourceOccupancy, card.aspectRatio, context.monitorAspectRatio);
    const XMFLOAT2 targetSize = card.hasOriginalRect ? card.targetWorldSize : originalSize;
    const XMFLOAT2 proxyOriginalSize = {
        std::abs(originalSize.x) * kProxyCardScale,
        -std::abs(originalSize.y) * kProxyCardScale,
    };
    const XMFLOAT2 proxyTargetSize = {
        std::abs(targetSize.x) * kProxyCardScale,
        -std::abs(targetSize.y) * kProxyCardScale,
    };
    const float normalizationScale = EvaluateNormalizationScale(
        std::max(std::abs(targetSize.x), std::abs(targetSize.y)));
    const XMFLOAT2 finalSize = {
        proxyTargetSize.x * normalizationScale,
        proxyTargetSize.y * normalizationScale,
    };
    const XMFLOAT2 currentSize = Lerp(proxyOriginalSize, finalSize, enterProgress);
    const XMFLOAT3 flatPosition = card.hasOriginalRect
        ? XMFLOAT3{card.originalWorldPosition.x, card.originalWorldPosition.y, 0.0f}
        : BuildDesktopWindowPosition(position, m_cards.size(), proxyOriginalSize);
    const XMFLOAT3 finalPosition = {
        curvePosition.x,
        curvePosition.y - finalSize.y,
        curvePosition.z,
    };
    const XMFLOAT3 anchoredPosition = Lerp(
        XMFLOAT3{flatPosition.x, flatPosition.y, zIndex / 10000.0f},
        finalPosition, enterProgress);

    XMMATRIX world = XMMatrixScaling(currentSize.x, currentSize.y, 1.0f);
    world = world * XMMatrixTranslation(anchoredPosition.x, anchoredPosition.y, anchoredPosition.z);

    if (m_fRTLMirror)
        world = XMMatrixTranslation(1.0f, 0.0f, 0.0f) * XMMatrixScaling(-1.0f, 1.0f, 1.0f) * world;

    CardWorldState worldState = {};
    XMStoreFloat4x4(&worldState.world, world);
    worldState.depth = anchoredPosition.z;
    return worldState;
}

float Flip3DRenderer::ResolveDrawItemOpacity(
    const VisibleCardStructure &entry, const DrawBuildContext &context,
    const CardAnimationState &animationState) const
{
    if (animationState.useDirectSteadyOpacity) return animationState.directSteadyOpacity;

    if (context.rotationSteps != 0)
    {
        const float startSteadyOpacity = SteadyOpacityForRelative(context, animationState.startRelativeForOpacity);
        const float endSteadyOpacity = SteadyOpacityForRelative(context, animationState.endRelativeForOpacity);
        if (entry.isOutgoingCard) return startSteadyOpacity;
        if (entry.isIncomingCard) return endSteadyOpacity;
        return Lerp(startSteadyOpacity, endSteadyOpacity, context.rotationProgress);
    }

    if (context.hasHiddenQualifiedWindows && entry.endSlot == (context.visibleCount - 1))
        return 0.5f;
    return 1.0f;
}

float Flip3DRenderer::ApplyEnterProgressToOpacity(float windowOpacity, float enterProgress) const
{
    if (std::abs(enterProgress - 1.0f) >= 0.0000012f && std::abs(windowOpacity - 1.0f) >= 0.0000012f)
        return (enterProgress * windowOpacity) + (1.0f - enterProgress);
    return windowOpacity;
}

float Flip3DRenderer::ResolveDrawItemStateOpacity(const CardModel &card, float enterProgress) const
{
    if (card.isMinimized && card.hwnd != m_selectedHWND
        && std::abs(enterProgress - 1.0f) >= 0.0000012f)
        return enterProgress * enterProgress;
    if (card.hwnd == GetShellWindow()) return enterProgress;
    return 1.0f;
}

float Flip3DRenderer::ResolveOutgoingTransitionOpacity(const DrawBuildContext &context) const
{
    if (context.useOriginalHiddenWindowRotation) return 1.0f - context.hiddenWindowFlipOutProgress;
    return 1.0f - (2.0f * std::min(context.rotationProgress, 0.5f));
}

float Flip3DRenderer::ResolveIncomingTransitionOpacity(const DrawBuildContext &context) const
{
    if (context.useOriginalHiddenWindowRotation) return context.hiddenWindowFlipInProgress;
    return std::clamp((context.rotationProgress - 0.5f) * 2.0f, 0.0f, 1.0f);
}

float Flip3DRenderer::ResolveCycleTransitionOpacity(const DrawBuildContext &context) const
{
    if (context.rotationProgress < 0.5f) { const float phase = context.rotationProgress * 2.0f; return 1.0f - phase; }
    const float phase = (context.rotationProgress - 0.5f) * 2.0f; return phase;
}

float Flip3DRenderer::ResolveDrawItemTransitionOpacity(
    const VisibleCardStructure &entry, const DrawBuildContext &context) const
{
    if (context.rotationSteps == 0) return 1.0f;
    if (entry.isCycleCard) return ResolveCycleTransitionOpacity(context);
    if (entry.isOutgoingCard) return ResolveOutgoingTransitionOpacity(context);
    if (entry.isBoundaryIncomingCard || entry.isIncomingCard) return ResolveIncomingTransitionOpacity(context);
    return 1.0f;
}

float Flip3DRenderer::UpdateDrawItemAlpha(
    const VisibleCardStructure &entry, const DrawBuildContext &context,
    const CardModel &card, const CardAnimationState &animationState,
    float enterProgress) const
{
    const float transitionOpacity = ResolveDrawItemTransitionOpacity(entry, context);
    const float windowOpacity = ApplyEnterProgressToOpacity(
        ResolveDrawItemOpacity(entry, context, animationState), enterProgress);
    const float stateOpacity = ResolveDrawItemStateOpacity(card, enterProgress);
    return transitionOpacity * windowOpacity * stateOpacity;
}

bool Flip3DRenderer::TryBuildDrawItemForStructure(
    const VisibleCardStructure &entry, const DrawBuildContext &context,
    float enterProgress, DrawItem &item) const
{
    size_t pos = 0;
    const CardModel *cardPtr = nullptr;
    for (auto &card : m_cards) { if (pos == entry.cardPosition) { cardPtr = &card; break; } ++pos; }
    if (!cardPtr) return false;
    
    const CardAnimationState animationState = ResolveCardAnimationState(entry, context);
    const CardWorldState worldState = GetWorldFromParametric(context, *cardPtr, entry.cardPosition, animationState, enterProgress);
    
    item.world = worldState.world;
    item.color = cardPtr->color;
    item.color.w = UpdateDrawItemAlpha(entry, context, *cardPtr, animationState, enterProgress);
    if (item.color.w <= 0.001f) return false;
    
    item.accent = XMFLOAT4{0.16f, 0.90f, 0.92f, 0.98f};
    item.accent.x *= std::clamp((item.color.w - 0.08f) / 0.72f, 0.0f, 1.0f);
    item.flags = XMFLOAT4{ cardPtr->isMinimized ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
    item.depth = worldState.depth;
    item.cardPosition = static_cast<int>(entry.cardPosition);
    return true;
}

std::vector<DrawItem> Flip3DRenderer::BuildDrawItems(float enterProgress) const
{
    const DrawBuildContext context = CreateDrawBuildContext();
    std::vector<DrawItem> items;
    items.reserve(m_cards.size());
    if (context.countInt <= 0) return items;

    const std::vector<VisibleCardStructure> structure = BuildVisibleCardStructure(context);
    for (const VisibleCardStructure &entry : structure)
    {
        DrawItem item = {};
        if (TryBuildDrawItemForStructure(entry, context, enterProgress, item))
            items.push_back(item);
    }
    return items;
}

void Flip3DRenderer::Render()
{
    if (!m_swapChain || !m_renderTargetView || !m_depthStencilView) return;

    const float enterProgress = EnterProgress();
    const XMMATRIX view = BuildViewMatrix(enterProgress);
    const XMMATRIX projection = BuildProjectionMatrix(enterProgress);
    const XMMATRIX viewProj = view * projection;

    XMVECTOR det = XMMatrixDeterminant(view);
    m_matHitTestInverse = XMMatrixInverse(&det, view);
    if (XMVectorGetX(det) < 0.000001f) m_matHitTestInverse = XMMatrixIdentity();
    m_monitorWidth = static_cast<int>(m_width);
    m_monitorHeight = static_cast<int>(m_height);

    FrameConstants frameConstants = {};
    XMStoreFloat4x4(&frameConstants.viewProj, viewProj);
    frameConstants.washParams = XMFLOAT4(enterProgress * 0.0f, m_totalTime, static_cast<float>(m_cards.size()), 0.85f); //0.5f, 0.85f 
    frameConstants.viewport = XMFLOAT4(static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, enterProgress);
    m_context->UpdateSubresource(m_frameConstantsBuffer.Get(), 0, nullptr, &frameConstants, 0, 0);

    static constexpr float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    m_context->OMSetRenderTargets(1, m_msaaRTV.GetAddressOf(), m_depthStencilView.Get());
    m_context->RSSetViewports(1, &m_viewport);
    m_context->RSSetState(m_rasterizerState.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFFu);
    m_context->ClearRenderTargetView(m_msaaRTV.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    ID3D11Buffer *frameBuffers[] = {m_frameConstantsBuffer.Get()};
    m_context->IASetInputLayout(nullptr);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_backgroundVertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_backgroundPixelShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, frameBuffers);
    m_context->PSSetConstantBuffers(0, 1, frameBuffers);
    m_context->Draw(3, 0);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    ID3D11Buffer *vertexBuffers[] = {m_vertexBuffer.Get()};
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    m_context->VSSetShader(m_cardVertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_cardPixelShader.Get(), nullptr, 0);
    m_context->OMSetBlendState(m_blendState.Get(), nullptr, 0xFFFFFFFFu);
    m_context->PSSetSamplers(0, 1, m_cardSampler.GetAddressOf());

    const auto drawItems = BuildDrawItems(enterProgress);

    for (const auto &item : drawItems)
    {
        size_t pos = 0;
        for (auto &card : m_cards) { if (pos == static_cast<size_t>(item.cardPosition) && card.capture) { card.capture->PollFrame(); break; } ++pos; }
    }

    for (const DrawItem &item : drawItems)
    {
        ObjectConstants objectConstants = {};
        objectConstants.world = item.world;
        objectConstants.color = item.color;
        objectConstants.accent = item.accent;
        objectConstants.flags  = item.flags;
        m_context->UpdateSubresource(m_objectConstantsBuffer.Get(), 0, nullptr, &objectConstants, 0, 0);

        size_t pos = 0;
        ID3D11ShaderResourceView *srv = nullptr;
        for (auto &card : m_cards) { if (pos == static_cast<size_t>(item.cardPosition)) { srv = card.captureSRV; break; } ++pos; }
        if (!srv) continue;

        m_context->PSSetShaderResources(0, 1, &srv);
        ID3D11Buffer *objectBuffers[] = {m_objectConstantsBuffer.Get()};
        m_context->VSSetConstantBuffers(1, 1, objectBuffers);
        m_context->PSSetConstantBuffers(1, 1, objectBuffers);
        m_context->DrawIndexed(6, 0, 0);
    }

    ID3D11ShaderResourceView *nullSRV[1] = {nullptr};
    m_context->PSSetShaderResources(0, 1, nullSRV);
    m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFFu);

    {
        ComPtr<ID3D11Texture2D> backBuffer;
        if (SUCCEEDED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
            m_context->ResolveSubresource(backBuffer.Get(), 0, m_msaaRenderTarget.Get(), 0, DXGI_FORMAT_B8G8R8A8_UNORM);
    }

    m_swapChain->Present(1, 0);
}

// ============================================================================
// Window message handling
// ============================================================================

LRESULT Flip3DRenderer::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
    {
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate) 
            {
                DestroyWindow(m_hwnd);
            }
            else 
            {
                BeginExitView();
            }
        }
        return 0;
    }
    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED) { m_minimized = true; return 0; }
        m_minimized = false;
        m_width = std::max<UINT>(1, LOWORD(lParam));
        m_height = std::max<UINT>(1, HIWORD(lParam));
        if (m_swapChain) CreateWindowSizeResources(true);
        return 0;
    }
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        if (ProcessMouseInput(message, wParam, lParam)) return 0;
        break;
    case WM_KEYDOWN:
        if (wParam == VK_SPACE) { if ((lParam & 0x40000000) == 0) ReplayEnterAnimation(); return 0; }
        if (ProcessKeyboardInput(true, static_cast<UINT>(wParam), (lParam & 0x40000000) != 0)) return 0;
        break;
    case WM_KEYUP:
        if (ProcessKeyboardInput(false, static_cast<UINT>(wParam), false)) return 0;
        break;
    case WM_CLOSE:
        if (m_state == ViewState::Exit || m_state == ViewState::ExitRepeatedRotate) DestroyWindow(m_hwnd);
        else BeginExitView();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(m_hwnd, message, wParam, lParam);
}
