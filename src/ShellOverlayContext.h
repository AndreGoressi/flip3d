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
    HWND WindowHandle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    bool ApplyAcrylic();

    HINSTANCE m_instance;
    HWND      m_hwnd;
    UINT      m_shellHookMsg;

    int m_x, m_y;        
    int m_screenW, m_screenH;
};
