#include "stdafx.h"
#include "dxRenderDeviceRender.h"

#include "ResourceManager.h"

dxRenderDeviceRender::dxRenderDeviceRender() : Resources(0) { ; }

void dxRenderDeviceRender::Copy(IRenderDeviceRender& _in) { *this = *(dxRenderDeviceRender*)&_in; }

void dxRenderDeviceRender::setGamma(float fGamma) { m_Gamma.Gamma(fGamma); }

void dxRenderDeviceRender::setBrightness(float fGamma) { m_Gamma.Brightness(fGamma); }

void dxRenderDeviceRender::setContrast(float fGamma) { m_Gamma.Contrast(fGamma); }

void dxRenderDeviceRender::updateGamma() { m_Gamma.Update(); }

void dxRenderDeviceRender::OnDeviceDestroy(BOOL bKeepTextures) {
    m_WireShader.destroy();
    m_SelectionShader.destroy();

    Resources->OnDeviceDestroy(bKeepTextures);
    RCache.OnDeviceDestroy();
}

void dxRenderDeviceRender::ValidateHW() { HW.Validate(); }

void dxRenderDeviceRender::DestroyHW() {
    xr_delete(Resources);
    HW.DestroyDevice();
}

void dxRenderDeviceRender::Reset(HWND hWnd, u32& dwWidth, u32& dwHeight, float& fWidth_2,
                                 float& fHeight_2) {
#ifdef DEBUG
    _SHOW_REF("*ref -CRenderDevice::ResetTotal: DeviceREF:", HW.pDevice);
#endif // DEBUG

    Resources->reset_begin();
    Memory.mem_compact();
    HW.Reset(hWnd);

#if defined(USE_DX10) || defined(USE_DX11)
    dwWidth = HW.m_ChainDesc.BufferDesc.Width;
    dwHeight = HW.m_ChainDesc.BufferDesc.Height;
#else //	USE_DX10
    dwWidth = HW.DevPP.BackBufferWidth;
    dwHeight = HW.DevPP.BackBufferHeight;
#endif //	USE_DX10

    fWidth_2 = float(dwWidth / 2);
    fHeight_2 = float(dwHeight / 2);
    Resources->reset_end();

#ifdef DEBUG
    _SHOW_REF("*ref +CRenderDevice::ResetTotal: DeviceREF:", HW.pDevice);
#endif // DEBUG
}

void dxRenderDeviceRender::SetupStates() {
    HW.Caps.Update();

#if defined(USE_DX10) || defined(USE_DX11)
//	TODO: DX10: Implement Resetting of render states into default mode
// VERIFY(!"dxRenderDeviceRender::SetupStates not implemented.");
#else //	USE_DX10
    for (u32 i = 0; i < HW.Caps.raster.dwStages; i++) {
        float fBias = -.5f;
        CHK_DX(HW.pDevice->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, 4));
        CHK_DX(HW.pDevice->SetSamplerState(i, D3DSAMP_MIPMAPLODBIAS, *((LPDWORD)(&fBias))));
        CHK_DX(HW.pDevice->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
        CHK_DX(HW.pDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
        CHK_DX(HW.pDevice->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR));
    }
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_COLORVERTEX, TRUE));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_ZENABLE, TRUE));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_LOCALVIEWER, TRUE));

    CHK_DX(HW.pDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE));

    if (psDeviceFlags.test(rsWireframe)) {
        CHK_DX(HW.pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME));
    } else {
        CHK_DX(HW.pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID));
    }

    // ******************** Fog parameters
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_FOGCOLOR, 0));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE));
    if (HW.Caps.bTableFog) {
        CHK_DX(HW.pDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR));
        CHK_DX(HW.pDevice->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE));
    } else {
        CHK_DX(HW.pDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE));
        CHK_DX(HW.pDevice->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR));
    }

#endif //	USE_DX10
}

void dxRenderDeviceRender::OnDeviceCreate(LPCSTR shName) {
    // Signal everyone - device created
    RCache.OnDeviceCreate();
    m_Gamma.Update();
    Resources->OnDeviceCreate(shName);
    ::Render->create();
    Device.Statistic->OnDeviceCreate();

    m_WireShader.create("editor\\wire");
    m_SelectionShader.create("editor\\selection");

    DUImpl.OnDeviceCreate();
}

