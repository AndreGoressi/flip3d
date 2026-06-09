cmake_minimum_required(VERSION 3.20)

project(flip3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# Flip3D Executable
# ---------------------------------------------------------
add_executable(flip3d WIN32
    src/main.cpp
    src/Config.cpp
    src/Capture.cpp
    src/WindowCapture.cpp
    src/Flip3DWindow.cpp

    # FSR2 Core
    src/ffx-fsr2-api/ffx_fsr2.cpp
    src/ffx-fsr2-api/ffx_assert.cpp
)

# ---------------------------------------------------------
# Include directories
# ---------------------------------------------------------
target_include_directories(flip3d PRIVATE
    src
    src/ffx-fsr2-api
)

# ---------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------
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
endif()

# ---------------------------------------------------------
# Linker libraries
# ---------------------------------------------------------
target_link_libraries(flip3d PRIVATE
    d3d11
    d3dcompiler
    dxgi
    dwmapi
    dcomp
    d2d1
    runtimeobject
    windowsapp
    user32
    gdi32
)
