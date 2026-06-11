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
    bool ApplyAcrylic();

private:
    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HINSTANCE m_instance;
    HWND      m_hwnd;
    UINT      m_shellHookMsg;

    int m_x, m_y;        // Ursprung des Arbeitsbereichs (oben links, ohne Taskleiste)
    int m_screenW, m_screenH;
};
