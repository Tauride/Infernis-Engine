#include "stdafx.h"

#include "xrEmbreeTrace.h"
#include "xrLC_GlobalData.h"
#include "xrface.h"
#include "global_calculation_data.h"
#include "../shader_xrlc.h"
#include "xrcdb/xrcdb.h"

#ifdef XRLC_EMBREE
#include <embree4/rtcore.h>

namespace {
// The faces of one geometry, by primitive id
struct GeometryFaces {
    xr_vector<const void*> faces; // xrLC: each triangle's base_Face (the face being lit is skipped); xrDO_Light: none
    xr_vector<u32> tris;          // the model's triangle of each primitive
};

// The share of light a transparent triangle lets through at the hit (u, v as CDB's)
using TransmissionFn = float (*)(u32 tri, float u, float v);

RTCDevice g_device = nullptr;
RTCScene g_scene = nullptr;
const CDB::MODEL* g_model = nullptr;
GeometryFaces g_opaque, g_transparent;
TransmissionFn g_transmission = nullptr;

// The query's state: Embree hands the filter functions the context it was given
struct OcclusionContext {
    RTCRayQueryContext context; // first: the filters get its address
    const void* skip; // the face being lit, or null
    bool cull;        // OPCODE's CDB::OPT_CULL: faces seen from behind let the ray through
    Fvector dir;
    float energy;
    s32 blocker; // the model's triangle of the opaque face that blocked the ray, or -1
    // The transparent primitives already counted: with spatial splits (RTC_BUILD_QUALITY_HIGH) Embree may report one
    // primitive more than once along a ray, and its texel must let the light through only once
    u32 counted[32];
    u32 counted_size;

