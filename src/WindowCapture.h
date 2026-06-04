#pragma once

#include "Config.h"
#include "DwmThumbApi.h"

// WinRT interop headers for Windows.Graphics.Capture + Composition
#include <roapi.h>
#include <winstring.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <windows.ui.composition.h>
#include <windows.ui.composition.interop.h>

class WindowCapture
{
public:
    WindowCapture() = default;
    ~WindowCapture() { Release(); }

    WindowCapture(WindowCapture &&other) noexcept;
    WindowCapture &operator=(WindowCapture &&other) noexcept;

    static bool IsDwmThumbnailPlatformSupported();

    HRESULT Initialize(HWND hwndCapture, HWND hwndDestination, ID3D11Device *device);
    void Release();

    bool IsValid() const { return m_srv != nullptr; }
    ID3D11ShaderResourceView *GetSRV() const { return m_srv.Get(); }

    // Returns true once the first real WGC frame has been copied.
    // Use this to skip rendering until real content is available.
    bool HasFirstFrame() const { return m_hasFirstFrame; }

    void PollFrame();

private:
    HRESULT InitViaThumbnail(HWND hwndCapture, HWND hwndDestination);
    HRESULT CreateTextureAndSRV(UINT width, UINT height);
    HRESULT StartWGCSession(
        ABI::Windows::Graphics::Capture::IGraphicsCaptureItem *captureItem,
        UINT width, UINT height);

    ComPtr<ID3D11Device>             m_device;
    ComPtr<ID3D11DeviceContext>      m_context;
    ComPtr<ID3D11Texture2D>          m_captureTexture;
    ComPtr<ID3D11ShaderResourceView> m_srv;

    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>         m_captureItem;
    ComPtr<ABI::Windows::Graphics::Capture::IDirect3D11CaptureFramePool>  m_framePool;
    ComPtr<ABI::Windows::Graphics::Capture::IGraphicsCaptureSession>      m_session;

    ComPtr<IDCompositionVisual> m_thumbVisual;
    HTHUMBNAIL m_hThumbnail = nullptr;

    // Set to true after the first real WGC frame arrives in PollFrame()
    bool m_hasFirstFrame = false;

    static DwmThumbApi s_api;
    static ComPtr<IDCompositionDesktopDevice> s_dcompDevice;
};
