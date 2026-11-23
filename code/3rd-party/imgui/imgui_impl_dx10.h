// dear imgui: Renderer Backend for DirectX10
// This needs to be used along with a Platform Backend (e.g. Win32)

// NOT AN UPSTREAM FILE. Dear ImGui has its own imgui_impl_dx10, but it is not part of the
// copy vendored here. This one was written for the Infernis engine from the vendored
// imgui_impl_dx11 of the same version (1.93.0), with the Direct3D 11 calls replaced by
// their Direct3D 10 counterparts. It can be replaced with the upstream backend.

// Implemented features:
//  [X] Renderer: User texture binding. Use 'ID3D10ShaderResourceView*' as texture identifier.
//  [X] Renderer: Large meshes support (64k+ vertices) even with 16-bit indices (ImGuiBackendFlags_RendererHasVtxOffset).
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API
#ifndef IMGUI_DISABLE

struct ID3D10Device;

IMGUI_IMPL_API bool     ImGui_ImplDX10_Init(ID3D10Device* device);
IMGUI_IMPL_API void     ImGui_ImplDX10_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplDX10_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplDX10_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API bool     ImGui_ImplDX10_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplDX10_InvalidateDeviceObjects();

// (Advanced) Use e.g. if you need to precisely control the timing of texture updates, by setting ImDrawData::Textures = nullptr to handle this manually.
IMGUI_IMPL_API void     ImGui_ImplDX10_UpdateTexture(ImTextureData* tex);

#endif // #ifndef IMGUI_DISABLE
