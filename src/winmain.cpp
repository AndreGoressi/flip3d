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
        // --- NUR TEXT LOG: Komplett ohne Getter-Zugriffe! ---
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        
        wchar_t* lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) *(lastSlash + 1) = L'\0';
        
        wchar_t fullLogPath[MAX_PATH];
        swprintf_s(fullLogPath, L"%sflip3d_debug.txt", exePath);
        
        FILE* f = nullptr;
        if (_wfopen_s(&f, fullLogPath, L"w, cccs=UTF-8") == 0 && f != nullptr) 
        {
            fwprintf(f, L"=== PROGRAMM ERREICHT DIE MAIN ===\n");
            fwprintf(f, L"Wenn du das lesen kannst, lebt die Main noch!\n");
            fwprintf(f, L"==================================\n");
            fclose(f);
        }
        // ----------------------------------------------------

        // Zum Testen nehmen wir hier temporär wieder feste Standardwerte,
        // damit das Programm wegen der Getter auf keinen Fall hier crasht:
        SetWindowPos(overlayHwnd, nullptr, 0, 0, 1920, 1080, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(renderHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
