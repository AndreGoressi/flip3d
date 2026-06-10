/*#include <windows.h>
#include "ShellOverlayContext.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

    {
        ShellOverlayContext overlay;

        if (overlay.Initialize(instance))
        {
            overlay.RunMessageLoop();
        }
        else
        {
            OutputDebugStringW(L"[Main] Schwerwiegender Fehler beim Initialisieren des Overlays.\n");
        }

        overlay.Cleanup();
    }

    CoUninitialize();
    return 0;
}*/

#include <windows.h>
#include "ShellOverlayContext.h"

#define HOTKEY_WIN_TAB 1

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

    if (!RegisterHotKey(nullptr, HOTKEY_WIN_TAB, MOD_WIN | MOD_NOREPEAT, VK_TAB))
    {
        OutputDebugStringW(L"[Main] Hotkey Win+Tab konnte nicht registriert werden.\n");
        CoUninitialize();
        return -1;
    }

    // Äußere Loop: Programm läuft dauerhaft, wartet immer wieder auf Win+Tab
    while (true)
    {
        // Auf Win+Tab warten
        MSG msg = {};
        bool hotKeyReceived = false;
        while (GetMessageW(&msg, nullptr, 0, 0))
        {
            if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_WIN_TAB)
            {
                hotKeyReceived = true;
                break;
            }
            // WM_QUIT beendet alles sauber
            if (msg.message == WM_QUIT)
            {
                UnregisterHotKey(nullptr, HOTKEY_WIN_TAB);
                CoUninitialize();
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!hotKeyReceived)
            break; // GetMessage Fehler (-1), raus

        // Overlay ausführen
        ShellOverlayContext overlay;
        if (overlay.Initialize(instance))
        {
            overlay.RunMessageLoop();
        }
        else
        {
            OutputDebugStringW(L"[Main] Schwerwiegender Fehler beim Initialisieren des Overlays.\n");
        }
        overlay.Cleanup();

        // Hier angekommen: Overlay wurde geschlossen → wieder von vorne warten
    }

    UnregisterHotKey(nullptr, HOTKEY_WIN_TAB);
    CoUninitialize();
    return 0;
}
