#pragma once
#include <windows.h>

class ShellOverlayContext
{
public:
    ShellOverlayContext();
    ~ShellOverlayContext();

    bool Initialize(HINSTANCE instance);
    
    void RunMessageLoop();
    void Cleanup();
    
    HWND ShellHandle() const { return m_hwnd; }
    int GetX() const { return m_x; }
    int GetY() const { return m_y; }
    int GetWidth() const { return m_screenW; }
    int GetHeight() const { return m_screenH; }

    UINT m_shellHookMsg = 0; 

private:
    bool ApplyAcrylic();
    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HINSTANCE m_instance = nullptr;
    HWND m_hwnd = nullptr;
    
    int m_x = 0;
    int m_y = 0;
    int m_screenW = 0;
    int m_screenH = 0;
};
