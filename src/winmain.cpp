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
            if (winDown)
                return 1; // schlucken
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    // Hotkey registrieren — optional, kein hard exit wenn es scheitert
    bool hotKeyOk = RegisterHotKey(nullptr, HOTKEY_WIN_TAB, MOD_WIN | MOD_NOREPEAT, VK_TAB);

    // Task View blockieren — optional
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);

    while (true)
    {
        MSG msg = {};
        bool shouldOpenOverlay = false;

        while (GetMessageW(&msg, nullptr, 0, 0))
        {
            if (hotKeyOk && msg.message == WM_HOTKEY && msg.wParam == HOTKEY_WIN_TAB)
            {
                shouldOpenOverlay = true;
                break;
            }
            if (msg.message == WM_QUIT)
                goto cleanup;

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!shouldOpenOverlay) break;

        ShellOverlayContext overlay;
        if (overlay.Initialize(instance))
            overlay.RunMessageLoop();
        else
            OutputDebugStringW(L"[Main] Fehler beim Initialisieren des Overlays.\n");
        overlay.Cleanup();
    }

cleanup:
    if (g_hKeyboardHook) UnhookWindowsHookEx(g_hKeyboardHook);
    if (hotKeyOk) UnregisterHotKey(nullptr, HOTKEY_WIN_TAB);
    CoUninitialize();
    return 0;
}
