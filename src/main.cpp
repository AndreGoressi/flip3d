// Flip3D D3D11 Prototype — Entry point
// All implementation lives in Flip3DApp, Capture, and Config modules.

#include "Flip3DApp.h"

#include <windows.h>
#include <shlobj.h> 
#include <string>

std::wstring GetAppHomeDirectory()
{
    PWSTR pszPath = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &pszPath);
    
    std::wstring fakeHomePath = L"";
    if (SUCCEEDED(hr))
    {
        fakeHomePath = pszPath;
        CoTaskMemFree(pszPath);
        fakeHomePath += L"\\";
    }
    else
    {
        fakeHomePath = L"C:\\Users\\Public\\Desktop\\";
    }

    return fakeHomePath; 
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{

    SetCurrentDirectoryW(GetAppHomeDirectory().c_str());

    Flip3DPrototypeApp app;
    if (!app.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    const int initialShow = (showCommand == SW_HIDE) ? SW_SHOWNORMAL : showCommand;
    ShowWindow(app.WindowHandle(), initialShow);
    UpdateWindow(app.WindowHandle());
    return app.Run();
}
