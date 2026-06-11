#include <windows.h>
#include "ShellOverlayContext.h"
#include "Flip3DRenderer.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int nShowCmd)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    ShellOverlayContext overlay;
    if (!overlay.Initialize(instance))
        OutputDebugStringW(L"[Main] Fatal error initializing the overlay.\n");

    Flip3DRenderer wnd;
    if (!wnd.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize.", kTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    SetWindowPos(overlay.ShellHandle(), wnd.RenderHandle(),
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    ShowWindow(wnd.RenderHandle(), nShowCmd);
    UpdateWindow(wnd.RenderHandle());

    int result = wnd.Run();
    CoUninitialize();
    return result;
}
