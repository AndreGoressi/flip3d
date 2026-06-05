// Flip3D D3D11 Prototype — Entry point
// All implementation lives in Flip3D, Capture, and Config modules.

#include "flip3d.h"
#include <shlobj.h>   
#include <shlwapi.h>
#include <TlHelp32.h> 

#pragma comment(lib, "Shlwapi.lib")

DWORD GetDwmProcessId()
{
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry = { sizeof(entry) };
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, L"dwm.exe") == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

bool InjectDwmHelper(const wchar_t* dllPath)
{
    DWORD pid = GetDwmProcessId();
    if (pid == 0) return false;

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) return false;

    size_t size = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    void* pLibSpace = VirtualAllocEx(hProcess, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pLibSpace) { CloseHandle(hProcess); return false; }

    WriteProcessMemory(hProcess, pLibSpace, dllPath, size, nullptr);

    PTHREAD_START_ROUTINE pLoadLibrary = reinterpret_cast<PTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")
    );

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, pLoadLibrary, pLibSpace, 0, nullptr);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    CloseHandle(hProcess);
    return hThread != nullptr;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    wchar_t dllPath[MAX_PATH];
    GetModuleFileNameW(nullptr, dllPath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(dllPath, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcscat_s(dllPath, L"flip3d_dwm_hook.dll");

    InjectDwmHelper(dllPath);

    Flip3DPrototype wnd;
    if (!app.Initialize(instance))
    {
        MessageBoxW(nullptr, L"Failed to initialize the Flip3D D3D11 prototype.", kWindowTitle, MB_OK | MB_ICONERROR);
        return 1;
    }

    const int initialShow = (showCommand == SW_HIDE) ? SW_SHOWNORMAL : showCommand;
    ShowWindow(wnd.WindowHandle(), initialShow);
    UpdateWindow(wnd.WindowHandle());
    return wnd.Run();
}
