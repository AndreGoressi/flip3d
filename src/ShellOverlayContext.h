#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

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
};
