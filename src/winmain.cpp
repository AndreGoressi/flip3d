#include <windows.h>
#include "Flip3DRenderer.h"
#include "Config.h"

/*int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    Flip3DRenderer rnd;
    if (!rnd.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize.", kTitle, MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    HWND renderHwnd = rnd.RenderHandle();

    ShowWindow(renderHwnd, showCommand == SW_HIDE ? SW_MAXIMIZE : showCommand);
    UpdateWindow(renderHwnd);

    SetForegroundWindow(renderHwnd);
    SetActiveWindow(renderHwnd);

    int result = rnd.Run();
    CoUninitialize();
    return result;
}*/

#include <dwmapi.h> 



HWINEVENTHOOK g_minimizeHook = nullptr;

void CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{

    if (event == EVENT_SYSTEM_MINIMIZESTART && idObject == OBJID_WINDOW)
    {

        wchar_t className[256];
        GetClassNameW(hwnd, className, 256);
        if (wcscmp(className, kRenderClassName) == 0)
            return;


        DWORD cloakAttribute = DWM_CLOAKED_APP;
        DwmSetWindowAttribute(hwnd, DWMWA_CLOAK, &cloakAttribute, sizeof(cloakAttribute));

        ShowWindow(hwnd, SW_HIDE);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); 
    if (FAILED(hr)) return -1; 
    g_minimizeHook = SetWinEventHook(
        EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZESTART,
        nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

    Flip3DRenderer rnd; 
    if (!rnd.Initialize(instance)) 
    {
        MessageBoxW(nullptr, L"Failed to initialize.", kTitle, MB_OK | MB_ICONERROR); 
        if (g_minimizeHook) UnhookWinEvent(g_minimizeHook); 
        CoUninitialize(); 
        return 1; 
    }

    HWND renderHwnd = rnd.RenderHandle(); 

    ShowWindow(renderHwnd, showCommand == SW_HIDE ? SW_MAXIMIZE : showCommand); 
    UpdateWindow(renderHwnd); 

    SetForegroundWindow(renderHwnd); 
    SetActiveWindow(renderHwnd); 

    int result = rnd.Run(); 

    if (g_minimizeHook)
    {
        UnhookWinEvent(g_minimizeHook);
    }

    CoUninitialize(); 
    return result; 
}
