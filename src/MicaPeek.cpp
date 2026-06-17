#include "MicaPeek.h"

MicaPeek::MicaPeek(Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice)
    : m_device(dcompDevice)
{
}

MicaPeek::~MicaPeek()
{
    m_windows.clear();
}

void MicaPeek::AttachThumbnail(HWND hwnd, Microsoft::WRL::ComPtr<IDCompositionVisual> visual)
{
    if (!m_device || !visual) return;

    WindowEntry w{};
    w.hwnd = hwnd;
    w.visual = visual;
    visual->GetContent(&w.originalContent);
    m_windows.push_back(w);
}

void MicaPeek::SetSelected(HWND hwnd)
{
    m_selected = hwnd;
}

void MicaPeek::ApplyPeek()
{
    if (!m_device) return;

    for (auto& w : m_windows)
    {
        if (!w.visual) continue;

        if (w.hwnd == m_selected)
        {
            w.visual->SetContent(w.originalContent.Get());
            D2D_MATRIX_3X2_F scale = { 1.01f, 0.0f, 0.0f, 1.01f, 0.0f, 0.0f };
            w.visual->SetTransform(scale);
        }
        else
        {
            w.visual->SetContent(nullptr);
        }
    }

    m_device->Commit();
}

void MicaPeek::ClearPeek()
{
    if (!m_device) return;

    for (auto& w : m_windows)
    {
        if (!w.visual) continue;

        w.visual->SetContent(w.originalContent.Get());

        if (w.hwnd == m_selected)
        {
            Microsoft::WRL::ComPtr<IDCompositionAnimation> zoomAnim;
            m_device->CreateAnimation(&zoomAnim);
            
            if (zoomAnim)
            {
                zoomAnim->AddCubic(0.0, 0.95f, 0.25f, 0.0f, 0.0f);
            }
        }
        
        D2D_MATRIX_3X2_F identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
        w.visual->SetTransform(identity);
    }

    m_device->Commit();
}
