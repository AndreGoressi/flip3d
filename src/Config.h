#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <list>
#include <memory>

class WindowCapture;
#include <DirectXMath.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ============================================================================
// Compile-time constants
// ============================================================================
// Actual uDWM binary values (IDA: .data segment)
constexpr float kEnterExitDurationSec = 0.25f;       // g_secEnterExitViewDuration = 0.25
constexpr float kRotateListDurationSec = 0.175f;      // g_secRotateListDuration = 0.175
constexpr float kRotateToHomeMaxDurationSec = 0.75f;   // g_secRotateToHomeMaxDuration = 0.75
constexpr float kNearPlaneEdgeSize = 1.15f;
constexpr float kNearPlaneDistance = 1.0f;
constexpr float kProxyPathScale = 1.0f;
constexpr float kProxyCardScale = 1.0f;
constexpr XMFLOAT2 kProxySceneOffset = {0.0f, 0.0f};
constexpr float kFinalMinRectWidthPercentage = 0.6f;
constexpr float kFinalMinRectLeftPercentage = 0.3f;
constexpr DWORD kFlip3DPolicyAttribute = 8;
constexpr DWORD kFlip3DPolicyDefault = 0;
constexpr LONG_PTR kFlip3DDisqualifyExStyleMask = 0x08000080;
constexpr wchar_t kDwmPreferencePath[] = L"Software\\Microsoft\\Windows\\DWM";
constexpr int kInitialWidth = 0; //1600
constexpr int kInitialHeight = 0; //900 
constexpr int kMaxVisibleCards = 10;
constexpr size_t kMaxProxyCards = 24;
constexpr wchar_t kRenderClassName[] = L"Flip3DCore";
constexpr wchar_t kTitle[] = L"Flip3D";
constexpr std::array<float, 3> kNormalizationBezier = {1.0f, 0.85f, 0.75f};

constexpr XMFLOAT3 kBezierControls[] = {
    {-2.0f, 0.5f, -2.25f},
    {-1.8f, 0.55f, -1.25f},
    {-1.45f, -0.1f, -0.3f},
};

constexpr XMFLOAT3 kCameraFinalTranslate = {-1.1f, 0.35f, 0.35f};
constexpr XMFLOAT3 kCameraFinalRotate = {
    XMConvertToRadians(5.0f),
    XMConvertToRadians(25.0f),
    XMConvertToRadians(3.5f),
};

// ============================================================================
// Mutable globals (loaded from registry)
// ============================================================================
extern float gEnterExitDurationSec;
extern float gRotateListDurationSec;
extern float gRotateToHomeMaxDurationSec;
extern float gNearPlaneEdgeSize;
extern float gFinalMinRectWidthPercentage;
extern float gFinalMinRectLeftPercentage;
extern int gMaxVisibleCards;
extern std::array<float, 3> gNormalizationBezier;
extern std::array<XMFLOAT3, 3> gBezierControls;
extern XMFLOAT3 gCameraFinalTranslate;
extern XMFLOAT3 gCameraFinalRotate;

// ============================================================================
// Template math utilities (must precede Timeline which uses Lerp)
// ============================================================================
template <typename T>
T Lerp(const T &start, const T &end, float t)
{
    return start + ((end - start) * t);
}

inline XMFLOAT3 Lerp(const XMFLOAT3 &start, const XMFLOAT3 &end, float t)
{
    return {
        Lerp(start.x, end.x, t),
        Lerp(start.y, end.y, t),
        Lerp(start.z, end.z, t),
    };
}

inline XMFLOAT2 Lerp(const XMFLOAT2 &start, const XMFLOAT2 &end, float t)
{
    return {
        Lerp(start.x, end.x, t),
        Lerp(start.y, end.y, t),
    };
}

// ============================================================================
// Enums
// ============================================================================
enum class InterpolationMode
{
    Linear,
    Cubic,
};

// uDWM CFlip3D::Flip3DState — exact values verified from the binary
// (ExitRepeatedRotate=2, Exit=3 — ExitRepeatedRotate precedes Exit numerically)
enum class ViewState
{
    Inactive = 0,
    Enter = 1,
    ExitRepeatedRotate = 2,
    Exit = 3,
    Interactive = 4,
    InteractiveRepeatedRotate = 5,
};

