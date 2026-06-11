cmake_minimum_required(VERSION 3.20)

project(flip3d LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# Flip3D Executable (Nur das reinrassige neue Fundament)
# ---------------------------------------------------------
add_executable(flip3d WIN32
    src/resources.rc
    src/winmain.cpp
    src/Config.cpp
    src/Capture.cpp
    src/WindowCapture.cpp
    src/Flip3DRenderer.cpp
    src/ShellOverlayContext.cpp
)

# ---------------------------------------------------------
# Include directories
# ---------------------------------------------------------
target_include_directories(flip3d PRIVATE
    src
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
    dxgi
    dcomp
    user32
    gdi32
    d3dcompiler
    runtimeobject
    dwmapi
    d2d1
)
