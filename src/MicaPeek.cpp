#include "MicaPeek.h"

MicaPeek::MicaPeek(Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice)
    : m_device(dcompDevice)
{
}

MicaPeek::~MicaPeek()
{
    m_windows.clear();
}

void MicaPeek::AttachThumbnail(HWND hwnd, Microsoft::WRL::ComPtr<IDCompositionVisual> visual, float originalX)
{
    if (!m_device || !visual) return;
    WindowEntry w{};
    w.hwnd = hwnd;
    w.visual = visual;
    w.originalOffsetX = originalX; 
    
    m_windows.push_back(w);
}

void MicaPeek::SetSelected(HWND hwnd)
{
    m_selected = hwnd;
}

void MicaPeek::ApplyPeek()
{
    for (auto& w : m_windows)
    {
        if (!w.hwnd) continue;

        if (w.hwnd == m_selected)
        {
            SetLayeredWindowAttributes(w.hwnd, 0, 255, LWA_ALPHA);
            SetWindowPos(w.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            RedrawWindow(w.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

            if (w.visual)
            {
                w.visual->SetOffsetX(w.originalOffsetX);
                w.visual->SetOffsetY(0.0f); 
                D2D_MATRIX_3X2_F scale = { 1.01f, 0.0f, 0.0f, 1.01f, 0.0f, 0.0f };
                w.visual->SetTransform(scale);
            }
        }
        else
        {
            SetLayeredWindowAttributes(w.hwnd, 0, 0, LWA_ALPHA);
            SetWindowPos(w.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            RedrawWindow(w.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

            if (w.visual)
            {
                w.visual->SetOffsetY(99999.0f);
            }
        }
    }

    if (m_device) m_device->Commit();
}

void MicaPeek::ClearPeek()
{
    if (!m_device) return;

    for (auto& w : m_windows)
    {
        if (!w.visual) continue;
        w.visual->SetOffsetX(w.originalOffsetX);
        w.visual->SetOffsetY(0.0f);
        D2D_MATRIX_3X2_F identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
        w.visual->SetTransform(identity);
    }

    m_device->Commit();
}
