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
    //
    int GetX() const { return m_x; }
    int GetY() const { return m_y; }
    int GetWidth() const { return m_screenW; }
    int GetHeight() const { return m_screenH; }

private:
    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    bool ApplyAcrylic();

    HINSTANCE m_instance;
    HWND      m_hwnd;
    UINT      m_shellHookMsg;

    int m_x, m_y;        
    int m_screenW, m_screenH;
};
