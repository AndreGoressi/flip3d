#include <Windows.h>
#include <atlbase.h>
#include <d3d11.h>
#include <dxgi1_5.h>
#include <vector>

// Einbinden der undokumentierten DWM-Interfaces aus Aurenex' Projekt
#include "undocumented.h"
#include "vtablehook.h"

// Einbinden deines originalen Mathe- und Shader-Kerns
#include "Shaders.h"
#include "Config.h"

// Funktionszeiger für die originalen DWM-Present-Funktionen
HRESULT(STDMETHODCALLTYPE* fn_PresentDWM)(IDXGISwapChainDWMLegacy* pSwapChain, UINT SyncInterval, UINT Flags) = nullptr;
HRESULT(STDMETHODCALLTYPE* fn_PresentMultiplaneOverlay)(IDXGISwapChainDWMLegacy* pSwapChain, UINT SyncInterval, UINT Flags, void* pUnk) = nullptr;

// Globale Grafikressourcen (werden aus dem DWM-Kontext übernommen)
CComPtr<ID3D11Device>        g_D3DDevice = nullptr;
CComPtr<ID3D11DeviceContext> g_D3DContext = nullptr;

// Ressourcen aus deinem originalen Prototypen (Exakt übernommen!)
CComPtr<ID3D11VertexShader>  m_cardVertexShader;
CComPtr<ID3D11PixelShader>   m_cardPixelShader;
CComPtr<ID3D11Buffer>        m_frameConstantsBuffer;
CComPtr<ID3D11Buffer>        m_objectConstantsBuffer;

bool g_Flip3DActive = false; // Wird durch Hotkey (Win+Tab) umgeschaltet

// ============================================================================
// Dein originaler Render-Kern, angepasst an die native DWM SwapChain
// ============================================================================
void RenderFlip3DNative(IDXGISwapChainDWMLegacy* pSwapChain)
{
    if (!g_Flip3DActive) return;

    // Beim ersten Frame initialisieren wir deine Shader direkt mit dem DWM-Device
    if (!g_D3DDevice)
    {
        pSwapChain->GetDevice(IID_PPV_ARGS(&g_D3DDevice));
        g_D3DDevice->GetImmediateContext(&g_D3DContext);

        // Kompiliere und erstelle deine originalen Card-Shader aus Shaders.h
        // (Analog zu deinem Setup in Flip3DPrototypeApp::CreateDeviceResources)
        g_D3DDevice->CreateVertexShader(kCardVertexShader, strlen(kCardVertexShader), nullptr, &m_cardVertexShader);
        g_D3DDevice->CreatePixelShader(kCardPixelShader, strlen(kCardPixelShader), nullptr, &m_cardPixelShader);
        
        // Konstantenpuffer für Matrizen erstellen
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.ByteWidth = sizeof(XMMATRIX); // ViewProj Matrix
        g_D3DDevice->CreateBuffer(&cbDesc, nullptr, &m_frameConstantsBuffer);
    }

    // --- AB HIER RUNTER LÄUFT DIE MAGIE ---
    // Wir holen uns den aktuellen Backbuffer des Desktops zum Rendern!
    CComPtr<ID3D11Texture2D> pBackBuffer = nullptr;
    if (SUCCEEDED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))))
    {
        CComPtr<ID3D11RenderTargetView> pRTV = nullptr;
        g_D3DDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRTV);
        
        // Bindest du deine RenderTargets an die DWM-Pipeline
        g_D3DContext->OMSetRenderTargets(1, &pRTV.p, nullptr);

        // Shaders und Pipeline setzen (Aus deiner Flip3DPrototypeApp::Render)
        g_D3DContext->VSSetShader(m_cardVertexShader, nullptr, 0);
        g_D3DContext->PSSetShader(m_cardPixelShader, nullptr, 0);

        // Hier loopen wir durch die Fensterliste und wenden deine 3D-Matrizen an!
        // Da wir uns INSIDE DWM befinden, können wir die Fenster-Texturen direkt greifen,
        // OHNE dass Windows uns mit schwarzen Boxen zensieren kann!
        
        /* Hier greift deine originale Schleife:
           for (const auto& card : m_cards) {
               // Berechne Weltmatrix (world)
               // g_D3DContext->UpdateSubresource(m_objectConstantsBuffer, ...);
               // g_D3DContext->DrawIndexed(...);
           }
        */
    }
}

