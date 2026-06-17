#include "MicaPeek.h"
#include <d2d1.h>

MicaPeek::MicaPeek(Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice)
    : m_device(dcompDevice)
{
}

MicaPeek::~MicaPeek()
{
    m_windows.clear();
}

void MicaPeek::AttachThumbnail(HWND hwnd, Microsoft::WRL::ComPtr<IDCompositionVisual2> visual)
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

void MicaPeek::ApplyPeek()
{
    if (!m_device) return;

    for (auto& w : m_windows)
    {
        if (!w.visual) continue;

        if (w.hwnd == m_selected)
        {
            w.visual->SetOpacity(1.0f);
            D2D_MATRIX_3X2_F scale = { 1.01f, 0.0f, 0.0f, 1.01f, 0.0f, 0.0f };
            w.visual->SetTransform(reinterpret_cast<const float*>(&scale));
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
                w.visual->SetTransform64(zoomAnim.Get()); 
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
            
            D2D_MATRIX_3X2_F identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
            w.visual->SetTransform(reinterpret_cast<const float*>(&identity));
        }
    }

    m_device->Commit();
}
