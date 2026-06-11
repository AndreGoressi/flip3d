#include <windows.h>
#include "ShellOverlayContext.h"
#include "Flip3DRenderer.h"

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
        // --- DIAGNOSE-BOX: Zeigt uns, was die Klasse wirklich liefert ---
        wchar_t debugBuf[256];
        swprintf_s(debugBuf, L"Getter Werte:\nX: %d, Y: %d\nBreite: %d, Höhe: %d", 
            overlay.GetX(), overlay.GetY(), overlay.GetWidth(), overlay.GetHeight());
        MessageBoxW(nullptr, debugBuf, L"Win32 Debug", MB_OK | MB_ICONINFORMATION);
        // -----------------------------------------------------------------

        // 1. Größe erzwingen mit den Gettern
        SetWindowPos(overlayHwnd, nullptr, 
            overlay.GetX(), 
            overlay.GetY(), 
            overlay.GetWidth(), 
            overlay.GetHeight(), 
            SWP_NOZORDER | SWP_NOACTIVATE);

        // 2. Stack nach oben
        SetWindowPos(renderHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        // 3. Overlay dahinter
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