void dxRenderDeviceRender::Create(HWND hWnd, u32& dwWidth, u32& dwHeight, float& fWidth_2,
                                  float& fHeight_2, bool move_window) {
    HW.CreateDevice(hWnd, move_window);
#if defined(USE_DX10) || defined(USE_DX11)
    dwWidth = HW.m_ChainDesc.BufferDesc.Width;
    dwHeight = HW.m_ChainDesc.BufferDesc.Height;
#else //	USE_DX10
    dwWidth = HW.DevPP.BackBufferWidth;
    dwHeight = HW.DevPP.BackBufferHeight;
#endif //	USE_DX10
    fWidth_2 = float(dwWidth / 2);
    fHeight_2 = float(dwHeight / 2);
    Resources = xr_new<CResourceManager>();
}

void dxRenderDeviceRender::SetupGPU(BOOL bForceGPU_SW, BOOL bForceGPU_NonPure, BOOL bForceGPU_REF) {
    HW.Caps.bForceGPU_SW = bForceGPU_SW;
    HW.Caps.bForceGPU_NonPure = bForceGPU_NonPure;
    HW.Caps.bForceGPU_REF = bForceGPU_REF;
}

void dxRenderDeviceRender::overdrawBegin() {
#if defined(USE_DX10) || defined(USE_DX11)
    //	TODO: DX10: Implement overdrawBegin
    VERIFY(!"dxRenderDeviceRender::overdrawBegin not implemented.");
#else  //	USE_DX10
    // Turn stenciling
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILENABLE, TRUE));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILREF, 0));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILMASK, 0x00000000));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILWRITEMASK, 0xffffffff));

    // Increment the stencil buffer for each pixel drawn
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT));

    if (1 == HW.Caps.SceneMode) {
        CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP));
    } // Overdraw
    else {
        CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_INCRSAT));
    } // ZB access
#endif //	USE_DX10
}

void dxRenderDeviceRender::overdrawEnd() {
#if defined(USE_DX10) || defined(USE_DX11)
    //	TODO: DX10: Implement overdrawEnd
    VERIFY(!"dxRenderDeviceRender::overdrawBegin not implemented.");
#else  //	USE_DX10
    // Set up the stencil states
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL));
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILMASK, 0xff));

    // Set the background to black
    CHK_DX(HW.pDevice->Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 0, 0), 0, 0));

    // Draw a rectangle wherever the count equal I
    RCache.OnFrameEnd();
    CHK_DX(HW.pDevice->SetFVF(FVF::F_TL));

    // Render gradients
    for (int I = 0; I < 12; I++) {
        u32 _c = I * 256 / 13;
        u32 c = D3DCOLOR_XRGB(_c, _c, _c);

        FVF::TL pv[4];
        pv[0].set(float(0), float(Device.dwHeight), c, 0, 0);
        pv[1].set(float(0), float(0), c, 0, 0);
        pv[2].set(float(Device.dwWidth), float(Device.dwHeight), c, 0, 0);
        pv[3].set(float(Device.dwWidth), float(0), c, 0, 0);

        CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILREF, I));
        CHK_DX(HW.pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, pv, sizeof(FVF::TL)));
    }
    CHK_DX(HW.pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE));
#endif //	USE_DX10
}

void dxRenderDeviceRender::DeferredLoad(BOOL E) { Resources->DeferredLoad(E); }

void dxRenderDeviceRender::ResourcesDeferredUpload() { Resources->DeferredUpload(); }

void dxRenderDeviceRender::ResourcesGetMemoryUsage(u32& m_base, u32& c_base, u32& m_lmaps,
                                                   u32& c_lmaps) {
    if (Resources)
        Resources->_GetMemoryUsage(m_base, c_base, m_lmaps, c_lmaps);
}

void dxRenderDeviceRender::ResourcesStoreNecessaryTextures() {
    dxRenderDeviceRender::Instance().Resources->StoreNecessaryTextures();
}

void dxRenderDeviceRender::ResourcesDumpMemoryUsage() {
    dxRenderDeviceRender::Instance().Resources->_DumpMemoryUsage();
}

dxRenderDeviceRender::DeviceState dxRenderDeviceRender::GetDeviceState() {
    HW.Validate();
#if defined(USE_DX10) || defined(USE_DX11)
//	TODO: DX10: Implement GetDeviceState
//	TODO: DX10: Implement DXGI_PRESENT_TEST testing
// VERIFY(!"dxRenderDeviceRender::overdrawBegin not implemented.");
#else  //	USE_DX10
    HRESULT _hr = HW.pDevice->TestCooperativeLevel();
    if (FAILED(_hr)) {
        // If the device was lost, do not render until we get it back
        if (D3DERR_DEVICELOST == _hr)
            return dsLost;

        // Check if the device is ready to be reset
        if (D3DERR_DEVICENOTRESET == _hr)
            return dsNeedReset;
    }
#endif //	USE_DX10

    return dsOK;
}

