cmake_minimum_required(VERSION 3.8)

project(imgui)

# Dear ImGui 1.92.1, docking branch (https://github.com/ocornut/imgui), MIT license, see LICENSE.txt.
# The core files are the IX-Ray editors' copy (ixray-team/ixray-1.6-stcop, commit 09c1a3de9), so that the game
# and the level editor SDK (code/editors) share one ImGui. IX-Ray's changes to upstream v1.92.1-docking:
# - imgui.h: IMGUI_API exports when IMGUI_DLL is defined; ImTextureID is void*; CheckboxFlags(ImU64*).
# - imgui_widgets.cpp: icon layout in BeginMenuEx / MenuItemEx.
# - imgui.cpp, imgui_draw.cpp: two asserts commented out (cursor past window bounds, inverted clip rect).
# - imconfig.h: IMGUI_INCLUDE_IMGUI_USER_H; imgui_user.* and imgui_ixr.cpp are IX-Ray's additions.
# The backends (imgui_impl_*) are upstream v1.92.1-docking, unmodified.
#
# Game: the core is a static library linked into xrEngine and into every renderer that draws it. Each module
# has its own copy, and the renderers are pointed at the context the engine created (ImGui::SetCurrentContext).
# The backends are separate static libraries so that they build without the engine's precompiled headers.
# Editors: the same core as one DLL (imgui_editor), shared by all editor modules.

set(IMGUI_CORE_SOURCES imconfig.h
                       imgui.h
                       imgui_internal.h
                       imgui_user.h
                       imstb_rectpack.h
                       imstb_textedit.h
                       imstb_truetype.h
                       imgui.cpp
                       imgui_draw.cpp
                       imgui_tables.cpp
                       imgui_widgets.cpp
                       imgui_user.cpp
                       imgui_ixr.cpp)

add_library(imgui STATIC ${IMGUI_CORE_SOURCES})
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

# Direct3D 10 renderer backend (R3)
add_library(imgui_dx10 STATIC imgui_impl_dx10.h imgui_impl_dx10.cpp)
target_link_libraries(imgui_dx10 PUBLIC imgui)

# Direct3D 11 renderer backend (R4)
add_library(imgui_dx11 STATIC imgui_impl_dx11.h imgui_impl_dx11.cpp)
target_link_libraries(imgui_dx11 PUBLIC imgui)

if (BUILD_EDITORS)
    add_library(imgui_editor SHARED ${IMGUI_CORE_SOURCES} imgui_demo.cpp)
    target_include_directories(imgui_editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(imgui_editor PUBLIC IMGUI_DLL PRIVATE IMGUI_EXPORTS)
endif()
