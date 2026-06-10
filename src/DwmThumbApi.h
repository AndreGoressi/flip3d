#pragma once

#include <windows.h>
#include <dwmapi.h>
#include <dcomp.h>

#define DWM_TNP_FREEZE            0x100000
#define DWM_TNP_ENABLE3D          0x4000000
#define DWM_TNP_DISABLE3D         0x8000000
#define DWM_TNP_FORCECVI          0x40000000
#define DWM_TNP_DISABLEFORCECVI   0x80000000
#define DWM_TNP_SOURCECLIENTAREAONLY   0x00000010

// ============================================================================
// Private DWM thumbnail API types (loaded by ordinal from dwmapi.dll)
// ============================================================================

enum THUMBNAIL_TYPE
{
    TT_DEFAULT = 0x0,
    TT_SNAPSHOT = 0x1,
    TT_ICONIC = 0x2,
    TT_BITMAPPENDING = 0x3,
    TT_BITMAP = 0x4,
};

// Ordinals from ADeltaX research — may differ between Windows builds.
// Currently validated for Windows 11 22H2 (22621) / 24H2 (26100).
#define DWM_ORD_QUERY_THUMBNAIL_TYPE               114
#define DWM_ORD_CREATE_SHARED_THUMBNAIL_VISUAL     147
#define DWM_ORD_QUERY_WINDOW_THUMBNAIL_SOURCE_SIZE 162
// Pre-iron (deprecated)
#define DWM_ORD_CREATE_SHARED_VIRTUAL_DESKTOP_VISUAL  163
#define DWM_ORD_UPDATE_SHARED_VIRTUAL_DESKTOP_VISUAL  164

typedef HRESULT(WINAPI *DwmpCreateSharedThumbnailVisual_fn)(
    HWND hwndDestination,
    HWND hwndSource,
    DWORD dwThumbnailFlags,
    DWM_THUMBNAIL_PROPERTIES *pThumbnailProperties,
    void *pDCompDevice,
    void **ppVisual,
    HTHUMBNAIL *phThumbnailId);

typedef HRESULT(WINAPI *DwmpQueryWindowThumbnailSourceSize_fn)(
    HWND hwndSource,
    BOOL fSourceClientAreaOnly,
    SIZE *pSize);

typedef HRESULT(WINAPI *DwmpQueryThumbnailType_fn)(
    HTHUMBNAIL hThumbnailId,
    THUMBNAIL_TYPE *thumbType);

// ============================================================================
// Runtime loader — returns true if all required entry points are found.
// ============================================================================
struct DwmThumbApi
{
    HMODULE dwmapiLib = nullptr;

    DwmpCreateSharedThumbnailVisual_fn CreateSharedThumbnailVisual = nullptr;
    DwmpQueryWindowThumbnailSourceSize_fn QueryWindowThumbnailSourceSize = nullptr;
    DwmpQueryThumbnailType_fn QueryThumbnailType = nullptr;

    bool Load()
    {
        dwmapiLib = LoadLibraryW(L"dwmapi.dll");
        if (!dwmapiLib) return false;

        CreateSharedThumbnailVisual = (DwmpCreateSharedThumbnailVisual_fn)
            GetProcAddress(dwmapiLib, MAKEINTRESOURCEA(DWM_ORD_CREATE_SHARED_THUMBNAIL_VISUAL));
        QueryWindowThumbnailSourceSize = (DwmpQueryWindowThumbnailSourceSize_fn)
            GetProcAddress(dwmapiLib, MAKEINTRESOURCEA(DWM_ORD_QUERY_WINDOW_THUMBNAIL_SOURCE_SIZE));
        QueryThumbnailType = (DwmpQueryThumbnailType_fn)
            GetProcAddress(dwmapiLib, MAKEINTRESOURCEA(DWM_ORD_QUERY_THUMBNAIL_TYPE));

        return CreateSharedThumbnailVisual != nullptr
            && QueryWindowThumbnailSourceSize != nullptr
            && QueryThumbnailType != nullptr;
    }

    void Unload()
    {
        if (dwmapiLib) { FreeLibrary(dwmapiLib); dwmapiLib = nullptr; }
        CreateSharedThumbnailVisual = nullptr;
        QueryWindowThumbnailSourceSize = nullptr;
        QueryThumbnailType = nullptr;
    }
};
