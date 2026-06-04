// Flip3D D3D11 Prototype — Entry point
// All implementation lives in Flip3DApp, Capture, and Config modules.

#include "Flip3DApp.h"
#include <shlobj.h>   
#include <shlwapi.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    wchar_t desktopPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktopPath)))
    {
        SetCurrentDirectoryW(desktopPath);
    }

    if (!RegisterHotKey(nullptr, 1, MOD_WIN, VK_TAB))
    {
        RegisterHotKey(nullptr, 1, MOD_WIN | MOD_SHIFT, VK_TAB);
    }

    Flip3DPrototypeApp app;
    if (!app.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }
    
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_HOTKEY && msg.wParam == 1)
        {
            ShowWindow(app.WindowHandle(), SW_SHOWNORMAL);
            SetForegroundWindow(app.WindowHandle());
            SetActiveWindow(app.WindowHandle());
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnregisterHotKey(nullptr, 1);

    return (int)msg.wParam;
}
