// DXT.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4018)
#include "dxtlib.h"
#pragma warning(pop)
#include "ETextureParams.h"
#include "dds.h"
#include <ddraw.h>

// NVIDIA's nvDXTcompress for an RGBAImage, with the routine that writes the .dds.
typedef NV_ERROR_CODE(__cdecl* nvDXTcompressFuncT)(const RGBAImage*, nvCompressionOptions*,
                                                   DXTWriteCallback, const RECT*);

static nvDXTcompressFuncT nvDXTcompressFunc = nullptr;

// nvdxtwrap.dll's only export, nvDXTCompressFunc(image, options), is
//     xor r9d, r9d / xor r8d, r8d / jmp nvDXTcompress
// that is, nvDXTcompress with no write routine, and NVIDIA's library calls the write routine for each part of the
// .dds without checking it: every compression crashed on a null call. Take the jump's target and call it with
// WriteDDS. Any other code at the export (another build of the wrapper) is refused here, not guessed at.
static nvDXTcompressFuncT FindCompressWithWriter(const void* Export) {
    static const u8 Pattern[] = {0x45, 0x33, 0xC9, 0x45, 0x33, 0xC0, 0xE9};
    const u8* Code = static_cast<const u8*>(Export);
    if (!Code || std::memcmp(Code, Pattern, sizeof(Pattern)) != 0)
        return nullptr;
    s32 Offset;
    std::memcpy(&Offset, Code + sizeof(Pattern), sizeof(Offset));
    return reinterpret_cast<nvDXTcompressFuncT>(Code + sizeof(Pattern) + sizeof(Offset) + Offset);
}

BOOL APIENTRY DllMain(HANDLE hModule, u32 ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call != DLL_PROCESS_ATTACH)
        return TRUE;
    auto dxtWrapHandle = LoadLibraryW(L"nvdxtwrap.dll");
    if (!dxtWrapHandle) {
        FATAL("nvdxtwrap.dll not found");
    }
    nvDXTcompressFunc = FindCompressWithWriter(GetProcAddress(dxtWrapHandle, "nvDXTCompressFunc"));
    if (!nvDXTcompressFunc) {
        FATAL("nvdxtwrap.dll: nvDXTCompressFunc not found, or not the wrapper xrDXT knows");
    }
    return TRUE;
}

static HFILE gFileOut;
static HFILE gFileIn;

const u32 fcc_DXT1 = MAKEFOURCC('D', 'X', 'T', '1');
const u32 fcc_DXT2 = MAKEFOURCC('D', 'X', 'T', '2');
const u32 fcc_DXT3 = MAKEFOURCC('D', 'X', 'T', '3');
const u32 fcc_DXT4 = MAKEFOURCC('D', 'X', 'T', '4');
const u32 fcc_DXT5 = MAKEFOURCC('D', 'X', 'T', '5');

// NVIDIA's write routine: each part of the .dds in turn (the magic, the header, the image data).
static NV_ERROR_CODE WriteDDS(const void* constBuffer, size_t count, const MIPMapData* mipMapData,
                              void* userData) {
    void* buffer = const_cast<void*>(constBuffer);
    if (count == sizeof(DDS_HEADER)) {
        // correct DDS header
        DDS_HEADER* hdr = (DDS_HEADER*)buffer;
        if (hdr->dwSize == count) {
            switch (hdr->ddspf.dwFourCC) {
            case fcc_DXT1:
            case fcc_DXT2:
            case fcc_DXT3:
            case fcc_DXT4:
            case fcc_DXT5:
                hdr->ddspf.dwRGBBitCount = 0;
                break;
            }
        }
    }
    _write(gFileOut, buffer, (unsigned int)count);
    return NV_OK;
}

void __cdecl ReadDTXnFile(DWORD count, void* buffer, void* userData) {
    _read(gFileIn, buffer, count);
}

HRESULT WriteCompressedData(void* data, int miplevel, u32 size) {
    _write(gFileOut, data, size);
    std::memset(data, 0xff, size);
    return 0;
}

