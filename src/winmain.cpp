#include <windows.h>
#include "ShellOverlayContext.h"
#include "Flip3DRenderer.h"

#include <shlobj.h>
#include <cstdio>

// Das zwingt den Visual Studio Compiler (MSBuild), die richtige Windows-Bibliothek zu linken
#pragma comment(lib, "shell32.lib")

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    ShellOverlayContext overlay;
    if (!overlay.Initialize(instance))
        OutputDebugStringW(L"[Main] Fatal error initializing the overlay.\n");

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
        // 1. Größe erzwingen
        SetWindowPos(overlayHwnd, nullptr, 
            overlay.GetX(), 
            overlay.GetY(), 
            overlay.GetWidth(), 
            overlay.GetHeight(), 
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

        // 2. Stack nach ganz oben
        SetWindowPos(renderHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        // 3. Overlay dahinter kette
        SetWindowPos(overlayHwnd, renderHwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    ShowWindow(renderHwnd, showCommand == SW_HIDE ? SW_MAXIMIZE : showCommand);
    UpdateWindow(renderHwnd);

    SetForegroundWindow(renderHwnd);
    SetActiveWindow(renderHwnd);

    int result = rnd.Run();
    CoUninitialize();
    return result;
}
