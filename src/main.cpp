// Flip3D D3D11 Prototype — Entry point
// All implementation lives in Flip3DApp, Capture, and Config modules.

#include "Flip3DApp.h"
#include <shlobj.h>   // for CSIDL_DESKTOPDIRECTORY & SHGetFolderPathW
#include <shlwapi.h>

HHOOK g_hKeyboardHook = nullptr;
HWND g_hAppWindow = nullptr;

LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        KBDLLHOOKSTRUCT* pKeyStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN && pKeyStruct->vkCode == VK_TAB)
        {
            if ((GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000))
            {
                if (g_hAppWindow && IsWindow(g_hAppWindow))
                {
                    ShowWindow(g_hAppWindow, SW_SHOWNORMAL);
                    SetForegroundWindow(g_hAppWindow);
                    SetActiveWindow(g_hAppWindow);
                    
                    return 1;
                }
            }
        }
    }
    return CallNextHookEx(g_hKeyboardHook, nCode, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    wchar_t desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktopPath)))
    {
        SetCurrentDirectoryW(desktopPath);
    }

    Flip3DPrototypeApp app;
    if (!app.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    g_hAppWindow = app.WindowHandle();
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, instance, 0);

    int exitCode = app.Run();
    if (g_hKeyboardHook)
    {
        UnhookWindowsHookEx(g_hKeyboardHook);
    }

    return exitCode;
}
