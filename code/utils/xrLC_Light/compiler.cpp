#include "stdafx.h"
#include "xrEngine/xrlevel.h"

#include "xrThread.h"

#include "global_calculation_data.h"
#include "lightthread.h"
#include "xrLightDoNet.h"

void xrLight() {
    const u32 range = gl_data.slots_data.size_z();

    // One worker per hardware thread, taking rows of slots from a counter (IX-Ray's scheme; GSC split the rows into
    // fixed ranges, one per thread)
    CTimer start_time;
    start_time.Start();
    std::atomic<u32> next{0};
    lc_run_workers([&](u32 thread) {
        for (u32 z; lc_take(next, range, z);) {
            LightThread row(thread, z, z + 1);
            row.thMessages = FALSE;
            row.thMonitor = FALSE;
            row.Execute();
            if (thread == 0)
                Progress(float(z) / float(range));
        }
    });
    Msg("%d seconds elapsed.", (start_time.GetElapsed_ms()) / 1000);
}

void xrCompileDO(bool net) {
    Phase("Loading level...");
    gl_data.xrLoad();

    Phase("Lighting nodes...");
    if (net)
        lc_net::xrNetDOLight();
    else
        xrLight();

    gl_data.slots_data.Free();
}
