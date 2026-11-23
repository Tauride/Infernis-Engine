cmake_minimum_required(VERSION 3.8)

project(imgui)

# Dear ImGui 1.93.0 (https://github.com/ocornut/imgui), MIT license, see LICENSE.txt.
# All files are upstream's, unmodified, except imgui_impl_dx10.*, which was written here
# from imgui_impl_dx11.* because the vendored copy came without a Direct3D 10 backend.
# The core is a static library linked into xrEngine and into every renderer that draws
# it: each module has its own copy, and the renderers are pointed at the context the
# engine created (ImGui::SetCurrentContext). The backends are separate static libraries
# so that they build without the engine's precompiled headers.

add_library(imgui STATIC imconfig.h
                         imgui.h
                         imgui_internal.h
                         imstb_rectpack.h
                         imstb_textedit.h
                         imstb_truetype.h
                         imgui.cpp
                         imgui_draw.cpp
                         imgui_tables.cpp
                         imgui_widgets.cpp)
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Win32 platform backend (xrEngine). Without the gamepad: the DirectX SDK's XInput.h
# comes first on the include path of the engine.
add_library(imgui_win32 STATIC imgui_impl_win32.h imgui_impl_win32.cpp)
target_compile_definitions(imgui_win32 PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)
target_link_libraries(imgui_win32 PUBLIC imgui)

# Direct3D 9 renderer backend (R1, R2)
add_library(imgui_dx9 STATIC imgui_impl_dx9.h imgui_impl_dx9.cpp)
target_include_directories(imgui_dx9 PRIVATE ${DXSDK_INCLUDE_DIR})
target_link_libraries(imgui_dx9 PUBLIC imgui)

# Direct3D 10 renderer backend (R3). Not an upstream file: see imgui_impl_dx10.h
add_library(imgui_dx10 STATIC imgui_impl_dx10.h imgui_impl_dx10.cpp)
target_link_libraries(imgui_dx10 PUBLIC imgui)

# Direct3D 11 renderer backend (R4)
add_library(imgui_dx11 STATIC imgui_impl_dx11.h imgui_impl_dx11.cpp)
target_link_libraries(imgui_dx11 PUBLIC imgui)