    bool count_once(u32 prim) {
        for (u32 i = 0; i < counted_size; i++)
            if (counted[i] == prim)
                return false;
        if (counted_size < std::size(counted))
            counted[counted_size++] = prim;
        return true; // (past 32 transparent hits, repeats count again: the light is all but gone by then)
    }
};

// The texel's alpha at the hit, from the face's texture coordinates
float TexelAlpha(const b_texture& T, const Fvector2* cuv, float u, float v) {
    // barycentric coords: note W,U,V order (as CDB's)
    Fvector B;
    B.set(1.0f - u - v, u, v);
    Fvector2 uv;
    uv.x = cuv[0].x * B.x + cuv[1].x * B.y + cuv[2].x * B.z;
    uv.y = cuv[0].y * B.x + cuv[1].y * B.y + cuv[2].y * B.z;

    int U = iFloor(uv.x * float(T.dwWidth) + .5f);
    int V = iFloor(uv.y * float(T.dwHeight) + .5f);
    U %= T.dwWidth;
    if (U < 0)
        U += T.dwWidth;
    V %= T.dwHeight;
    if (V < 0)
        V += T.dwHeight;

    const u32 pixel = T.pSurface[V * T.dwWidth + U];
    return float(color_get_A(pixel)) / 255.f;
}

// xrLC (GSC's getLastRP_Scale in xrDeflectorLight.cpp): 1 - alpha^2
float TransmissionLC(u32 tri, float u, float v) {
    base_Face* F = convert_nax(g_model->get_tris()[tri].dummy);
    const b_material& M = inlc_global_data()->materials()[F->dwMaterial];
    const b_texture& T = inlc_global_data()->textures()[M.surfidx];
    if (T.pSurface == nullptr)
        return 0.f; // the texels are gone (cleared after the build): opaque, as GSC's getLastRP_Scale
    return 1.f - xr::sqr(TexelAlpha(T, F->getTC0(), u, v));
}

// xrDO_Light (GSC's getLastRP_Scale in detail_slot_calculate.cpp): 1 - alpha
float TransmissionDO(u32 tri, float u, float v) {
    const b_rc_face& F = gl_data.g_rc_faces[tri];
    const b_material& M = gl_data.g_materials[F.dwMaterial];
    const b_texture& T = gl_data.g_textures[M.surfidx];
    if (T.pSurface == nullptr)
        return 0.f;
    return 1.f - TexelAlpha(T, F.t, u, v);
}

// OPCODE's back-face test with CDB::OPT_CULL (as CDB::TestRayTri's culling branch): the ray misses a face whose
// determinant is below EPS
bool CulledByOPCODE(u32 tri, const Fvector& D) {
    const CDB::TRI& T = g_model->get_tris()[tri];
    const Fvector* V = g_model->get_verts();
    Fvector edge1, edge2, pvec;
    edge1.sub(V[T.verts[1]], V[T.verts[0]]);
    edge2.sub(V[T.verts[2]], V[T.verts[0]]);
    pvec.crossproduct(D, edge2);
    return edge1.dotproduct(pvec) < EPS;
}

void FilterOpaque(const RTCFilterFunctionNArguments* args) {
    OcclusionContext* ctx = reinterpret_cast<OcclusionContext*>(args->context);
    const GeometryFaces* geometry = static_cast<const GeometryFaces*>(args->geometryUserPtr);
    for (u32 i = 0; i < args->N; i++) {
        if (!args->valid[i])
            continue;
        const u32 prim = RTCHitN_primID(args->hit, args->N, i);
        if ((ctx->skip && geometry->faces[prim] == ctx->skip) || (ctx->cull && CulledByOPCODE(geometry->tris[prim], ctx->dir)))
            args->valid[i] = 0; // the face being lit, or a back face with culling: go on
        else {
            ctx->energy = 0.f; // blocked: the hit stands
            ctx->blocker = s32(geometry->tris[prim]);
        }
    }
}

void FilterTransparent(const RTCFilterFunctionNArguments* args) {
    OcclusionContext* ctx = reinterpret_cast<OcclusionContext*>(args->context);
    const GeometryFaces* geometry = static_cast<const GeometryFaces*>(args->geometryUserPtr);
    for (u32 i = 0; i < args->N; i++) {
        if (!args->valid[i])
            continue;
        const u32 prim = RTCHitN_primID(args->hit, args->N, i);
        if (ctx->cull && CulledByOPCODE(geometry->tris[prim], ctx->dir)) {
            args->valid[i] = 0;
            continue;
        }
        const bool lit_face = ctx->skip && geometry->faces[prim] == ctx->skip;
        if (!lit_face && ctx->count_once(prim))
            ctx->energy *=
                g_transmission(geometry->tris[prim], RTCHitN_u(args->hit, args->N, i), RTCHitN_v(args->hit, args->N, i));
        // the product over every hit, in any order; once nothing is left the hit stands and the query stops
        if (ctx->energy > 0.f)
            args->valid[i] = 0;
    }
}

void AttachGeometry(const CDB::MODEL* model, const xr_vector<u32>& tris, GeometryFaces& faces,
                    RTCFilterFunctionN filter, bool face_keys) {
    if (tris.empty())
        return;
    RTCGeometry geometry = rtcNewGeometry(g_device, RTC_GEOMETRY_TYPE_TRIANGLE);
    // the model's vertices, copied (Embree pads its own buffers for its vector loads)
    float* vertices = static_cast<float*>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
                                                                  sizeof(Fvector), model->get_verts_count()));
    std::memcpy(vertices, model->get_verts(), sizeof(Fvector) * model->get_verts_count());
    u32* indices = static_cast<u32*>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
                                                             3 * sizeof(u32), tris.size()));
    faces.faces.clear();
    faces.faces.reserve(tris.size());
    faces.tris = tris;
    for (size_t i = 0; i < tris.size(); i++) {
        const CDB::TRI& T = model->get_tris()[tris[i]];
        indices[i * 3 + 0] = T.verts[0];
        indices[i * 3 + 1] = T.verts[1];
        indices[i * 3 + 2] = T.verts[2];
        faces.faces.push_back(face_keys ? convert_nax(T.dummy) : nullptr);
    }
    rtcSetGeometryUserData(geometry, &faces);
    rtcSetGeometryOccludedFilterFunction(geometry, filter);
    rtcCommitGeometry(geometry);
    rtcAttachGeometry(g_scene, geometry);
    rtcReleaseGeometry(geometry);
}
} // namespace

XRLC_LIGHT_API bool lc_embree_enabled() { return strstr(Core.Params, "-embree") != nullptr; }

// A scene of the model's shadow casters: the blocking ones and those that let light through their texels
static void BuildScene(const CDB::MODEL* model, const xr_vector<u32>& opaque, const xr_vector<u32>& transparent,
                       TransmissionFn transmission, bool face_keys) {
    lc_embree_release();
    if (!g_device) {
        g_device = rtcNewDevice(nullptr);
        R_ASSERT2(g_device, "Embree: no device");
        rtcSetDeviceErrorFunction(
            g_device,
            [](void*, RTCError code, const char* text) { Msg("! Embree error %d: %s", int(code), text ? text : ""); },
            nullptr);
        Msg("* Embree %s", RTC_VERSION_STRING);
    }

    g_scene = rtcNewScene(g_device);
    rtcSetSceneBuildQuality(g_scene, RTC_BUILD_QUALITY_HIGH);
    rtcSetSceneFlags(g_scene, RTC_SCENE_FLAG_ROBUST);
    AttachGeometry(model, opaque, g_opaque, &FilterOpaque, face_keys);
    AttachGeometry(model, transparent, g_transparent, &FilterTransparent, face_keys);
    rtcCommitScene(g_scene);
    g_model = model;
    g_transmission = transmission;
    clMsg("Embree scene: %u opaque, %u transparent shadow casters", u32(opaque.size()), u32(transparent.size()));
}

