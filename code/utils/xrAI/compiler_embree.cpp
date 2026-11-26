#include "stdafx.h"
#include "compiler.h"
#include "compiler_embree.h"

bool ai_embree_enabled() {
#ifdef XRAI_EMBREE
    return strstr(Core.Params, "-embree") != nullptr;
#else
    return false;
#endif
}

#ifdef XRAI_EMBREE
#include <embree4/rtcore.h>

extern Shader_xrLC_LIB* g_shaders_xrlc;
extern xr_vector<b_material> g_materials;
extern xr_vector<b_BuildTexture> g_textures;
extern xr_vector<b_rc_face> g_rc_faces;

namespace {
RTCDevice g_device = nullptr;
RTCScene g_scene = nullptr;
xr_vector<u32> g_opaque_tris, g_transparent_tris; // the model's triangle of each primitive

struct CoverContext {
    RTCRayQueryContext context; // first: the filters get its address
    Fvector dir;
    float energy;
    // With spatial splits Embree may report a primitive more than once along a ray; a texel counts once
    u32 counted[32];
    u32 counted_size;

    bool count_once(u32 prim) {
        for (u32 i = 0; i < counted_size; i++)
            if (counted[i] == prim)
                return false;
        if (counted_size < std::size(counted))
            counted[counted_size++] = prim;
        return true;
    }
};

// CDB::OPT_CULL, as CDB::TestRayTri's culling branch: the ray misses a face whose determinant is below EPS
bool Culled(u32 tri, const Fvector& D) {
    const CDB::TRI& T = Level.get_tris()[tri];
    const Fvector* V = Level.get_verts();
    Fvector edge1, edge2, pvec;
    edge1.sub(V[T.verts[1]], V[T.verts[0]]);
    edge2.sub(V[T.verts[2]], V[T.verts[0]]);
    pvec.crossproduct(D, edge2);
    return edge1.dotproduct(pvec) < EPS;
}

// getLastRP_Scale's transparent face: 1 - alpha of the texel at the hit
float Transmission(u32 tri, float u, float v) {
    const b_rc_face& F = g_rc_faces[tri];
    const b_texture& T = g_textures[g_materials[F.dwMaterial].surfidx];
    Fvector B;
    B.set(1.0f - u - v, u, v); // W,U,V order, as CDB's
    Fvector2 uv;
    uv.x = F.t[0].x * B.x + F.t[1].x * B.y + F.t[2].x * B.z;
    uv.y = F.t[0].y * B.x + F.t[1].y * B.y + F.t[2].y * B.z;
    int U = iFloor(uv.x * float(T.dwWidth) + .5f);
    int V = iFloor(uv.y * float(T.dwHeight) + .5f);
    U %= T.dwWidth;
    if (U < 0)
        U += T.dwWidth;
    V %= T.dwHeight;
    if (V < 0)
        V += T.dwHeight;
    return 1.f - float(color_get_A(T.pSurface[V * T.dwWidth + U])) / 255.f;
}

void FilterOpaque(const RTCFilterFunctionNArguments* args) {
    CoverContext* ctx = reinterpret_cast<CoverContext*>(args->context);
    const xr_vector<u32>& tris = *static_cast<const xr_vector<u32>*>(args->geometryUserPtr);
    for (u32 i = 0; i < args->N; i++) {
        if (!args->valid[i])
            continue;
        if (Culled(tris[RTCHitN_primID(args->hit, args->N, i)], ctx->dir))
            args->valid[i] = 0; // a back face: go on
        else
            ctx->energy = 0.f; // blocked: the hit stands
    }
}

void FilterTransparent(const RTCFilterFunctionNArguments* args) {
    CoverContext* ctx = reinterpret_cast<CoverContext*>(args->context);
    const xr_vector<u32>& tris = *static_cast<const xr_vector<u32>*>(args->geometryUserPtr);
    for (u32 i = 0; i < args->N; i++) {
        if (!args->valid[i])
            continue;
        const u32 prim = RTCHitN_primID(args->hit, args->N, i);
        const u32 tri = tris[prim];
        if (!Culled(tri, ctx->dir) && ctx->count_once(prim))
            ctx->energy *= Transmission(tri, RTCHitN_u(args->hit, args->N, i), RTCHitN_v(args->hit, args->N, i));
        if (ctx->energy > 0.f)
            args->valid[i] = 0; // the light goes on; once nothing is left the hit stands and the query stops
    }
}

void AttachGeometry(xr_vector<u32>& tris, RTCFilterFunctionN filter) {
    if (tris.empty())
        return;
    RTCGeometry geometry = rtcNewGeometry(g_device, RTC_GEOMETRY_TYPE_TRIANGLE);
    float* vertices = static_cast<float*>(rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
                                                                  sizeof(Fvector), Level.get_verts_count()));
    std::memcpy(vertices, Level.get_verts(), sizeof(Fvector) * Level.get_verts_count());
    u32* indices = static_cast<u32*>(
        rtcSetNewGeometryBuffer(geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(u32), tris.size()));
    for (size_t i = 0; i < tris.size(); i++) {
        const CDB::TRI& T = Level.get_tris()[tris[i]];
        indices[i * 3 + 0] = T.verts[0];
        indices[i * 3 + 1] = T.verts[1];
        indices[i * 3 + 2] = T.verts[2];
    }
    rtcSetGeometryUserData(geometry, &tris);
    rtcSetGeometryOccludedFilterFunction(geometry, filter);
    rtcCommitGeometry(geometry);
    rtcAttachGeometry(g_scene, geometry);
    rtcReleaseGeometry(geometry);
}
} // namespace

