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
        // --- ABSOLUT SICHERER DESKTOP-LOG ---
        wchar_t desktopPath[MAX_PATH];
        // Holt den Pfad zum Desktop des aktuellen Nutzers
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOP, nullptr, 0, desktopPath))) 
        {
            wchar_t fullLogPath[MAX_PATH];
            swprintf_s(fullLogPath, L"%s\\flip3d_debug.txt", desktopPath);
            
            // Datei öffnen und Werte reinschreiben
            FILE* f = nullptr;
            if (_wfopen_s(&f, fullLogPath, L"w, cccs=UTF-8") == 0 && f != nullptr) 
            {
                fwprintf(f, L"=== FLIP3D Z-ORDER & SIZE DEBUG ===\n");
                fwprintf(f, L"Getter X: %d\n", overlay.GetX());
                fwprintf(f, L"Getter Y: %d\n", overlay.GetY());
                fwprintf(f, L"Getter Breite: %d\n", overlay.GetWidth());
                fwprintf(f, L"Getter Höhe: %d\n", overlay.GetHeight());
                fwprintf(f, L"==================================\n");
                fclose(f);
            }
        }
        // ------------------------------------

        // 1. Größe erzwingen
        SetWindowPos(overlayHwnd, nullptr, 
            overlay.GetX(), 
            overlay.GetY(), 
            overlay.GetWidth(), 
            overlay.GetHeight(), 
            SWP_NOZORDER | SWP_NOACTIVATE);

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
