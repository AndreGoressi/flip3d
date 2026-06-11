#include "ShellOverlayContext.h"

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

    RECT swa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &swa, 0);
    m_x       = swa.left;
    m_y       = swa.top;
    m_screenW = swa.right  - swa.left;
    m_screenH = swa.bottom - swa.top;

    WNDCLASSEXW shc   = { sizeof(WNDCLASSEXW) };
    shc.lpfnWndProc   = ShellOverlayContext::OverlayWndProc;
    shc.hInstance     = instance;
    shc.lpszClassName = L"ShellOverlayClass";
    shc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&shc)) return false;

    m_hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        shc.lpszClassName,
        nullptr,
        WS_POPUP | WS_VISIBLE,
        m_x, m_y, m_screenW, m_screenH,
        nullptr, nullptr, instance, this
    );

    
    if (!m_hwnd) return false;

    if (!ApplyAcrylic())
    {
        OutputDebugStringW(L"[Overlay] SetWindowCompositionAttribute nicht verfuegbar.\n");
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hwnd);

    RegisterShellHookWindow(m_hwnd);
    m_shellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");

    return true;
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
        if (msg == ctx->m_shellHookMsg && wp == HSHELL_WINDOWACTIVATED) {
            PostQuitMessage(0);   
            return 0;
        }
        /*if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }*/
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
