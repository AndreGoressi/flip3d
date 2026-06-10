// Flip3D D3D11 Renderer — Entry point
// All implementation lives in Flip3D, Capture, and Config modules.

#include "Flip3DRenderer.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    Flip3DRenderer wnd;
    if (!wnd.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    const int initialShow = (showCommand == SW_HIDE) ? SW_MAXIMIZE : showCommand;
    ShowWindow(wnd.WindowHandle(), initialShow);
    UpdateWindow(wnd.WindowHandle());
    return wnd.Run();
}
