#include <windows.h>
#include "ShellOverlayContext.h"

#include <windows.h>

void CloseStartMenuIfOpen()
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd)
        return;

    wchar_t cls[256] = {0};
    GetClassNameW(hwnd, cls, 256);

    bool isStartMenu = wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0;

    bool isSearch = wcscmp(cls, L"SearchHost") == 0;

    bool isStartMenuWin10 = FindWindowW(L"DV2ControlHost", nullptr) != nullptr;

    if (isStartMenu || isSearch || isStartMenuWin10)
    {
        keybd_event(VK_LWIN, 0, 0, 0);
        keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
    }
}


int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

    CloseStartMenuIfOpen();

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
