#pragma once

#include <windows.h>
#include "Flip3DRenderer.h"

#pragma once

class ShellOverlayContext
{
public:
    ShellOverlayContext();
    ~ShellOverlayContext();

    bool Initialize(HINSTANCE instance);
    void RunMessageLoop();
    void Cleanup();

private:
    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    bool ApplyAcrylic();

    HINSTANCE m_instance;
    HWND      m_hwnd;
    UINT      m_shellHookMsg;

    int m_x, m_y;        // Ursprung des Arbeitsbereichs (oben links, ohne Taskleiste)
    int m_screenW, m_screenH;

    Flip3DRenderer   m_renderer;   
};
