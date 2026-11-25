// IX-Ray RHI subset on Direct3D 9 (see compat/editor_rhi.h).
#include "stdafx.h"

static CEditorRHI EditorRHI;
XREUI_RHI_API CEditorRHI* GRHI = &EditorRHI;

namespace
{
D3DFORMAT ToD3DFormat(ERHI_FORMAT Format)
{
    // R8G8B8A8 is stored as A8R8G8B8 (red and blue swapped on upload).
    return Format == ERHI_FORMAT::R8G8B8A8_UNORM ? D3DFMT_A8R8G8B8 : static_cast<D3DFORMAT>(Format);
}

u32 BytesPerPixel(ERHI_FORMAT Format)
{
    switch (Format)
    {
    case ERHI_FORMAT::R16G16B16A16_FLOAT:
    case ERHI_FORMAT::R32G32_FLOAT: return 8;
    case ERHI_FORMAT::R32G32B32A32_FLOAT: return 16;
    default: return 4;
    }
}

void CopyRows(const D3DLOCKED_RECT& Locked, const RHISubResource& Data, ERHI_FORMAT Format, const RHIBox& Box)
{
    const u32 Width = Box.right - Box.left;
    const u32 Height = Box.bottom - Box.top;
    const u32 RowBytes = Width * BytesPerPixel(Format);
    const u32 SourcePitch = Data.RowPitch ? Data.RowPitch : RowBytes;
    for (u32 Row = 0; Row < Height; ++Row)
    {
        const u8* Source = static_cast<const u8*>(Data.Data) + Row * SourcePitch;
        u8* Destination = static_cast<u8*>(Locked.pBits) + Row * Locked.Pitch;
        if (Format == ERHI_FORMAT::R8G8B8A8_UNORM)
        {
            for (u32 X = 0; X < Width; ++X)
            {
                Destination[X * 4 + 0] = Source[X * 4 + 2];
                Destination[X * 4 + 1] = Source[X * 4 + 1];
                Destination[X * 4 + 2] = Source[X * 4 + 0];
                Destination[X * 4 + 3] = Source[X * 4 + 3];
            }
        }
        else
        {
            memcpy(Destination, Source, RowBytes);
        }
    }
}
} // namespace

IRHISurface::IRHISurface(IDirect3DBaseTexture9* Texture, ERHI_FORMAT Format) : Texture(Texture), Format(Format) {}

IRHISurface::~IRHISurface()
{
    if (Texture)
        Texture->Release();
}

IRHISurface* IRHISurface::Wrap(IDirect3DBaseTexture9* Texture)
{
    if (!Texture)
        return nullptr;
    Texture->AddRef();
    ERHI_FORMAT Format = ERHI_FORMAT::UNKNOWN;
    if (Texture->GetType() == D3DRTYPE_TEXTURE)
    {
        D3DSURFACE_DESC Desc;
        if (SUCCEEDED(static_cast<IDirect3DTexture9*>(Texture)->GetLevelDesc(0, &Desc)))
            Format = static_cast<ERHI_FORMAT>(Desc.Format);
    }
    return new IRHISurface(Texture, Format);
}

IRHISurface* IRHISurface::Adopt(IDirect3DBaseTexture9* Texture, ERHI_FORMAT Format)
{
    return Texture ? new IRHISurface(Texture, Format) : nullptr;
}

u32 IRHISurface::Release()
{
    const u32 Left = --RefCount;
    if (!Left)
        delete this;
    return Left;
}

u32 IRHISurface::GetWidth() const
{
    D3DSURFACE_DESC Desc{};
    if (Texture && Texture->GetType() == D3DRTYPE_TEXTURE)
        static_cast<IDirect3DTexture9*>(Texture)->GetLevelDesc(0, &Desc);
    return Desc.Width;
}

u32 IRHISurface::GetHeight() const
{
    D3DSURFACE_DESC Desc{};
    if (Texture && Texture->GetType() == D3DRTYPE_TEXTURE)
        static_cast<IDirect3DTexture9*>(Texture)->GetLevelDesc(0, &Desc);
    return Desc.Height;
}

