#include "stdafx.h"
#include "xrLightVertex.h"
#include "xrThread.h"
#include "xrface.h"
#include "xrLC_GlobalData.h"
#include "light_point.h"

#include "xrcdb/xrCDB.h"
//-----------------------------------------------------------------------
typedef xr_multimap<float, vecVertex> mapVert;
typedef mapVert::iterator mapVertIt;
mapVert* g_trans;
std::recursive_mutex g_trans_CS;
extern XRLC_LIGHT_API void LightPoint(CDB::COLLIDER* DB, CDB::MODEL* MDL, base_color_c& C,
                                      Fvector& P, Fvector& N, base_lighting& lights, u32 flags,
                                      Face* skip);

void g_trans_register_internal(Vertex* V) {
    R_ASSERT(V);

    const float eps = EPS_L;
    const float eps2 = 2.f * eps;

    // Search
    const float key = V->P.x;
    mapVertIt it = g_trans->lower_bound(key);
    mapVertIt it2 = it;

    // Decrement to the start and inc to end
    while (it != g_trans->begin() && ((it->first + eps2) > key))
        it--;
    while (it2 != g_trans->end() && ((it2->first - eps2) < key))
        it2++;
    if (it2 != g_trans->end())
        it2++;

    // Search
    for (; it != it2; it++) {
        vecVertex& VL = it->second;
        Vertex* Front = VL.front();
        R_ASSERT(Front);
        if (Front->P.similar(V->P, eps)) {
            VL.push_back(V);
            return;
        }
    }

    // Register
    mapVertIt ins = g_trans->insert(std::make_pair(key, vecVertex()));
    ins->second.reserve(32);
    ins->second.push_back(V);
}
void g_trans_register(Vertex* V) {
    std::lock_guard<decltype(g_trans_CS)> lock(g_trans_CS);
    g_trans_register_internal(V);
}

//////////////////////////////////////////////////////////////////////////
bool GetTranslucency(const Vertex* V, float& v_trans) {
    // Get transluency factor

    bool bVertexLight = FALSE;
    u32 L_flags = 0;
    for (u32 f = 0; f < V->m_adjacents.size(); ++f) {
        Face* F = V->m_adjacents[f];
        v_trans += F->Shader().vert_translucency;
        if (F->Shader().flags.bLIGHT_Vertex)
            bVertexLight = TRUE;
    }
    v_trans /= float(V->m_adjacents.size());
    return bVertexLight;
}

// The vertices [first, end) that are lit per vertex, with the worker's own collider
static void LightVertices(CDB::COLLIDER& DB, base_lighting& lights, u32 first, u32 end) {
    for (u32 id = first; id < end; id++) {
        Vertex* V = lc_global_data()->g_vertices()[id];
        R_ASSERT(V);

        float v_trans = 0.f;
        if (GetTranslucency(V, v_trans)) {
            base_color_c vC, old;
            V->C._get(old);

            LightPoint(&DB, lc_global_data()->RCAST_Model(), vC, V->P, V->N, lights,
                       (lc_global_data()->b_nosun() ? LP_dont_sun : 0) | LP_dont_hemi, 0);
            vC._tmp_ = v_trans;
            vC.mul(.5f);
            vC.hemi = old.hemi; // preserve pre-calculated hemisphere
            V->C._set(vC);

            g_trans_register(V);
        }
    }
}
namespace lc_net {
void RunLightVertexNet();
}
void LightVertex(bool net) {
    g_trans = xr_new<mapVert>();

    // One worker per hardware thread, taking runs of vertices (IX-Ray's scheme; GSC: 4 threads)
    Status("Calculating...");
    if (!net) {
        CTimer start_time;
        start_time.Start();
        const u32 count = u32(lc_global_data()->g_vertices().size());
        const u32 run = 64;
        std::atomic<u32> next{0};
        lc_run_workers([&](u32 thread) {
            CDB::COLLIDER DB;
            DB.ray_options(0);
            base_lighting lights = lc_global_data()->L_static(); // own copy: the lights cache their last occluder
            for (u32 first; lc_take(next, count, first, run);) {
                LightVertices(DB, lights, first, std::min(first + run, count));
                if (thread == 0)
                    Progress(float(first) / float(count));
            }
        });
        clMsg("%f seconds", start_time.GetElapsed_sec());
    } else {
        lc_net::RunLightVertexNet();
    }
    // Process all groups
    Status("Transluenting...");
    for (mapVertIt it = g_trans->begin(); it != g_trans->end(); it++) {
        // Unique
        vecVertex& VL = it->second;
        std::sort(VL.begin(), VL.end());
        VL.erase(std::unique(VL.begin(), VL.end()), VL.end());

        // Calc summary color
        base_color_c C;
        for (u32 v = 0; v < VL.size(); v++) {
            base_color_c cc;
            VL[v]->C._get(cc);
            C.max(cc);
        }

        // Calculate final vertex color
        for (u32 v = 0; v < VL.size(); v++) {
            base_color_c vC;
            VL[v]->C._get(vC);

            // trans-level
            float level = vC._tmp_;

            //
            base_color_c R;
            R.lerp(vC, C, level);
            R.max(vC);
            VL[v]->C._set(R);
        }
    }
    xr_delete(g_trans);
    Status("Wating...");
}