void ai_embree_build() {
    if (!ai_embree_enabled())
        return;
    ai_embree_release();
    g_device = rtcNewDevice(nullptr);
    R_ASSERT2(g_device, "Embree: no device");
    rtcSetDeviceErrorFunction(
        g_device, [](void*, RTCError code, const char* text) { Msg("! Embree error %d: %s", int(code), text ? text : ""); },
        nullptr);
    Msg("* Embree %s", RTC_VERSION_STRING);

    // The faces as getLastRP_Scale sees them: a shader index out of the library blocks (its "hack"); no shadow: left
    // out; without alpha or without texels: blocking; the rest: by their texels
    Shader_xrLCVec& library = g_shaders_xrlc->Library();
    for (int i = 0; i < Level.get_tris_count(); i++) {
        const b_material& M = g_materials[g_rc_faces[i].dwMaterial];
        if (M.shader_xrlc < library.size() && !library[M.shader_xrlc].flags.bLIGHT_CastShadow)
            continue;
        const b_BuildTexture& T = g_textures[M.surfidx];
        const bool blocks = M.shader_xrlc >= library.size() || !T.bHasAlpha || T.pSurface == nullptr;
        (blocks ? g_opaque_tris : g_transparent_tris).push_back(u32(i));
    }

    g_scene = rtcNewScene(g_device);
    rtcSetSceneBuildQuality(g_scene, RTC_BUILD_QUALITY_HIGH);
    rtcSetSceneFlags(g_scene, RTC_SCENE_FLAG_ROBUST);
    AttachGeometry(g_opaque_tris, &FilterOpaque);
    AttachGeometry(g_transparent_tris, &FilterTransparent);
    rtcCommitScene(g_scene);
    Msg("* Embree scene: %u opaque, %u transparent faces", u32(g_opaque_tris.size()), u32(g_transparent_tris.size()));
}

void ai_embree_release() {
    if (g_scene)
        rtcReleaseScene(g_scene);
    if (g_device)
        rtcReleaseDevice(g_device);
    g_scene = nullptr;
    g_device = nullptr;
    g_opaque_tris.clear();
    g_transparent_tris.clear();
}

bool ai_embree_ready() { return g_scene != nullptr; }

float ai_embree_trace(const Fvector& P, const Fvector& D, float range) {
    CoverContext ctx;
    rtcInitRayQueryContext(&ctx.context);
    ctx.dir = D;
    ctx.energy = 1.f;
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
    return ray.tfar < 0.f ? 0.f : ctx.energy; // tfar = -inf: a hit stood
}

#else // XRAI_EMBREE

void ai_embree_build() {
    if (strstr(Core.Params, "-embree"))
        Msg("! -embree: this xrAI is built without Embree; tracing with OPCODE");
}
void ai_embree_release() {}
bool ai_embree_ready() { return false; }
float ai_embree_trace(const Fvector&, const Fvector&, float) { return 1.f; }

#endif // XRAI_EMBREE
