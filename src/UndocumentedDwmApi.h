#pragma once
#include <windows.h>

namespace UndocumentedDwmApi
{
    enum class PeekTypes : UINT
    {
        NotUsed = 0,
        Desktop = 1,
        Window  = 3
    };

    using DwmpActivateLivePreview_t = BOOL(WINAPI*)(
        BOOL    enable,
        HWND    hwnd,
        HWND    hTopmostWindow,
        UINT    peekType,
        LPVOID  param5
    );

    void DwmpActivateLivePreview(HWND hTopmostWindow, HWND hwnd, BOOL enable);
}
