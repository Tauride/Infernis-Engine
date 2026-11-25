# Level editor SDK: the IX-Ray editors ported to Infernis (see README.md).
# Built only with -DBUILD_EDITORS=ON. Nothing here is linked by the game or the compilers.

if (CMAKE_VERSION VERSION_LESS 3.24)
    message(FATAL_ERROR "BUILD_EDITORS needs CMake 3.24 or newer (FetchContent with DOWNLOAD_EXTRACT_TIMESTAMP); this is ${CMAKE_VERSION}")
endif()

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

# Dear ImGui: imgui_editor is defined in code/3rd-party/imgui, as a DLL built from the same 1.92.1-docking sources
# as the game's static imgui library, so that every editor module shares one ImGui context.
set_target_properties(imgui_editor PROPERTIES FOLDER editors/3rd-party)

# xrEUI: editor window shell (ImGui on SDL3 + D3D9), theme, fonts, gizmo, plots, node editor.
file(GLOB_RECURSE EUI_SOURCES CONFIGURE_DEPENDS xrEUI/*.cpp xrEUI/*.h)
list(APPEND EUI_SOURCES compat/editor_compat.h compat/xr_path.h compat/xr_delegate.h
                        ${CMAKE_SOURCE_DIR}/code/3rd-party/imgui/imgui_impl_dx9.cpp)
add_library(xrEUI SHARED ${EUI_SOURCES})
target_include_directories(xrEUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/xrEUI ${CMAKE_CURRENT_SOURCE_DIR})
# The ImGui backends (SDL3, DX9) are compiled into xrEUI and exported from it (IMGUI_IMPL_API).
target_compile_definitions(xrEUI PRIVATE XREUI_EXPORTS _WINDOWS _USRDLL "IMGUI_IMPL_API=__declspec(dllexport)"
                                 INTERFACE "IMGUI_IMPL_API=__declspec(dllimport)" PUBLIC XR_EDITOR_NEW
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

# lunasvg, for the editor icons (xrECore). Same release IX-Ray uses, pinned by hash.
FetchContent_Declare(lunasvg
    URL https://github.com/sammycage/lunasvg/archive/refs/tags/v3.4.0.tar.gz
    URL_HASH SHA256=6ef03a7471fe4288def39e9fe55dfe2dbfb4041792d81a7e07e362f649cc7a0b
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(lunasvg)
foreach(LUNASVG_TARGET lunasvg plutovg)
    if (TARGET ${LUNASVG_TARGET})
        set_target_properties(${LUNASVG_TARGET} PROPERTIES FOLDER editors/3rd-party)
    endif()
endforeach()

# xrECore: the editor core (device, camera, scene objects, meshes, library, thumbnails). Built on Infernis's
# engine (xrEngineCore.dll, the engine sources as a DLL) and on Infernis's DX9 renderer sources, compiled here
# with _EDITOR as the renderer of the editor viewport.
set(ENGINE_DIR ${CMAKE_SOURCE_DIR}/code/engine)
file(GLOB_RECURSE ECORE_SOURCES CONFIGURE_DEPENDS xrECore/*.cpp xrECore/*.h)
# Not built yet: skinned-mesh export and bone-shape fitting need MagicSoftware and DirectXMesh (Actor Editor stage).
list(FILTER ECORE_SOURCES EXCLUDE REGEX "/xrECore/(WildMagic/|Editor/EditObjectSkin[.]cpp|Editor/ExportSkeleton)")
# Not built yet: the particle editor's library code (Particle Editor stage) and the image editor form, which uses
# IX-Ray's RedImage (comes with the DXTUtils work).
list(FILTER ECORE_SOURCES EXCLUDE REGEX "/xrECore/Editor/(ParticleEditorImpl|UIImageEditorForm)[.]cpp$")
# The material library editing subclass (Shader Editor stage); the core uses the plain library (GMLib).
list(FILTER ECORE_SOURCES EXCLUDE REGEX "/xrECore/Engine/XrGameMaterialLibraryEditors[.](cpp|h)$")
# The material library itself is Infernis's (format authority), compiled here in its editor layout (GM_NON_GAME:
# sounds, particles and marks as names) instead of IX-Ray's copy.
list(FILTER ECORE_SOURCES EXCLUDE REGEX "/xrECore/Engine/GameMtlLib[.]cpp$")
# Texture parameter properties: Infernis's ETextureParams.cpp has them (GSC editor code), not IX-Ray's copy.
list(FILTER ECORE_SOURCES EXCLUDE REGEX "/xrECore/Editor/EditorTextureParams[.]cpp$")
# The light preview (EDITOR_LIGHT_PREVIEW, Level Editor stage).
list(FILTER ECORE_SOURCES EXCLUDE REGEX "/xrECore/Render/LightSpot[.](cpp|h)$")
set(ECORE_RENDER_SOURCES
    ${ENGINE_DIR}/xrRenderCommon/Animation.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_BmmD.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_Editor_Selection.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_Editor_Wire.cpp
    "${ENGINE_DIR}/xrRenderCommon/Blender_Lm(EbB).cpp"
    ${ENGINE_DIR}/xrRenderCommon/Blender_Model_EbB.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_Particle.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_Recorder_R2.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_Recorder_StandartBinding.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_Screen_SET.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_detail_still.cpp
    ${ENGINE_DIR}/xrRenderCommon/Blender_tree.cpp
    ${ENGINE_DIR}/xrRenderCommon/DetailModel.cpp
    ${ENGINE_DIR}/xrRenderCommon/ETextureParams.cpp
    ${ENGINE_DIR}/xrRenderCommon/FBasicVisual.cpp
    ${ENGINE_DIR}/xrRenderCommon/FHierrarhyVisual.cpp
    ${ENGINE_DIR}/xrRenderCommon/FProgressive.cpp
    ${ENGINE_DIR}/xrRenderCommon/FSkinned.cpp
    ${ENGINE_DIR}/xrRenderCommon/FVisual.cpp
    ${ENGINE_DIR}/xrRenderCommon/ModelPool.cpp
    ${ENGINE_DIR}/xrRenderCommon/PSLibrary.cpp
    ${ENGINE_DIR}/xrRenderCommon/ParticleEffect.cpp
    ${ENGINE_DIR}/xrRenderCommon/ParticleEffectDef.cpp
    ${ENGINE_DIR}/xrRenderCommon/ParticleGroup.cpp
    ${ENGINE_DIR}/xrRenderCommon/R_Backend.cpp
    ${ENGINE_DIR}/xrRenderCommon/R_Backend_DBG.cpp
    ${ENGINE_DIR}/xrRenderCommon/R_Backend_Runtime.cpp
    ${ENGINE_DIR}/xrRenderCommon/R_Backend_hemi.cpp
    ${ENGINE_DIR}/xrRenderCommon/R_Backend_tree.cpp
    ${ENGINE_DIR}/xrRenderCommon/R_Backend_xform.cpp
    ${ENGINE_DIR}/xrRenderCommon/R_DStreams.cpp
    ${ENGINE_DIR}/xrRenderCommon/ResourceManager.cpp
    ${ENGINE_DIR}/xrRenderCommon/ResourceManager_Loader.cpp
    ${ENGINE_DIR}/xrRenderCommon/ResourceManager_Reset.cpp
    ${ENGINE_DIR}/xrRenderCommon/SH_Atomic.cpp
    ${ENGINE_DIR}/xrRenderCommon/SH_Constant.cpp
    ${ENGINE_DIR}/xrRenderCommon/SH_Matrix.cpp
    ${ENGINE_DIR}/xrRenderCommon/SH_RT.cpp
    ${ENGINE_DIR}/xrRenderCommon/SH_Texture.cpp
    ${ENGINE_DIR}/xrRenderCommon/Shader.cpp
    ${ENGINE_DIR}/xrRenderCommon/SkeletonAnimated.cpp
    ${ENGINE_DIR}/xrRenderCommon/SkeletonCustom.cpp
    ${ENGINE_DIR}/xrRenderCommon/SkeletonRigid.cpp
    ${ENGINE_DIR}/xrRenderCommon/SkeletonX.cpp
    ${ENGINE_DIR}/xrRenderCommon/TextureDescrManager.cpp
    ${ENGINE_DIR}/xrRenderCommon/blenders/Blender.cpp
    ${ENGINE_DIR}/xrRenderCommon/blenders/Blender_Palette.cpp
    ${ENGINE_DIR}/xrRenderCommon/blenders/Blender_Recorder.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxApplicationRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxConsoleRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxDebugRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxEnvironmentRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxFontRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxLensFlareRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxObjectSpaceRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxRainRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxRenderDeviceRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxRenderFactory.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxStatGraphRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxStatsRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxThunderboltDescRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxThunderboltRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxUIRender.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxUISequenceVideoItem.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxUIShader.cpp
    ${ENGINE_DIR}/xrRenderCommon/dxWallMarkArray.cpp
    ${ENGINE_DIR}/xrRenderCommon/r_constants.cpp
    ${ENGINE_DIR}/xrRenderCommon/stats_manager.cpp
    ${ENGINE_DIR}/xrRenderCommon/tga.cpp
    ${ENGINE_DIR}/xrRenderCommon/tss_def.cpp
    ${ENGINE_DIR}/xrRenderCommon/xrRender_console.cpp
    ${ENGINE_DIR}/xrRenderDX9/dx9r_constants_cache.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/BlenderDefault.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/Blender_Blur.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/Blender_LaEmB.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/Blender_Model.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/Blender_Screen_GRAY.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/Blender_Shadow_World.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/Blender_Vertex.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/Blender_Vertex_aref.cpp
    ${ENGINE_DIR}/xrRenderPC_R1/Blender_default_aref.cpp
    ${ENGINE_DIR}/xrEngine/GameMtlLib.cpp
    ${CMAKE_SOURCE_DIR}/code/utils/common/NvMender2003/NVMeshMender.cpp
    ${CMAKE_SOURCE_DIR}/code/utils/ETools/object.cpp
    ${ENGINE_DIR}/xrEngine/ai_script_lua_debug.cpp
    ${ENGINE_DIR}/xrEngine/ai_script_lua_extension.cpp
    ${ENGINE_DIR}/xrRenderCommon/xr_effgamma.cpp
    ${ENGINE_DIR}/xrRenderCommon/HW.cpp
    ${ENGINE_DIR}/xrRenderCommon/HWCaps.cpp
    ${ENGINE_DIR}/xrRenderCommon/Texture.cpp
    ${ENGINE_DIR}/xrRenderCommon/ResourceManager_Resources.cpp
    ${ENGINE_DIR}/xrRenderCommon/ResourceManager_Scripting.cpp)
add_library(xrECore SHARED ${ECORE_SOURCES} ${ECORE_RENDER_SOURCES} Public/VIMP_Processor.cpp
                           Public/object_sliding.cpp)
# (IX-Ray's xrECore.rc only embeds IX-Ray splash images for its launcher; not used here.)
# 3rd-party/nlohmann: json.hpp 3.11.2 (MIT), the single header IX-Ray downloads; editor settings use it.
# Infernis's renderer sources include editor headers by bare name in _EDITOR mode (GSC style: fmesh.h,
# ParticleEffectActions.h, UI_ToolsCustom.h, ETools.h), hence the extra folders.
target_include_directories(xrECore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/xrECore ${DXSDK_INCLUDE_DIR}
                                          ${CMAKE_CURRENT_SOURCE_DIR}/3rd-party
                                   PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/xrECore/Editor ${CMAKE_CURRENT_SOURCE_DIR}/xrECore/Engine
                                           ${ENGINE_DIR}/xrEngine ${CMAKE_SOURCE_DIR}/code/utils/ETools
                                           ${CMAKE_SOURCE_DIR}/code/utils/xrQSlim/src
                                           ${CMAKE_SOURCE_DIR}/code/utils/common/NvMender2003)
target_precompile_headers(xrECore PRIVATE xrECore/stdafx.h)
# The engine's Lua helpers are compiled the way Infernis's R1 compiles them (its export define switches off the
# game-only parts), without the editor's precompiled header.
set_source_files_properties(${ENGINE_DIR}/xrEngine/ai_script_lua_debug.cpp ${ENGINE_DIR}/xrEngine/ai_script_lua_extension.cpp
                            PROPERTIES SKIP_PRECOMPILE_HEADERS ON COMPILE_DEFINITIONS XRRENDER_R1_EXPORTS)
target_link_directories(xrECore PUBLIC ${DXSDK_LIB_DIR}) # d3dx9, as for the renderers
target_compile_definitions(xrECore PRIVATE XRECORE_EXPORTS _WINDOWS _USRDLL PUBLIC _EDITOR)
target_link_libraries(xrECore PUBLIC xrEUI xrEProps xrEngineCore xrCore xrCDB xrSound xrPhysics xrParticles
                                     xrAPI luabind lunasvg xrQSlim d3d9 d3dx9 dinput8 dxguid)
set_target_properties(xrECore PROPERTIES FOLDER editors)
