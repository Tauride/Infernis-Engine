# Level editor SDK: the IX-Ray editors ported to Infernis (see README.md).
# Built only with -DBUILD_EDITORS=ON. Nothing here is linked by the game or the compilers.

# The editor sources are C++23 (IX-Ray). The rest of Infernis stays C++17, so swap the standard for this
# directory only.
get_directory_property(EDITOR_COMPILE_OPTIONS COMPILE_OPTIONS)
list(REMOVE_ITEM EDITOR_COMPILE_OPTIONS /std:c++17)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "${EDITOR_COMPILE_OPTIONS}")
# /permissive (C++20 and later default to /permissive-) and the traditional preprocessor: Infernis's xrCore
# templates and its Boost copy need both.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/std:c++latest> $<$<COMPILE_LANGUAGE:CXX>:/permissive>
                    /wd4073 /wd4390 /wd4273 /wd4566 /wd4297 /wd4275 /wd4530)
# fmtlib 4 (pulled in by xrCore.h) calls std::allocator::allocate(n, hint), removed from C++20's library.
add_compile_definitions(_HAS_DEPRECATED_ALLOCATOR_MEMBERS=1 _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
                        _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS)

include(SDL3.cmake)

# Dear ImGui, docking branch with IX-Ray's additions (imgui_user.*, imgui_ixr.cpp). A DLL, so that every editor
# module shares one ImGui context. Separate from the game's code/3rd-party/imgui, which has no docking.
file(GLOB IMGUI_EDITOR_SOURCES CONFIGURE_DEPENDS 3rd-party/imgui/*.cpp 3rd-party/imgui/*.h)
add_library(imgui_editor SHARED ${IMGUI_EDITOR_SOURCES})
target_include_directories(imgui_editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/imgui)
target_compile_definitions(imgui_editor PUBLIC IMGUI_DLL PRIVATE IMGUI_EXPORTS)
target_link_libraries(imgui_editor PUBLIC xrCore)
set_target_properties(imgui_editor PROPERTIES FOLDER editors/3rd-party)

# xrEUI: editor window shell (ImGui on SDL3 + D3D9), theme, fonts, gizmo, plots, node editor.
file(GLOB_RECURSE EUI_SOURCES CONFIGURE_DEPENDS xrEUI/*.cpp xrEUI/*.h)
list(APPEND EUI_SOURCES compat/editor_compat.h compat/xr_path.h compat/xr_delegate.h)
add_library(xrEUI SHARED ${EUI_SOURCES})
target_include_directories(xrEUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/xrEUI ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(xrEUI PRIVATE XREUI_EXPORTS _WINDOWS _USRDLL PUBLIC XR_EDITOR_NEW
                           IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(xrEUI PUBLIC xrCore imgui_editor SDL3::SDL3 d3d9)
set_target_properties(xrEUI PROPERTIES FOLDER editors)

# xrEProps: property grids, item lists, choosers, small forms.
file(GLOB_RECURSE EPROPS_SOURCES CONFIGURE_DEPENDS xrEProps/*.cpp xrEProps/*.h xrEProps/*.H)
list(APPEND EPROPS_SOURCES Public/PropertiesListTypes.h Public/PropertiesListHelper.h Public/xrEProps.h
                           Public/ItemListTypes.h Public/xrEPropsTypes.h Public/WaveForm.h)
# Not built yet: the bone views need CBone from the engine sources the editor core compiles (Actor Editor stage),
# and the dialog nodes use IX-Ray's tinyxml2-based XML API (Level Editor stage).
list(FILTER EPROPS_SOURCES EXCLUDE REGEX "/(NodeBuilder/BoneNodes|UIBoneView|NodeBuilder/DialogNode)\\.(cpp|h)$")
add_library(xrEProps SHARED ${EPROPS_SOURCES})
target_include_directories(xrEProps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/xrEProps)
# No _EDITOR here (IX-Ray sets it): Infernis's xrCore _EDITOR branches are stale (string_concatenations.h) and
# nothing in xrEProps needs them. The editor core stage has to repair them.
target_compile_definitions(xrEProps PRIVATE XREPROPS_EXPORTS _WINDOWS)
target_link_libraries(xrEProps PUBLIC xrEUI xrXMLParser)
set_target_properties(xrEProps PROPERTIES FOLDER editors)

# Stage 1 check: a window with the editor shell and a property grid.
add_executable(xrEditorUITest WIN32 test/xrEditorUITest.cpp)
target_link_libraries(xrEditorUITest PRIVATE xrEProps)
set_target_properties(xrEditorUITest PROPERTIES FOLDER editors)
