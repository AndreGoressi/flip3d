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

// Hotkey-ID
#define HOTKEY_WIN_TAB 1

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

    // Win+Tab Hotkey registrieren (MOD_WIN | MOD_NOREPEAT + VK_TAB)
    if (!RegisterHotKey(nullptr, HOTKEY_WIN_TAB, MOD_WIN | MOD_NOREPEAT, VK_TAB))
    {
        OutputDebugStringW(L"[Main] Hotkey Win+Tab konnte nicht registriert werden.\n");
        CoUninitialize();
        return -1;
    }

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
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(nullptr, HOTKEY_WIN_TAB);

    // Overlay nur starten wenn Hotkey empfangen wurde
    if (hotKeyReceived)
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
}
