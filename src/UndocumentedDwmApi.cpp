#include "UndocumentedDwmApi.h"

void UndocumentedDwmApi::DwmpActivateLivePreview(HWND hwnd, HWND hTopmostWindow, BOOL enable)
{
    using DwmpActivateLivePreview_t = HRESULT(WINAPI*)(BOOL, HWND, HWND, UINT, LPVOID);
    
    static DwmpActivateLivePreview_t pDwmpActivateLivePreview = nullptr;
    static BOOL aeroPeekActive = FALSE;
    static bool isInitialized = false;

    if (!isInitialized)
    {
        HMODULE dwmapiModule = LoadLibraryEx(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (dwmapiModule)
        {
            pDwmpActivateLivePreview = reinterpret_cast<DwmpActivateLivePreview_t>(
                GetProcAddress(dwmapiModule, reinterpret_cast<PCSTR>(113))
            );
        }
        isInitialized = true;
    }

    if (!pDwmpActivateLivePreview) return;

    if (aeroPeekActive != enable)
    {
        if (enable)
        {
            pDwmpActivateLivePreview(TRUE, hwnd, hTopmostWindow, 3, reinterpret_cast<LPVOID>(0x3244));
        }
        else /*(disable)*/
        {
            pDwmpActivateLivePreview(FALSE, nullptr, nullptr, 3, nullptr);
        }
        
        aeroPeekActive = enable;
    }
}