BOOL dxRenderDeviceRender::GetForceGPU_REF() { return HW.Caps.bForceGPU_REF; }

u32 dxRenderDeviceRender::GetCacheStatPolys() { return RCache.stat.polys; }

void dxRenderDeviceRender::Begin() {
#if !defined(USE_DX10) && !defined(USE_DX11)
    CHK_DX(HW.pDevice->BeginScene());
#endif //	USE_DX10
    RCache.OnFrameBegin();
    RCache.set_CullMode(CULL_CW);
    RCache.set_CullMode(CULL_CCW);
    if (HW.Caps.SceneMode)
        overdrawBegin();
}

void dxRenderDeviceRender::Clear() {
#if defined(USE_DX10) || defined(USE_DX11)
    HW.pContext->ClearDepthStencilView(RCache.get_ZB(), D3D_CLEAR_DEPTH | D3D_CLEAR_STENCIL, 1.0f,
                                       0);

    if (psDeviceFlags.test(rsClearBB)) {
        FLOAT ColorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        HW.pContext->ClearRenderTargetView(RCache.get_RT(), ColorRGBA);
    }
#else  //	USE_DX10
    CHK_DX(HW.pDevice->Clear(0, 0,
                             D3DCLEAR_ZBUFFER |
                                 (psDeviceFlags.test(rsClearBB) ? D3DCLEAR_TARGET : 0) |
                                 (HW.Caps.bStencil ? D3DCLEAR_STENCIL : 0),
                             D3DCOLOR_XRGB(0, 0, 0), 1, 0));
#endif //	USE_DX10
}

void DoAsyncScreenshot();

void dxRenderDeviceRender::End() {
    VERIFY(HW.pDevice);

    if (HW.Caps.SceneMode)
        overdrawEnd();

    RCache.OnFrameEnd();

    DoAsyncScreenshot();

#if defined(USE_DX10) || defined(USE_DX11)
    HW.m_pSwapChain->Present(0, 0);
#else //	USE_DX10
    CHK_DX(HW.pDevice->EndScene());

    HW.pDevice->Present(NULL, NULL, NULL, NULL);
#endif //	USE_DX10
       // HRESULT _hr		= HW.pDevice->Present( NULL, NULL, NULL, NULL );
    // if				(D3DERR_DEVICELOST==_hr)	return;			// we will handle this
    // later
}

void dxRenderDeviceRender::ResourcesDestroyNecessaryTextures() {
    Resources->DestroyNecessaryTextures();
}

void dxRenderDeviceRender::ClearTarget() {
#if defined(USE_DX10) || defined(USE_DX11)
    FLOAT ColorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    HW.pContext->ClearRenderTargetView(RCache.get_RT(), ColorRGBA);
#else  //	USE_DX10
    CHK_DX(HW.pDevice->Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1, 0));
#endif //	USE_DX10
}

void dxRenderDeviceRender::SetCacheXform(Fmatrix& mView, Fmatrix& mProject) {
    RCache.set_xform_view(mView);
    RCache.set_xform_project(mProject);
}

bool dxRenderDeviceRender::HWSupportsShaderYUV2RGB() {
    u32 v_dev = CAP_VERSION(HW.Caps.raster_major, HW.Caps.raster_minor);
    u32 v_need = CAP_VERSION(2, 0);
    return (v_dev >= v_need);
}

void dxRenderDeviceRender::OnAssetsChanged() {
    Resources->m_textures_description.UnLoad();
    Resources->m_textures_description.Load();
}

//-----------------------------------------------------------------------------
// SaveBackBufferPNG: the frame as rendered so far, as an opaque PNG (autotest screenshots).
// The engine creates its swap chains without multisampling and with 8-bit colour; anything
// else is refused with an error rather than saved wrong.
//-----------------------------------------------------------------------------
#if defined(USE_DX10) || defined(USE_DX11)

#ifdef USE_DX11
#include <D3DX11tex.h>
#else
#include <D3DX10tex.h>
#endif

namespace {
enum class EAlpha { none, byte3, unsupported };

// Where the alpha is, so it can be overwritten: the back buffer's alpha is whatever the
// last pass left there, not the picture's
EAlpha alpha_layout(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return EAlpha::byte3;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return EAlpha::none;
    default:
        return EAlpha::unsupported;
    }
}

