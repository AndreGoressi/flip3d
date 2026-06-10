#include "ShellOverlayContext.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// HSHELL_RUDEAPPACTIVATED: Shell-eigene / "rude" Fenster (u.a. Startmenue)
// werden mit gesetztem HIGHBIT gemeldet, NICHT als HSHELL_WINDOWACTIVATED!
#ifndef HSHELL_RUDEAPPACTIVATED
#define HSHELL_RUDEAPPACTIVATED (HSHELL_WINDOWACTIVATED | HSHELL_HIGHBIT)
#endif

// --- Undokumentierte SetWindowCompositionAttribute-API ---
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

ShellOverlayContext::ShellOverlayContext()
    : m_instance(nullptr), m_hwnd(nullptr), m_shellHookMsg(0),
      m_x(0), m_y(0), m_screenW(0), m_screenH(0)
{
}

ShellOverlayContext::~ShellOverlayContext()
{
    Cleanup();
}

bool ShellOverlayContext::Initialize(HINSTANCE instance)
{
    m_instance = instance;

    // Arbeitsbereich = Bildschirm OHNE Taskleiste -> Taskleiste bleibt frei.
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    m_x       = wa.left;
    m_y       = wa.top;
    m_screenW = wa.right  - wa.left;
    m_screenH = wa.bottom - wa.top;

    WNDCLASSEXW wc   = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc   = ShellOverlayContext::OverlayWndProc;
    wc.hInstance     = instance;
    wc.lpszClassName = L"ShellOverlayClass";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc)) return false;

    // WS_EX_TRANSPARENT = klick-durchlaessig. KEINE leere Region!
    m_hwnd = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        nullptr,
        WS_POPUP,
        m_x, m_y, m_screenW, m_screenH,
        nullptr, nullptr, instance, this
    );
    if (!m_hwnd) return false;

    if (!ApplyAcrylic())
    {
        OutputDebugStringW(L"[Overlay] SetWindowCompositionAttribute nicht verfuegbar.\n");
        return false;
    }

    // Dokumentierte COM-API zur Startmenue-Sichtbarkeit (Win8+).
    // Faengt auch den Fall ab, dass das Startmenue BEREITS offen ist,
    // wenn das Overlay startet -> dann kommt naemlich nie ein ShellHook-Event.
    if (FAILED(CoCreateInstance(CLSID_AppVisibility, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&m_appVisibility))))
    {
        m_appVisibility = nullptr;  // Fallback im Timer greift trotzdem
        OutputDebugStringW(L"[Overlay] IAppVisibility nicht verfuegbar, nutze Fallback.\n");
    }

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hwnd);

    RegisterShellHookWindow(m_hwnd);
    m_shellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");

    // Leichtgewichtiger Poll-Timer: 100 ms reichen voellig und sind unmessbar billig.
    SetTimer(m_hwnd, TIMER_STARTMENU, 100, nullptr);

    return true;
}

bool ShellOverlayContext::IsStartMenuOpen() const
{
    // Primaer: die offizielle Shell-Antwort.
    if (m_appVisibility) {
        BOOL visible = FALSE;
        if (SUCCEEDED(m_appVisibility->IsLauncherVisible(&visible)))
            return visible != FALSE;
    }

    // Fallback: Startmenue/Suche laufen als UWP-CoreWindow im Vordergrund.
    HWND fg = GetForegroundWindow();
    if (fg) {
        wchar_t cls[64] = {};
        GetClassNameW(fg, cls, 64);
        if (wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0)
            return true;
    }
    return false;
}

bool ShellOverlayContext::ApplyAcrylic()
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return false;

    auto SetWCA = reinterpret_cast<SetWindowCompositionAttribute_t>(
        GetProcAddress(user32, "SetWindowCompositionAttribute"));
    if (!SetWCA) return false;

    ACCENT_POLICY accent = {};
    accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    accent.AccentFlags = 0;
    // 0xAABBGGRR : A=0x73 (~45%), B=25 (0x19), G=15 (0x0F), R=15 (0x0F)
    accent.GradientColor = 0x73190F0F;

    WINDOWCOMPOSITIONATTRIBDATA data = {};
    data.Attrib = WCA_ACCENT_POLICY;
    data.pvData = &accent;
    data.cbData = sizeof(accent);

    return SetWCA(m_hwnd, &data) != FALSE;
}

LRESULT CALLBACK ShellOverlayContext::OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    ShellOverlayContext* ctx = reinterpret_cast<ShellOverlayContext*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    if (ctx) {
        // ShellHook: normale UND "rude" Aktivierungen (Startmenue!) abfangen.
        if (msg == ctx->m_shellHookMsg &&
            (wp == HSHELL_WINDOWACTIVATED || wp == HSHELL_RUDEAPPACTIVATED)) {
            PostQuitMessage(0);
            return 0;
        }
        // Timer: deckt den Fall ab, dass das Startmenue schon offen war
        // oder ohne Aktivierungsereignis geoeffnet wird.
        if (msg == WM_TIMER && wp == TIMER_STARTMENU) {
            if (ctx->IsStartMenuOpen()) {
                PostQuitMessage(0);
            }
            return 0;
        }
        if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        if (msg == WM_ERASEBKGND) return 1;  // niemals weiss uebermalen
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);             // leer lassen -> Accent uebernimmt
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
        KillTimer(m_hwnd, TIMER_STARTMENU);
        DeregisterShellHookWindow(m_hwnd);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_appVisibility.Reset();  // ComPtr VOR CoUninitialize freigeben!
}
