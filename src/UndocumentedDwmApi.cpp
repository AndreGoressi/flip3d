#include "UndocumentedDwmApi.h"

void UndocumentedDwmApi::DwmpActivateLivePreview(HWND hwnd, BOOL enable)
{
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
        UINT m_PeekType = static_cast<UINT>(PeekTypes::Window);
        
        pDwmpActivateLivePreview(enable, hwnd, hwnd, m_PeekType, reinterpret_cast<LPVOID>(0x3244));
        
        aeroPeekActive = enable;
    }
}