#include <windows.h>
#include "ShellOverlayContext.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    // COM initialisieren für das DirectComposition-Framework
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return -1;
    }

    // Den fensterlosen Overlay-Kontext instanziieren
    ShellOverlayContext overlay;

    // Hochfahren des Systems (Dummy-HWND, Swapchain, DWM-Visual-Graph & Acrylic-Effekt)
    if (overlay.Initialize(instance))
    {
        // Ab in die Nachrichtenschleife! 
        // Dein gesamter Bildschirm wird jetzt augenblicklich im echten Windows-Acrylic versinken.
        overlay.RunMessageLoop();
    }
    else
    {
        OutputDebugStringW(L"[Main] Schwerwiegender Fehler beim Initialisieren des DComp-Overlays.\n");
    }

    // COM sauber entladen
    CoUninitialize();

    return 0;
}
