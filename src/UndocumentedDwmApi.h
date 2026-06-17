#pragma once
#include <windows.h>

enum class PeekTypes : UINT
{
    NotUsed = 0,
    Desktop = 1,
    Window  = 3
};

class UndocumentedDwmApi
{
public:
    static void DwmpActivateLivePreview(HWND hFlip3DWindow, BOOL enable);

private:
    using DwmpActivateLivePreview_t = HRESULT(WINAPI*)(BOOL, HWND, HWND, UINT, LPVOID);
};