// ============================================================================
// Data structures
// ============================================================================
struct Timeline
{
    float duration = 0.0f;
    float elapsed = 0.0f;
    float startValue = 0.0f;
    float endValue = 0.0f;
    InterpolationMode mode = InterpolationMode::Linear;
    bool active = false;

    void Restart(float start, float end, float seconds, InterpolationMode interpolation)
    {
        startValue = start;
        endValue = end;
        duration = std::max(seconds, 0.0001f);
        elapsed = 0.0f;
        mode = interpolation;
        active = true;
    }

    void Update(float deltaSeconds)
    {
        if (!active)
        {
            return;
        }

        elapsed = std::min(elapsed + deltaSeconds, duration);
        if (elapsed >= duration)
        {
            active = false;
        }
    }

    float Value() const
    {
        return Lerp(startValue, endValue, Progress());
    }

    // Raw linear value (0..1) — matches uDWM CTimeline::GetCurrentValue().
    // Used for opacity/z-index computations that must match uDWM exactly.
    float RawValue() const
    {
        if (duration <= 0.0f)
            return 1.0f;
        return std::clamp(elapsed / duration, 0.0f, 1.0f);
    }

    float Progress() const
    {
        const float linearT = RawValue();
        if (mode == InterpolationMode::Cubic)
        {
            return linearT * linearT * (3.0f - (2.0f * linearT));
        }
        return linearT;
    }
};

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 uv;
};

struct FrameConstants
{
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT4   washParams;
    DirectX::XMFLOAT4   viewport;
};

struct ObjectConstants
{
    XMFLOAT4X4 world;
    XMFLOAT4 color;
    XMFLOAT4 accent;
    XMFLOAT4 flags;
};

struct CardModel
{
    XMFLOAT4 color;
    float aspectRatio = 16.0f / 10.0f;
    float sourceOccupancy = 0.70f;
    XMFLOAT2 originalWorldPosition = {0.0f, 0.0f};
    XMFLOAT2 originalWorldSize = {0.0f, 0.0f};
    XMFLOAT2 targetWorldSize = {0.0f, 0.0f};
    bool isMinimized = false;
    bool hasOriginalRect = false;
    HWND hwnd = nullptr;
    ID3D11ShaderResourceView *captureSRV = nullptr;
    std::unique_ptr<WindowCapture> capture;
};

struct DrawItem
{
    XMFLOAT4X4 world;
    XMFLOAT4 color;
    XMFLOAT4 accent;
    XMFLOAT4 flags{};   // x = isMinimized (1.0/0.0)
    float depth = 0.0f;
    int cardPosition = -1;
};
struct DrawBuildContext
{
    int countInt = 0;
    float count = 0.0f;
    float front = 0.0f;
    int visibleCount = 0;
    float maxWindows = 5.0f;
    float monitorAspectRatio = 1.0f;
    bool hasHiddenQualifiedWindows = false;
    bool isRotating = false;
    int rotationSteps = 0;
    float rotationProgress = 1.0f;
    float hiddenWindowFlipOutProgress = 1.0f;
    float hiddenWindowFlipInProgress = 1.0f;
    int startFrontIndex = 0;
    int endFrontIndex = 0;
    bool useOriginalHiddenWindowRotation = false;
    int rotatingInCardIndex = -1;
    int rotatingOutCardIndex = -1;
    int windowBeingFlippedCardIndex = -1;
};

struct VisibleCardStructure
{
    size_t cardPosition = 0;     // position in the linked list (0 = front)
    int startSlot = 0;
    int endSlot = 0;
    bool isCycleCard = false;
    bool isBoundaryIncomingCard = false;
    bool isIncomingCard = false;
    bool isOutgoingCard = false;
};

struct CardAnimationState
{
    float logicalRelative = 0.0f;
    float pathRelative = 0.0f;
    float visualPosition = 0.0f;
    float startRelativeForOpacity = 0.0f;
    float endRelativeForOpacity = 0.0f;
    bool useDirectSteadyOpacity = false;
    float directSteadyOpacity = 1.0f;
};