ENGINE_API u32* Build32MipLevel(u32& _w, u32& _h, u32& _p, u32* pdwPixelSrc, STextureParams* fmt,
                                float blend) {
    R_ASSERT(pdwPixelSrc);
    R_ASSERT((_w % 2) == 0);
    R_ASSERT((_h % 2) == 0);
    R_ASSERT((_p % 4) == 0);

    u32 dwDestPitch = (_w / 2) * 4;
    u32* pNewData = xr_alloc<u32>((_h / 2) * dwDestPitch);
    u8* pDest = (u8*)pNewData;
    u8* pSrc = (u8*)pdwPixelSrc;

    float mixed_a = (float)u8(fmt->fade_color >> 24);
    float mixed_r = (float)u8(fmt->fade_color >> 16);
    float mixed_g = (float)u8(fmt->fade_color >> 8);
    float mixed_b = (float)u8(fmt->fade_color >> 0);

    float inv_blend = 1.f - blend;
    for (u32 y = 0; y < _h; y += 2) {
        u8* pScanline = pSrc + y * _p;
        for (u32 x = 0; x < _w; x += 2) {
            u8* p1 = pScanline + x * 4;
            u8* p2 = p1 + 4;
            if (1 == _w)
                p2 = p1;
            u8* p3 = p1 + _p;
            if (1 == _h)
                p3 = p1;
            u8* p4 = p2 + _p;
            if (1 == _h)
                p4 = p2;
            float c_r = float(u32(p1[0]) + u32(p2[0]) + u32(p3[0]) + u32(p4[0])) / 4.f;
            float c_g = float(u32(p1[1]) + u32(p2[1]) + u32(p3[1]) + u32(p4[1])) / 4.f;
            float c_b = float(u32(p1[2]) + u32(p2[2]) + u32(p3[2]) + u32(p4[2])) / 4.f;
            float c_a = float(u32(p1[3]) + u32(p2[3]) + u32(p3[3]) + u32(p4[3])) / 4.f;

            if (fmt->flags.is(STextureParams::flFadeToColor)) {
                c_r = c_r * inv_blend + mixed_r * blend;
                c_g = c_g * inv_blend + mixed_g * blend;
                c_b = c_b * inv_blend + mixed_b * blend;
            }
            if (fmt->flags.is(STextureParams::flFadeToAlpha)) {
                c_a = c_a * inv_blend + mixed_a * blend;
            }

            float A = (c_a + c_a / 8.f);
            int _r = int(c_r);
            clamp(_r, 0, 255);
            *pDest++ = u8(_r);
            int _g = int(c_g);
            clamp(_g, 0, 255);
            *pDest++ = u8(_g);
            int _b = int(c_b);
            clamp(_b, 0, 255);
            *pDest++ = u8(_b);
            int _a = int(A);
            clamp(_a, 0, 255);
            *pDest++ = u8(_a);
        }
    }
    _w /= 2;
    _h /= 2;
    _p = _w * 4;
    return pNewData;
}
/*
bool IsThisNumberPowerOfTwo(unsigned int n)
{
        return (((n & (n-1)) == 0) && (n != 0));
}
*/
int DXTCompressImage(LPCSTR out_name, u8* raw_data, u32 w, u32 h, u32 pitch, STextureParams* fmt,
                     u32 depth) {
    R_ASSERT((0 != w) && (0 != h));

    gFileOut = _open(out_name, _O_WRONLY | _O_BINARY | _O_CREAT | _O_TRUNC, _S_IWRITE);
    if (gFileOut == -1) {
        fprintf(stderr, "Can't open output file %s\n", out_name);
        return false;
    }

    HRESULT hr = -1;
    // convert to Options
    nvCompressionOptions nvOpt;
    std::memset(&nvOpt, 0, sizeof(nvOpt));

    if (fmt->flags.is(STextureParams::flGenerateMipMaps))
        nvOpt.mipMapGeneration = kGenerateMipMaps;
    else
        nvOpt.mipMapGeneration = kNoMipMaps;
    nvOpt.bBinaryAlpha = !!(fmt->flags.is(STextureParams::flBinaryAlpha));
    nvOpt.bAlphaBorder = !!(fmt->flags.is(STextureParams::flAlphaBorder));
    nvOpt.bBorder = !!(fmt->flags.is(STextureParams::flColorBorder));
    nvOpt.borderColor32F.set(color_get_R(fmt->border_color), color_get_G(fmt->border_color),
                             color_get_B(fmt->border_color), color_get_A(fmt->border_color));
    nvOpt.bFadeColor = !!(fmt->flags.is(STextureParams::flFadeToColor));
    nvOpt.fadeAmount32F = fmt->fade_amount;
    nvOpt.bFadeAlpha = !!(fmt->flags.is(STextureParams::flFadeToAlpha));
    nvOpt.fadeToColor32F.set(color_get_R(fmt->fade_color), color_get_G(fmt->fade_color),
                             color_get_B(fmt->fade_color), color_get_A(fmt->fade_color));
    nvOpt.fadeToDelay = fmt->fade_delay;
    nvOpt.bDitherColor = !!(fmt->flags.is(STextureParams::flDitherColor));
    nvOpt.bDitherMip0 = !!(fmt->flags.is(STextureParams::flDitherEachMIPLevel));
    nvOpt.textureType =
        (fmt->type == STextureParams::ttCubeMap) ? kTextureTypeCubeMap : kTextureTypeTexture2D;
    switch (fmt->fmt) {
    case STextureParams::tfDXT1:
        nvOpt.textureFormat = kDXT1;
        break;
    case STextureParams::tfADXT1:
        nvOpt.textureFormat = kDXT1a;
        break;
    case STextureParams::tfDXT3:
        nvOpt.textureFormat = kDXT3;
        break;
    case STextureParams::tfDXT5:
        nvOpt.textureFormat = kDXT5;
        break;
    case STextureParams::tf4444:
    case STextureParams::tf1555:
        nvOpt.textureFormat = k1555;
        break;
    case STextureParams::tf565:
        nvOpt.textureFormat = k565;
        break;
    case STextureParams::tfRGB:
        nvOpt.textureFormat = k888;
        break;
    case STextureParams::tfRGBA:
        nvOpt.textureFormat = k8888;
        break;
    case STextureParams::tfA8:
        nvOpt.textureFormat = kA8;
        break;
    case STextureParams::tfL8:
        nvOpt.textureFormat = kL8;
        break;
    case STextureParams::tfA8L8:
        nvOpt.textureFormat = kA8L8;
        break;
    }

    switch (fmt->mip_filter) {
    case STextureParams::kMIPFilterAdvanced:
        // GSC's "advanced" mips: xrDXT built the chain itself (2x2 averages, with the fade) and passed it side by
        // side in one image as existing mips. The wrapped NVIDIA library takes a single image and wrote that strip
        // as the top level under a full mip header: truncated files the game refuses. The library builds the mips
        // instead, with the same box filter and its own fade options (set above from the same parameters).
        nvOpt.mipFilterType = kMipFilterBox;
        break;

    case STextureParams::kMIPFilterPoint:
        nvOpt.mipFilterType = kMipFilterPoint;
        break;
    case STextureParams::kMIPFilterBox:
        nvOpt.mipFilterType = kMipFilterBox;
        break;
    case STextureParams::kMIPFilterTriangle:
        nvOpt.mipFilterType = kMipFilterTriangle;
        break;
    case STextureParams::kMIPFilterQuadratic:
        nvOpt.mipFilterType = kMipFilterQuadratic;
        break;
    case STextureParams::kMIPFilterCubic:
        nvOpt.mipFilterType = kMipFilterCubic;
        break;

    case STextureParams::kMIPFilterCatrom:
        nvOpt.mipFilterType = kMipFilterCatrom;
        break;
    case STextureParams::kMIPFilterMitchell:
        nvOpt.mipFilterType = kMipFilterMitchell;
        break;

    case STextureParams::kMIPFilterGaussian:
        nvOpt.mipFilterType = kMipFilterGaussian;
        break;
    case STextureParams::kMIPFilterSinc:
        nvOpt.mipFilterType = kMipFilterSinc;
        break;
    case STextureParams::kMIPFilterBessel:
        nvOpt.mipFilterType = kMipFilterBessel;
        break;

    case STextureParams::kMIPFilterHanning:
        nvOpt.mipFilterType = kMipFilterHanning;
        break;
    case STextureParams::kMIPFilterHamming:
        nvOpt.mipFilterType = kMipFilterHamming;
        break;
    case STextureParams::kMIPFilterBlackman:
        nvOpt.mipFilterType = kMipFilterBlackman;
        break;
    case STextureParams::kMIPFilterKaiser:
        nvOpt.mipFilterType = kMipFilterKaiser;
        break;
    }
    //-------------------
    RGBAImage pImage(w, h);
    rgba_t* pixels = pImage.pixels();
    u8* pixel = raw_data;
    for (u32 k = 0; k < w * h; k++, pixel += 4)
        pixels[k].set(pixel[2], pixel[1], pixel[0], pixel[3]);
    hr = nvDXTcompressFunc(&pImage, &nvOpt, WriteDDS, nullptr);
    _close(gFileOut);
    if (hr != DD_OK) {
        switch (hr) {
        case NV_INPUT_POINTER_ZERO:
            MessageBox(0, "Empty source.", "DXT compress error", MB_ICONERROR | MB_OK);
            break;
        case NV_DEPTH_IS_NOT_3_OR_4:
            MessageBox(0, "Source depth is not 3 or 4.", "DXT compress error",
                       MB_ICONERROR | MB_OK);
            break;
        case NV_IMAGE_NOT_POWER_2:
            MessageBox(0, "Source size non power 2.", "DXT compress error", MB_ICONERROR | MB_OK);
            break;
        case NV_NEED_4_PLANES_FOR_RGBE:
            MessageBox(0, "Source incorrect number of planes.", "DXT compress error",
                       MB_ICONERROR | MB_OK);
            break;
        case NV_IMAGE_NOT_MULT4:
            MessageBox(0, "Source size non mul 4.", "DXT compress error", MB_ICONERROR | MB_OK);
            break;
        }
        unlink(out_name);
        return 0;
    } else
        return 1;
}

extern int DXTCompressBump(LPCSTR out_name, u8* raw_data, u8* normal_map, u32 w, u32 h, u32 pitch,
                           STextureParams* fmt, u32 depth);

extern "C" __declspec(dllexport) int __stdcall DXTCompress(LPCSTR out_name, u8* raw_data,
                                                           u8* normal_map, u32 w, u32 h, u32 pitch,
                                                           STextureParams* fmt, u32 depth) {
    switch (fmt->type) {
    case STextureParams::ttImage:
    case STextureParams::ttCubeMap:
    case STextureParams::ttNormalMap:
    case STextureParams::ttTerrain:
        return DXTCompressImage(out_name, raw_data, w, h, pitch, fmt, depth);
        break;
    case STextureParams::ttBumpMap:
        return DXTCompressBump(out_name, raw_data, normal_map, w, h, pitch, fmt, depth);
        break;
    default:
        NODEFAULT;
    }
    return -1;
}