void make_opaque(void* data, u32 pitch, u32 width, u32 height) {
    for (u32 y = 0; y < height; ++y) {
        u8* row = (u8*)data + size_t(y) * pitch;
        for (u32 x = 0; x < width; ++x)
            row[x * 4 + 3] = 0xff;
    }
}
} // namespace

LPCSTR dxRenderDeviceRender::SaveBackBufferPNG(LPCSTR path) {
    if (!HW.pBaseRT)
        return "no back buffer";

    ID3DResource* resource = nullptr;
    HW.pBaseRT->GetResource(&resource);
    if (!resource)
        return "the back buffer has no resource";

    ID3DTexture2D* source = nullptr;
    HRESULT hr = resource->QueryInterface(__uuidof(ID3DTexture2D), (void**)&source);
    _RELEASE(resource);
    if (FAILED(hr) || !source)
        return "the back buffer is not a 2D texture";

    D3D_TEXTURE2D_DESC desc;
    source->GetDesc(&desc);

    const EAlpha alpha = alpha_layout(desc.Format);
    LPCSTR error = nullptr;
    if (desc.SampleDesc.Count > 1)
        error = "a multisampled back buffer is not supported";
    else if (alpha == EAlpha::unsupported)
        error = "the back buffer format is not supported (8-bit RGBA or BGRA expected)";

    ID3DTexture2D* staging = nullptr;
    if (!error) {
        D3D_TEXTURE2D_DESC sd = desc;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        sd.BindFlags = 0;
        sd.MiscFlags = 0;
#ifdef USE_DX11
        sd.Usage = D3D11_USAGE_STAGING;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
#else
        sd.Usage = D3D10_USAGE_STAGING;
        sd.CPUAccessFlags = D3D10_CPU_ACCESS_READ | D3D10_CPU_ACCESS_WRITE;
#endif
        if (FAILED(HW.pDevice->CreateTexture2D(&sd, nullptr, &staging)))
            error = "cannot create the staging texture";
    }

    if (!error) {
        HW.pContext->CopySubresourceRegion(staging, 0, 0, 0, 0, source, 0, nullptr);
        if (alpha == EAlpha::byte3) {
#ifdef USE_DX11
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(HW.pContext->Map(staging, 0, D3D11_MAP_READ_WRITE, 0, &mapped))) {
                make_opaque(mapped.pData, mapped.RowPitch, desc.Width, desc.Height);
                HW.pContext->Unmap(staging, 0);
            } else
                error = "cannot map the staging texture";
#else
            D3D10_MAPPED_TEXTURE2D mapped;
            if (SUCCEEDED(staging->Map(0, D3D10_MAP_READ_WRITE, 0, &mapped))) {
                make_opaque(mapped.pData, mapped.RowPitch, desc.Width, desc.Height);
                staging->Unmap(0);
            } else
                error = "cannot map the staging texture";
#endif
        }
    }

    if (!error) {
#ifdef USE_DX11
        hr = D3DX11SaveTextureToFileA(HW.pContext, staging, D3DX11_IFF_PNG, path);
#else
        hr = D3DX10SaveTextureToFileA(staging, D3DX10_IFF_PNG, path);
#endif
        if (FAILED(hr))
            error = "cannot save the texture to the file";
    }

    _RELEASE(staging);
    _RELEASE(source);
    return error;
}

#else // USE_DX10 || USE_DX11

LPCSTR dxRenderDeviceRender::SaveBackBufferPNG(LPCSTR path) {
    IDirect3DSurface9* rt = HW.pBaseRT;
    if (!rt)
        return "no back buffer";
    D3DSURFACE_DESC desc;
    if (FAILED(rt->GetDesc(&desc)))
        return "GetDesc failed";
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
        return "a multisampled back buffer is not supported";

    // The alpha is not the picture's: it is overwritten where the format has one
    bool alpha_byte = false;
    switch (desc.Format) {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
        alpha_byte = true;
        break;
    case D3DFMT_R8G8B8:
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
        break;
    default:
        return "the back buffer format is not supported (8-bit RGB expected)";
    }

    IDirect3DSurface9* sys = nullptr;
    LPCSTR error = nullptr;
    if (FAILED(HW.pDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                       D3DPOOL_SYSTEMMEM, &sys, nullptr)))
        error = "CreateOffscreenPlainSurface failed";
    if (!error && FAILED(HW.pDevice->GetRenderTargetData(rt, sys)))
        error = "GetRenderTargetData failed";
    if (!error && alpha_byte) {
        D3DLOCKED_RECT locked;
        if (SUCCEEDED(sys->LockRect(&locked, nullptr, 0))) {
            for (u32 y = 0; y < desc.Height; ++y) {
                u8* row = (u8*)locked.pBits + size_t(y) * locked.Pitch;
                for (u32 x = 0; x < desc.Width; ++x)
                    row[x * 4 + 3] = 0xff;
            }
            sys->UnlockRect();
        } else
            error = "cannot lock the copy of the back buffer";
    }
    if (!error && FAILED(D3DXSaveSurfaceToFileA(path, D3DXIFF_PNG, sys, nullptr, nullptr)))
        error = "D3DXSaveSurfaceToFile failed";
    _RELEASE(sys);
    return error;
}

