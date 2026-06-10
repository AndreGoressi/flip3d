#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

using Microsoft::WRL::ComPtr;

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
    bool CreateD3DAndComposition();
    void RenderAcrylicWash();

    HINSTANCE   m_instance;
    HWND        m_hwnd;        // Das echte, bildschirmgrosse Ankerfenster (fuer DComp)
    UINT        m_shellHookMsg;

    int m_screenW;
    int m_screenH;

    ComPtr<ID3D11Device>            m_d3dDevice;
    ComPtr<ID3D11DeviceContext>     m_d3dContext;
    ComPtr<IDXGISwapChain1>         m_swapChain;
    ComPtr<IDCompositionDevice>     m_dcompDevice;
    ComPtr<IDCompositionTarget>     m_dcompTarget;
    ComPtr<IDCompositionVisual>     m_rootVisual;
};
