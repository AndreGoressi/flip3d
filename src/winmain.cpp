#include <windows.h>
#include "ShellOverlayContext.h"
#include "Flip3DRenderer.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    Flip3DRenderer wnd;
    if (!wnd.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    ShellOverlayContext overlay;
    if (!overlay.Initialize(instance))
    {
        OutputDebugStringW(L"[Main] Fatal error initializing the overlay.\n");
    }

    const int initialShow = (showCommand == SW_HIDE) ? SW_MAXIMIZE : showCommand;
    ShowWindow(wnd.RenderHandle(), initialShow);
    UpdateWindow(wnd.RenderHandle());

    HWND overlayHwnd = overlay.GetHwnd(); 
    
    if (wnd.RenderHandle() && overlayHwnd) 
    {
        SetWindowLongPtrW(overlayHwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS);
        SetParent(overlayHwnd, wnd.RenderHandle());
    }

    int result = wnd.Run();

    CoUninitialize();
    return result;
}
