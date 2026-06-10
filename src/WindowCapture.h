#pragma once

#include "Config.h"
#include "DwmThumbApi.h"

// WinRT interop headers for Windows.Graphics.Capture + Composition
#include <roapi.h>
#include <winstring.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.ui.composition.h>          // ABI::Windows::UI::Composition::IVisual
#include <windows.ui.composition.interop.h>  // ICompositorInterop

// ---------------------------------------------------------------------------
// WindowCapture — captures a single HWND's content via Windows.Graphics.Capture
// into an ID3D11ShaderResourceView for use as a card texture.
// ---------------------------------------------------------------------------
class WindowCapture
{
public:
    WindowCapture() = default;
    ~WindowCapture() { Release(); }

    bool HasFirstFrame() const { return m_hasFirstFrame; }

    WindowCapture(WindowCapture &&other) noexcept;
    WindowCapture &operator=(WindowCapture &&other) noexcept;

    // --- Static platform support ---
    static bool IsDwmThumbnailPlatformSupported();

    // --- Init / teardown ---
    HRESULT Initialize(HWND hwndCapture, HWND hwndDestination, ID3D11Device *device);
    void Release();

    bool IsValid() const { return m_srv != nullptr; }
    ID3D11ShaderResourceView *GetSRV() const { return m_srv.Get(); }

    // Poll for a new captured frame (call from render thread).
    void PollFrame();

private:
    // DWM thumbnail visual → InteropCompositor → WGC(CreateFromVisual)
    HRESULT InitViaThumbnail(HWND hwndCapture, HWND hwndDestination);
    bool m_hasFirstFrame = false;

    // Shared helpers
    HRESULT CreateTextureAndSRV(UINT width, UINT height);
    HRESULT StartWGCSession(
        ABI::Windows::Graphics::Capture::IGraphicsCaptureItem *captureItem,
        UINT width, UINT height);

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<ID3D11Texture2D> m_captureTexture;
    ComPtr<ID3D11ShaderResourceView> m_srv;

    // ABI:: (WinRT interop) capture objects (only for WGC paths)
    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem> m_captureItem;
    ComPtr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool> m_framePool;
    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession> m_session;

    // DWM thumbnail visual (DwmThumbnail path only)
    ComPtr<IDCompositionVisual> m_thumbVisual;
    HTHUMBNAIL m_hThumbnail = nullptr;



    // Global compositor / interop state (lazily initialized)
    static DwmThumbApi s_api;
    static ComPtr<IDCompositionDesktopDevice> s_dcompDevice;
};
