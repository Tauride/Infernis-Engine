#include "stdafx.h"
#include "mu_model_light_threads.h"

#include "xrface.h"
#include "xrMU_Model.h"
#include "xrMU_Model_Reference.h"

#include "xrlc_globaldata.h"

#include "mu_model_light.h"
#include "mu_light_net.h"

//#include "mu_model_face.h"

#include "xrThread.h"

CThreadManager mu_base;
CThreadManager mu_secondary;
// mu-light
bool mu_models_local_calc_lightening = false;
std::recursive_mutex mu_models_local_calc_lightening_wait_lock;
void WaitMuModelsLocalCalcLightening() {
    for (;;) {
        bool complited = false;
        Sleep(1000);
        mu_models_local_calc_lightening_wait_lock.lock();
        complited = mu_models_local_calc_lightening;
        mu_models_local_calc_lightening_wait_lock.unlock();
        if (complited)
            break;
    }
}
void SetMuModelsLocalCalcLighteningCompleted() {
    std::lock_guard<std::recursive_mutex> lock(mu_models_local_calc_lightening_wait_lock);
    mu_models_local_calc_lightening = true;
}
// void LC_WaitRefModelsNet();
class CMUThread : public CThread {
public:
    CMUThread(u32 ID) : CThread(ID) { thMessages = FALSE; }
    virtual void Execute() {

        // Priority
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        Sleep(0);

        // Light models

        if (mu_light_net) {
            lc_net::RunBaseModelsNet();
            lc_net::RunRefModelsNet();
            return;
            // lc_net::WaitRefModelsNet();
        }

        // One worker per hardware thread each time, taking models, then references, from a counter (IX-Ray's
        // scheme; GSC lit the models on this thread alone and split the references into fixed ranges)
        {
            const u32 count = u32(inlc_global_data()->mu_models().size());
            std::atomic<u32> next{0};
            lc_run_workers([&](u32) {
                // below normal, as GSC's MU thread: the MU models fill the time the main phases leave
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
                for (u32 m; lc_take(next, count, m);) {
                    inlc_global_data()->mu_models()[m]->calc_materials();
                    inlc_global_data()->mu_models()[m]->calc_lighting();
                }
            });
        }

        SetMuModelsLocalCalcLighteningCompleted();

        {
            const u32 count = u32(inlc_global_data()->mu_refs().size());
            std::atomic<u32> next{0};
            lc_run_workers([&](u32) {
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
                for (u32 r; lc_take(next, count, r);)
                    inlc_global_data()->mu_refs()[r]->calc_lighting();
            });
        }
    }
};

void run_mu_base(bool net) { mu_base.start(xr_new<CMUThread>(0)); }

void wait_mu_base_thread() { mu_base.wait(500); }
void wait_mu_secondary_thread() { mu_secondary.wait(500); }