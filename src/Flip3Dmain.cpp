// Flip3D D3D11 Renderer — Entry point
// All implementation lives in Flip3D, Capture, and Config modules.

#include "Flip3DRenderer.h"

int WINAPI Flip3d(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    Flip3DRenderer r_3d;
    if (!r_3d.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 renderer.", kTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    const int initialShow = (showCommand == SW_HIDE) ? SW_MAXIMIZE : showCommand;
    ShowWindow(r_3d.RenderHandle(), initialShow);
    UpdateWindow(r_3d.RenderHandle());
    return r_3d.Run();
}
