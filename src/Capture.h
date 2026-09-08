#pragma once

#include "Config.h"

// ============================================================================
// Registry preference helpers
// ============================================================================
bool TryReadDwmDwordPreference(const wchar_t *name, DWORD &value);
bool TryReadDwmFloatPreference(const wchar_t *name, float &value);
void LoadFlip3DPreferences();

// ============================================================================
// Window capture / enumeration
// ============================================================================
GetWindowMinimizeRectFn ResolveGetWindowMinimizeRect();
bool TryGetFlip3DWindowPolicy(HWND hwnd, DWORD &policy);
bool QualifiesForFlip3DProxyWindow(HWND hwnd, LONG_PTR style, LONG_PTR exStyle);
RECT BuildFinalMinRect(const RECT &minimizeRect, float aspectRatio);
BOOL CALLBACK CollectFlip3DWindowRects(HWND hwnd, LPARAM lParam);
std::vector<CapturedWindowLayout> CapturePrimaryMonitorWindowRects(size_t limit, HWND skipHwnd);
bool TryBuildFlip3DWindowLayout(HWND hwnd, const RECT &primaryWorkArea, HWND skipHwnd, CapturedWindowLayout &outLayout);
bool CaptureSingleWindowLayout(HWND hwnd, HWND skipHwnd, CapturedWindowLayout &outLayout);

// ============================================================================
// D3D shader compilation
// ============================================================================
HRESULT CompileShader(const char *source, const char *entryPoint, const char *target, ComPtr<ID3DBlob> &blob);
