#pragma once
#include <Windows.h>
#include <dcomp.h>
#include <vector>
#include <wrl/client.h>

class MicaPeek
{
public:
    MicaPeek(Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice);
    ~MicaPeek();

    void AttachThumbnail(HWND hwnd, Microsoft::WRL::ComPtr<IDCompositionVisual> visual);
    void SetSelected(HWND hwnd);
    void ApplyPeek();
    void ClearPeek();

private:
    struct WindowEntry
    {
        HWND hwnd;
        Microsoft::WRL::ComPtr<IDCompositionVisual> visual;
        Microsoft::WRL::ComPtr<IUnknown> originalContent; 
    };

    Microsoft::WRL::ComPtr<IDCompositionDevice> m_device;
    std::vector<WindowEntry> m_windows;
    HWND m_selected = nullptr;
};