#endif // USE_DX10 || USE_DX11

//-----------------------------------------------------------------------------
// Dear ImGui renderer backend. The engine creates the context and drives the frames
// (xrEngine/xr_imgui.cpp); this module has its own copy of the ImGui core, pointed at
// that context.
//-----------------------------------------------------------------------------
#if defined(USE_DX10) && !defined(USE_DX11)

// No ImGui backend for Direct3D 10: the debug overlays are off in this renderer
bool dxRenderDeviceRender::ImGuiCreate(ImGuiContext*) { return false; }
void dxRenderDeviceRender::ImGuiDestroy() {}
void dxRenderDeviceRender::ImGuiResetBegin() {}
void dxRenderDeviceRender::ImGuiResetEnd() {}
void dxRenderDeviceRender::ImGuiNewFrame() {}
void dxRenderDeviceRender::ImGuiRender(ImDrawData*) {}

#else

// xrCore marks memset/memmove/strcmp deprecated (C4995); imgui.h uses them.
#pragma warning(push)
#pragma warning(disable : 4995)
#include <imgui.h>
#ifdef USE_DX11
#include <imgui_impl_dx11.h>
#else
#include <imgui_impl_dx9.h>
#endif
#pragma warning(pop)

#ifdef USE_DX11

bool dxRenderDeviceRender::ImGuiCreate(ImGuiContext* context) {
    ImGui::SetCurrentContext(context);
    R_ASSERT(ImGui_ImplDX11_Init(HW.pDevice, HW.pContext));
    return true;
}

void dxRenderDeviceRender::ImGuiDestroy() { ImGui_ImplDX11_Shutdown(); }
void dxRenderDeviceRender::ImGuiResetBegin() { ImGui_ImplDX11_InvalidateDeviceObjects(); }
void dxRenderDeviceRender::ImGuiResetEnd() { ImGui_ImplDX11_CreateDeviceObjects(); }
void dxRenderDeviceRender::ImGuiNewFrame() { ImGui_ImplDX11_NewFrame(); }

void dxRenderDeviceRender::ImGuiRender(ImDrawData* data) {
    // Onto the back buffer, whatever the last pass left bound
    RCache.set_RT(HW.pBaseRT);
    RCache.set_ZB(HW.pBaseZB);
    // The DX11 backend saves and restores the context state it changes, but not the
    // render backend's cache of it: render targets and states are set again
    ImGui_ImplDX11_RenderDrawData(data);
    RCache.Invalidate();
    RCache.set_RT(HW.pBaseRT);
    RCache.set_ZB(HW.pBaseZB);
}

#else // Direct3D 9

bool dxRenderDeviceRender::ImGuiCreate(ImGuiContext* context) {
    ImGui::SetCurrentContext(context);
    R_ASSERT(ImGui_ImplDX9_Init(HW.pDevice));
    return true;
}

void dxRenderDeviceRender::ImGuiDestroy() { ImGui_ImplDX9_Shutdown(); }
void dxRenderDeviceRender::ImGuiResetBegin() { ImGui_ImplDX9_InvalidateDeviceObjects(); }
void dxRenderDeviceRender::ImGuiResetEnd() { ImGui_ImplDX9_CreateDeviceObjects(); }
void dxRenderDeviceRender::ImGuiNewFrame() { ImGui_ImplDX9_NewFrame(); }

void dxRenderDeviceRender::ImGuiRender(ImDrawData* data) {
    // The DX9 backend saves and restores the device state (a state block), so the
    // render backend's cached state stays valid
    ImGui_ImplDX9_RenderDrawData(data);
}

#endif // USE_DX11

#endif // Direct3D 10
