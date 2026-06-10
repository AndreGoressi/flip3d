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

const int HOTKEY_ID = 1;

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

    // 1. Hotkey registrieren. Die App lauscht jetzt nur auf diese ID!
    if (!RegisterHotKey(nullptr, HOTKEY_ID, MOD_CONTROL | MOD_WIN, VK_TAB)) {
        OutputDebugStringW(L"[Main] Hotkey-Registrierung fehlgeschlagen.\n");
        CoUninitialize();
        return -1;
    }

    MSG msg;
    ShellOverlayContext* overlay = nullptr; // Pointer, damit wir es dynamisch steuern können
    bool isVisible = false;

    // 2. Die Schleife startet und BLOCKIERT sofort bei GetMessageW, bis eine Taste gedrückt wird
    while (GetMessageW(&msg, nullptr, 0, 0)) 
    {
        // Wurde unser Hotkey gedrückt?
        if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_ID) 
        {
            if (!isVisible) 
            {
                // ERST JETZT wird die Klasse erstellt und initialisiert!
                overlay = new ShellOverlayContext();
                if (overlay->Initialize(instance)) 
                {
                    isVisible = true;
                    OutputDebugStringW(L"[Main] Overlay ERST JETZT initialisiert und sichtbar.\n");
                } 
                else 
                {
                    delete overlay;
                    overlay = nullptr;
                }
            }
            else 
            {
                // Zweites Mal drücken -> Zerstören und in den Tiefschlaf gehen
                if (overlay) {
                    overlay->Cleanup();
                    delete overlay;
                    overlay = nullptr;
                }
                isVisible = false;
                OutputDebugStringW(L"[Main] Overlay zerstört, warte auf nächsten Hotkey.\n");
            }
            continue;
        }

        // Wenn der ShellHook das Fenster schließt (weil du woanders hinklickst)
        if (msg.message == WM_QUIT && isVisible)
        {
            OutputDebugStringW(L"[Main] ShellHook hat gefeuert -> Zerstöre Overlay.\n");
            if (overlay) {
                // Nicht nur verstecken, sondern komplett aufräumen!
                delete overlay; 
                overlay = nullptr;
            }
            isVisible = false;
            continue; // Wichtig: Verhindert, dass die wWinMain-Schleife stirbt!
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Falls beim Beenden noch was offen war, wegräumen
    if (overlay) {
        delete overlay;
    }

    UnregisterHotKey(nullptr, HOTKEY_ID);
    CoUninitialize();
    return 0;
}
