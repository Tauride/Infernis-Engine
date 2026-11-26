#pragma once
// The part of IX-Ray's RHI (render hardware interface) the editors call, implemented on Direct3D 9, which is what
// the Infernis editors render with. Same names as IX-Ray, so the editor code stays as it is. Implementation:
// xrEUI/EditorRHI.cpp. The device is set by the editor core once it exists (GRHI->SetDevice).

#include <d3d9.h>

#ifdef XREUI_EXPORTS
#define XREUI_RHI_API __declspec(dllexport)
#else
#define XREUI_RHI_API __declspec(dllimport)
#endif

// Formats by IX-Ray name. R8G8B8A8_UNORM data is uploaded as D3DFMT_A8R8G8B8 with red and blue swapped, since
// D3D9 drivers rarely support A8B8G8R8 textures.
// A struct rather than an enum class so that it converts to D3DFORMAT wherever Infernis's renderer wants one
// (render targets: CRT::create).
struct ERHI_FORMAT
{
    D3DFORMAT Value = D3DFMT_UNKNOWN;

    constexpr ERHI_FORMAT() = default;
    constexpr ERHI_FORMAT(D3DFORMAT Format) : Value(Format) {}
    constexpr operator D3DFORMAT() const { return Value; }

    static const ERHI_FORMAT UNKNOWN;
    static const ERHI_FORMAT R8G8B8A8_UNORM;
    static const ERHI_FORMAT B8G8R8A8_UNORM;
    static const ERHI_FORMAT B8G8R8X8_UNORM;
    static const ERHI_FORMAT R16G16B16A16_FLOAT;
    static const ERHI_FORMAT R32G32B32A32_FLOAT;
    static const ERHI_FORMAT R32G32_FLOAT;
    static const ERHI_FORMAT D24_UNORM_S8_UINT;
};
inline constexpr ERHI_FORMAT ERHI_FORMAT::UNKNOWN{D3DFMT_UNKNOWN};
inline constexpr ERHI_FORMAT ERHI_FORMAT::R8G8B8A8_UNORM{D3DFMT_A8B8G8R8};
inline constexpr ERHI_FORMAT ERHI_FORMAT::B8G8R8A8_UNORM{D3DFMT_A8R8G8B8};
inline constexpr ERHI_FORMAT ERHI_FORMAT::B8G8R8X8_UNORM{D3DFMT_X8R8G8B8};
inline constexpr ERHI_FORMAT ERHI_FORMAT::R16G16B16A16_FLOAT{D3DFMT_A16B16G16R16F};
inline constexpr ERHI_FORMAT ERHI_FORMAT::R32G32B32A32_FLOAT{D3DFMT_A32B32G32R32F};
inline constexpr ERHI_FORMAT ERHI_FORMAT::R32G32_FLOAT{D3DFMT_G32R32F};
inline constexpr ERHI_FORMAT ERHI_FORMAT::D24_UNORM_S8_UINT{D3DFMT_D24S8};

enum class ERHI_USAGE : u32
{
    USAGE_DEFAULT,
    USAGE_IMMUTABLE,
    USAGE_DYNAMIC,
    USAGE_STAGING,
};

enum class ERHI_BIND_FLAG : u32
{
    NONE = 0,
    SHADER_RESOURCE = 0x8,
    RENDER_TARGET = 0x20,
    DEPTH_STENCIL = 0x40,
};

enum class ERHI_CULLMODE : u32
{
    NONE = D3DCULL_NONE,
    FRONT = D3DCULL_CW, // D3D9 culls by winding: X-Ray's front faces are counter-clockwise
    BACK = D3DCULL_CCW,
};

enum class ERHI_CLEAR_TARGET : u32
{
    DEPTH = D3DCLEAR_ZBUFFER,
    STENCIL = D3DCLEAR_STENCIL,
};
inline ERHI_CLEAR_TARGET operator|(ERHI_CLEAR_TARGET Left, ERHI_CLEAR_TARGET Right)
{
    return static_cast<ERHI_CLEAR_TARGET>(static_cast<u32>(Left) | static_cast<u32>(Right));
}