void lc_embree_build(const CDB::MODEL* model) {
    if (!lc_embree_enabled())
        return;

    // The shadow casters, as GSC's ray tracing sees them: no face or no shadow: left out; opaque (base_Face::
    // CacheOpacity's rule, worked out here: the first model is built before the faces cache it), or transparent
    // without a texture (GSC turns those opaque when it meets them): blocking; the rest: by their texels
    xr_vector<u32> opaque, transparent;
    for (int i = 0; i < model->get_tris_count(); i++) {
        base_Face* F = convert_nax(model->get_tris()[i].dummy);
        if (!F || !F->Shader().flags.bLIGHT_CastShadow)
            continue;
        const b_material& M = inlc_global_data()->materials()[F->dwMaterial];
        const b_BuildTexture& T = inlc_global_data()->textures()[M.surfidx];
        const bool blocks = !T.bHasAlpha || !T.THM.HasSurface() || T.pSurface == nullptr;
        (blocks ? opaque : transparent).push_back(u32(i));
    }
    BuildScene(model, opaque, transparent, &TransmissionLC, true);
}

void lc_embree_build_details() {
    if (!lc_embree_enabled())
        return;

    // The shadow casters, as xrDO_Light's getLastRP_Scale sees them: faces whose shader casts no shadow are left
    // out; textures without alpha or without texels block; the rest let light through by their texels
    const CDB::MODEL* model = &gl_data.RCAST_Model;
    xr_vector<u32> opaque, transparent;
    for (int i = 0; i < model->get_tris_count(); i++) {
        const b_rc_face& F = gl_data.g_rc_faces[i];
        if (!shader(F.dwMaterial, *gl_data.g_shaders_xrlc, gl_data.g_materials).flags.bLIGHT_CastShadow)
            continue;
        const b_material& M = gl_data.g_materials[F.dwMaterial];
        const b_BuildTexture& T = gl_data.g_textures[M.surfidx];
        const bool blocks = !T.bHasAlpha || T.pSurface == nullptr;
        (blocks ? opaque : transparent).push_back(u32(i));
    }
    BuildScene(model, opaque, transparent, &TransmissionDO, false);
}

void lc_embree_release() {
    if (g_scene)
        rtcReleaseScene(g_scene);
    if (g_device)
        rtcReleaseDevice(g_device); // (its TBB threads with it; the next scene creates it again)
    g_device = nullptr;
    g_scene = nullptr;
    g_model = nullptr;
    g_transmission = nullptr;
    g_opaque.faces.clear();
    g_opaque.tris.clear();
    g_transparent.faces.clear();
    g_transparent.tris.clear();
}

bool lc_embree_traces(const CDB::MODEL* model) { return g_scene && model == g_model; }

float lc_embree_occlusion(const Fvector& P, const Fvector& D, float range, const base_Face* skip, Fvector blocker[3],
                          bool cull) {
    OcclusionContext ctx;
    rtcInitRayQueryContext(&ctx.context);
    ctx.skip = skip;
    ctx.cull = cull;
    ctx.dir = D;
    ctx.energy = 1.f;
    ctx.blocker = -1;
    ctx.counted_size = 0;

    RTCOccludedArguments args;
    rtcInitOccludedArguments(&args);
    args.context = &ctx.context;
    args.flags = RTC_RAY_QUERY_FLAG_INCOHERENT;

    RTCRay ray;
    ray.org_x = P.x;
    ray.org_y = P.y;
    ray.org_z = P.z;
    ray.dir_x = D.x;
    ray.dir_y = D.y;
    ray.dir_z = D.z;
    ray.tnear = 0.f;
    ray.tfar = range;
    ray.time = 0.f;
    ray.mask = u32(-1);
    ray.id = 0;
    ray.flags = 0;
    rtcOccluded1(g_scene, &ray, &args);
    if (ctx.blocker >= 0) {
        const CDB::TRI& T = g_model->get_tris()[ctx.blocker];
        for (int v = 0; v < 3; v++)
            blocker[v].set(g_model->get_verts()[T.verts[v]]);
    }
    return ray.tfar < 0.f ? 0.f : ctx.energy; // tfar = -inf: a hit stood
}

#else // XRLC_EMBREE

XRLC_LIGHT_API bool lc_embree_enabled() { return false; }
void lc_embree_build(const CDB::MODEL*) {
    static bool warned = false;
    if (!warned && strstr(Core.Params, "-embree")) {
        Msg("! -embree: this xrLC is built without Embree (CMake's XRLC_EMBREE_DIR, XRLC_TBB_DIR); tracing with OPCODE");
        warned = true;
    }
}
void lc_embree_build_details() { lc_embree_build(nullptr); }
void lc_embree_release() {}
bool lc_embree_traces(const CDB::MODEL*) { return false; }
float lc_embree_occlusion(const Fvector&, const Fvector&, float, const base_Face*, Fvector[3], bool) { return 1.f; }

#endif // XRLC_EMBREE
