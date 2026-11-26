// DXTUtils (IX-Ray's editor texture helpers, see DXTUtils.h) on what Infernis uses:
// - reading images: D3DX into an A8R8G8B8 surface (pixels are u32 0xAARRGGBB, as in GSC's editors);
// - resampling: the engine's imf_Process (xrImage_Resampler);
// - DDS output: xrDXT.dll's DXTCompress, the compressor Infernis's own tools use;
// - TGA output: the renderer's TGAdesc.
#include "stdafx.h"
#include "DXTUtils.h"
#include "xrEngine/xrImage_Resampler.h"
#include "xrRenderCommon/tga.h"

namespace
{
// Loads an image file into 32-bit pixels. False when the device is not up or D3DX cannot read the file.
bool LoadPixels(const char* FileName, u32& Width, u32& Height, U32Vec& Pixels)
{
    D3DXIMAGE_INFO Info;
    if (!HW.pDevice || FAILED(D3DXGetImageInfoFromFileA(FileName, &Info)))
        return false;

    IDirect3DSurface9* Surface = nullptr;
    if (FAILED(HW.pDevice->CreateOffscreenPlainSurface(Info.Width, Info.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH,
                                                      &Surface, nullptr)))
        return false;

    bool Loaded = false;
    if (SUCCEEDED(D3DXLoadSurfaceFromFileA(Surface, nullptr, nullptr, FileName, nullptr, D3DX_FILTER_NONE, 0, nullptr)))
    {
        D3DLOCKED_RECT Locked;
        if (SUCCEEDED(Surface->LockRect(&Locked, nullptr, D3DLOCK_READONLY)))
        {
            Width = Info.Width;
            Height = Info.Height;
            Pixels.resize(size_t(Width) * Height);
            for (u32 Row = 0; Row < Height; ++Row)
                std::memcpy(Pixels.data() + size_t(Row) * Width, static_cast<const u8*>(Locked.pBits) + Row * Locked.Pitch,
                            Width * 4);
            Surface->UnlockRect();
            Loaded = true;
        }
    }
    Surface->Release();
    return Loaded;
}

void WriteTGA(u32 Width, u32 Height, const u32* Pixels, const xr_path& To)
{
    TGAdesc Desc;
    Desc.format = IMG_32B;
    Desc.scanlenght = Width * 4;
    Desc.width = Width;
    Desc.height = Height;
    Desc.data = const_cast<u32*>(Pixels);

    CMemoryWriter Writer;
    Desc.maketga(Writer);
    if (!Writer.save_to(To.xstring().c_str()))
        Msg("! DXTUtils: can't write '%s'", To.xstring().c_str());
}

using DXTCompressFunc = int(__stdcall*)(LPCSTR, u8*, u8*, u32, u32, u32, STextureParams*, u32);

DXTCompressFunc GetDXTCompress()
{
    static DXTCompressFunc Function = []() -> DXTCompressFunc {
        HMODULE Module = LoadLibraryA("xrDXT.dll");
        if (!Module)
        {
            Msg("! DXTUtils: xrDXT.dll not found (built with BUILD_EDITORS or BUILD_UTILS; it loads nvdxtwrap.dll)");
            return nullptr;
        }
        return reinterpret_cast<DXTCompressFunc>(GetProcAddress(Module, "DXTCompress"));
    }();
    return Function;
}
} // namespace

namespace DXTUtils
{
void Converter::MakeTGA(xr_path From, xr_path To)
{
    u32 Width = 0, Height = 0;
    U32Vec Pixels;
    if (LoadPixels(From.xstring().c_str(), Width, Height, Pixels))
        WriteTGA(Width, Height, Pixels.data(), To);
    else
        Msg("! DXTUtils: can't read '%s'", From.xstring().c_str());
}

void Converter::MakeTGA(u32 w, u32 h, U32Vec& From, xr_path To)
{
    WriteTGA(w, h, From.data(), To);
}

void Converter::MakePNG(xr_path From, xr_path To)
{
    D3DXIMAGE_INFO Info;
    IDirect3DSurface9* Surface = nullptr;
    const xr_string Source = From.xstring();
    if (HW.pDevice && SUCCEEDED(D3DXGetImageInfoFromFileA(Source.c_str(), &Info)) &&
        SUCCEEDED(HW.pDevice->CreateOffscreenPlainSurface(Info.Width, Info.Height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH,
                                                         &Surface, nullptr)) &&
        SUCCEEDED(D3DXLoadSurfaceFromFileA(Surface, nullptr, nullptr, Source.c_str(), nullptr, D3DX_FILTER_NONE, 0,
                                           nullptr)))
    {
        if (FAILED(D3DXSaveSurfaceToFileA(To.xstring().c_str(), D3DXIFF_PNG, Surface, nullptr, nullptr)))
            Msg("! DXTUtils: can't write '%s'", To.xstring().c_str());
    }
    else
    {
        Msg("! DXTUtils: can't read '%s'", Source.c_str());
    }
    if (Surface)
        Surface->Release();
}

void Filter::Process(u32* dst, u32 dstW, u32 dstH, u32* src, u32 srcW, u32 srcH, EIMF_Type FILTER)
{
    // Same filter numbering as the engine's resampler.
    imf_Process(dst, dstW, dstH, src, srcW, srcH, ::EIMF_Type(FILTER));
}

void Filter::Resize(u32* dst, u32 dstW, u32 dstH, u32* out, u32 outW, u32 outH)
{
    imf_Process(out, outW, outH, dst, dstW, dstH, ::imf_box);
}

int Compress(const char* out_name, u8* raw_data, u8* normal_map, u32 w, u32 h, u32 pitch, STextureParams* fmt, u32 depth)
{
    DXTCompressFunc Function = GetDXTCompress();
    if (!Function)
        return 0;
    return Function(out_name, raw_data, normal_map, w, h, pitch, fmt, depth);
}

U8Vec GitPixels(const char* FileName, u32 NewW, u32 NewH)
{
    U8Vec Result;
    u32 Width = 0, Height = 0;
    U32Vec Pixels;
    if (!LoadPixels(FileName, Width, Height, Pixels))
        return Result;

    Result.resize(size_t(NewW) * NewH * 4);
    if (Width == NewW && Height == NewH)
        std::memcpy(Result.data(), Pixels.data(), Result.size());
    else
        imf_Process(reinterpret_cast<u32*>(Result.data()), NewW, NewH, Pixels.data(), Width, Height, ::imf_lanczos3);
    return Result;
}

ImageInfo GitPixels(const char* FileName)
{
    ImageInfo Info{};
    U32Vec Pixels;
    if (LoadPixels(FileName, Info.W, Info.H, Pixels))
    {
        Info.P.resize(Pixels.size() * 4);
        std::memcpy(Info.P.data(), Pixels.data(), Info.P.size());
        Info.Format = "A8R8G8B8";
    }
    return Info;
}
} // namespace DXTUtils
