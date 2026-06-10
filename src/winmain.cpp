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
HHOOK g_hKeyboardHook = nullptr;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (kb->vkCode == VK_TAB)
        {
            bool winDown = (GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000;
            if (winDown) return 1;
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

// Fensterprozedur für das unsichtbare Nachrichtenfenster
HWND g_hWnd = nullptr;
HINSTANCE g_hInstance = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_HOTKEY && wParam == HOTKEY_WIN_TAB)
    {
        // Overlay starten
        ShellOverlayContext overlay;
        if (overlay.Initialize(g_hInstance))
            overlay.RunMessageLoop();
        else
            OutputDebugStringW(L"[Main] Fehler beim Initialisieren.\n");
        overlay.Cleanup();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    g_hInstance = instance;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    // Fensterklasse registrieren
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = instance;
    wc.lpszClassName = L"OverlayHotkeyWindow";
    RegisterClassExW(&wc);

    // Unsichtbares Message-Only Fenster erstellen
    g_hWnd = CreateWindowExW(
        0, L"OverlayHotkeyWindow", L"",
        0, 0, 0, 0, 0,
        HWND_MESSAGE,  // Message-Only, kein echtes Fenster
        nullptr, instance, nullptr
    );

    if (!g_hWnd)
    {
        OutputDebugStringW(L"[Main] Fenster konnte nicht erstellt werden.\n");
        CoUninitialize();
        return -1;
    }

    // Hotkey ans Fenster binden
    RegisterHotKey(g_hWnd, HOTKEY_WIN_TAB, MOD_WIN | MOD_NOREPEAT, VK_TAB);

    // Task View optional blockieren
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);

    // Message Loop
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
    UnregisterHotKey(g_hWnd, HOTKEY_WIN_TAB);
    DestroyWindow(g_hWnd);
    CoUninitialize();
    return 0;
}
}
