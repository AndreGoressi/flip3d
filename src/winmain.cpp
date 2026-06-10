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

// Eindeutige ID für unseren globalen Hotkey
const int HOTKEY_ID = 1;

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

    {
        ShellOverlayContext overlay;

        // Wir registrieren den Hotkey direkt beim System.
        // MOD_CONTROL | MOD_WIN + VK_TAB = STRG + WIN + TAB
        // (Verhindert, dass das Windows-Startmenü dazwischengrätscht!)
        if (!RegisterHotKey(nullptr, HOTKEY_ID, MOD_CONTROL | MOD_WIN, VK_TAB)) {
            OutputDebugStringW(L"[Main] Hotkey-Registrierung fehlgeschlagen.\n");
            CoUninitialize();
            return -1;
        }

        // Wir rufen dein originales Initialize auf. 
        // WICHTIG: In der ShellOverlayContext.cpp müssen wir ShowWindow(m_hwnd, SW_SHOWNOACTIVATE)
        // beim Start entfernen, damit das Overlay erst mal unsichtbar bleibt!
        if (overlay.Initialize(instance))
        {
            // Ab hier läuft deine originale Nachrichtenschleife, nur erweitert um den Hotkey-Trigger
            MSG msg;
            bool isVisible = false;

            while (GetMessageW(&msg, nullptr, 0, 0)) 
            {
                // Fange den magischen Hotkey ab
                if (msg.message == WM_HOTKEY && msg.wParam == HOTKEY_ID) 
                {
                    if (!isVisible) 
                    {
                        // ZÜNDUNG: Hol das Acrylic auf den Schirm!
                        // Wir holen uns das HWND aus der Klasse (falls du es public machst, 
                        // oder wir fügen eine GetHwnd() Methode hinzu)
                        // Alternativ schalten wir es direkt über Windows-Befehle sichtbar:
                        HWND targetHwnd = FindWindowW(L"ShellOverlayClass", nullptr);
                        if (targetHwnd) {
                            ShowWindow(targetHwnd, SW_SHOWNOACTIVATE);
                            isVisible = true;
                            OutputDebugStringW(L"[Main] Acrylic eingeblendet.\n");
                        }
                    }
                    else 
                    {
                        // Zweites Mal drücken -> Wieder verstecken
                        HWND targetHwnd = FindWindowW(L"ShellOverlayClass", nullptr);
                        if (targetHwnd) {
                            ShowWindow(targetHwnd, SW_HIDE);
                            isVisible = false;
                        }
                    }
                    continue;
                }

                // Wenn der ShellHook das Fenster im Hintergrund gekillt hat (WM_QUIT),
                // fangen wir das ab, beenden aber nicht die App, sondern verstecken nur das Fenster
                if (msg.message == WM_QUIT && isVisible)
                {
                    OutputDebugStringW(L"[Main] ShellHook hat angeschlagen -> Verstecke Overlay.\n");
                    HWND targetHwnd = FindWindowW(L"ShellOverlayClass", nullptr);
                    if (targetHwnd) {
                        ShowWindow(targetHwnd, SW_HIDE);
                    }
                    isVisible = false;
                    continue; // Verhindert das echte Beenden der App-Schleife!
                }

                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        else
        {
            OutputDebugStringW(L"[Main] Schwerwiegender Fehler beim Initialisieren des Overlays.\n");
        }

        // Hotkey wieder sauber freigeben beim Beenden
        UnregisterHotKey(nullptr, HOTKEY_ID);
        overlay.Cleanup();
    }

    CoUninitialize();
    return 0;
}
