#include "stdafx.h"
#include "xrlight_implicitrun.h"
#include "xrThread.h"
#include "xrLight_Implicit.h"
#include "xrlight_implicitdeflector.h"
// One worker per hardware thread, taking runs of 8 rows (IX-Ray's scheme). GSC split the rows into 8 fixed bands of
// Height / 8, which left the last Height % 8 rows unlit.
void RunImplicitMultithread(ImplicitDeflector& defl) {
    const u32 rows = defl.Height();
    const u32 run = 8;
    std::atomic<u32> next{0};
    lc_run_workers([&](u32) {
        for (u32 first; lc_take(next, rows, first, run);) {
            ImplicitExecute execute(first, std::min(first + run, rows));
            execute.Execute(0);
        }
    });
}