enum class ERHI_BUFFER_MAP : u32
{
    WRITE,
};

struct RHITextureDesc
{
    u32 Width = 0;
    u32 Height = 0;
    u32 MipLevels = 1;
    u32 ArraySize = 1;
    ERHI_FORMAT Format = ERHI_FORMAT::UNKNOWN;
    ERHI_USAGE Usage = ERHI_USAGE::USAGE_DEFAULT;
    ERHI_BIND_FLAG BindFlags = ERHI_BIND_FLAG::SHADER_RESOURCE;
};

struct RHISubResource
{
    u32 Width = 0;
    u32 Height = 0;
    ERHI_FORMAT TextureFormat = ERHI_FORMAT::UNKNOWN;
    u32 RowPitch = 0;
    u32 DepthPitch = 0;
    const void* Data = nullptr;
};

struct RHIBox
{
    u32 left = 0;
    u32 top = 0;
    u32 front = 0;
    u32 right = 0;
    u32 bottom = 0;
    u32 back = 1;
};

struct RHIMappedSubresource
{
    void* pData = nullptr;
    u32 RowPitch = 0;
    u32 DepthPitch = 0;
};

// A 2D texture. Reference counted like a COM object: Release() frees it with its D3D texture.
class XREUI_RHI_API IRHISurface
{
public:
    // Takes a reference to an existing D3D texture.
    static IRHISurface* Wrap(IDirect3DBaseTexture9* Texture);
    // Takes over the caller's reference; Format is the IX-Ray format the data comes in.
    static IRHISurface* Adopt(IDirect3DBaseTexture9* Texture, ERHI_FORMAT Format);

    u32 GetWidth() const;
    u32 GetHeight() const;
    ERHI_FORMAT GetFormat() const { return Format; }
    void UpdateData(u32 MipLevel, u32 ArraySlice, const RHISubResource* Data, const RHIBox& Box);

    void AddRef() { ++RefCount; }
    u32 Release();

    IDirect3DBaseTexture9* GetD3D() const { return Texture; }
    void* GetImTextureID() const { return Texture; }

private:
    explicit IRHISurface(IDirect3DBaseTexture9* Texture, ERHI_FORMAT Format);
    ~IRHISurface();

    IDirect3DBaseTexture9* Texture = nullptr;
    ERHI_FORMAT Format = ERHI_FORMAT::UNKNOWN;
    u32 RefCount = 1;
};

class XREUI_RHI_API CEditorRHIStateManager
{
public:
    ERHI_CULLMODE GetCullMode() const;
    void SetCullMode(ERHI_CULLMODE Mode);
};

class XREUI_RHI_API CEditorRHI
{
public:
    CEditorRHIStateManager* StateManager = &States;

    void SetDevice(IDirect3DDevice9* NewDevice) { Device = NewDevice; }
    IDirect3DDevice9* GetDevice() const { return Device; }

    IRHISurface* CreateTexture2D(const RHITextureDesc& Desc, const RHISubResource& Data);
    // IX-Ray creates a shader resource view of an engine texture; on D3D9 the texture itself is used.
    IRHISurface* CreateTextureFromMemory(IDirect3DBaseTexture9* Texture, u32, const RHISubResource&);

    IDirect3DSurface9* GetRenderTargetView(u32 Index) const;
    IDirect3DSurface9* GetDepthStencilView() const;
    void SetDepthStencilView(IDirect3DSurface9* Surface);
    void ClearTarget(IDirect3DSurface9* Target, const float Color[4]);
    void ClearDepthStencil(IDirect3DSurface9* Surface, ERHI_CLEAR_TARGET Flags, float Depth, u8 Stencil);
    void CopySurface(IDirect3DSurface9* Destination, IDirect3DSurface9* Source);
    void Present();

private:
    IDirect3DDevice9* Device = nullptr;
    CEditorRHIStateManager States;
};

extern XREUI_RHI_API CEditorRHI* GRHI;