struct CardWorldState
{
    XMFLOAT4X4 world = {};
    float depth = 0.0f;
};

struct CapturedWindowLayout
{
    RECT originalRect = {};
    RECT targetRect = {};
    RECT monitorWork = {};  // work area of the monitor this window is on
    bool isMinimized = false;
    HWND hwnd = nullptr; // for creating WindowCapture later
};

struct WindowLayoutCaptureContext
{
    RECT workArea = {};
    HWND skipHwnd = nullptr;
    std::vector<CapturedWindowLayout> layouts;
};

using GetWindowMinimizeRectFn = BOOL (WINAPI *)(HWND, LPRECT);

// ============================================================================
// Additional inline math utilities
// ============================================================================
inline float EvaluateNormalizationScale(float occupancy)
{
    occupancy = std::clamp(occupancy, 0.0f, 1.0f);

    const float inverse = 1.0f - occupancy;
    return (inverse * inverse * gNormalizationBezier[0])
        + ((2.0f * gNormalizationBezier[1] * occupancy) * inverse)
        + (occupancy * occupancy * gNormalizationBezier[2]);
}

inline XMFLOAT2 BuildWorldSizeFromOccupancy(float occupancy, float windowAspectRatio, float monitorAspectRatio)
{
    occupancy = std::clamp(occupancy, 0.05f, 1.0f);
    monitorAspectRatio = std::max(monitorAspectRatio, 0.0001f);
    windowAspectRatio = std::max(windowAspectRatio, 0.0001f);

    if (windowAspectRatio >= monitorAspectRatio)
    {
        return {
            occupancy,
            occupancy * (monitorAspectRatio / windowAspectRatio),
        };
    }

    return {
        occupancy * (windowAspectRatio / monitorAspectRatio),
        occupancy,
    };
}

inline XMFLOAT3 BuildDesktopWindowPosition(size_t index, size_t count, const XMFLOAT2 &originalSize)
{
    const size_t columns = (count >= 10) ? 4 : ((count >= 5) ? 3 : 2);
    const size_t rows = std::max<size_t>(1, (count + columns - 1) / columns);

    const float desktopLeft = -0.46f;
    const float desktopTop = 0.39f;
    const float desktopWidth = 0.92f;
    const float desktopHeight = 0.78f;

    const size_t column = index % columns;
    const size_t row = index / columns;

    const float cellWidth = desktopWidth / static_cast<float>(columns);
    const float cellHeight = desktopHeight / static_cast<float>(rows);
    const float sizeHeight = -originalSize.y;

    const float horizontalPadding = std::max(0.0f, cellWidth - originalSize.x);
    const float verticalPadding = std::max(0.0f, cellHeight - sizeHeight);
    const float cascadeOffset = 0.012f * static_cast<float>((column + row) % 3);

    return {
        desktopLeft + (static_cast<float>(column) * cellWidth) + (horizontalPadding * 0.5f) + cascadeOffset,
        desktopTop - (static_cast<float>(row) * cellHeight) - (verticalPadding * 0.5f) - (cascadeOffset * 0.8f),
        0.0f,
    };
}

inline float WrapPositive(float value, float modulus)
{
    if (modulus <= 0.0f)
    {
        return 0.0f;
    }

    float wrapped = std::fmod(value, modulus);
    return (wrapped < 0.0f) ? (wrapped + modulus) : wrapped;
}

inline int WrapPositiveInt(int value, int modulus)
{
    if (modulus <= 0)
    {
        return 0;
    }

    const int wrapped = value % modulus;
    return (wrapped < 0) ? (wrapped + modulus) : wrapped;
}

inline XMFLOAT3 EvaluateBezier(float t)
{
    const float s = 1.0f - t;
    const float b0 = s * s;
    const float b1 = 2.0f * s * t;
    const float b2 = t * t;

    return {
        (b0 * gBezierControls[0].x) + (b1 * gBezierControls[1].x) + (b2 * gBezierControls[2].x),
        (b0 * gBezierControls[0].y) + (b1 * gBezierControls[1].y) + (b2 * gBezierControls[2].y),
        (b0 * gBezierControls[0].z) + (b1 * gBezierControls[1].z) + (b2 * gBezierControls[2].z),
    };
}