void IRHISurface::UpdateData(u32 MipLevel, u32, const RHISubResource* Data, const RHIBox& Box)
{
    if (!Texture || !Data || !Data->Data || Texture->GetType() != D3DRTYPE_TEXTURE)
        return;
    auto* Texture2D = static_cast<IDirect3DTexture9*>(Texture);
    RECT Rect{LONG(Box.left), LONG(Box.top), LONG(Box.right), LONG(Box.bottom)};
    D3DLOCKED_RECT Locked;
    if (SUCCEEDED(Texture2D->LockRect(MipLevel, &Locked, &Rect, 0)))
    {
        CopyRows(Locked, *Data, Format, Box);
        Texture2D->UnlockRect(MipLevel);
    }
}

ERHI_CULLMODE CEditorRHIStateManager::GetCullMode() const
{
    DWORD Mode = D3DCULL_CCW;
    if (GRHI->GetDevice())
        GRHI->GetDevice()->GetRenderState(D3DRS_CULLMODE, &Mode);
    return static_cast<ERHI_CULLMODE>(Mode);
}

void CEditorRHIStateManager::SetCullMode(ERHI_CULLMODE Mode)
{
    if (GRHI->GetDevice())
        GRHI->GetDevice()->SetRenderState(D3DRS_CULLMODE, static_cast<DWORD>(Mode));
}

IRHISurface* CEditorRHI::CreateTexture2D(const RHITextureDesc& Desc, const RHISubResource& Data)
{
    if (!Device)
        return nullptr;
    // Managed pool: survives device resets, and LockRect works on it.
    IDirect3DTexture9* Texture = nullptr;
    if (FAILED(Device->CreateTexture(Desc.Width, Desc.Height, Desc.MipLevels, 0, ToD3DFormat(Desc.Format),
                                     D3DPOOL_MANAGED, &Texture, nullptr)))
        return nullptr;

    // Keep the IX-Ray format, so that R8G8B8A8 data is swizzled on upload.
    IRHISurface* Surface = IRHISurface::Adopt(Texture, Desc.Format);
    if (Data.Data)
    {
        RHIBox Box;
        Box.right = Desc.Width;
        Box.bottom = Desc.Height;
        Surface->UpdateData(0, 0, &Data, Box);
    }
    return Surface;
}

IRHISurface* CEditorRHI::CreateTextureFromMemory(IDirect3DBaseTexture9* Texture, u32, const RHISubResource&)
{
    return IRHISurface::Wrap(Texture);
}

IDirect3DSurface9* CEditorRHI::GetRenderTargetView(u32 Index) const
{
    IDirect3DSurface9* Surface = nullptr;
    if (Device && SUCCEEDED(Device->GetRenderTarget(Index, &Surface)))
        Surface->Release(); // the device keeps it alive; IX-Ray's call returns a borrowed view
    return Surface;
}

IDirect3DSurface9* CEditorRHI::GetDepthStencilView() const
{
    IDirect3DSurface9* Surface = nullptr;
    if (Device && SUCCEEDED(Device->GetDepthStencilSurface(&Surface)))
        Surface->Release();
    return Surface;
}

void CEditorRHI::SetDepthStencilView(IDirect3DSurface9* Surface)
{
    if (Device)
        Device->SetDepthStencilSurface(Surface);
}

void CEditorRHI::ClearTarget(IDirect3DSurface9*, const float Color[4])
{
    // D3D9 clears the bound render target.
    if (Device)
        Device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(Color[0], Color[1], Color[2], Color[3]), 1.f, 0);
}

void CEditorRHI::ClearDepthStencil(IDirect3DSurface9*, ERHI_CLEAR_TARGET Flags, float Depth, u8 Stencil)
{
    if (Device)
        Device->Clear(0, nullptr, static_cast<DWORD>(Flags), 0, Depth, Stencil);
}

void CEditorRHI::CopySurface(IDirect3DSurface9* Destination, IDirect3DSurface9* Source)
{
    if (Device && Destination && Source)
        Device->StretchRect(Source, nullptr, Destination, nullptr, D3DTEXF_NONE);
}

void CEditorRHI::Present()
{
    if (Device)
        Device->Present(nullptr, nullptr, nullptr, nullptr);
}
