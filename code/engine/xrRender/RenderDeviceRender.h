#ifndef RenderDeviceRender_included
#define RenderDeviceRender_included
#pragma once

struct ImGuiContext;
struct ImDrawData;

class IRenderDeviceRender {
public:
    enum DeviceState { dsOK = 0, dsLost, dsNeedReset };

public:
    virtual ~IRenderDeviceRender() { ; }
    virtual void Copy(IRenderDeviceRender& _in) = 0;

    //	Gamma correction functions
    virtual void setGamma(float fGamma) = 0;
    virtual void setBrightness(float fGamma) = 0;
    virtual void setContrast(float fGamma) = 0;
    virtual void updateGamma() = 0;

    //	Destroy
    virtual void OnDeviceDestroy(BOOL bKeepTextures) = 0;
    virtual void ValidateHW() = 0;
    virtual void DestroyHW() = 0;
    virtual void Reset(HWND hWnd, u32& dwWidth, u32& dwHeight, float& fWidth_2,
                       float& fHeight_2) = 0;
    //	Init
    virtual void SetupStates() = 0;
    virtual void OnDeviceCreate(LPCSTR shName) = 0;
    virtual void Create(HWND hWnd, u32& dwWidth, u32& dwHeight, float& fWidth_2, float& fHeight_2,
                        bool) = 0;
    virtual void SetupGPU(BOOL bForceGPU_SW, BOOL bForceGPU_NonPure, BOOL bForceGPU_REF) = 0;
    //	Overdraw
    virtual void overdrawBegin() = 0;
    virtual void overdrawEnd() = 0;

    //	Resources control
    virtual void DeferredLoad(BOOL E) = 0;
    virtual void ResourcesDeferredUpload() = 0;
    virtual void ResourcesGetMemoryUsage(u32& m_base, u32& c_base, u32& m_lmaps, u32& c_lmaps) = 0;
    virtual void ResourcesDestroyNecessaryTextures() = 0;
    virtual void ResourcesStoreNecessaryTextures() = 0;
    virtual void ResourcesDumpMemoryUsage() = 0;

    //	HWSupport
    virtual bool HWSupportsShaderYUV2RGB() = 0;

    //	Device state
    virtual DeviceState GetDeviceState() = 0;
    virtual BOOL GetForceGPU_REF() = 0;
    virtual u32 GetCacheStatPolys() = 0;
    virtual void Begin() = 0;
    virtual void Clear() = 0;
    virtual void End() = 0;
    virtual void ClearTarget() = 0;
    virtual void SetCacheXform(Fmatrix& mView, Fmatrix& mProject) = 0;
    virtual void OnAssetsChanged() = 0;

    // Saves the back buffer (the frame as rendered so far) as an opaque PNG. Works in
    // windowed mode. Returns NULL, or what went wrong. Call between Begin() and End().
    virtual LPCSTR SaveBackBufferPNG(LPCSTR path) = 0;

    // Dear ImGui renderer backend. The engine creates the context and drives the frames
    // (xr_imgui.cpp); the renderer has its own copy of the ImGui core, pointed at that
    // context. ImGuiCreate returns false when the renderer has no backend.
    virtual bool ImGuiCreate(ImGuiContext* context) = 0;
    virtual void ImGuiDestroy() = 0;
    virtual void ImGuiResetBegin() = 0; // before the device reset
    virtual void ImGuiResetEnd() = 0;   // after it
    virtual void ImGuiNewFrame() = 0;
    virtual void ImGuiRender(ImDrawData* data) = 0;
};

#endif //	RenderDeviceRender_included