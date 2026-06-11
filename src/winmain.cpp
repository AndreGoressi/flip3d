#include <windows.h>
#include <thread> 
#include "ShellOverlayContext.h"
#include "Flip3DRenderer.h"

void RunOverlayThread(HINSTANCE instance)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        ShellOverlayContext overlay;
        if (overlay.Initialize(instance))
        {
            overlay.RunMessageLoop(); 
        }
        else
        {
            OutputDebugStringW(L"[Main] Fatal error initializing the overlay in thread.\n");
        }
        CoUninitialize();
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    std::thread overlayThread(RunOverlayThread, instance);
    
    overlayThread.detach(); 

    //Sleep(100);

    Flip3DRenderer wnd;
    if (!wnd.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    const int initialShow = (showCommand == SW_HIDE) ? SW_MAXIMIZE : showCommand;
    ShowWindow(wnd.RenderHandle(), initialShow);
    UpdateWindow(wnd.RenderHandle());

    int result = wnd.Run();

    CoUninitialize();
    return result;
}
