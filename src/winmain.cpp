#include <windows.h>
#include "Flip3DRenderer.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    Flip3DRenderer rnd;
    if (!rnd.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize.", kTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    ShowWindow(rnd.RenderHandle(), showCommand == SW_HIDE ? SW_MAXIMIZE : showCommand);
    UpdateWindow(rnd.RenderHandle());
    SetForegroundWindow(rnd.RenderHandle());

    int result = rnd.Run();
    CoUninitialize();
    return result;
}
