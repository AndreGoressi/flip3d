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
    w.baseOpacity = 1.0f; 
    
    m_windows.push_back(w);
}

void MicaPeek::SetSelected(HWND hwnd)
{
    m_selected = hwnd;
}

void MicaPeek::DrawPeek()
{
    if (!m_device) return;

    for (auto& w : m_windows)
    {
        if (!w.visual) continue;

        if (w.hwnd == m_selected)
        {
            w.visual->SetOpacity(1.0f);
            w.visual->SetScaleX(1.01f);
            w.visual->SetScaleY(1.01f);
        }
        else
        {
            w.visual->SetOpacity(0.0f);
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

        if (w.hwnd == m_selected)
        {
            Microsoft::WRL::ComPtr<IDCompositionAnimation> zoomAnim;
            m_device->CreateAnimation(&zoomAnim);
            
            if (zoomAnim)
            {
                zoomAnim->AddCubic(0.0, 0.95f, 0.25f, 0.0f, 0.0f);
                w.visual->SetScaleX(zoomAnim.Get());
                w.visual->SetScaleY(zoomAnim.Get());
            }
            w.visual->SetOpacity(1.0f);
        }
        else
        {
            Microsoft::WRL::ComPtr<IDCompositionAnimation> fadeAnim;
            m_device->CreateAnimation(&fadeAnim);
            
            if (fadeAnim)
            {
                fadeAnim->AddCubic(0.0, 0.0f, 3.33f, 0.0f, 0.0f); 
                w.visual->SetOpacity(fadeAnim.Get());
            }
            
            w.visual->SetScaleX(1.0f);
            w.visual->SetScaleY(1.0f);
        }
    }

    m_device->Commit();
}