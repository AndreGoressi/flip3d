cmake_minimum_required(VERSION 3.20)

project(flip3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(flip3d WIN32
    src/main.cpp
    src/Config.cpp
    src/Capture.cpp
    src/WindowCapture.cpp
    src/Flip3DApp.cpp
)

target_compile_definitions(flip3d PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(flip3d PRIVATE
        /W4
        /EHsc
        /permissive-
        /utf-8
    )
    # (C++/WinRT bridge removed — CreateFromVisual is now pure ABI)
endif()

target_link_libraries(flip3d PRIVATE
    d3d11
    d3dcompiler
    dxgi
    dwmapi
    dcomp
    d2d1
    runtimeobject
    windowsapp
    windowsapp
    user32
    gdi32
)