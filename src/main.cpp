// Flip3D D3D11 Prototype — Entry point
// All implementation lives in Flip3D, Capture, and Config modules.

#include "Flip3DWindow.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    Flip3DPrototype window;
    if (!window.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    auto IsWindowsDarkModeActive = []() -> bool {
        HKEY hKey;
        DWORD value = 1; 
        DWORD size = sizeof(value);
        
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
            RegCloseKey(hKey);
        }
        return (value == 0);
    };

    bool isDark = IsWindowsDarkModeActive();
    UINT iconResourceId = isDark ? 101 : 102; 

    HICON hThemeIcon = (HICON)LoadImageW(
        GetModuleHandleW(nullptr), 
        MAKEINTRESOURCE(iconResourceId), 
        IMAGE_ICON, 
        0, 0, 
        LR_DEFAULTSIZE | LR_SHARED
    );

    HWND hwnd = window.WindowHandle();
    if (hwnd && hThemeIcon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hThemeIcon);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hThemeIcon);
    }
    // ============================================================================

    const int initialShow = (showCommand == SW_HIDE) ? SW_MAXIMIZE : showCommand;
    ShowWindow(window.WindowHandle(), initialShow);
    UpdateWindow(window.WindowHandle());
    return window.Run();
}
