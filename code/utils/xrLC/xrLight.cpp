#include "stdafx.h"
#include <numeric>
#include "build.h"

#include "../xrlc_light/xrdeflector.h"
#include "../xrlc_light/xrThread.h"
#include "../xrLC_Light/xrLC_GlobalData.h"
#include "../xrLC_Light/xrLightVertex.h"

#include "net.h"
//#include "../xrLC_Light/net_task_manager.h"
#include "../xrLC_Light/lcnet_task_manager.h"
#include "../xrLC_Light/mu_model_light.h"
void CBuild::LMapsLocal() {
    FPU::m64r();

    mem_Compact();

    // Randomize deflectors
    xr_vector<CDeflector*>& deflectors = lc_global_data()->g_deflectors();
    std::shuffle(deflectors.begin(), deflectors.end(), rng);

    // The largest deflectors first: a large one taken near the end would keep the other workers waiting. Each
    // deflector is lit on its own, so the order does not change the result.
    xr_vector<u32> order(deflectors.size());
    std::iota(order.begin(), order.end(), 0u);
    std::stable_sort(order.begin(), order.end(), [&deflectors](u32 a, u32 b) {
        return deflectors[a]->layer.Area() > deflectors[b]->layer.Area();
    });

    // Main process: one worker per hardware thread, each with its own hash, collider and lights (IX-Ray's scheme)
    Status("Lighting...");
    CTimer start_time;
    start_time.Start();
    std::atomic<u32> next{0};
    const u32 count = u32(order.size());
    lc_run_workers([&](u32 thread) {
        auto H = std::make_unique<HASH>(); // 128x128 cells: too large for a thread's stack
        CDB::COLLIDER DB;
        base_lighting LightsSelected;
        for (u32 item; lc_take(next, count, item);) {
            try {
                deflectors[order[item]]->Light(&DB, &LightsSelected, *H);
            } catch (...) {
                clMsg("* ERROR: CBuild::LMapsLocal - light");
            }
            if (thread == 0)
                Progress(float(item) / float(count));
        }
    });
    Progress(1.f);
    clMsg("%f seconds", start_time.GetElapsed_sec());
}

void CBuild::LMaps() {
    //****************************************** Lmaps
    Phase("LIGHT: LMaps...");
// DeflectorsStats ();
#ifndef NET_CMP
    if (g_build_options.b_net_light)

        // net_light ();
        lc_net::net_lightmaps();
    else {
        LMapsLocal();
    }
#else
    create_net_task_manager();
    get_net_task_manager()->create_global_data_write(pBuild->path);
    LMapsLocal();
    get_net_task_manager()->run();
    destroy_net_task_manager();
// net_light ();
#endif
}
void XRLC_LIGHT_API ImplicitNetWait();
void CBuild::Light() {
    //****************************************** Implicit
    {
        FPU::m64r();
        Phase("LIGHT: Implicit...");
        mem_Compact();
        ImplicitLighting();
    }

    LMaps();

    //****************************************** Vertex
    FPU::m64r();
    Phase("LIGHT: Vertex...");
    mem_Compact();

    LightVertex();

    //

    ImplicitNetWait();
    WaitMuModelsLocalCalcLightening();
    lc_net::get_task_manager().wait_all();
    //	get_task_manager().wait_all();
    lc_net::get_task_manager().release();
    //
    //****************************************** Merge LMAPS
    {
        FPU::m64r();
        Phase("LIGHT: Merging lightmaps...");
        mem_Compact();

        xrPhase_MergeLM();
    }
}

void CBuild::LightVertex() { ::LightVertex(!!g_build_options.b_net_light); }