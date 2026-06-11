#include <windows.h>
#include "ShellOverlayContext.h"
#include "Flip3DRenderer.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

    ShellOverlayContext overlay;

    if (overlay.Initialize(instance))
    {
        overlay.RunMessageLoop();
    }
    else
    {
        OutputDebugStringW(L"[Main] Fatal error initializing the overlay.\n");
    }

    CoUninitialize();

    return 0;
}

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

