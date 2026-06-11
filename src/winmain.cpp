#include <windows.h>
#include "ShellOverlayContext.h"
#include "Flip3DRenderer.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    Flip3DRenderer r_stack;
    if (!r_stack.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 renderer.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    const int initialShow = (showCommand == SW_HIDE) ? SW_MAXIMIZE : showCommand;
    ShowWindow(r_stack.RenderHandle(), initialShow);
    UpdateWindow(r_stack.RenderHandle());
    //------------------------------------------------------------
    
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

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
    return r_stack.Run();;
}
