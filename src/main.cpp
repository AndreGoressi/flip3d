// Flip3D D3D11 Prototype — Entry point
// All implementation lives in Flip3DApp, Capture, and Config modules.

#include "Flip3DApp.h"
#include <shlobj.h>   // Für CSIDL_DESKTOPDIRECTORY und SHGetFolderPathW
#include <shlwapi.h>
#include <thread>     // Für den Hintergrund-Thread

// Globale Variablen für den Hook und das Fenster
HHOOK g_hKeyboardHook = nullptr;
HWND g_hAppWindow = nullptr;

// Diese Funktion läuft unabhängig im Hintergrund und fängt die Tasten ab
LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        KBDLLHOOKSTRUCT* pKeyStruct = (KBDLLHOOKSTRUCT*)lParam;
        
        // Wenn die TAB-Taste gedrückt wird...
        if (wParam == WM_KEYDOWN && pKeyStruct->vkCode == VK_TAB)
        {
            // ...und gleichzeitig die linke oder rechte Windows-Taste gehalten wird
            if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000))
            {
                if (g_hAppWindow && IsWindow(g_hAppWindow))
                {
                    // Flip3D-Fenster anzeigen und in den Fokus zwingen
                    ShowWindow(g_hAppWindow, SW_SHOWNORMAL);
                    SetForegroundWindow(g_hAppWindow);
                    SetActiveWindow(g_hAppWindow);
                    
                    // 1 zurückgeben = Wir verschlucken die Taste, damit Windows 11 
                    // nicht seine eigene Task-Ansicht öffnet!
                    return 1;
                }
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

// Der separate Thread für den Tastatur-Hook
void HookThreadWorker(HINSTANCE instance)
{
    // Hook im eigenen Thread registrieren
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, instance, 0);
    
    // Eigene kleine Nachrichtenschleife, damit der Hook im Hintergrund Signale empfangen kann
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // Wenn der Thread beendet wird, Hook sauber entfernen
    if (g_hKeyboardHook)
    {
        UnhookWindowsHookEx(g_hKeyboardHook);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    // ========================================================================
    // 1. DIE DESKTOP-TARNING DIREKT IM PROGRAMM
    // ========================================================================
    wchar_t desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktopPath)))
    {
        SetCurrentDirectoryW(desktopPath);
    }

    // ========================================================================
    // 2. INITIALISIERUNG DER APP
    // ========================================================================
    Flip3DPrototypeApp app;
    if (!app.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    // Fensterhandle für den Hintergrund-Thread speichern
    g_hAppWindow = app.WindowHandle();

    // ========================================================================
    // 3. START DES HINTERGRUND-THREADS FÜR DEN HOTKEY
    // ========================================================================
    // Wir starten den Hook in einem eigenen, isolierten Thread, damit DirectX 
    // und das Hauptprogramm ungestört laufen können.
    std::thread hookThread(HookThreadWorker, instance);
    hookThread.detach(); // Vom Hauptthread lösen, läuft autark im Hintergrund

    // Das Fenster bleibt beim Starten unsichtbar, bis der Hotkey gedrückt wird!
    // (ShowWindow wird hier absichtlich übersprungen)

    // ========================================================================
    // 4. HAUPT-SCHLEIFE STARTEN (DirectX / Rendering)
    // ========================================================================
    int exitCode = app.Run();

    return exitCode;
}
