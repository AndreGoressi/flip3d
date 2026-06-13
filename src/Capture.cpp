#include "Capture.h"

// ============================================================================
// Registry preference helpers
// ============================================================================
bool TryReadDwmDwordPreference(const wchar_t *name, DWORD &value)
{
    DWORD size = sizeof(value);
    LONG status = RegGetValueW(HKEY_CURRENT_USER, kDwmPreferencePath, name, RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (status != ERROR_SUCCESS)
    {
        size = sizeof(value);
        status = RegGetValueW(HKEY_LOCAL_MACHINE, kDwmPreferencePath, name, RRF_RT_REG_DWORD, nullptr, &value, &size);
    }

    return status == ERROR_SUCCESS;
}

bool TryReadDwmFloatPreference(const wchar_t *name, float &value)
{
    wchar_t buffer[32] = {};
    DWORD size = sizeof(buffer);
    LONG status = RegGetValueW(HKEY_CURRENT_USER, kDwmPreferencePath, name, RRF_RT_REG_SZ, nullptr, buffer, &size);
    if (status != ERROR_SUCCESS)
    {
        size = sizeof(buffer);
        status = RegGetValueW(HKEY_LOCAL_MACHINE, kDwmPreferencePath, name, RRF_RT_REG_SZ, nullptr, buffer, &size);
    }

    if (status != ERROR_SUCCESS)
    {
        return false;
    }

    wchar_t *parseEnd = nullptr;
    const float parsed = std::wcstof(buffer, &parseEnd);
    if (parseEnd == buffer)
    {
        return false;
    }

    value = parsed;
    return true;
}

void LoadFlip3DPreferences()
{
    gEnterExitDurationSec = kEnterExitDurationSec;
    gRotateListDurationSec = kRotateListDurationSec;
    gRotateToHomeMaxDurationSec = kRotateToHomeMaxDurationSec;
    gNearPlaneEdgeSize = kNearPlaneEdgeSize;
    gFinalMinRectWidthPercentage = kFinalMinRectWidthPercentage;
    gFinalMinRectLeftPercentage = kFinalMinRectLeftPercentage;
    gMaxVisibleCards = kMaxVisibleCards;
    gNormalizationBezier = kNormalizationBezier;
    gBezierControls = {
        kBezierControls[0],
        kBezierControls[1],
        kBezierControls[2],
    };
    gCameraFinalTranslate = kCameraFinalTranslate;
    gCameraFinalRotate = kCameraFinalRotate;

    DWORD dwordValue = 0;
    float floatValue = 0.0f;

    if (TryReadDwmDwordPreference(L"Max3DWindows", dwordValue) && dwordValue != 0)
    {
        gMaxVisibleCards = static_cast<int>(dwordValue);
    }

    if (TryReadDwmFloatPreference(L"FinalRectWidthPercentageForMinMax", floatValue))
    {
        gFinalMinRectWidthPercentage = floatValue;
    }

    if (TryReadDwmFloatPreference(L"FinalRectLeftForMinMax", floatValue))
    {
        gFinalMinRectLeftPercentage = floatValue;
    }

    if (TryReadDwmDwordPreference(L"ReadFlip3DModel", dwordValue) && dwordValue != 0)
    {
        const struct FloatPreference
        {
            const wchar_t *name;
            float *value;
        } floatPreferences[] = {
            {L"Flip3DBezierX0", &gBezierControls[0].x},
            {L"Flip3DBezierY0", &gBezierControls[0].y},
            {L"Flip3DBezierZ0", &gBezierControls[0].z},
            {L"Flip3DBezierX1", &gBezierControls[1].x},
            {L"Flip3DBezierY1", &gBezierControls[1].y},
            {L"Flip3DBezierZ1", &gBezierControls[1].z},
            {L"Flip3DBezierX2", &gBezierControls[2].x},
            {L"Flip3DBezierY2", &gBezierControls[2].y},
            {L"Flip3DBezierZ2", &gBezierControls[2].z},
            {L"Flip3DCameraNearPlaneEdgeSize", &gNearPlaneEdgeSize},
            {L"Flip3DNormalization0", &gNormalizationBezier[0]},
            {L"Flip3DNormalization1", &gNormalizationBezier[1]},
            {L"Flip3DNormalization2", &gNormalizationBezier[2]},
            {L"Flip3DCameraX", &gCameraFinalTranslate.x},
            {L"Flip3DCameraY", &gCameraFinalTranslate.y},
            {L"Flip3DCameraZ", &gCameraFinalTranslate.z},
            {L"Flip3DCameraPitch", &gCameraFinalRotate.x},
            {L"Flip3DCameraYaw", &gCameraFinalRotate.y},
            {L"Flip3DCameraRoll", &gCameraFinalRotate.z},
        };

        for (const FloatPreference &preference : floatPreferences)
        {
            if (TryReadDwmFloatPreference(preference.name, floatValue))
            {
                *preference.value = floatValue;
            }
        }
    }

    if (TryReadDwmDwordPreference(L"Flip3DEnterExitViewDurationMsec", dwordValue) && dwordValue != 0)
    {
        gEnterExitDurationSec = static_cast<float>(dwordValue) / 1000.0f;
    }

    if (TryReadDwmDwordPreference(L"Flip3DRotateListDurationMSec", dwordValue) && dwordValue != 0)
    {
        gRotateListDurationSec = static_cast<float>(dwordValue) / 1000.0f;
    }

    if (TryReadDwmDwordPreference(L"Flip3DRotateToHomeMaxDurationMsec", dwordValue) && dwordValue != 0)
    {
        gRotateToHomeMaxDurationSec = static_cast<float>(dwordValue) / 1000.0f;
    }
}

// ============================================================================
// Window capture / enumeration
// ============================================================================
GetWindowMinimizeRectFn ResolveGetWindowMinimizeRect()
{
    static GetWindowMinimizeRectFn function = reinterpret_cast<GetWindowMinimizeRectFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetWindowMinimizeRect"));
    return function;
}

bool TryGetFlip3DWindowPolicy(HWND hwnd, DWORD &policy)
{
    policy = kFlip3DPolicyDefault;
    return SUCCEEDED(DwmGetWindowAttribute(
        hwnd,
        static_cast<DWMWINDOWATTRIBUTE>(kFlip3DPolicyAttribute),
        &policy,
        sizeof(policy)));
}

bool QualifiesForFlip3DProxyWindow(HWND hwnd, LONG_PTR style, LONG_PTR exStyle)
{
    if (hwnd == nullptr || hwnd == GetDesktopWindow())
    {
        return false;
    }

    if ((style & WS_CHILD) != 0 || (style & WS_VISIBLE) == 0)
    {
        return false;
    }

    // uDWM: the Shell desktop window is explicitly allowed even with
    // disqualifying exstyle (it's the Progman window behind the icons).
    bool isShellDesktop = (hwnd == GetShellWindow());

    if (!isShellDesktop
        && (exStyle & kFlip3DDisqualifyExStyleMask) != 0
        && (exStyle & WS_EX_APPWINDOW) == 0)
    {
        return false;
    }

    if (!isShellDesktop
        && (exStyle & WS_EX_APPWINDOW) == 0
        && GetWindow(hwnd, GW_OWNER) != nullptr)
    {
        return false;
    }

    DWORD flip3DPolicy = kFlip3DPolicyDefault;
    if (TryGetFlip3DWindowPolicy(hwnd, flip3DPolicy) && flip3DPolicy != kFlip3DPolicyDefault)
    {
        return false;
    }

    return true;
}


RECT BuildFinalMinRect(const RECT &minimizeRect, float aspectRatio)
{
    RECT result = minimizeRect;
    const float minimizeWidth = static_cast<float>(std::max(0L, minimizeRect.right - minimizeRect.left));
    const float finalWidth = minimizeWidth * gFinalMinRectWidthPercentage;
    const float finalHeight = finalWidth * aspectRatio;
    const float finalLeft = static_cast<float>(minimizeRect.left) + (finalWidth * gFinalMinRectLeftPercentage);
    const float finalTop = static_cast<float>(minimizeRect.top) - finalHeight;

    result.left = static_cast<LONG>(std::lround(finalLeft));
    result.top = static_cast<LONG>(std::lround(finalTop));
    result.right = static_cast<LONG>(std::lround(finalLeft + finalWidth));
    result.bottom = static_cast<LONG>(std::lround(finalTop + finalHeight));
    return result;
}

BOOL CALLBACK CollectFlip3DWindowRects(HWND hwnd, LPARAM lParam)
{
    auto *context = reinterpret_cast<WindowLayoutCaptureContext *>(lParam);
    if (!context || hwnd == context->skipHwnd || !IsWindowVisible(hwnd))
    {
        return TRUE;
    }

    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!QualifiesForFlip3DProxyWindow(hwnd, style, exStyle))
    {
        return TRUE;
    }

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
    {
        return TRUE;
    }
    
    wchar_t className[256];
    GetClassNameW(hWnd, className, 256);
    if (wcscmp(exeName, L"SnippingTool.exe") == 0 && wcscmp(className, L"XamlWindow") == 0) {
        return TRUE; 
    }

    const bool isMinimized = IsIconic(hwnd) != FALSE;

    // Get the window's *own* monitor work area (not the primary monitor).
    RECT winWorkArea = context->workArea;
    if (const HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST))
    {
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hMon, &mi))
            winWorkArea = mi.rcWork;
    }

    RECT targetBounds = {};
    if (isMinimized)
    {
        WINDOWPLACEMENT placement = {};
        placement.length = sizeof(placement);
        if (GetWindowPlacement(hwnd, &placement) && !IsRectEmpty(&placement.rcNormalPosition))
        {
            // rcNormalPosition is in workspace coordinates.
            targetBounds = placement.rcNormalPosition;
            OffsetRect(&targetBounds, winWorkArea.left, winWorkArea.top);

            // If WPF_RESTORETOMAXIMIZED is set the window will restore to
            // maximised, not to rcNormalPosition.  Use the work area instead.
            if (placement.flags & WPF_RESTORETOMAXIMIZED)
                targetBounds = winWorkArea;
        }
    }

    if (IsRectEmpty(&targetBounds)
        && FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &targetBounds, sizeof(targetBounds))))
    {
        if (!GetWindowRect(hwnd, &targetBounds))
            return TRUE;
    }

    RECT targetClipped = {};
    if (!IntersectRect(&targetClipped, &targetBounds, &winWorkArea))
        return TRUE;

    if ((targetClipped.right - targetClipped.left) < 80 || (targetClipped.bottom - targetClipped.top) < 80)
        return TRUE;

    CapturedWindowLayout layout = {};
    layout.targetRect   = targetClipped;
    layout.monitorWork  = winWorkArea;
    layout.isMinimized  = isMinimized;
    layout.hwnd         = hwnd;

    if (isMinimized)
    {
        RECT minimizeRect = {};
        const auto getWindowMinimizeRect = ResolveGetWindowMinimizeRect();
        if (getWindowMinimizeRect != nullptr && getWindowMinimizeRect(hwnd, &minimizeRect) && !IsRectEmpty(&minimizeRect))
        {
            const float width = static_cast<float>(std::max(1L, targetClipped.right - targetClipped.left));
            const float height = static_cast<float>(std::max(1L, targetClipped.bottom - targetClipped.top));
            layout.originalRect = BuildFinalMinRect(minimizeRect, height / width);
        }
    }

    if (IsRectEmpty(&layout.originalRect))
    {
        layout.originalRect = targetClipped;
    }

    context->layouts.push_back(layout);
    return TRUE;
}

std::vector<CapturedWindowLayout> CapturePrimaryMonitorWindowRects(size_t limit, HWND skipHwnd)
{
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    const HMONITOR monitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitorInfo))
    {
        return {};
    }

    WindowLayoutCaptureContext context = {};
    context.workArea = monitorInfo.rcWork;
    context.skipHwnd = skipHwnd;
    EnumWindows(&CollectFlip3DWindowRects, reinterpret_cast<LPARAM>(&context));

    if (context.layouts.size() > limit)
    {
        context.layouts.resize(limit);
    }

    return context.layouts;
}

// ============================================================================
// D3D shader compilation
// ============================================================================
HRESULT CompileShader(const char *source, const char *entryPoint, const char *target, ComPtr<ID3DBlob> &blob)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompile(
        source,
        std::strlen(source),
        nullptr,
        nullptr,
        nullptr,
        entryPoint,
        target,
        flags,
        0,
        &blob,
        &errors);

    if (FAILED(hr) && errors)
    {
        OutputDebugStringA(static_cast<const char *>(errors->GetBufferPointer()));
    }

    return hr;
}
