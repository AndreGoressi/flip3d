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

// Low-Level Keyboard Hook: blockiert Win+Tab systemweit für explorer.exe
HHOOK g_hKeyboardHook = nullptr;

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool winDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                       (GetAsyncKeyState(VK_RWIN) & 0x8000);

        if (winDown && kb->vkCode == VK_TAB)
        {
            // Win+Tab schlucken → explorer.exe bekommt es nicht
            return 1;
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    // Win+Tab für unser Programm registrieren
    if (!RegisterHotKey(nullptr, HOTKEY_WIN_TAB, MOD_WIN | MOD_NOREPEAT, VK_TAB))
    {
        OutputDebugStringW(L"[Main] Hotkey Win+Tab konnte nicht registriert werden.\n");
        CoUninitialize();
        return -1;
    }

    // Low-Level Hook setzen → blockiert Win+Tab in explorer.exe
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
    if (!g_hKeyboardHook)
    {
        OutputDebugStringW(L"[Main] Keyboard Hook konnte nicht gesetzt werden.\n");
    }

    while (true)
    {
        MSG msg = {};
        bool hotKeyReceived = false;

        while (GetMessageW(&msg, nullptr, 0, 0))
        {
            if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_WIN_TAB)
            {
                hotKeyReceived = true;
                break;
            }
            if (msg.message == WM_QUIT)
            {
                if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
                UnregisterHotKey(nullptr, HOTKEY_WIN_TAB);
                CoUninitialize();
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!hotKeyReceived) break;

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
    }

    if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
    UnregisterHotKey(nullptr, HOTKEY_WIN_TAB);
    CoUninitialize();
    return 0;
}
