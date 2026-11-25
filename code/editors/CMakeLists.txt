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
# The bone view and bone nodes are built by xrECore (they need its CBone). Not built yet: the dialog nodes use
# IX-Ray's tinyxml2-based XML API (Level Editor stage).
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

# FreeMagic (Magic Software), the part the skinned-mesh export uses for bone shapes (3rd-party/MagicSoftware).
file(GLOB FREEMAGIC_SOURCES CONFIGURE_DEPENDS 3rd-party/MagicSoftware/FreeMagic/Source/*.cpp)
add_library(FreeMagic_editor STATIC ${FREEMAGIC_SOURCES})
target_include_directories(FreeMagic_editor PUBLIC 3rd-party/MagicSoftware/FreeMagic/Include
                                            PRIVATE 3rd-party/MagicSoftware/FreeMagic/Source)
target_compile_options(FreeMagic_editor PRIVATE /W0)
set_target_properties(FreeMagic_editor PROPERTIES FOLDER editors/3rd-party)

# xrECore: the editor core (device, camera, scene objects, meshes, library, thumbnails). Built on Infernis's
# engine (xrEngineCore.dll, the engine sources as a DLL) and on Infernis's DX9 renderer sources, compiled here
# with _EDITOR as the renderer of the editor viewport.
set(ENGINE_DIR ${CMAKE_SOURCE_DIR}/code/engine)
file(GLOB_RECURSE ECORE_SOURCES CONFIGURE_DEPENDS xrECore/*.cpp xrECore/*.h)
# The bone view and its nodes are xrEProps sources, but they use CBone, whose editor layout only the editor core
# has (_EDITOR); they are compiled here.
list(APPEND ECORE_SOURCES xrEProps/UIBoneView.cpp xrEProps/UIBoneView.h xrEProps/NodeBuilder/BoneNodes.cpp
                          xrEProps/NodeBuilder/BoneNodes.h)
# Not built yet: the image editor form, which uses IX-Ray's RedImage (comes with the DXTUtils work).
list(FILTER ECORE_SOURCES EXCLUDE REGEX "/xrECore/Editor/UIImageEditorForm[.]cpp$")
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
                                     xrAPI luabind lunasvg xrQSlim FreeMagic_editor d3d9 d3dx9 dinput8 dxguid)
# Loaded at run time, so built with the core: the CPU pipeline (software skinning, bound by Engine.Initialize) and
# the texture compressor (DXTUtils).
add_dependencies(xrECore xrCPU_Pipe xrDXT)
set_target_properties(xrECore PROPERTIES FOLDER editors)

# --- Shader Editor (stage 3): shaders.xr, shaders_xrlc.xr, gamemtl.xr and senvironment.xr -------------------
file(GLOB SHADER_EDITOR_SOURCES CONFIGURE_DEPENDS ShaderEditor/*.cpp ShaderEditor/*.h)
# The sound environment library is not exported from xrSound.dll (GSC exported it only for its Borland editors), so
# the Shader Editor compiles that file itself; xrSound's headers want the Ogg Vorbis, OpenAL and (in _EDITOR mode)
# ETools headers.
add_executable(ShaderEditor WIN32 ${SHADER_EDITOR_SOURCES} ShaderEditor/ShaderEditor.rc ShaderEditor/ShaderEditor.manifest
                                  ${ENGINE_DIR}/xrSound/SoundRender_Environment.cpp)
set_source_files_properties(${ENGINE_DIR}/xrSound/SoundRender_Environment.cpp PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_include_directories(ShaderEditor PRIVATE ${CMAKE_SOURCE_DIR}/code/3rd-party/openal/include
                                                ${CMAKE_SOURCE_DIR}/code/utils/ETools)
target_precompile_headers(ShaderEditor PRIVATE ShaderEditor/stdafx.h)
target_compile_definitions(ShaderEditor PRIVATE _SHADER_EDITOR _WINDOWS)
target_link_libraries(ShaderEditor PRIVATE xrECore libogg libvorbis libvorbisfile)
set_target_properties(ShaderEditor PROPERTIES FOLDER editors)

# --- Particle Editor (stage 3): particles.xr, or the effect sources (.pe/.pg) under $game_particles$ ------------
# IX-Ray's particle animation curves (PAC, an IX-Ray extension of particles.xr) are left out: Infernis's particle
# format has no place for them. Their code stays behind EDITOR_PARTICLE_ANIM_CURVES (off).
file(GLOB PARTICLE_EDITOR_SOURCES CONFIGURE_DEPENDS ParticleEditor/*.cpp ParticleEditor/*.h)
add_executable(ParticleEditor WIN32 ${PARTICLE_EDITOR_SOURCES} ParticleEditor/ParticleEditor.rc
                              ParticleEditor/ParticleEditor.manifest) # the manifests make the editors DPI-aware
target_precompile_headers(ParticleEditor PRIVATE ParticleEditor/stdafx.h)
target_compile_definitions(ParticleEditor PRIVATE _PARTICLE_EDITOR _WINDOWS)
target_include_directories(ParticleEditor PRIVATE ${ENGINE_DIR}/xrParticles) # the editor's action classes
target_link_libraries(ParticleEditor PRIVATE xrECore)
set_target_properties(ParticleEditor PROPERTIES FOLDER editors)

# --- Actor Editor (stage 4): editable objects (.object), their skeletons and motions (.skl/.skls), export to .ogf
# and .omf in Infernis's (GSC's) format, and post-process effects (.ppe, through Infernis's own PostprocessAnimator,
# which keeps GSC's _PP_EDITOR_ branches).
file(GLOB ACTOR_EDITOR_SOURCES CONFIGURE_DEPENDS ActorEditor/*.cpp ActorEditor/*.h ActorEditor/CustomTools/*.cpp
                                                 ActorEditor/CustomTools/*.h)
add_executable(ActorEditor WIN32 ${ACTOR_EDITOR_SOURCES} ActorEditor/ActorEditor.rc ActorEditor/ActorEditor.manifest
                           ${ENGINE_DIR}/xrGame/PostprocessAnimator.cpp)
target_precompile_headers(ActorEditor PRIVATE ActorEditor/stdafx.h)
target_compile_definitions(ActorEditor PRIVATE _ACTOR_EDITOR _PP_EDITOR_ _WINDOWS)
target_include_directories(ActorEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ActorEditor ${ENGINE_DIR}/xrGame
                                               ${ENGINE_DIR}/xrEngine) # xrGame/PostprocessAnimator.h: envelope.h
target_link_libraries(ActorEditor PRIVATE xrECore)
set_target_properties(ActorEditor PROPERTIES FOLDER editors)

# --- Level Editor (stage 5): scenes (.level), their objects and tools, and the build data for Infernis's compilers
# (xrLC, xrDO_Light, xrAI). Left out (see README): IX-Ray's play in editor (Engine/XRayEditor, XrGameManager,
# EDITOR_PLAY_IN_EDITOR: it runs the game DLL inside the editor), with the in-memory AI graphs and spawn it plays
# (AI/, scene_spawn.cpp; Infernis's xrAI builds those), and its Python plugin. Server entities come from Infernis's xrSE_Factory, built for the editors (xrSE_Factory_Editor).
file(GLOB_RECURSE LEVEL_EDITOR_SOURCES CONFIGURE_DEPENDS LevelEditor/*.cpp LevelEditor/*.h)
list(FILTER LEVEL_EDITOR_SOURCES EXCLUDE REGEX
     "/LevelEditor/(Engine/(XRayEditor|XrGameManager)[^/]*|AI/.*|Plugins/PluginPython[^/]*|Editor/Scene/scene_spawn[.]cpp|Nodes/UIDialogsView[.](cpp|h))$")
# (Nodes/UIDialogsView: IX-Ray's dialog editor uses its tinyxml2-based XML API, like the dialog nodes of xrEProps.)
add_executable(LevelEditor WIN32 ${LEVEL_EDITOR_SOURCES} LevelEditor/LevelEditor.rc LevelEditor/LevelEditor.manifest)
target_precompile_headers(LevelEditor PRIVATE LevelEditor/stdafx.h)
target_compile_definitions(LevelEditor PRIVATE _LEVEL_EDITOR _WINDOWS)
target_include_directories(LevelEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/LevelEditor ${CMAKE_CURRENT_SOURCE_DIR}
                                               ${ENGINE_DIR} ${ENGINE_DIR}/xrServerEntities
                                               ${CMAKE_SOURCE_DIR}/code/3rd-party/openal/include
                                               # xrECore's own folders, which its headers include by bare name
                                               ${CMAKE_CURRENT_SOURCE_DIR}/xrECore/Editor ${CMAKE_CURRENT_SOURCE_DIR}/xrECore/Engine
                                               ${ENGINE_DIR}/xrEngine ${CMAKE_SOURCE_DIR}/code/utils/ETools
                                               ${CMAKE_SOURCE_DIR}/code/utils/xrQSlim/src
                                               ${CMAKE_SOURCE_DIR}/code/utils/common/NvMender2003)
target_link_libraries(LevelEditor PRIVATE xrECore xrSE_Factory_Editor libogg libvorbis libvorbisfile)
set_target_properties(LevelEditor PROPERTIES FOLDER editors)
