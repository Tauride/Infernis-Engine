#pragma once
// Pieces of IX-Ray's engine and RHI that the editor core uses and Infernis's engine does not have. The editor
// renders with Direct3D 9, so these map straight onto D3D9.

// IX-Ray engine iterator aliases (Infernis defines the vectors only).
#include "xrEngine/motion.h"
using BoneMotionIt = BoneMotionVec::iterator;
#include "xrEngine/bone.h"
using BoneIt = BoneVec::iterator;

// IX-Ray's D3D9 device accessor in editor code.
#define RDevice (HW.pDevice)

// IX-Ray RHI primitive topology. The values are the D3D9 primitive types, so a cast converts.
enum class ERHI_PRIMITIVE_TOPOLOGY : u32
{
    POINT_LIST = D3DPT_POINTLIST,
    LINE_LIST = D3DPT_LINELIST,
    LINE_STRIP = D3DPT_LINESTRIP,
    TRIANGLE_LIST = D3DPT_TRIANGLELIST,
    TRIANGLE_STRIP = D3DPT_TRIANGLESTRIP,
    TRIANGLE_FAN = D3DPT_TRIANGLEFAN,
};

inline D3DPRIMITIVETYPE ToD3DPrimitive(ERHI_PRIMITIVE_TOPOLOGY Topology)
{
    return static_cast<D3DPRIMITIVETYPE>(Topology);
}

// IX-Ray's engine declares this message in pure.h (DrawUI: the editor's ImGui pass); Infernis's engine does not.
class pureDrawUI
{
public:
    virtual void OnDrawUI() = 0;
};
ECORE_API void __fastcall rp_DrawUI(void* p);