// ============================================================================
// Die gehookten DWM Present-Funktionen (Aus vtablehook extrahiert)
// ============================================================================
HRESULT STDMETHODCALLTYPE hk_PresentDWM(IDXGISwapChainDWMLegacy* pSwapChain, UINT SyncInterval, UINT Flags)
{
    RenderFlip3DNative(pSwapChain); // Unser Flip3D direkt vor der Bildausgabe reinzeichnen!
    return fn_PresentDWM(pSwapChain, SyncInterval, Flags);
}

HRESULT STDMETHODCALLTYPE hk_PresentMultiplaneOverlay(IDXGISwapChainDWMLegacy* pSwapChain, UINT SyncInterval, UINT Flags, void* pUnk)
{
    RenderFlip3DNative(pSwapChain);
    return fn_PresentMultiplaneOverlay(pSwapChain, SyncInterval, Flags, pUnk);
}

// ============================================================================
// Tastatur-Hook, um Win+Tab direkt abzufangen
// ============================================================================
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            // Wenn WIN gedrückt gehalten wird und TAB getippt wird
            if (pKey->vkCode == VK_TAB && (GetAsyncKeyState(VK_LWIN) < 0 || GetAsyncKeyState(VK_RWIN) < 0))
            {
                g_Flip3DActive = !g_Flip3DActive; // Toggle Flip3D Zustand!
                return 1; // Blockiert das originale Windows 11 Task-View komplett!
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

// ============================================================================
// DLL Einstiegspunkt für den Injektor
// ============================================================================
DWORD WINAPI MainThread(LPVOID lpReserved)
{
    // Suche nach der geheimen DWM-Grafikfabrik
    const HMODULE hDxgi = GetModuleHandleW(L"dxgi.dll");
    if (!hDxgi) return EXIT_FAILURE;

    using CreateDXGIFactoryDWM_t = HRESULT(WINAPI*)(REFIID, void**);
    const auto fn_CreateDXGIFactoryDWM = reinterpret_cast<CreateDXGIFactoryDWM_t>(
        GetProcAddress(hDxgi, "CreateDXGIFactoryDWM")
    );

    if (!fn_CreateDXGIFactoryDWM) return EXIT_FAILURE;

    CComPtr<IDXGIFactoryDWM> pFactoryDWM = nullptr;
    if (FAILED(fn_CreateDXGIFactoryDWM(IID_PPV_ARGS(&pFactoryDWM)))) return EXIT_FAILURE;

    CComPtr<ID3D11Device> pDevice = nullptr;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &pDevice, nullptr, nullptr);

    DXGI_SWAP_CHAIN_DESC SwapChainDesc {};
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.BufferCount = 1;

    CComPtr<IDXGISwapChainDWMLegacy> pSwapChain = nullptr;
    pFactoryDWM->CreateSwapChain(pDevice, &SwapChainDesc, nullptr, &pSwapChain);

    // VTable-Hooks aktivieren (Nutzt deine vtablehook.cpp!)
    fn_PresentDWM = reinterpret_cast<decltype(fn_PresentDWM)>(
        vtable::hook(pSwapChain, &hk_PresentDWM, 16)
    );
    fn_PresentMultiplaneOverlay = reinterpret_cast<decltype(fn_PresentMultiplaneOverlay)>(
        vtable::hook(pSwapChain, &hk_PresentMultiplaneOverlay, 23)
    );

    // Aktiviert den globalen Tastatur-Hook im System
    SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc, nullptr, 0);

    MSG message {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return EXIT_SUCCESS;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, &MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}