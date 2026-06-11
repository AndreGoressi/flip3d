#include <windows.h>
#include "ShellOverlayContext.h"
#include "Flip3DRenderer.h"

UINT g_shellHookMsg = 0;

#pragma comment(lib, "shell32.lib")

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    RECT swa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &swa, 0);
    int workWidth  = swa.right  - swa.left;
    int workHeight = swa.bottom - swa.top;

    ShellOverlayContext overlay;
    if (!overlay.Initialize(instance, workWidth, workHeight))
    {
        OutputDebugStringW(L"[Main] Fatal error initializing the overlay.\n");
    }

    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Flip3DRenderer rnd;
    if (!rnd.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize.", kTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    HWND overlayHwnd = overlay.ShellHandle();
    HWND renderHwnd  = rnd.RenderHandle();

    if (renderHwnd && overlayHwnd) 
    {
        g_shellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");
        RegisterShellHookWindow(overlayHwnd);
        
        overlay.m_shellHookMsg = g_shellHookMsg; 

        SetWindowPos(renderHwnd, HWND_TOPMOST, 0, 0, 0, 0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        SetWindowPos(overlayHwnd, renderHwnd, 0, 0, 0, 0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    
    ShowWindow(renderHwnd, showCommand == SW_HIDE ? SW_MAXIMIZE : showCommand);
    UpdateWindow(renderHwnd);

    SetForegroundWindow(renderHwnd);
    SetActiveWindow(renderHwnd);

    int result = rnd.Run();
    CoUninitialize();
    return result;
}
