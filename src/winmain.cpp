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
