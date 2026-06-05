// Flip3D D3D11 Prototype — Entry point
// All implementation lives in Flip3DApp, Capture, and Config modules.

#include "Flip3DApp.h"
#include <shlobj.h>   
#include <shlwapi.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    Flip3DPrototypeApp app;
    if (!app.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    const int initialShow = (showCommand == SW_HIDE) ? SW_SHOWNORMAL : showCommand;
    ShowWindow(app.WindowHandle(), initialShow);
    UpdateWindow(app.WindowHandle());
    return app.Run();
}
