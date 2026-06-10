#pragma once

#include <windows.h>
#include <shobjidl_core.h>   // IAppVisibility / CLSID_AppVisibility
#include <wrl/client.h>

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
    bool IsStartMenuOpen() const;

    static constexpr UINT_PTR TIMER_STARTMENU = 1;

    HINSTANCE m_instance;
    HWND      m_hwnd;
    UINT      m_shellHookMsg;

    Microsoft::WRL::ComPtr<IAppVisibility> m_appVisibility;

    int m_x, m_y;        // Ursprung des Arbeitsbereichs (oben links, ohne Taskleiste)
    int m_screenW, m_screenH;
};
